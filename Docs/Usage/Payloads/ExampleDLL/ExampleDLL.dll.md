# ExampleDLL.dll — Usage Guide

## Overview
**ExampleDLL** is a self-injecting demonstration payload DLL. When loaded by the **LoadMe** launcher, it resolves its own path on disk and injects itself into a hardcoded target process (`notepad.exe` by default) using process-hollowing / manual mapping via `DllTools::AutoInject`. Inside the target, it runs a message-box loop to confirm successful injection.

The DLL exports two functions:

| Export | Called by | Purpose |
| :--- | :--- | :--- |
| `fristEntry` | LoadMe (`rundll32.exe`) | Resolves the DLL path and triggers self-injection into the target process. |
| `entryPoint` | `AutoInject` shellcode | Runs inside the injected process; displays the host executable path in a loop. |

---

## Setup

### 1 — Compile ExampleDLL
Build `main.cpp` + `dllTools.cpp` as a 64-bit DLL. Ensure the linker exports both `fristEntry` and `entryPoint` (the `extern "C" __declspec(dllexport)` declarations in `main.cpp` handle this automatically with MSVC).

Required libraries (already declared via `#pragma comment` in `dllTools.h`):
- `ntdll.lib`
- `psapi.lib`

### 2 — Embed the DLL into LoadMe
Use `Injector.exe` to append the compiled DLL as a new section inside `LoadMe.exe`:
```
Injector.exe .extra LoadMe.exe ExampleDLL.dll
```
> Do not use `.data` as the section name — it conflicts with the standard PE `.data` section.

### 3 — Run the modified LoadMe.exe
Execute the modified `LoadMe.exe`. The launcher will:
1. Extract `ExampleDLL.dll` from its own PE body and drop it to `%APPDATA%\<ticks>.dll`.
2. Launch it via `rundll32.exe`, calling `fristEntry`.
3. Register the `rundll32.exe` command under `HKCU\...\Run` for startup persistence.
4. Delete itself from disk.

---

## Execution Flow

```
LoadMe.exe
  └─ drops ExampleDLL to %APPDATA%\<ticks>.dll
  └─ rundll32.exe "%APPDATA%\<ticks>.dll",fristEntry
       └─ DllMain called by Windows loader  →  dllParam = HMODULE, wasDllMainCalled = TRUE
       └─ fristEntry()
            └─ GetModuleFileNameA(dllParam)  →  resolves DLL path on disk
            └─ MessageBoxA  →  shows resolved path (debug confirmation)
            └─ DllTools::AutoInject("notepad.exe", dllPath)
                 └─ CreateProcess(notepad.exe, SUSPENDED)
                 └─ NtUnmapViewOfSection  →  removes original notepad image
                 └─ VirtualAllocEx + WriteProcessMemory  →  maps ExampleDLL headers & sections
                 └─ Relocation patching  (IMAGE_REL_BASED_DIR64)
                 └─ IAT patching
                 └─ PEB ImageBase update
                 └─ Shellcode → calls DllMain(newBase, DLL_PROCESS_ATTACH, 0)
                             → jmp entryPoint()
                                  └─ infinite loop: MessageBoxA("FunDLL.dll is inside: notepad.exe")
```

---

## Changing the Injection Target

Open `main.cpp` and update `targetPath`:
```cpp
LPSTR targetPath = const_cast<LPSTR>("C:\\Path\\To\\YourTarget.exe");
```
Recompile and re-embed with `Injector.exe`. The target must be a 64-bit executable.

---

## Important Notes

> **64-bit only.** The relocation loop in `AutoInject` handles only `IMAGE_REL_BASED_DIR64` (x64 relocations). The shellcode uses x64 register conventions (`RCX`, `RDX`, `R8`). Do not use this DLL as a payload for 32-bit target processes.

> **IAT resolution uses the injector's address space.** Function addresses for the payload's imports are resolved via `GetProcAddress` in the `rundll32.exe` process, not in the target. This works correctly when both processes load the same DLL versions at the same base addresses (common when ASLR produces the same layout). If imports resolve incorrectly, see the *Fix the IAT remote-address limitation* section in [`dllTools.cpp`](dllTools.cpp.md).

> **Debug message boxes.** `fristEntry` displays a `MessageBoxA` showing the DLL path before injecting. Remove the `wsprintfA` / `MessageBoxA` block in `main.cpp` before using this in a non-interactive or silent context.

> **Export name typo.** The export `fristEntry` is a typo of `firstEntry`. It must match the `runningFunctionName` constant in `loadder.cpp` exactly. See [`main.cpp`](main.cpp.md) and [`loadder.cpp`](../LoadMe/loadder.cpp.md) if you want to rename it.

> **Persistence bug.** The registry value written by LoadMe uses a literal format string as the value name. The DLL command line (the data) is stored correctly. See [`loadder.cpp`](../LoadMe/loadder.cpp.md#WinMain) for the fix.