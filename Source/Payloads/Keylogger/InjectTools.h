#pragma once

#include <Windows.h>
#include <winternl.h>
#include <stdint.h>
#include <vector>
#include <iostream>
#include <psapi.h>
#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "psapi.lib")

using NtUnmapViewOfSection = NTSTATUS(WINAPI*)(HANDLE, PVOID);

typedef struct BASE_RELOCATION_BLOCK {
	DWORD PageAddress;
	DWORD BlockSize;
} BASE_RELOCATION_BLOCK, * PBASE_RELOCATION_BLOCK;

typedef struct BASE_RELOCATION_ENTRY {
	USHORT Offset : 12;
	USHORT Type : 4;
} BASE_RELOCATION_ENTRY, * PBASE_RELOCATION_ENTRY;

namespace InjectTools {
	int AutoInject(LPSTR target, LPCSTR payload);
}