# tools.cpp

## Overview
`tools.cpp` implements the two low-level PE-parsing utilities declared in [`tools.h`](tools.h.md). Together they form the payload extraction pipeline: `GetImageBase` finds where the running executable is loaded, and `ExtractDllFile` navigates the section table to return a pointer to the embedded payload.

---

## Functions

### `Tools::GetImageBase`
```cpp
PVOID Tools::GetImageBase();
```

Dynamically resolves the base address of the currently running executable in memory.

**Step-by-step logic:**

1. Calls `VirtualQuery` on the address of `GetImageBase` itself (a pointer guaranteed to lie within the current module's memory region) to obtain a `MEMORY_BASIC_INFORMATION` structure.
2. Reads `mbi.AllocationBase` — the start of the allocation containing the function, which is the module base.
3. Casts it to `PIMAGE_DOS_HEADER` and checks `e_magic == IMAGE_DOS_SIGNATURE` (`MZ`).
4. Walks `e_lfanew` to reach `IMAGE_NT_HEADERS` and checks `Signature == IMAGE_NT_SIGNATURE` (`PE\0\0`).
5. Returns the base address as `PVOID` on success, or `NULL` if either signature check fails.

> **Architecture note.** This function uses the unspecialized `IMAGE_NT_HEADERS` type, which resolves to `IMAGE_NT_HEADERS32` or `IMAGE_NT_HEADERS64` at **compile time** depending on the build target. A single binary cannot adapt at runtime — compile separate x86 and x64 builds as needed. `ExtractDllFile` does not share this limitation (see below).

**Modifying:** If you need to find the base of a *different* loaded module (e.g., an injected DLL), pass any pointer known to lie inside that module's memory region to `VirtualQuery` instead of `Tools::GetImageBase`.

---

### `Tools::ExtractDllFile`
```cpp
PBYTE Tools::ExtractDllFile(PBYTE moduleBase, PDWORD moduleSize);
```

Navigates the PE section table of the running executable to locate the **last section** — the slot where `Injector.exe` places embedded data — and returns a pointer to it.

**Step-by-step logic:**

1. Casts `moduleBase` to `PIMAGE_DOS_HEADER` and validates `e_magic == IMAGE_DOS_SIGNATURE`.
2. Walks `e_lfanew` to cast both `PIMAGE_NT_HEADERS32` and `PIMAGE_NT_HEADERS64` pointers at the same address (safe, as the Signature and Magic fields are at the same offset in both).
3. Validates `Signature == IMAGE_NT_SIGNATURE`.
4. Branches on `OptionalHeader.Magic` to select the correct header layout at **runtime**:

| `Magic` value | Layout used |
| :--- | :--- |
| `IMAGE_NT_OPTIONAL_HDR32_MAGIC` (0x010B) | `IMAGE_NT_HEADERS32` |
| `IMAGE_NT_OPTIONAL_HDR64_MAGIC` (0x020B) | `IMAGE_NT_HEADERS64` |

5. Calls `IMAGE_FIRST_SECTION` on the selected header to reach the section table.
6. Indexes to `firstSection + NumberOfSections - 1` to get the last section.
7. Sets `*moduleSize = dllSection->Misc.VirtualSize`.
8. Returns `RtlOffsetToPointer(moduleBase, dllSection->VirtualAddress)`.

Returns `NULL` if any signature check fails or if `Magic` is unrecognised.

> **Runtime architecture handling.** Unlike `GetImageBase`, this function handles both PE32 and PE32+ within a single build by inspecting `OptionalHeader.Magic` at runtime. No separate compilation is needed for the extraction logic itself.

**Modifying:**
- **To extract a section by name** instead of always taking the last one, iterate over `firstSection` to `firstSection + NumberOfSections - 1` and compare `dllSection->Name` against your target string using `memcmp`.
- **To extract a section by index**, replace `NumberOfSections - 1` with the desired zero-based index.
- **To validate the payload** before returning it, add a signature check against the first bytes of the pointed-to memory after the `RtlOffsetToPointer` calculation.