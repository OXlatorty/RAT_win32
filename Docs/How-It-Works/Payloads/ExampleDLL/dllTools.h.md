# dllTools.h

## Overview
`dllTools.h` is the single header for the **DllTools** module. It centralises all external dependencies, defines the custom relocation structs used during manual mapping, and declares the `DllTools` namespace interface consumed by `main.cpp`.

---

## Dependencies

| Include / Pragma | Purpose |
| :--- | :--- |
| `<Windows.h>` | Core Win32 API — process/memory/file primitives. |
| `<winternl.h>` | Undocumented NT structures: `PROCESS_BASIC_INFORMATION`, `PROCESSINFOCLASS`. |
| `<stdint.h>` | Fixed-width integer types (`uintptr_t`, `uint64_t`, …). |
| `<vector>` | `std::vector<BYTE>` used as the DLL file buffer in `AutoInject`. |
| `<iostream>` | Available for debug output; not actively used in the current build. |
| `<psapi.h>` | `EnumProcessModules` / `GetModuleBaseNameA` for remote module enumeration. |
| `#pragma comment(lib, "ntdll.lib")` | Links `ntdll.lib` so `NtUnmapViewOfSection` can be resolved at link time. |
| `#pragma comment(lib, "psapi.lib")` | Links `psapi.lib` for the PSAPI functions above. |

---

## Type Alias

```cpp
using NtUnmapViewOfSection = NTSTATUS(WINAPI*)(HANDLE, PVOID);
```

Function-pointer type for `NtUnmapViewOfSection`, resolved at runtime via `GetProcAddress` inside `AutoInject`. Defined here so it can be reused if needed in other translation units.

---

## Structs

### `BASE_RELOCATION_BLOCK`
```cpp
typedef struct BASE_RELOCATION_BLOCK {
    DWORD PageAddress;
    DWORD BlockSize;
} BASE_RELOCATION_BLOCK, *PBASE_RELOCATION_BLOCK;
```
Mirrors the layout of `IMAGE_BASE_RELOCATION`. Kept as a named alias for readability when iterating relocation blocks. Not directly used in the current implementation (the code casts to `PIMAGE_BASE_RELOCATION` directly), but available for alternative traversal patterns.

### `BASE_RELOCATION_ENTRY`
```cpp
typedef struct BASE_RELOCATION_ENTRY {
    USHORT Offset : 12;
    USHORT Type   : 4;
} BASE_RELOCATION_ENTRY, *PBASE_RELOCATION_ENTRY;
```
Splits a 16-bit relocation entry into its two fields via bit-fields. The current implementation extracts these fields manually with bit-shifts (`entry[i] >> 12` for type, `entry[i] & 0xFFF` for offset); this struct provides a cleaner alternative if you want to refactor that loop.

---

## Namespace

### `DllTools`
```cpp
namespace DllTools {
    int AutoInject(LPSTR target, LPCSTR payload);
}
```

| Function | Description |
| :--- | :--- |
| `AutoInject` | Launches `target` suspended, manually maps `payload` DLL into it, fixes relocations and imports, then resumes execution. Returns `0` on success, `-1` on any failure. See [`dllTools.cpp`](dllTools.cpp.md) for full details. |

---

## Modifying

- **Adding a new utility** — declare it inside `namespace DllTools` here and implement it in `dllTools.cpp`.
- **Using `BASE_RELOCATION_ENTRY` in the reloc loop** — replace the manual bit-shift extraction in `AutoInject` with a cast to `PBASE_RELOCATION_ENTRY` for cleaner, self-documenting code.
- **Removing the PSAPI dependency** — if the target environment does not have `psapi.dll`, replace `GetRemoteModuleHandle` with a `TlHelp32`-based alternative (`CreateToolhelp32Snapshot` + `Module32First`/`Module32Next`) and drop both `<psapi.h>` and its `#pragma comment`.