/*
	Copyright (C) 2010  Stefan Sundin (recover89@gmail.com)
	
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
*/

#define UNICODE
#define _UNICODE
#define _WIN32_WINNT 0x0501
#define _WIN32_IE 0x0600

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <shlwapi.h>
#include <psapi.h>

//App
#define APP_NAME            L"ShutdownPatcher"
#define APP_VERSION         "0.1"
#define APP_URL             L"http://code.google.com/p/shutdownguard/"

//Messages
#define WM_TRAY                WM_USER+1
#define SWM_TOGGLE             WM_APP+1
#define SWM_AUTOSTART_ON       WM_APP+2
#define SWM_AUTOSTART_OFF      WM_APP+3
#define SWM_SETTINGS           WM_APP+4
#define SWM_ABOUT              WM_APP+5
#define SWM_EXIT               WM_APP+6
#define PATCHTIMER             WM_APP+7

//Stuff missing in MinGW
#define HWND_MESSAGE ((HWND)-3)
#ifndef NIIF_USER
#define NIIF_USER 4
#define NIN_BALLOONSHOW        WM_USER+2
#define NIN_BALLOONHIDE        WM_USER+3
#define NIN_BALLOONTIMEOUT     WM_USER+4
#define NIN_BALLOONUSERCLICK   WM_USER+5
#endif
#ifndef EWX_RESTARTAPPS
#define EWX_RESTARTAPPS        0x00000040
#endif

//Boring stuff
LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
HINSTANCE g_hinst = NULL;
HWND g_hwnd = NULL;
UINT WM_TASKBARCREATED = 0;
UINT WM_UPDATESETTINGS = 0;
UINT WM_SHUTDOWNBLOCKED = 0;

//Cool stuff
int enabled = 1;
struct patchlist {
	wchar_t **items;
	int length;
	wchar_t *data;
};
struct patchlist PatchList = {NULL,0};
#define PATCHINTERVAL 1000

//Patch
HMODULE WINAPI (*pfnLoadLibrary)(LPCTSTR) = NULL;
HMODULE WINAPI (*pfnFreeLibrary)(LPCTSTR) = NULL;

//Include stuff
#include "../localization/strings.h"
#include "../include/error.c"
#include "include/autostart.c"
#include "include/tray.c"

//Entry point
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInstance, LPSTR szCmdLine, int iCmdShow) {
	g_hinst = hInst;
	
	//Look for previous instance
	WM_UPDATESETTINGS = RegisterWindowMessage(L"UpdateSettings");
	HWND previnst = FindWindow(APP_NAME, NULL);
	if (previnst != NULL) {
		PostMessage(previnst, WM_UPDATESETTINGS, 0, 0);
		return 0;
	}
	
	//Load settings
	wchar_t path[MAX_PATH];
	GetModuleFileName(NULL, path, sizeof(path)/sizeof(wchar_t));
	PathRemoveFileSpec(path);
	wcscat(path, L"\\"APP_NAME".ini");
	wchar_t txt[1000];
	//PatchList
	int patchlist_alloc = 0;
	GetPrivateProfileString(APP_NAME, L"PatchList", L"", txt, sizeof(txt)/sizeof(wchar_t), path);
	PatchList.data = malloc((wcslen(txt)+1)*sizeof(wchar_t));
	wcscpy(PatchList.data, txt);
	wchar_t *pos = PatchList.data;
	while (pos != NULL) {
		wchar_t *program = pos;
		//Move pos to next item (if any)
		pos = wcsstr(pos, L"|");
		if (pos != NULL) {
			*pos = '\0';
			pos++;
		}
		//Skip this item if it's empty
		if (wcslen(program) == 0) {
			continue;
		}
		//Make sure we have enough space
		if (PatchList.length == patchlist_alloc) {
			patchlist_alloc += 10;
			PatchList.items = realloc(PatchList.items, patchlist_alloc*sizeof(wchar_t*));
		}
		//Store item
		PatchList.items[PatchList.length] = program;
		PatchList.length++;
	}
	
	//Create window
	WNDCLASSEX wnd;
	wnd.cbSize = sizeof(WNDCLASSEX);
	wnd.style = 0;
	wnd.lpfnWndProc = WindowProc;
	wnd.cbClsExtra = 0;
	wnd.cbWndExtra = 0;
	wnd.hInstance = hInst;
	wnd.hIcon = NULL;
	wnd.hIconSm = NULL;
	wnd.hCursor = LoadImage(NULL, IDC_ARROW, IMAGE_CURSOR, 0, 0, LR_DEFAULTCOLOR|LR_SHARED);
	wnd.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
	wnd.lpszMenuName = NULL;
	wnd.lpszClassName = APP_NAME;
	RegisterClassEx(&wnd);
	g_hwnd = CreateWindowEx(0, wnd.lpszClassName, APP_NAME, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, HWND_MESSAGE, NULL, hInst, NULL);
	
	//Tray icon
	InitTray();
	UpdateTray();
	
	//Patch
	WM_SHUTDOWNBLOCKED = RegisterWindowMessage(L"ShutdownBlocked");
	PatchApps(0);
	SetTimer(g_hwnd, PATCHTIMER, PATCHINTERVAL, NULL);
	
	//Message loop
	MSG msg;
	while (GetMessage(&msg,NULL,0,0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return msg.wParam;
}

int PatchApps(int unpatch) {
	wchar_t txt[100];
	//We load the dll with ANSI functions
	//Get path to patch.dll
	#ifdef _WIN64
	char *dll, dll_x86[MAX_PATH], dll_x64[MAX_PATH];
	GetModuleFileNameA(NULL, dll_x86, sizeof(dll_x86));
	PathRemoveFileSpecA(dll_x86);
	strcpy(dll_x64, dll_x86);
	strcat(dll_x86, "\\patch.dll");
	strcat(dll_x64, "\\patch_x64.dll");
	#else
	char dll[MAX_PATH];
	GetModuleFileNameA(NULL, dll, sizeof(dll));
	PathRemoveFileSpecA(dll);
	strcat(dll, "\\patch.dll");
	#endif
	
	//Get address to LoadLibrary and FreeLibrary
	if (pfnLoadLibrary == NULL) {
		HMODULE kernel32 = GetModuleHandle(L"kernel32.dll");
		pfnLoadLibrary = (PVOID)GetProcAddress(kernel32,"LoadLibraryA");
		if (pfnLoadLibrary == NULL) {
			Error(L"GetProcAddress('LoadLibraryA')", L"Failed to load LoadLibrary().", GetLastError(), TEXT(__FILE__), __LINE__);
		}
	}
	if (pfnFreeLibrary == NULL) {
		HMODULE kernel32 = GetModuleHandle(L"kernel32.dll");
		pfnFreeLibrary = (PVOID)GetProcAddress(kernel32,"FreeLibrary");
		if (pfnFreeLibrary == NULL) {
			Error(L"GetProcAddress('FreeLibrary')", L"Failed to load FreeLibrary().", GetLastError(), TEXT(__FILE__), __LINE__);
		}
	}
	if (pfnLoadLibrary == NULL || pfnFreeLibrary == NULL) {
		enabled = 0;
		KillTimer(g_hwnd, PATCHTIMER);
		UpdateTray();
		return 1;
	}
	
	//Get SeDebugPrivilege so we can access all processes
	int SeDebugPrivilege = 0;
	//Get process token
	HANDLE hToken;
	TOKEN_PRIVILEGES tkp;
	if (OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&hToken) == 0) {
		#ifdef DEBUG
		Error(L"OpenProcessToken()", L"Could not get SeDebugPrivilege.\nPatchApps might not be able to patch some processes.\nYou might want to try running ShutdownGuard as admin.", GetLastError(), TEXT(__FILE__), __LINE__);
		#endif
	}
	else {
		//Get LUID for SeDebugPrivilege
		LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tkp.Privileges[0].Luid);
		tkp.PrivilegeCount = 1;
		tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		
		//Enable SeDebugPrivilege
		if (AdjustTokenPrivileges(hToken,FALSE,&tkp,0,NULL,0) == 0 || GetLastError() != ERROR_SUCCESS) {
			CloseHandle(hToken);
			#ifdef DEBUG
			Error(L"AdjustTokenPrivileges()", L"Could not get SeDebugPrivilege.\n"APP_NAME" might not be able to patch some processes.\nYou might want to try running "APP_NAME" as admin.", GetLastError(), TEXT(__FILE__), __LINE__);
			#endif
		}
		else {
			//Got it
			SeDebugPrivilege = 1;
		}
	}
	
	//Enumerate processes
	DWORD pids[1024], cbNeeded=0;
	if (EnumProcesses(pids,sizeof(pids),&cbNeeded) == 0) {
		Error(L"EnumProcesses()", L"Could not enumerate processes.", GetLastError(), TEXT(__FILE__), __LINE__);
		enabled = 0;
		KillTimer(g_hwnd, PATCHTIMER);
		UpdateTray();
		return 1;
	}
	
	//Loop processes
	int num = cbNeeded/sizeof(DWORD);
	int i;
	for (i=0; i < num; i++) {
		DWORD pid = pids[i];
		HANDLE process;
		
		//Check if we really want to patch this process
		//Bail if we have already patched it before or if it is a crucial system process
		if (pid < 10) {
			//"System Idle Process": pid 0
			//"System" process, pid 4 on XP and later, pid 8 on Win2K, and pid 2 on NT4
			continue;
		}
		
		//Open process
		process = OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ, FALSE, pid);
		if (process == NULL) {
			#ifdef DEBUG
			if (GetLastError() == 5) continue; //Access denied
			wsprintf(txt, L"Could not open process. pid: %d.", pid);
			Error(L"OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ)", txt, GetLastError(), TEXT(__FILE__), __LINE__);
			#endif
			continue;
		}
		
		//Get path of executable
		if (GetModuleFileNameEx(process,NULL,txt,MAX_PATH) != 0) {
			PathStripPath(txt);
			if (!wcscmp(txt,L"smss.exe") || !wcscmp(txt,L"csrss.exe") || !wcscmp(txt,L"ShutdownGuard.exe")) {
				//Don't even try it
				//Patching csrss.exe will crash the computer
				//Patching smss.exe fails
				//We don't want to patch ShutdownGuard
				CloseHandle(process);
				continue;
			}
		}
		
		//Check if this process is on the PatchList
		//When unpatching, ignore the PatchList
		int patch = 0;
		int j;
		if (PatchList.length == 0 || unpatch) {
			patch = 1;
		}
		else {
			for (j=0; j < PatchList.length; j++) {
				if (!wcsicmp(txt,PatchList.items[j])) {
					patch = 1;
					break;
				}
			}
		}
		if (!patch) {
			CloseHandle(process);
			continue;
		}
		
		//x64
		#ifdef _WIN64
		BOOL wow64 = FALSE;
		IsWow64Process(process, &wow64);
		dll = (wow64?dll_x86:dll_x64);
		#endif
		
		//Enumerate modules to check if this process has already been patched
		HMODULE modules[1024];
		int nummodules = 0;
		if (EnumProcessModules(process,modules,sizeof(modules),&cbNeeded) == 0) {
			#ifdef DEBUG
			wsprintf(txt, L"Could not enumerate modules. pid: %d.", pid);
			Error(L"EnumProcessModules()", txt, GetLastError(), TEXT(__FILE__), __LINE__);
			#endif
			//If unpatching, bail out since we have no chance to get a reference to the module
			if (unpatch) {
				CloseHandle(process);
				continue;
			}
		}
		else {
			nummodules = cbNeeded/sizeof(HMODULE);
		}
		
		//Loop modules
		HMODULE patchmod = NULL;
		for (j=0; j < nummodules; j++) {
			char modname[MAX_PATH];
			if (GetModuleFileNameExA(process,modules[j],modname,sizeof(modname)) == 0) {
				#ifdef DEBUG
				wsprintf(txt, L"Could not get module filename. pid: %d, module %d.", pid, j);
				Error(L"GetModuleFileNameExA()", txt, GetLastError(), TEXT(__FILE__), __LINE__);
				#endif
				//If one fails, all usually fails... bail out
				break;
			}
			//patch already loaded?
			if (!stricmp(modname,dll)) {
				patchmod = modules[j];
				patch = 0;
				break;
			}
		}
		
		//Close process
		CloseHandle(process);
		
		//Already patched?
		if (!patch && !unpatch) {
			continue;
		}
		
		//Ask the user
		/*int response = IDCANCEL;
		response = MessageBox(NULL, txt, L"Patch?", MB_ICONQUESTION|MB_YESNOCANCEL);
		if (response == IDNO) {
			continue;
		}
		else if (response == IDCANCEL) {
			break;
		}*/
		
		//Inject/Unload the dll
		if (!unpatch) {
			InjectDLL(pid, dll);
		}
		else {
			UnloadDLL(pid, patchmod);
		}
	}
	
	//Disable SeDebugPrivilege
	if (SeDebugPrivilege) {
		tkp.Privileges[0].Attributes = 0;
		AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0);
		CloseHandle(hToken);
	}
}

int InjectDLL(DWORD pid, char *dll) {
	wchar_t txt[100];
	//Open process
	//HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, TRUE, pid);
	HANDLE process = OpenProcess(PROCESS_CREATE_THREAD|PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ, TRUE, pid);
	if (process == NULL) {
		#ifdef DEBUG
		wsprintf(txt, L"Could not open process. pid: %d", pid);
		Error(L"OpenProcess()", txt, GetLastError(), TEXT(__FILE__), __LINE__);
		#endif
		return 1;
	}
	
	//Write dll path to process memory
	PVOID memory = VirtualAllocEx(process, NULL, strlen(dll)+1, MEM_COMMIT, PAGE_READWRITE);
	if (memory == NULL) {
		#ifdef DEBUG
		wsprintf(txt, L"Could not allocate memory in process. pid: %d.", pid);
		Error(L"VirtualAllocEx()", txt, GetLastError(), TEXT(__FILE__), __LINE__);
		#endif
		CloseHandle(process);
		return 1;
	}
	if (WriteProcessMemory(process,memory,dll,strlen(dll)+1,NULL) == 0) {
		#ifdef DEBUG
		wsprintf(txt, L"Could not write dll path to process memory. pid: %d.", pid);
		Error(L"WriteProcessMemory()", txt, GetLastError(), TEXT(__FILE__), __LINE__);
		#endif
		CloseHandle(process);
		return 1;
	}
	
	//Inject dll
	HANDLE thread = CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE)pfnLoadLibrary, memory, 0, NULL);
	if (thread == NULL) {
		#ifdef DEBUG
		wsprintf(txt, L"Could not inject the dll. pid: %d.", pid);
		Error(L"CreateRemoteThread()", txt, GetLastError(), TEXT(__FILE__), __LINE__);
		#endif
	}
	CloseHandle(thread);
	
	//Free memory (I don't think it's safe to do it here, right now this is a memory leak)
	//VirtualFreeEx(process, memory, strlen(dll)+1, MEM_RELEASE);
	
	//Close process
	CloseHandle(process);
	
	return 0;
}

int UnloadDLL(DWORD pid, HMODULE patch) {
	wchar_t txt[100];
	//Open process
	HANDLE process = OpenProcess(PROCESS_CREATE_THREAD|PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ, TRUE, pid);
	if (process == NULL) {
		#ifdef DEBUG
		wsprintf(txt, L"Could not open process. pid: %d", pid);
		Error(L"OpenProcess()", txt, GetLastError(), TEXT(__FILE__), __LINE__);
		#endif
		return 1;
	}
	
	//Unload dll
	if (CreateRemoteThread(process,NULL,0,(LPTHREAD_START_ROUTINE)pfnFreeLibrary,patch,0,NULL) == NULL) {
		#ifdef DEBUG
		wsprintf(txt, L"Could not unload the dll. pid: %d.", pid);
		Error(L"CreateRemoteThread()", txt, GetLastError(), TEXT(__FILE__), __LINE__);
		#endif
		CloseHandle(process);
	}
	
	//Close process
	CloseHandle(process);
	
	return 0;
}

void ToggleState() {
	enabled = !enabled;
	if (enabled) {
		SendMessage(g_hwnd, WM_UPDATESETTINGS, 0, 0);
	}
	else {
		KillTimer(g_hwnd, PATCHTIMER);
		PatchApps(1);
	}
	UpdateTray();
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_TRAY) {
		if (lParam == WM_LBUTTONDOWN || lParam == WM_LBUTTONDBLCLK) {
			ToggleState();
		}
		else if (lParam == WM_RBUTTONDOWN) {
			ShowContextMenu(hwnd);
		}
		else if (lParam == NIN_BALLOONUSERCLICK) { }
		else if (lParam == NIN_BALLOONTIMEOUT) { }
	}
	else if (msg == WM_UPDATESETTINGS) {
		//Load settings
		wchar_t path[MAX_PATH];
		GetModuleFileName(NULL, path, sizeof(path)/sizeof(wchar_t));
		PathRemoveFileSpec(path);
		wcscat(path, L"\\"APP_NAME".ini");
		wchar_t txt[1000];
		//PatchList
		KillTimer(g_hwnd, PATCHTIMER);
		PatchApps(1);
		int patchlist_alloc = 0;
		GetPrivateProfileString(APP_NAME, L"PatchList", L"", txt, sizeof(txt)/sizeof(wchar_t), path);
		PatchList.data = malloc((wcslen(txt)+1)*sizeof(wchar_t));
		wcscpy(PatchList.data, txt);
		wchar_t *pos = PatchList.data;
		while (pos != NULL) {
			wchar_t *program = pos;
			//Move pos to next item (if any)
			pos = wcsstr(pos, L"|");
			if (pos != NULL) {
				*pos = '\0';
				pos++;
			}
			//Skip this item if it's empty
			if (wcslen(program) == 0) {
				continue;
			}
			//Make sure we have enough space
			if (PatchList.length == patchlist_alloc) {
				patchlist_alloc += 10;
				PatchList.items = realloc(PatchList.items, patchlist_alloc*sizeof(wchar_t*));
			}
			//Store item
			PatchList.items[PatchList.length] = program;
			PatchList.length++;
		}
		PatchApps(0);
		SetTimer(hwnd, PATCHTIMER, PATCHINTERVAL, NULL);
	}
	else if (msg == WM_TASKBARCREATED) {
		tray_added = 0;
		UpdateTray();
	}
	else if (msg == WM_COMMAND) {
		int wmId=LOWORD(wParam), wmEvent=HIWORD(wParam);
		if (wmId == SWM_TOGGLE) {
			ToggleState();
		}
		else if (wmId == SWM_AUTOSTART_ON) {
			SetAutostart(1);
		}
		else if (wmId == SWM_AUTOSTART_OFF) {
			SetAutostart(0);
		}
		else if (wmId == SWM_SETTINGS) {
			wchar_t path[MAX_PATH];
			GetModuleFileName(NULL, path, sizeof(path)/sizeof(wchar_t));
			PathRemoveFileSpec(path);
			wcscat(path, L"\\"APP_NAME".ini");
			ShellExecute(NULL, L"open", path, NULL, NULL, SW_SHOWNORMAL);
		}
		else if (wmId == SWM_EXIT) {
			DestroyWindow(hwnd);
		}
	}
	else if (msg == WM_DESTROY) {
		showerror = 0;
		KillTimer(g_hwnd, PATCHTIMER);
		PatchApps(1);
		RemoveTray();
		PostQuitMessage(0);
		return 0;
	}
	else if (msg == WM_TIMER && enabled) {
		KillTimer(g_hwnd, PATCHTIMER);
		PatchApps(0);
		SetTimer(g_hwnd, PATCHTIMER, PATCHINTERVAL, NULL);
	}
	else if (msg == WM_SHUTDOWNBLOCKED) {
		//wParam = uFlags, lParam = dwReason
		UINT forced = (wParam&EWX_FORCE&EWX_FORCEIFHUNG);
		UINT uFlags = (wParam^forced);
		//Assemble message
		wchar_t text[1000];
		if (uFlags == EWX_LOGOFF) {
			wcscpy(text, L"Prevented log off.");
		}
		else if (uFlags == EWX_SHUTDOWN || wParam == EWX_POWEROFF) {
			wcscpy(text, L"Prevented shutdown.");
		}
		else if (uFlags == EWX_REBOOT || wParam == EWX_RESTARTAPPS) {
			wcscpy(text, L"Prevented reboot.");
		}
		else {
			wsprintf(text, L"Unrecognized shutdown code (%d).", uFlags);
		}
		//Display balloon tip
		wcsncpy(tray.szInfo, text, sizeof(tray.szInfo)/sizeof(wchar_t));
		tray.uFlags |= NIF_INFO;
		UpdateTray();
		tray.uFlags ^= NIF_INFO;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}
