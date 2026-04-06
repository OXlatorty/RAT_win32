# Injector.cpp

## Overview
`Injector.cpp` is the sole source file of the **Injector** tool. It modifies an existing Windows PE file (EXE or DLL) by appending a new section header and embedding the raw bytes of a secondary file directly into the target binary's structure.

---

## Functions

### `AliginSectionHeader`
> **Note**: The function name contains a typo (`Aligin` instead of `Align`). Consider renaming it to `AlignSectionHeader` for clarity.

```cpp
DWORD AliginSectionHeader(DWORD sectionSize, DWORD aligment, DWORD address)
```

Calculates an alignment-compliant size or offset boundary according to PE format rules.

| Parameter | Description |
| :--- | :--- |
| `sectionSize` | Raw size of the data to be aligned. |
| `aligment` | Alignment granularity — pass `SectionAlignment` for virtual addresses, `FileAlignment` for raw file offsets. Both values are taken from `IMAGE_OPTIONAL_HEADER`. |
| `address` | Base offset to which the aligned size is added. |

**Logic:** If `sectionSize` is already a multiple of `aligment`, returns `address + sectionSize`. Otherwise rounds up to the next multiple: `address + ((sectionSize / aligment) + 1) * aligment`.

**Used for calculating:**
- `VirtualSize` and `VirtualAddress` of the new section (using `SectionAlignment`).
- `SizeOfRawData` and `PointerToRawData` of the new section (using `FileAlignment`).

---

### `main`
```cpp
INT main(INT arg, PCHAR argv[])
```

Entry point and orchestrator for the entire injection process.

**Arguments (argv):**

| Index | Value |
| :--- | :--- |
| `argv[1]` | Name for the new PE section (max 8 characters). |
| `argv[2]` | Path to the target PE file to be modified. |
| `argv[3]` | Path to the source file whose bytes will be embedded. |

**Execution flow:**

1. **Validation** — checks that all three arguments are present; exits with `-1` otherwise.

2. **Target file buffering** — opens the target with `CreateFileA` (`GENERIC_READ | GENERIC_WRITE`), reads the entire file into a heap buffer allocated via `LocalAlloc(LPTR, fileSize)`.

3. **PE header parsing** — casts the buffer to `PIMAGE_DOS_HEADER`, validates `e_magic == IMAGE_DOS_SIGNATURE`, then walks `e_lfanew` to reach:
   - `IMAGE_FILE_HEADER` at `buffer + e_lfanew + sizeof(DWORD)`
   - `IMAGE_OPTIONAL_HEADER` at `buffer + e_lfanew + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER)`
   - Section table (`IMAGE_SECTION_HEADER[]`) at `buffer + e_lfanew + sizeof(IMAGE_NT_HEADERS)`

4. **New section header setup** — zeroes out the slot at index `NumberOfSections`, copies the provided name with `CopyMemory` (fixed 8-byte copy), then calculates all four layout fields using `AliginSectionHeader` against the previous section's values:
   ```
   VirtualSize      = align(dllSize,  SectionAlignment,  0)
   VirtualAddress   = align(prev.VirtualSize,  SectionAlignment,  prev.VirtualAddress)
   SizeOfRawData    = align(dllSize,  FileAlignment,     0)
   PointerToRawData = align(prev.SizeOfRawData, FileAlignment, prev.PointerToRawData)
   ```
   Characteristics are set to `MEM_READ | MEM_WRITE | MEM_EXECUTE | CNT_CODE | CNT_INITIALIZED_DATA | CNT_UNINITIALIZED_DATA`.

5. **File extension** — calls `SetFilePointer` to `PointerToRawData + SizeOfRawData`, then `SetEndOfFile` to pre-allocate the exact space needed.

6. **Header flush** — seeks to offset `0` and writes the modified in-memory buffer (now containing the updated `NumberOfSections` and `SizeOfImage`) back to the target file.

7. **Payload append** — writes the source file's bytes immediately after the flushed headers.

---

## Important Notes

> **No header-space validation.** Writing a new `IMAGE_SECTION_HEADER` (40 bytes) directly after the last existing header assumes there is enough padding between the header region and the first section's raw data. If that gap is smaller than 40 bytes, the target file will be silently corrupted. Verify available slack space with a PE viewer (e.g., CFF Explorer, PE-bear) before use.

> **Broad section characteristics.** Granting `EXECUTE | READ | WRITE` on a single section is a common heuristic trigger for antivirus and endpoint-detection tools.

> **Section name truncation.** A name of exactly 8 characters has no null terminator — this is valid per the PE specification. Names shorter than 8 characters are null-padded by the preceding `ZeroMemory` call.

> **Always back up the target file.** The tool modifies the binary in-place with no backup and no undo.