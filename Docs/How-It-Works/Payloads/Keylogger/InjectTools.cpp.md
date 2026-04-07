# InjectTools.cpp

## Overview
`InjectTools.cpp` implements the same process-hollowing / manual-map injection engine as `dllTools.cpp` in the ExampleDLL module. The only difference is the namespace name: **`InjectTools::`** instead of `DllTools::`.

For full documentation of every function and step see:
**[ExampleDLL/dllTools.cpp.md](../ExampleDLL/dllTools.cpp.md)**

---

## Namespace difference

| ExampleDLL | Keylogger |
| :--- | :--- |
| `DllTools::AutoInject(...)` | `InjectTools::AutoInject(...)` |

All internal helpers (`RvaToRaw`, `GetExportAddress`, `GetRemoteModuleHandle`) and the full `AutoInject` step-by-step logic are identical.