#include "dllTools.h"

BOOL wasDllMainCalled = FALSE;
HMODULE dllParam;

LPSTR targetPath = const_cast<LPSTR>("C:\\Windows\\System32\\notepad.exe");

extern "C" __declspec(dllexport) void firstEntry() {
	char dllPath[MAX_PATH];

	DWORD ret = GetModuleFileNameA((HINSTANCE)dllParam, dllPath, MAX_PATH);
	if (ret == 0) return;

	char test[1024];
	wsprintfA(test, "%s", dllPath);
	MessageBoxA(0, test, "title", MB_ICONINFORMATION | MB_OK);

	DllTools::AutoInject(targetPath, dllPath);
}

BOOL APIENTRY DllMain(HMODULE Base, DWORD Callback, LPVOID Parm) {
	dllParam = (HMODULE)Base;
	wasDllMainCalled = TRUE;

	switch (Callback) {
		case DLL_PROCESS_ATTACH:
			break;
		case DLL_PROCESS_DETACH:
			break;
		default:
			break;
	}

	return TRUE;
}

extern "C" __declspec(dllexport) void entryPoint() {
	if (wasDllMainCalled) {
		for (;;) {
			char exe[MAX_PATH + 1];
			GetModuleFileNameA(0, exe, sizeof(exe));
			MessageBoxA(0, exe, "FunDLL.dll is inside: ", MB_ICONINFORMATION | MB_OK);
		}
	}
	else {
		MessageBoxA(0, "DllMain was not called", "FunDLL.dll", MB_ICONINFORMATION | MB_OK);
	}
}
