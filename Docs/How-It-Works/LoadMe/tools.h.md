# tools.h

## Overview
`tools.h` defines the `Tools` namespace interface and the `RtlOffsetToPointer` helper macro. It is the public contract for the PE-parsing utilities implemented in `tools.cpp`, and is pulled into every translation unit via `includes.h`.

---

## Macro

### `RtlOffsetToPointer`
```cpp
#define RtlOffsetToPointer(Module, Pointer) PBYTE(PBYTE(Module) + DWORD(Pointer))
```

Computes an absolute memory address from a base pointer and a 32-bit relative offset. Both operands are cast before addition (`Module` → `PBYTE`, `Pointer` → `DWORD`) to ensure correct pointer arithmetic regardless of the calling context.

**Typical use:** converting a section's `VirtualAddress` field (a relative offset stored in the PE header) into an actual in-memory pointer:
```cpp
return RtlOffsetToPointer(moduleBase, dllSection->VirtualAddress);
```

---

## Functions

### `Tools::GetImageBase`
```cpp
PVOID Tools::GetImageBase();
```
Returns the base address at which the current executable is loaded in memory, or `NULL` if PE validation fails. See [`tools.cpp`](tools.cpp.md) for implementation details.

---

### `Tools::ExtractDllFile`
```cpp
PBYTE Tools::ExtractDllFile(PBYTE moduleBase, PDWORD moduleSize);
```

Locates the last section of the running PE image (the slot where Injector places embedded data), writes its `VirtualSize` into `*moduleSize`, and returns a pointer to the start of that section in memory. Returns `NULL` on invalid or unrecognised PE structures. Handles both PE32 and PE32+ layouts at runtime. See [`tools.cpp`](tools.cpp.md) for implementation details.

| Parameter | Direction | Description |
| :--- | :--- | :--- |
| `moduleBase` | in | Base address of the running executable, obtained from `GetImageBase()`. |
| `moduleSize` | out | Receives the `VirtualSize` of the located section. |
| return value | — | Pointer to the start of the last section, or `NULL` on failure. |

---

## Modifying

- **Adding a utility function** — declare it inside the `Tools` namespace here and implement it in `tools.cpp`. Include `includes.h` at the top of the implementation file.
- **Changing the offset macro** — if porting to a context where offsets can exceed 32 bits (e.g., PE32+ with large-address-aware images), change the `DWORD` cast in `RtlOffsetToPointer` to `ULONGLONG`.