#include "dllTools.h"

uintptr_t RvaToRaw(uintptr_t rva, PIMAGE_NT_HEADERS ntHeaders, LPVOID fileBuffer) {
    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(ntHeaders);
    for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
        if (rva >= section->VirtualAddress && rva < (section->VirtualAddress + section->Misc.VirtualSize)) {
            return (uintptr_t)fileBuffer + section->PointerToRawData + (rva - section->VirtualAddress);
        }
        section++;
    }
    return 0;
}

uintptr_t GetExportAddress(LPVOID fileBuffer, const char* exportName) {
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)fileBuffer;
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((uintptr_t)fileBuffer + dosHeader->e_lfanew);

    IMAGE_DATA_DIRECTORY exportDir = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (exportDir.Size == 0) return 0;

    PIMAGE_EXPORT_DIRECTORY exports = (PIMAGE_EXPORT_DIRECTORY)RvaToRaw(exportDir.VirtualAddress, ntHeaders, fileBuffer);
    DWORD* names = (DWORD*)RvaToRaw(exports->AddressOfNames, ntHeaders, fileBuffer);
    DWORD* functions = (DWORD*)RvaToRaw(exports->AddressOfFunctions, ntHeaders, fileBuffer);
    WORD* ordinals = (WORD*)RvaToRaw(exports->AddressOfNameOrdinals, ntHeaders, fileBuffer);

    for (DWORD i = 0; i < exports->NumberOfNames; i++) {
        const char* name = (const char*)RvaToRaw(names[i], ntHeaders, fileBuffer);
        if (strcmp(name, exportName) == 0) {
            return (uintptr_t)functions[ordinals[i]];
        }
    }

    return 0;
}

uintptr_t GetRemoteModuleHandle(HANDLE hProcess, const char* moduleName) {
    HMODULE hMods[1024];
    DWORD cbNeeded;
    if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
        for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
            char szModName[MAX_PATH];
            if (GetModuleBaseNameA(hProcess, hMods[i], szModName, sizeof(szModName))) {
                if (_stricmp(szModName, moduleName) == 0) {
                    return (uintptr_t)hMods[i];
                }
            }
        }
    }
    return 0;
}

int DllTools::AutoInject(LPSTR target, LPCSTR payload) {
    HMODULE hNtDll = GetModuleHandleA("ntdll.dll");
    if (!hNtDll) return -1;

    STARTUPINFOA startupInfo = { sizeof(startupInfo) };
    PROCESS_INFORMATION processInformation = { 0 };
    PROCESS_BASIC_INFORMATION pbi = { 0 };

    if (!CreateProcessA(NULL, target, NULL, NULL, TRUE, CREATE_SUSPENDED, NULL, NULL, &startupInfo, &processInformation)) return -1;
    HANDLE targetProcess = processInformation.hProcess;

    typedef NTSTATUS(NTAPI* pNtQueryInformationProcess)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);
    auto NtQueryInfo = (pNtQueryInformationProcess)GetProcAddress(hNtDll, "NtQueryInformationProcess");
    ULONG retLen = 0;
    NtQueryInfo(targetProcess, ProcessBasicInformation, &pbi, sizeof(pbi), &retLen);

    HANDLE hFile = CreateFileA(payload, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        TerminateProcess(targetProcess, 0);
        return -1;
    }
    
    DWORD fileSize = GetFileSize(hFile, NULL);
    std::vector<BYTE> dllBuffer(fileSize);
    DWORD read = 0;
    ReadFile(hFile, dllBuffer.data(), fileSize, &read, NULL);
    CloseHandle(hFile);

    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)dllBuffer.data();
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        TerminateProcess(targetProcess, 0);
        return -1;
    }

    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((uintptr_t)dllBuffer.data() + dosHeader->e_lfanew);
    uintptr_t remoteImageBasePtr = (uintptr_t)pbi.PebBaseAddress + 0x10;
    LPVOID remoteImageBase = 0;
    ReadProcessMemory(targetProcess, (LPCVOID)remoteImageBasePtr, &remoteImageBase, sizeof(LPVOID), NULL);

    typedef NTSTATUS(NTAPI* pNtUnmapViewOfSection)(HANDLE, PVOID);
    auto unmap = (pNtUnmapViewOfSection)GetProcAddress(hNtDll, "NtUnmapViewOfSection");
    if (unmap) unmap(targetProcess, remoteImageBase);

    LPVOID newBase = VirtualAllocEx(targetProcess, remoteImageBase, ntHeaders->OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!newBase) newBase = VirtualAllocEx(targetProcess, NULL, ntHeaders->OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!newBase) {
        TerminateProcess(targetProcess, 0);
        return -1;
    }

    uintptr_t delta = (uintptr_t)newBase - ntHeaders->OptionalHeader.ImageBase;
    ntHeaders->OptionalHeader.ImageBase = (uintptr_t)newBase;
    WriteProcessMemory(targetProcess, newBase, dllBuffer.data(), ntHeaders->OptionalHeader.SizeOfHeaders, NULL);

    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(ntHeaders);
    for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
        if (section->SizeOfRawData > 0) {
            PVOID dest = (PVOID)((uintptr_t)newBase + section->VirtualAddress);
            PVOID src = (PVOID)((uintptr_t)dllBuffer.data() + section->PointerToRawData);
            WriteProcessMemory(targetProcess, dest, src, section->SizeOfRawData, NULL);
        }
        section++;
    }

    IMAGE_DATA_DIRECTORY relocDir = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (relocDir.Size > 0 && delta != 0) {
        uintptr_t relocAddr = RvaToRaw(relocDir.VirtualAddress, ntHeaders, dllBuffer.data());
        uintptr_t endReloc = relocAddr + relocDir.Size;

        while (relocAddr < endReloc) {
            PIMAGE_BASE_RELOCATION block = (PIMAGE_BASE_RELOCATION)relocAddr;
            if (block->SizeOfBlock == 0) break;

            DWORD count = (block->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
            PWORD entry = (PWORD)(relocAddr + sizeof(IMAGE_BASE_RELOCATION));
            for (DWORD i = 0; i < count; i++) {
                uintptr_t type = entry[i] >> 12;
                uintptr_t offset = entry[i] & 0xFFF;
                if (type == IMAGE_REL_BASED_DIR64) {
                    uintptr_t patchAddr = block->VirtualAddress + offset;
                    uintptr_t remoteValue = 0;
                    ReadProcessMemory(targetProcess, (PVOID)((uintptr_t)newBase + patchAddr), &remoteValue, sizeof(uintptr_t), NULL);
                    remoteValue += delta;
                    WriteProcessMemory(targetProcess, (PVOID)((uintptr_t)newBase + patchAddr), &remoteValue, sizeof(uintptr_t), NULL);
                }
            }

            relocAddr += block->SizeOfBlock;
        }
    }

	IMAGE_DATA_DIRECTORY importDir = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	if (importDir.Size > 0) {
		PIMAGE_IMPORT_DESCRIPTOR importDesc = (PIMAGE_IMPORT_DESCRIPTOR)RvaToRaw(importDir.VirtualAddress, ntHeaders, dllBuffer.data());

		while (importDesc->Name) {
			const char* libName = (const char*)RvaToRaw(importDesc->Name, ntHeaders, dllBuffer.data());
			
			uintptr_t hRemoteLib = GetRemoteModuleHandle(targetProcess, libName);
			if (!hRemoteLib) hRemoteLib = (uintptr_t)LoadLibraryA(libName); 
			
			PIMAGE_THUNK_DATA firstThunk = (PIMAGE_THUNK_DATA)RvaToRaw(importDesc->FirstThunk, ntHeaders, dllBuffer.data());
			PIMAGE_THUNK_DATA originalFirstThunk = (PIMAGE_THUNK_DATA)RvaToRaw(importDesc->OriginalFirstThunk, ntHeaders, dllBuffer.data());

			while (originalFirstThunk->u1.AddressOfData) {
				uintptr_t funcAddr = 0;
				if (IMAGE_SNAP_BY_ORDINAL(originalFirstThunk->u1.Ordinal)) {
					funcAddr = (uintptr_t)GetProcAddress(GetModuleHandleA(libName), (LPCSTR)IMAGE_ORDINAL(originalFirstThunk->u1.Ordinal));
				} else {
					PIMAGE_IMPORT_BY_NAME importByName = (PIMAGE_IMPORT_BY_NAME)RvaToRaw(originalFirstThunk->u1.AddressOfData, ntHeaders, dllBuffer.data());
					funcAddr = (uintptr_t)GetProcAddress(GetModuleHandleA(libName), importByName->Name);
				}

				WriteProcessMemory(targetProcess, (LPVOID)((uintptr_t)newBase + importDesc->FirstThunk + ((uintptr_t)firstThunk - (uintptr_t)RvaToRaw(importDesc->FirstThunk, ntHeaders, dllBuffer.data()))), &funcAddr, sizeof(uintptr_t), NULL);

				firstThunk++;
				originalFirstThunk++;
			}
			importDesc++;
		}
	}

    WriteProcessMemory(targetProcess, (PVOID)remoteImageBasePtr, &newBase, sizeof(LPVOID), NULL);

    CONTEXT ctx;
    ctx.ContextFlags = CONTEXT_FULL;
    GetThreadContext(processInformation.hThread, &ctx);

	uintptr_t customEntryRVA = GetExportAddress(dllBuffer.data(), "entryPoint");
    uintptr_t dllMainRVA = ntHeaders->OptionalHeader.AddressOfEntryPoint;
    if (customEntryRVA == 0) customEntryRVA = ntHeaders->OptionalHeader.AddressOfEntryPoint;

    // Shellcode ensures DllMain is called before execution of custom entryPoint.
    // This is critical for C++ Runtime initialization (e.g., std::ofstream).
    BYTE shellcode[] = {
        0x48, 0x83, 0xEC, 0x28,                                     // sub rsp, 40 (Shadow space + stack alignment)
        
        // --- Call DllMain(hinstDLL, DLL_PROCESS_ATTACH, NULL) ---
        0x48, 0xB9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rcx, <newBase> (hInstance)
        0x48, 0xC7, 0xC2, 0x01, 0x00, 0x00, 0x00,                   // mov rdx, 1 (DLL_PROCESS_ATTACH)
        0x4D, 0x31, 0xC0,                                           // xor r8, r8 (lpvReserved = 0)
        0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, <DllMainAddr>
        0xFF, 0xD0,                                                 // call rax
        
        // --- Call custom entryPoint() ---
        0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, <entryPointAddr>
        0xFF, 0xD0,                                                 // call rax
        
        0x48, 0x83, 0xC4, 0x28,                                     // add rsp, 40 (Restore stack pointer)
        
        // --- Return to original execution flow ---
        0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, <OriginalRIP>
        0xFF, 0xE0                                                  // jmp rax
    };

    // Patch addresses into the shellcode buffer
    *(uintptr_t*)(shellcode + 6)  = (uintptr_t)newBase;                // Update rcx with newBase
    *(uintptr_t*)(shellcode + 26) = (uintptr_t)newBase + dllMainRVA;   // Update rax with DllMain address
    *(uintptr_t*)(shellcode + 38) = (uintptr_t)newBase + customEntryRVA;// Update rax with entryPoint address
    *(uintptr_t*)(shellcode + 54) = (uintptr_t)ctx.Rip;                // Update rax with original RIP
    
    LPVOID scAddr = VirtualAllocEx(targetProcess, NULL, sizeof(shellcode), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    WriteProcessMemory(targetProcess, scAddr, shellcode, sizeof(shellcode), NULL);

    ctx.Rip = (uintptr_t)scAddr;
    SetThreadContext(processInformation.hThread, &ctx);
    ResumeThread(processInformation.hThread);

    CloseHandle(processInformation.hThread);
    CloseHandle(processInformation.hProcess);

    return 0;
}