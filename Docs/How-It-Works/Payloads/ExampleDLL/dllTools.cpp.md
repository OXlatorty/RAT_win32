# dllTools.cpp

## Overview
`dllTools.cpp` implements the process-hollowing / manual-map injection engine. It provides three internal helper functions and the public `DllTools::AutoInject` entry point. The injection technique launches a target process in a suspended state, replaces its mapped image with a manually loaded DLL, patches relocations and imports, then resumes execution via a small shellcode stub.

---

## Helper Functions

### `RvaToRaw`
```cpp
uintptr_t RvaToRaw(uintptr_t rva, PIMAGE_NT_HEADERS ntHeaders, LPVOID fileBuffer)
```

Converts a Relative Virtual Address (RVA) into a raw file offset within an in-memory file buffer.

| Parameter | Description |
| :--- | :--- |
| `rva` | The RVA to resolve (e.g. from a Data Directory or thunk field). |
| `ntHeaders` | Pointer to the PE NT headers of the file in the buffer. |
| `fileBuffer` | Base pointer of the raw file data in memory. |

**Logic:** Iterates all section headers via `IMAGE_FIRST_SECTION`. For each section, checks whether `rva` falls within `[VirtualAddress, VirtualAddress + VirtualSize)`. On a match, returns:
```
(uintptr_t)fileBuffer + section->PointerToRawData + (rva - section->VirtualAddress)
```
Returns `0` if no section contains the RVA — callers must check for this before dereferencing the result.

> **Note:** This function works on a file loaded flat into memory (not mapped by the OS loader). RVAs inside a memory-mapped image should be resolved with simple addition instead: `moduleBase + rva`.

---

### `GetExportAddress`
```cpp
uintptr_t GetExportAddress(LPVOID fileBuffer, const char* exportName)
```

Walks the export directory of a PE file loaded flat in memory and returns the **RVA** of the named export.

| Parameter | Description |
| :--- | :--- |
| `fileBuffer` | Base pointer of the raw DLL file data. |
| `exportName` | Null-terminated name of the export to find. Case-sensitive (`strcmp`). |

**Logic:**
1. Parses `IMAGE_DOS_HEADER` → `IMAGE_NT_HEADERS` → `DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT]`.
2. Returns `0` immediately if `exportDir.Size == 0` (no export table).
3. Resolves `AddressOfNames`, `AddressOfFunctions`, and `AddressOfNameOrdinals` via `RvaToRaw`.
4. Iterates `NumberOfNames` entries, comparing each name string against `exportName`.
5. On a match, returns `functions[ordinals[i]]` — the function's RVA.

> **Return value is an RVA, not an absolute address.** The caller adds `newBase` to get the final virtual address: `(uintptr_t)newBase + customEntryRVA`.

**Used by:** `AutoInject` to locate the `entryPoint` export, giving the DLL a chance to override the standard `DllMain` entry with a custom function.

---

### `GetRemoteModuleHandle`
```cpp
uintptr_t GetRemoteModuleHandle(HANDLE hProcess, const char* moduleName)
```

Finds the load address of a named module inside a remote (target) process.

| Parameter | Description |
| :--- | :--- |
| `hProcess` | Handle to the remote process (must have `PROCESS_QUERY_INFORMATION | PROCESS_VM_READ`). |
| `moduleName` | DLL filename to find, e.g. `"kernel32.dll"`. Case-insensitive (`_stricmp`). |

**Logic:** Calls `EnumProcessModules` to fill an array of up to 1 024 `HMODULE` handles, then calls `GetModuleBaseNameA` on each. Returns the base address of the first case-insensitive match, or `0` if not found.

**Used by:** The import-patching loop inside `AutoInject` to check whether a required library is already loaded in the target process before falling back to a local `LoadLibraryA` call.

> **Limitation:** The 1 024-slot array silently misses modules beyond that count. For processes with very large module lists, replace with a two-pass `EnumProcessModules` call (first to get the required buffer size, then to fill it).

---

## `DllTools::AutoInject`
```cpp
int DllTools::AutoInject(LPSTR target, LPCSTR payload)
```

The main injection function. Launches `target` suspended, manually maps `payload` into it using process-hollowing, patches the image, and resumes execution. Returns `0` on success, `-1` on any failure (the target process is terminated before returning on most error paths).

### Step-by-step logic

#### 1 — Resolve `ntdll.dll`
```cpp
HMODULE hNtDll = GetModuleHandleA("ntdll.dll");
```
Acquires a handle to the already-loaded `ntdll.dll` in the injector's own process. Needed to dynamically resolve `NtQueryInformationProcess` and `NtUnmapViewOfSection` later.

---

#### 2 — Launch target suspended
```cpp
CreateProcessA(NULL, target, ..., CREATE_SUSPENDED, ..., &processInformation)
```
Creates the target process with its main thread immediately suspended. This prevents any code from running until injection is complete.

---

#### 3 — Read target PEB image base
```cpp
NtQueryInfo(targetProcess, ProcessBasicInformation, &pbi, ...)
uintptr_t remoteImageBasePtr = (uintptr_t)pbi.PebBaseAddress + 0x10;
ReadProcessMemory(targetProcess, remoteImageBasePtr, &remoteImageBase, ...)
```
Uses `NtQueryInformationProcess` to get the target's `PEB` address. On 64-bit Windows, the `ImageBaseAddress` field sits at offset `0x10` inside the PEB. `ReadProcessMemory` reads the currently mapped base so it can be unmapped next.

> **Architecture note:** The `0x10` offset is correct for 64-bit (`PEB64`). On a 32-bit process the offset is `0x08`. If you need to support x86 targets, branch on the target architecture here.

---

#### 4 — Read and validate payload DLL from disk
```cpp
ReadFile(hFile, dllBuffer.data(), fileSize, &read, NULL);
if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) { TerminateProcess(...); return -1; }
```
Reads the entire DLL file into a `std::vector<BYTE>` buffer and validates the `MZ` signature. The flat buffer is kept throughout for all `RvaToRaw`-based parsing.

---

#### 5 — Unmap existing image
```cpp
auto unmap = (pNtUnmapViewOfSection)GetProcAddress(hNtDll, "NtUnmapViewOfSection");
if (unmap) unmap(targetProcess, remoteImageBase);
```
Removes the original executable image from the target's address space, freeing the virtual address range so the DLL can be placed there.

---

#### 6 — Allocate memory in target
```cpp
LPVOID newBase = VirtualAllocEx(targetProcess, remoteImageBase, SizeOfImage, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE);
if (!newBase) newBase = VirtualAllocEx(targetProcess, NULL, SizeOfImage, ...);
```
First attempts to allocate at the original image base (preferred — avoids the need for relocation). Falls back to any available address if the preferred base is taken.

---

#### 7 — Write PE headers
```cpp
WriteProcessMemory(targetProcess, newBase, dllBuffer.data(), ntHeaders->OptionalHeader.SizeOfHeaders, NULL);
```
Copies the flat PE headers (DOS header through end of section table) into the target at `newBase`.

---

#### 8 — Write sections
Iterates all section headers and copies each section's raw data from the flat buffer to `newBase + VirtualAddress` in the target. Sections with `SizeOfRawData == 0` (e.g. purely zero-initialised BSS sections) are skipped.

---

#### 9 — Apply base relocations
Only executed if `relocDir.Size > 0` and `delta != 0` (i.e. allocation landed at a different address than `ImageBase`).

Iterates relocation blocks:
- Each `IMAGE_BASE_RELOCATION` block covers a 4 KB page.
- Entries after the block header are 16-bit values: upper 4 bits = type, lower 12 bits = offset within the page.
- Only `IMAGE_REL_BASED_DIR64` (type `0xA`) entries are patched — appropriate for 64-bit images. For each:
  1. `ReadProcessMemory` reads the 8-byte value at `newBase + block->VirtualAddress + offset`.
  2. Adds `delta` to it.
  3. `WriteProcessMemory` writes it back.

> **x86 support:** 32-bit images use `IMAGE_REL_BASED_HIGHLOW` (type `0x3`) with 4-byte patch values. Add a branch on `type == IMAGE_REL_BASED_HIGHLOW` with `DWORD`-sized read/write if you need to support x86 payloads.

---

#### 10 — Resolve imports (IAT patching)
Iterates `IMAGE_IMPORT_DESCRIPTOR` entries until a null terminator entry:

For each imported library:
1. Resolves the library name via `RvaToRaw`.
2. Calls `GetRemoteModuleHandle` to check if the library is already loaded in the target. If not, calls `LoadLibraryA` locally to ensure `GetModuleHandleA` can resolve it.
3. Walks `OriginalFirstThunk` (the hint/name table) in parallel with `FirstThunk` (the IAT):
   - **Ordinal import** (`IMAGE_SNAP_BY_ORDINAL`) → resolves via `GetProcAddress` with the ordinal cast to `LPCSTR`.
   - **Name import** → resolves the `IMAGE_IMPORT_BY_NAME` structure via `RvaToRaw`, then calls `GetProcAddress` by name.
4. Writes the resolved function address directly into the remote IAT slot via `WriteProcessMemory`.

> **Known limitation:** `GetProcAddress` is called on the **injector's own** copy of each library (`GetModuleHandleA(libName)`), not the remote one. This works when the injector and target share the same address layout (e.g. same architecture, ASLR disabled or same randomisation seed), but will produce wrong addresses if the libraries are loaded at different bases in the two processes. For a robust solution, read the remote IAT base via `ReadProcessMemory` and compute the delta between local and remote module bases.

---

#### 11 — Update PEB image base
```cpp
WriteProcessMemory(targetProcess, remoteImageBasePtr, &newBase, sizeof(LPVOID), NULL);
```
Writes the new allocation address back into `PEB->ImageBaseAddress` so that `GetModuleHandleA(NULL)` inside the injected image returns the correct base.

---

#### 12 — Build and inject shellcode
The suspended thread's `RIP` is redirected to an x64 shellcode stub (64 bytes) that explicitly calls `DllMain` first, then calls the custom entry point, and finally returns execution to the thread's original `RIP`. This two-call sequence is the key change from the previous single-`jmp` design.

**Entry point selection:**
```cpp
uintptr_t customEntryRVA = GetExportAddress(dllBuffer.data(), "entryPoint");
uintptr_t dllMainRVA     = ntHeaders->OptionalHeader.AddressOfEntryPoint;
if (customEntryRVA == 0) customEntryRVA = ntHeaders->OptionalHeader.AddressOfEntryPoint;
```
Both addresses are resolved before the shellcode is built. `dllMainRVA` always points to `AddressOfEntryPoint`. `customEntryRVA` points to the exported `entryPoint` function, or falls back to `AddressOfEntryPoint` if the export is not found.

**Why call `DllMain` explicitly?**
When the injector manually maps the DLL, the Windows loader never runs — so the C++ runtime is never initialised by the normal loader path. Calling `DllMain(newBase, DLL_PROCESS_ATTACH, NULL)` explicitly through the shellcode triggers CRT initialisation (global constructors, `std::ofstream`, static objects, etc.) before any payload code runs.

**Shellcode layout (64 bytes):**
```asm
sub rsp, 40                        ; allocate shadow space (32 bytes) + align stack to 16 bytes

; --- Call DllMain(newBase, DLL_PROCESS_ATTACH, NULL) ---
mov rcx, <newBase>                 ; RCX = hinstDLL (1st arg, x64 ABI)
mov rdx, 1                         ; RDX = DLL_PROCESS_ATTACH
xor r8,  r8                        ; R8  = lpvReserved = NULL
mov rax, <newBase + dllMainRVA>    ; absolute address of DllMain
call rax

; --- Call entryPoint() ---
mov rax, <newBase + customEntryRVA>
call rax

add rsp, 40                        ; restore stack pointer

; --- Return to original thread execution ---
mov rax, <original ctx.Rip>
jmp rax
```

**Address patching:**

| Shellcode offset | Value written |
| :--- | :--- |
| `+6`  | `newBase` (for `mov rcx`) |
| `+26` | `newBase + dllMainRVA` |
| `+38` | `newBase + customEntryRVA` |
| `+54` | `ctx.Rip` saved before `SetThreadContext` |

The shellcode is written into the target via `VirtualAllocEx` (`PAGE_EXECUTE_READWRITE`) + `WriteProcessMemory`. `ctx.Rip` is set to `scAddr`, then `SetThreadContext` and `ResumeThread` start execution.

---

## Modifying

**Change the custom entry point name:**
In `AutoInject`, modify the string passed to `GetExportAddress`:
```cpp
uintptr_t customEntryRVA = GetExportAddress(dllBuffer.data(), "YourExportName");
```
Ensure the payload DLL exports a function with exactly that name.

**Skip the explicit `DllMain` call in the shellcode:**
If your payload does not rely on CRT initialisation and you want a shorter stub, remove the `DllMain` `call` block and the `sub/add rsp` frame, reducing back to a single `jmp` to `customEntryRVA`. Remember to update all byte offsets in the patch lines accordingly.

**Support x86 payloads:**
- Change relocation patching to handle `IMAGE_REL_BASED_HIGHLOW` with `DWORD`-sized reads/writes.
- Change the shellcode to x86 calling convention (`push` arguments, `call`/`jmp`).
- Fix the PEB `ImageBaseAddress` offset from `0x10` to `0x08`.

**Fix the IAT remote-address limitation:**
Replace the local `GetProcAddress` result with a remote-base-adjusted address:
```cpp
uintptr_t localBase  = (uintptr_t)GetModuleHandleA(libName);
uintptr_t remoteBase = GetRemoteModuleHandle(targetProcess, libName);
uintptr_t localAddr  = (uintptr_t)GetProcAddress(GetModuleHandleA(libName), funcName);
funcAddr = remoteBase + (localAddr - localBase);
```

**Target a running process instead of spawning a new one:**
Replace the `CreateProcessA` + `NtUnmapViewOfSection` block with `OpenProcess` on an existing PID, skip the unmap step, and allocate the DLL image at any free address (`VirtualAllocEx` with `NULL` preferred base). Remove the `CONTEXT`/`SetThreadContext` path and use `CreateRemoteThread` to call the shellcode instead.