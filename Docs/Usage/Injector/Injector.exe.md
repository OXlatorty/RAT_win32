## Injector.exe
 
### Overview
`Injector.exe` is a command-line utility that appends a new section to an existing Windows PE file (EXE or DLL) and embeds the raw content of a secondary file into it. Typical use cases include bundling a payload DLL inside a loader executable before distribution.
 
### Installation
No installation is required. Compile the source (`Injector.cpp`) with any MSVC-compatible C++ compiler and place the resulting binary in a convenient location. Write access to the target file's directory is required; administrative privileges may be necessary if the target resides in a protected system path.
 
### Syntax
```
Injector.exe <SectionName> <TargetFile> <SourceData>
```
 
### Arguments
 
| Argument | Description |
| :--- | :--- |
| `SectionName` | Name for the new PE section. Maximum **8 characters** — names shorter than 8 characters are null-padded automatically; a name of exactly 8 characters has no null terminator, which is valid per the PE specification. Conventional names start with a dot (e.g., `.extra`). |
| `TargetFile` | Path to the PE executable or DLL to be modified. |
| `SourceData` | Path to the file whose bytes will be embedded into the new section. |
 
### Example Usage
 
**Embedding a DLL payload into a loader executable:**
```
Injector.exe .extra "C:\Projects\LoadMe.exe" "C:\Projects\Payload.dll"
```
 
**Injecting a binary resource into a plugin DLL:**
```
Injector.exe .assets "C:\Projects\Plugin.dll" "C:\Projects\resource.bin"
```
 
### Expected Inputs & Outputs
 
- **Input:** A valid Windows PE file as the target, and any binary file as the source data.
- **Output:**
  - **Success:** The target file is modified in-place and the message `Operation successfully, thanks for using my Injector.` is printed to the console.
  - **Failure:** The program exits with `-1` and the target file is not modified (e.g., file in use, invalid path, or read-only target).
 
### Important Notes
 
> **Always back up the target file before running.** The tool modifies the binary in-place with no automatic backup and no undo operation.
 
> **Header space is not validated.** The tool assumes there is at least 40 bytes of padding between the last existing section header and the first section's raw data. If this space is insufficient the target file will be silently corrupted. Verify the available header slack with a PE viewer (e.g., CFF Explorer, PE-bear) before use.
 
> **Avoid standard section names.** Do not use names such as `.text`, `.data`, or `.rdata` — they will conflict with sections already present in most executables.
 
> **Broad section permissions.** The injected section is marked `READ | WRITE | EXECUTE`. This combination is a common trigger for antivirus and endpoint-detection tools.