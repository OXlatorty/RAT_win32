#include "InjectTools.h"

BOOL wasDllMainCalled = FALSE;
DWORD dllParam;

LPSTR targetPath = const_cast<LPSTR>("C:\\Windows\\System32\\notepad.exe");

/* Nazwa FristEntry jest znana w LoadMe i uruchamiana tam */
extern "C" __declspec(dllexport) void FristEntry() {
	char dllPath[MAX_PATH];
	DWORD ret = GetModuleFileNameA((HINSTANCE)dllParam, dllPath, MAX_PATH);
	char test[1024];
	wsprintfA(test, "%s", dllPath);
	MessageBoxA(0, test, "title", MB_ICONINFORMATION | MB_OK);

	InjectTools::AutoInject(targetPath, dllPath);
}

void Run() {
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

DWORD WINAPI ThreadFunc(LPVOID) {
    Run();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE Base, DWORD Callback, LPVOID Parm) {
	dllParam = (DWORD)Base;
	wasDllMainCalled = TRUE;

	switch (Callback) {
		case DLL_PROCESS_ATTACH: {
			char exe[MAX_PATH];
			GetModuleFileNameA(0, exe, sizeof(exe));

			if (strstr(exe, "notepad.exe")) {
				CreateThread(NULL, 0, ThreadFunc, NULL, 0, NULL);
			}

			break;
		}
		case DLL_PROCESS_DETACH:
			break;
		default:
			break;
	}

	return TRUE;
}