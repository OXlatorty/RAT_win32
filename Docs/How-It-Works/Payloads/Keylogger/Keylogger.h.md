# Keylogger.h

## Overview
`Keylogger.h` declares the `Keylogger` namespace interface. It is included by `Includes.h`, making both functions available to every translation unit in the module.

---

## Contents

```cpp
#pragma once
#include "Includes.h"

namespace Keylogger {
    LRESULT HookProc(int code, WPARAM wParam, LPARAM lParam);
    int Init();
}
```

## Functions

### `Keylogger::HookProc`
```cpp
LRESULT HookProc(int code, WPARAM wParam, LPARAM lParam);
```
Low-level keyboard hook callback. Passed directly to `SetWindowsHookExA` by `Init`. See [`Keylogger.cpp.md`](Keylogger.cpp.md) for full details.

---

### `Keylogger::Init`
```cpp
int Init();
```
Installs the hook and runs the message loop. Blocks the calling thread until the loop exits. Returns `0` on clean exit, `-1` on failure. See [`Keylogger.cpp.md`](Keylogger.cpp.md) for full details.

---

## Modifying

- **Adding a new function** — declare it inside `namespace Keylogger` here and implement it in `Keylogger.cpp`.
- **Exposing configuration** — if you want the log file path or log format to be configurable at runtime, add a `void SetLogPath(const char* path)` declaration here and implement it in `Keylogger.cpp` using a module-level variable.