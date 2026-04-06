#include "includes.h"

PVOID Tools::GetImageBase() {
	MEMORY_BASIC_INFORMATION mbi;
	VirtualQuery((LPCVOID)Tools::GetImageBase, &mbi, sizeof(mbi));
	PBYTE moduleBase = (PBYTE)mbi.AllocationBase;

	PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)moduleBase;
	if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return NULL; 

	PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)(moduleBase + dosHeader->e_lfanew);
	if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return NULL;

	return (PVOID)moduleBase;
}

PBYTE Tools::ExtractDllFile(PBYTE moduleBase, PDWORD moduleSize) {
	PIMAGE_DOS_HEADER imageDosHeader = (PIMAGE_DOS_HEADER)(moduleBase);
	if (imageDosHeader->e_magic == IMAGE_DOS_SIGNATURE) {
		PIMAGE_NT_HEADERS32 imageNtHeaders32 = (PIMAGE_NT_HEADERS32)(moduleBase + imageDosHeader->e_lfanew);
		PIMAGE_NT_HEADERS64 imageNtHeaders64 = (PIMAGE_NT_HEADERS64)(moduleBase + imageDosHeader->e_lfanew);

		if (imageNtHeaders32->Signature == IMAGE_NT_SIGNATURE) {
			if (imageNtHeaders32->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
				PIMAGE_SECTION_HEADER firstSection = IMAGE_FIRST_SECTION(imageNtHeaders32);
				PIMAGE_SECTION_HEADER dllSection = (firstSection + imageNtHeaders32->FileHeader.NumberOfSections - 1);

				if (dllSection) {
					*moduleSize = dllSection->Misc.VirtualSize;
					return RtlOffsetToPointer(moduleBase, dllSection->VirtualAddress);
				}
			} else if (imageNtHeaders64->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
				PIMAGE_SECTION_HEADER firstSection = IMAGE_FIRST_SECTION(imageNtHeaders64);
				PIMAGE_SECTION_HEADER dllSection = (firstSection + imageNtHeaders64->FileHeader.NumberOfSections - 1);

				if (dllSection) {
					*moduleSize = dllSection->Misc.VirtualSize;
					return RtlOffsetToPointer(moduleBase, dllSection->VirtualAddress);
				}
			}
		}
	}

	return NULL;
}
