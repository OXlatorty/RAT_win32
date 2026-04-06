# includes.h

## Overview
`includes.h` is the shared precompiled header for the **LoadMe** module. It acts as a single inclusion point — every `.cpp` file in the module includes this file instead of managing dependencies individually.

---

## Contents

```cpp
#pragma once

#include <Windows.h>
#include "tools.h"
```

| Include | Purpose |
| :--- | :--- |
| `<Windows.h>` | Provides the full Windows API surface: memory management, process creation, file I/O, registry access, and PE type definitions. |
| `"tools.h"` | Exposes the `Tools` namespace (`GetImageBase`, `ExtractDllFile`) and the `RtlOffsetToPointer` macro to every file that includes `includes.h`. |

`#pragma once` prevents the header from being processed more than once per translation unit.

---

## Modifying

To add a new dependency available across the entire LoadMe module, include it here rather than in individual `.cpp` files. Keep this file minimal — only genuinely shared dependencies belong here.