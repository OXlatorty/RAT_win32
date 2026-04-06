## LoadMe.exe
 
### Overview
**LoadMe** is a self-contained payload wrapper. Once executed, it extracts the DLL embedded in its own PE body, writes it to disk, launches it via `rundll32.exe`, registers it for automatic startup under the current user account, and then deletes itself from disk.
 
### Setup
 
1. Compile both `LoadMe` and `Injector` from source.
2. Use `Injector.exe` to embed your payload DLL:
   ```
   Injector.exe .extra LoadMe.exe YourPayload.dll
   ```
   > **Important:** Do not use `.data` as the section name — it conflicts with the standard `.data` section already present in most PE files. Use a custom name such as `.extra` or `.payload`.
3. Distribute or execute the modified `LoadMe.exe`.
 
### Configuration
 
**DLL entry point.** The loader is hardcoded to call the exported function `fristEntry` from the dropped DLL (note the lowercase `f`). Ensure your DLL exports a function with exactly that name. To rename it, update the `runningFunctionName` constant in `loadder.cpp` and recompile.
 
**Drop location.** The payload is written to `%APPDATA%\<ticks>.dll`, where `<ticks>` is the current value of `GetTickCount()`. Example: `%APPDATA%\3891204.dll`.
 
### Execution Flow
 
When the modified `LoadMe.exe` is run, the following steps occur in order:
 
1. **Extraction** — The embedded DLL is located in the last PE section of `LoadMe.exe` and written to `%APPDATA%\<ticks>.dll`.
2. **Execution** — A hidden `rundll32.exe` process is spawned to load the DLL and call `fristEntry`:
   ```
   "<WINDIR>\System32\rundll32.exe" "<APPDATA>\<ticks>.dll",fristEntry
   ```
3. **Persistence** — A `REG_SZ` value is created under `HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run`. Its data is the full `rundll32.exe` command line, causing the DLL to be executed automatically on every user login.
4. **Self-deletion** — A hidden `cmd.exe` process deletes `LoadMe.exe` from disk.
 
### Known Issue — Registry Value Name
 
The registry value is currently written with the literal string `%s\%lu` as its **name** rather than a meaningful application identifier. The value **data** (the `rundll32.exe` command line) is written correctly. The persistence mechanism itself functions, but the entry will appear under the raw format-string name in the registry. To fix this, replace the `valueName` literal in `loadder.cpp` with a proper string and recompile.
 
### Inputs & Outputs
 
- **Input:** A `LoadMe.exe` binary that has been processed by `Injector.exe` to contain an embedded DLL.
- **Output:**
  - A DLL file at `%APPDATA%\<ticks>.dll`.
  - A startup registry entry under `HKCU\...\Run`.
  - `LoadMe.exe` is deleted from disk after execution.
 
> **Warning:** This tool writes files to the user profile directory and modifies the Windows Registry. Ensure you have the appropriate permissions and that this behavior is intended for your deployment scenario.