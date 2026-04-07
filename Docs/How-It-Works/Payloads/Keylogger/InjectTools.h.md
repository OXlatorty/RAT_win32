# InjectTools.h

## Overview
`InjectTools.h` is the header for the injection engine used in the Keylogger module. It is identical to `dllTools.h` from the ExampleDLL module apart from the namespace name.

For full documentation of all dependencies, structs, the `NtUnmapViewOfSection` alias, and the `BASE_RELOCATION_*` struct definitions see:
**[ExampleDLL/dllTools.h.md](../ExampleDLL/dllTools.h.md)**

---

## Namespace difference

| ExampleDLL | Keylogger |
| :--- | :--- |
| `namespace DllTools` | `namespace InjectTools` |
| `DllTools::AutoInject(LPSTR, LPCSTR)` | `InjectTools::AutoInject(LPSTR, LPCSTR)` |