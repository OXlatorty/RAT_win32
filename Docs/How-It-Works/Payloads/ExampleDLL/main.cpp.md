# main.cpp

## Overview
`main.cpp` is the payload DLL source file. It defines the two exported functions that `AutoInject` calls, plus the standard `DllMain` callback. When loaded by the injector, the DLL demonstrates the full injection pipeline by: capturing its own HMODULE in `DllMain`, resolving its own path in `fristEntry`, injecting itself into a hardcoded target process, and running a message-box loop in `entryPoint` to confirm successful injection.

---

## Globals

```cpp
BOOL wasDllMainCalled = FALSE;
HMODULE dllParam;
LPSTR targetPath = const_cast<LPSTR>("C:\\Windows\\System32\\notepad.exe");
```

| Variable | Purpose |
| :--- | :--- |
| `wasDllMainCalled` | Flag set to `TRUE` by `DllMain` on `DLL_PROCESS_ATTACH`. Used by `entryPoint` to verify that `DllMain` was executed by the injector before the custom entry was called. |
| `dllParam` | Stores the `HMODULE` (image base) passed to `DllMain`. Needed by `fristEntry` to resolve the DLL's own file path via `GetModuleFileNameA`. |
| `targetPath` | Hardcoded path of the process that `fristEntry` will inject this DLL into. Change this to retarget the injection. |

---

## Functions

### `DllMain`
```cpp
BOOL APIENTRY DllMain(HMODULE Base, DWORD Callback, LPVOID Parm)
```

Standard DLL entry point called by the Windows loader — and, in this project, explicitly triggered by the shellcode inside `AutoInject` with `DLL_PROCESS_ATTACH`.

**Logic:**
- Stores `Base` in `dllParam` for later use by `fristEntry`.
- Sets `wasDllMainCalled = TRUE` so `entryPoint` can confirm proper initialisation.
- `DLL_PROCESS_ATTACH` and `DLL_PROCESS_DETACH` cases are present but empty — add initialisation and cleanup code there as needed.
- Always returns `TRUE`.

> **When is this called?** The injector's shellcode calls this function directly with `RCX = newBase`, `RDX = 1` (`DLL_PROCESS_ATTACH`), `R8 = 0` before jumping to `entryPoint`. It is not called by the OS loader in this scenario.

---

### `fristEntry`
```cpp
extern "C" __declspec(dllexport) void fristEntry()
```

> **Note:** `fristEntry` is a typo of `firstEntry`. This name must match the `runningFunctionName` constant in `loadder.cpp` exactly (case-sensitive) for the LoadMe launcher to call it correctly.

The export invoked by **LoadMe** (`rundll32.exe … ,fristEntry`) when the DLL is first dropped into the user's profile. Its role is to identify the DLL's own location on disk and inject it into the hardcoded target process.

**Logic:**
1. Calls `GetModuleFileNameA(dllParam, dllPath, MAX_PATH)` to resolve the full path of this DLL on disk. Returns immediately if the call fails.
2. Displays the resolved path in a `MessageBoxA` for debug confirmation.
3. Calls `DllTools::AutoInject(targetPath, dllPath)` to inject this DLL into `notepad.exe` (or whichever path `targetPath` points to).

> **Dependency on `dllParam`:** This function relies on `DllMain` having been called first to populate `dllParam`. When launched via `rundll32.exe`, `DllMain` is called by the Windows loader before any export, so `dllParam` will be valid. When called from the injector's shellcode path (see `entryPoint`), `DllMain` is invoked by the shellcode immediately before `entryPoint` — not before `fristEntry` — so `dllParam` is also valid by the time `fristEntry` is called from `rundll32`.

---

### `entryPoint`
```cpp
extern "C" __declspec(dllexport) void entryPoint()
```

The custom entry point recognised by `AutoInject` (searched by name via `GetExportAddress`). Called by the injector's shellcode instead of `DllMain` when this DLL is mapped into a new process.

**Logic:**
- If `wasDllMainCalled == TRUE` (shellcode called `DllMain` first): enters an infinite loop, repeatedly calling `GetModuleFileNameA(0, …)` to get the host process's executable path and displaying it in a `MessageBoxA`. The loop title confirms `FunDLL.dll is inside: <host exe>`.
- If `wasDllMainCalled == FALSE`: displays a single message box `"DllMain was not called"` as a diagnostic.

> **Why an infinite loop?** This is a demonstration payload — the loop keeps the injected DLL alive and visible inside the target process. In a real payload, replace the loop with the intended functionality.

---

## Modifying

**Change the injection target:**
```cpp
LPSTR targetPath = const_cast<LPSTR>("C:\\Path\\To\\YourTarget.exe");
```

**Fix the export name typo:**
Rename `fristEntry` to `firstEntry` in both the function definition and the `extern "C" __declspec(dllexport)` declaration, then update `runningFunctionName` in `loadder.cpp` to match and recompile both.

**Add initialisation logic:**
Put startup code inside `DllMain` under `DLL_PROCESS_ATTACH` and cleanup under `DLL_PROCESS_DETACH`. Avoid calling complex Win32 APIs from `DllMain` directly due to loader-lock constraints — use a separate thread or defer to `entryPoint` instead.

**Remove the debug message box in `fristEntry`:**
Delete the `wsprintfA` / `MessageBoxA` block if you do not want a visible confirmation dialog during injection.

**Replace the `entryPoint` loop with real payload logic:**
Remove the `for(;;)` loop and implement the intended behaviour. The `wasDllMainCalled` guard remains useful to detect whether the DLL was loaded correctly by the injector.