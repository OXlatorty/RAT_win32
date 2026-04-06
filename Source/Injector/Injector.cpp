#include <Windows.h>
#include <iostream>

DWORD AliginSectionHeader(DWORD sectionSize, DWORD aligment, DWORD address) {
	if (sectionSize % aligment == 0)
		return address + sectionSize;
	return address + ((sectionSize / aligment) + 1) * aligment;
}

INT main(INT arg, PCHAR argv[]) {
	if (argv[0] && argv[1] && argv[2] && argv[3]) {
		PCHAR sectionName = argv[1];
		char* targetProcess = argv[2];
		char* dllPath = argv[3];

		HANDLE xFile = CreateFileA(targetProcess, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		if (xFile != INVALID_HANDLE_VALUE) {
			DWORD fileSize = GetFileSize(xFile, NULL);
			PBYTE fileBuffer = PBYTE(LocalAlloc(LPTR, fileSize));
			DWORD returnedBytes;

			BOOL fileRead = ReadFile(xFile, fileBuffer, fileSize, &returnedBytes, NULL);
			if (fileRead == TRUE && returnedBytes == fileSize) {
				PIMAGE_DOS_HEADER imageDosHeader = (PIMAGE_DOS_HEADER)fileBuffer;
				if (imageDosHeader->e_magic == IMAGE_DOS_SIGNATURE) {
					PIMAGE_FILE_HEADER imageFileHeader = (PIMAGE_FILE_HEADER)(fileBuffer + imageDosHeader->e_lfanew + sizeof(DWORD));
					PIMAGE_OPTIONAL_HEADER imageOptionalHeader = (PIMAGE_OPTIONAL_HEADER)(fileBuffer + imageDosHeader->e_lfanew + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER));
					PIMAGE_SECTION_HEADER imageSectionHeader = (PIMAGE_SECTION_HEADER)(fileBuffer + imageDosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS));
					WORD PESections = imageFileHeader->NumberOfSections;

					ZeroMemory(&imageSectionHeader[PESections], sizeof(IMAGE_SECTION_HEADER));
					CopyMemory(&imageSectionHeader[PESections].Name, sectionName, 8);

					HANDLE codeFile = CreateFileA(dllPath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
					if (codeFile != INVALID_HANDLE_VALUE) {
						DWORD dllSize = GetFileSize(codeFile, 0);
						if (dllSize > 0) {
							PBYTE dllBuffer = PBYTE(LocalAlloc(LPTR, dllSize));
							if (dllBuffer != NULL) {
								DWORD returnedDllBytes, unusedBytes;
								BOOL dllRead = ReadFile(codeFile, dllBuffer, dllSize, &returnedDllBytes, NULL);
								if (dllRead == TRUE && returnedDllBytes == dllSize) {
									if (SetFilePointer(xFile, imageSectionHeader[PESections].PointerToRawData + imageSectionHeader[PESections].SizeOfRawData, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER) {
										imageSectionHeader[PESections].Misc.VirtualSize = AliginSectionHeader(dllSize, imageOptionalHeader->SectionAlignment, 0);
										imageSectionHeader[PESections].VirtualAddress = AliginSectionHeader(imageSectionHeader[PESections - 1].Misc.VirtualSize, imageOptionalHeader->SectionAlignment, imageSectionHeader[PESections - 1].VirtualAddress);
										imageSectionHeader[PESections].SizeOfRawData = AliginSectionHeader(dllSize, imageOptionalHeader->FileAlignment, 0);
										imageSectionHeader[PESections].PointerToRawData = AliginSectionHeader(imageSectionHeader[PESections - 1].SizeOfRawData, imageOptionalHeader->FileAlignment, imageSectionHeader[PESections - 1].PointerToRawData);
										imageSectionHeader[PESections].Characteristics = IMAGE_SCN_MEM_WRITE | IMAGE_SCN_CNT_CODE | IMAGE_SCN_CNT_UNINITIALIZED_DATA | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ;

										if (SetFilePointer(xFile, imageSectionHeader[PESections].PointerToRawData + imageSectionHeader[PESections].SizeOfRawData, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER) {
											SetEndOfFile(xFile);
										}

										imageOptionalHeader->SizeOfImage = imageSectionHeader[PESections].VirtualAddress + imageSectionHeader[PESections].Misc.VirtualSize;
										imageFileHeader->NumberOfSections++;

										if (SetFilePointer(xFile, 0, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER) {
											WriteFile(xFile, fileBuffer, fileSize, &returnedBytes, NULL);
										}

										WriteFile(xFile, dllBuffer, dllSize, &unusedBytes, NULL);
										std::cout << "\nOperation successfully, thanks for using my Injector.\nAuthor: OXlatorty." << std::endl;

										return 0;
									}
								}
								LocalFree(dllBuffer);
							}
						}
					}
					CloseHandle(codeFile);
				}
			}
			LocalFree(fileBuffer);
		}
		CloseHandle(xFile);
	}

	return -1;
}