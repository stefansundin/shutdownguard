/*
	ShutdownGuard - Prevent Windows shutdown
	Copyright (C) 2009  Stefan Sundin (recover89@gmail.com)
	
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
*/

#define UNICODE
#define _UNICODE

#include <stdio.h>
#include <stdlib.h>
#define _WIN32_IE 0x0600
#include <windows.h>
#include <shlwapi.h>
#include <psapi.h>

//App
#define APP_NAME      L"ShutdownGuard"
#define APP_VERSION   "0.4"
#define APP_URL       L"http://shutdownguard.googlecode.com/"
#define APP_UPDATEURL L"http://shutdownguard.googlecode.com/svn/wiki/latest-stable.txt"

//Messages
#define WM_ICONTRAY            WM_USER+1
#define SWM_TOGGLE             WM_APP+1
#define SWM_HIDE               WM_APP+2
#define SWM_AUTOSTART_ON       WM_APP+3
#define SWM_AUTOSTART_OFF      WM_APP+4
#define SWM_AUTOSTART_HIDE_ON  WM_APP+5
#define SWM_AUTOSTART_HIDE_OFF WM_APP+6
#define SWM_SHUTDOWN           WM_APP+7
#define SWM_UPDATE             WM_APP+8
#define SWM_ABOUT              WM_APP+9
#define SWM_EXIT               WM_APP+10

//Stuff missing in MinGW
#define HWND_MESSAGE ((HWND)-3)
#define NIIF_USER 4
#define NIN_BALLOONSHOW        WM_USER+2
#define NIN_BALLOONHIDE        WM_USER+3
#define NIN_BALLOONTIMEOUT     WM_USER+4
#define NIN_BALLOONUSERCLICK   WM_USER+5
//Vista shutdown stuff
HINSTANCE user32 = NULL;
BOOL WINAPI (*ShutdownBlockReasonCreate)(HWND,LPCWSTR) = NULL;
BOOL WINAPI (*ShutdownBlockReasonDestroy)(HWND) = NULL;

//Localization
struct strings {
	wchar_t *menu_enable;
	wchar_t *menu_disable;
	wchar_t *menu_hide;
	wchar_t *menu_autostart;
	wchar_t *menu_shutdown;
	wchar_t *menu_update;
	wchar_t *menu_about;
	wchar_t *menu_exit;
	wchar_t *tray_enabled;
	wchar_t *tray_disabled;
	wchar_t *prevent;
	wchar_t *balloon;
	wchar_t *shutdown_ask;
	wchar_t *shutdown_logoff;
	wchar_t *shutdown_shutdown;
	wchar_t *shutdown_reboot;
	wchar_t *shutdown_nothing;
	wchar_t *shutdown_help;
	wchar_t *update_balloon;
	wchar_t *update_dialog;
	wchar_t *about_title;
	wchar_t *about;
};
#include "localization/strings.h"
struct strings *l10n = &en_US;

//Boring stuff
LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
HICON icon[2];
NOTIFYICONDATA traydata;
UINT WM_TASKBARCREATED = 0;
UINT WM_UPDATESETTINGS = 0;
UINT WM_ADDTRAY = 0;
UINT WM_HIDETRAY = 0;
int tray_added = 0;
int hide = 0;
struct {
	wchar_t *Prevent;
	int Silent;
	wchar_t *HelpUrl;
	int PatchApps;
	int CheckForUpdate;
} settings = {NULL,0,NULL,0,0};
wchar_t txt[1000];

//Cool stuff
UINT WM_SHUTDOWNBLOCKED = 0;
int enabled = 1;
int vista = 0;

//Error() and CheckForUpdate()
#include "include/error.h"
#include "include/update.h"

//Entry point
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInstance, LPSTR szCmdLine, int iCmdShow) {
	//Check command line
	if (!strcmp(szCmdLine,"-hide")) {
		hide=1;
	}
	
	//Register messages
	WM_UPDATESETTINGS = RegisterWindowMessage(L"UpdateSettings");
	WM_ADDTRAY = RegisterWindowMessage(L"AddTray");
	WM_HIDETRAY = RegisterWindowMessage(L"HideTray");
	WM_SHUTDOWNBLOCKED = RegisterWindowMessage(L"ShutdownBlocked");
	
	//Look for previous instance
	HWND previnst = FindWindow(APP_NAME,NULL);
	if (previnst != NULL) {
		PostMessage(previnst, WM_UPDATESETTINGS, 0, 0);
		if (hide) {
			PostMessage(previnst, WM_HIDETRAY, 0, 0);
		}
		else {
			PostMessage(previnst, WM_ADDTRAY, 0, 0);
		}
		return 0;
	}
	
	//Load settings
	wchar_t path[MAX_PATH];
	GetModuleFileName(NULL, path, sizeof(path)/sizeof(wchar_t));
	PathRenameExtension(path, L".ini");
	//Language
	GetPrivateProfileString(APP_NAME, L"Language", L"en-US", txt, sizeof(txt)/sizeof(wchar_t), path);
	int i;
	for (i=0; i < num_languages; i++) {
		if (!wcscmp(txt,languages[i].code)) {
			l10n = languages[i].strings;
			break;
		}
	}
	//Prevent
	settings.Prevent = l10n->prevent;
	GetPrivateProfileString(APP_NAME, L"Prevent", L"", txt, sizeof(txt)/sizeof(wchar_t), path);
	if (wcslen(txt) != 0) {
		settings.Prevent = malloc((wcslen(txt)+1)*sizeof(wchar_t));
		wcscpy(settings.Prevent, txt);
	}
	//Silent
	GetPrivateProfileString(APP_NAME, L"Silent", L"0", txt, sizeof(txt)/sizeof(wchar_t), path);
	swscanf(txt, L"%d", &settings.Silent);
	//HelpUrl
	GetPrivateProfileString(APP_NAME, L"HelpUrl", L"", txt, sizeof(txt)/sizeof(wchar_t), path);
	if (wcslen(txt) != 0 && (!wcsncmp(txt,L"http://",7) || !wcsncmp(txt,L"https://",8))) {
		settings.HelpUrl = malloc((wcslen(txt)+1)*sizeof(wchar_t));
		wcscpy(settings.HelpUrl, txt);
	}
	//PatchApps
	GetPrivateProfileString(APP_NAME, L"PatchApps", L"0", txt, sizeof(txt)/sizeof(wchar_t), path);
	swscanf(txt, L"%d", &settings.PatchApps);
	//Update
	GetPrivateProfileString(L"Update", L"CheckForUpdate", L"0", txt, sizeof(txt)/sizeof(wchar_t), path);
	swscanf(txt, L"%d", &settings.CheckForUpdate);
	
	//Create window class
	WNDCLASSEX wnd;
	wnd.cbSize = sizeof(WNDCLASSEX);
	wnd.style = 0;
	wnd.lpfnWndProc = WindowProc;
	wnd.cbClsExtra = 0;
	wnd.cbWndExtra = 0;
	wnd.hInstance = hInst;
	wnd.hIcon = NULL;
	wnd.hIconSm = NULL;
	wnd.hCursor = LoadImage(NULL,IDC_ARROW,IMAGE_CURSOR,0,0,LR_DEFAULTCOLOR|LR_SHARED);
	wnd.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wnd.lpszMenuName = NULL;
	wnd.lpszClassName = APP_NAME;
	//Register class
	RegisterClassEx(&wnd);
	
	//Create window
	HWND hwnd = CreateWindowEx(0, wnd.lpszClassName, APP_NAME, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, HWND_MESSAGE, NULL, hInst, NULL);
	
	//Load icons
	icon[0] = LoadImage(hInst,L"tray-disabled",IMAGE_ICON,0,0,LR_DEFAULTCOLOR);
	icon[1] = LoadImage(hInst,L"tray-enabled",IMAGE_ICON,0,0,LR_DEFAULTCOLOR);
	if (icon[0] == NULL || icon[1] == NULL) {
		Error(L"LoadImage('tray-*')", L"Fatal error.", GetLastError(), TEXT(__FILE__), __LINE__);
		PostQuitMessage(1);
	}
	
	//Create icondata
	traydata.cbSize = sizeof(NOTIFYICONDATA);
	traydata.uID = 0;
	traydata.uFlags = NIF_MESSAGE|NIF_ICON|NIF_TIP;
	traydata.hWnd = hwnd;
	traydata.uCallbackMessage = WM_ICONTRAY;
	//Balloon tooltip
	traydata.uTimeout = 10000;
	wcsncpy(traydata.szInfoTitle, APP_NAME, sizeof(traydata.szInfoTitle)/sizeof(wchar_t));
	traydata.dwInfoFlags = NIIF_USER;
	
	//Register TaskbarCreated so we can re-add the tray icon if explorer.exe crashes
	WM_TASKBARCREATED=RegisterWindowMessage(L"TaskbarCreated");
	
	//Update tray icon
	UpdateTray();
	
	//Associate icon to hwnd to make it appear in the Vista shutdown dialog
	SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)icon[1]);
	
	//Check if we are in Vista and load vista-specific shutdown functions if we are
	OSVERSIONINFO vi;
	vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&vi);
	if (vi.dwMajorVersion >= 6) {
		//Load user32.dll
		user32 = LoadLibrary(L"user32.dll");
		if (user32 == NULL) {
			Error(L"LoadLibrary('user32.dll')", L"This really shouldn't have happened.\nGo check the "APP_NAME" website for an update. If the latest version doesn't fix this, please report it.", GetLastError(), TEXT(__FILE__), __LINE__);
		}
		else {
			//Get address to ShutdownBlockReasonCreate
			ShutdownBlockReasonCreate = GetProcAddress(user32,"ShutdownBlockReasonCreate");
			if (ShutdownBlockReasonCreate == NULL) {
				Error(L"GetProcAddress('ShutdownBlockReasonCreate')", L"Failed to load Vista specific function.\nGo check the "APP_NAME" website for an update. If the latest version doesn't fix this, please report it.", GetLastError(), TEXT(__FILE__), __LINE__);
			}
			//ShutdownBlockReasonDestroy
			ShutdownBlockReasonDestroy = GetProcAddress(user32,"ShutdownBlockReasonDestroy");
			if (ShutdownBlockReasonDestroy == NULL) {
				Error(L"GetProcAddress('ShutdownBlockReasonDestroy')", L"Failed to load Vista specific function.\nGo check the "APP_NAME" website for an update. If the latest version doesn't fix this, please report it.", GetLastError(), TEXT(__FILE__), __LINE__);
			}
			vista = 1;
		}
	}
	
	//Make Windows query this program first
	if (SetProcessShutdownParameters(0x4FF,0) == 0) {
		Error(L"SetProcessShutdownParameters(0x4FF)", L"This means that programs started before "APP_NAME" will probably be closed before the shutdown can be stopped.", GetLastError(), TEXT(__FILE__), __LINE__);
	}
	
	//Check for update
	if (settings.CheckForUpdate) {
		CheckForUpdate();
	}
	
	//Patch the IAT table of this process (proof-of-concept)
	if (settings.PatchApps) {
		PatchApps();
	}
	
	//Message loop
	MSG msg;
	while (GetMessage(&msg,NULL,0,0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return msg.wParam;
}

int PatchApps() {
	//Get path to patch.dll
	char path[MAX_PATH];
	GetModuleFileNameA(NULL, path, sizeof(path));
	PathRemoveFileSpecA(path);
	strcat(path, "\\patch.dll");
	
	//Get SeDebugPrivilege so we can access all processes
	int SeDebugPrivilege = 0;
	//Get process token
	HANDLE hToken;
	TOKEN_PRIVILEGES tkp;
	if (OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&hToken) == 0) {
		#ifdef DEBUG
		Error(L"OpenProcessToken()", L"PatchApps()", GetLastError(), TEXT(__FILE__), __LINE__);
		#endif
	}
	else {
		//Get LUID for SeDebugPrivilege
		LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tkp.Privileges[0].Luid);
		tkp.PrivilegeCount = 1;
		tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		
		//Enable SeDebugPrivilege
		if (AdjustTokenPrivileges(hToken,FALSE,&tkp,0,NULL,0) == 0 || GetLastError() != ERROR_SUCCESS) {
			#ifdef DEBUG
			Error(L"AdjustTokenPrivileges()", L"PatchApps()", GetLastError(), TEXT(__FILE__), __LINE__);
			#endif
		}
		else {
			//Got it
			SeDebugPrivilege = 1;
		}
	}
	
	//Get address to LoadLibrary
	HMODULE kernel32 = GetModuleHandle(L"kernel32.dll");
	HMODULE WINAPI (*pfnLoadLibrary)(LPCTSTR) = NULL;
	pfnLoadLibrary = (PVOID)GetProcAddress(kernel32,"LoadLibraryA");
	if (pfnLoadLibrary == NULL) {
		Error(L"GetProcAddress('LoadLibraryA')", L"Failed to load LoadLibrary().", GetLastError(), TEXT(__FILE__), __LINE__);
		return 1;
	}
	
	/*
	wsprintf(txt, L"LoadLibraryA: %d\nGetProcAddress: %d\nLoadLibraryA == GetProcAddress(): %d", LoadLibraryA, GetProcAddress(kernel32,"LoadLibraryA"),(PROC)LoadLibraryA == GetProcAddress(kernel32,"LoadLibraryA"));
	MessageBox(NULL, txt, APP_NAME, MB_ICONINFORMATION|MB_OK);
	*/
	
	//Load QueryFullProcessImageName
	BOOL WINAPI (*QueryFullProcessImageName)(HANDLE, DWORD, LPTSTR, PDWORD) = NULL;
	QueryFullProcessImageName = GetProcAddress(kernel32,"QueryFullProcessImageNameW");
	if (QueryFullProcessImageName == NULL) {
		Error(L"GetProcAddress('QueryFullProcessImageName')", L"Failed to load QueryFullProcessImageName().", GetLastError(), TEXT(__FILE__), __LINE__);
		return 1;
	}
	
	//Enumerate processes
	DWORD pids[1024], cbNeeded;
	if (EnumProcesses(pids,sizeof(pids),&cbNeeded) == 0) {
		Error(L"EnumProcesses()", L"Could not enumerate processes. PatchApps() failed.", GetLastError(), TEXT(__FILE__), __LINE__);
		return 1;
	}
	
	//Loop pids
	int num = cbNeeded/sizeof(DWORD);
	int i;
	for (i=0; i < num; i++) {
		if (pids[i] == 0) {
			continue;
		}
		HANDLE process;
		//Really do this?
		process=OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pids[i]);
		if (process == NULL) {
			wchar_t txt2[MAX_PATH];
			wsprintf(txt2, L"Could not open process.\npid: %d", pids[i]);
			Error(L"OpenProcess(PROCESS_QUERY_INFORMATION)", txt2, GetLastError(), TEXT(__FILE__), __LINE__);
			continue;
		}
		DWORD len = MAX_PATH;
		if (QueryFullProcessImageName(process,0,txt,&len) == 0) {
			Error(L"QueryFullProcessImageName()", L"Could not get path of process.", GetLastError(), TEXT(__FILE__), __LINE__);
		}
		CloseHandle(process);
		
		int response = MessageBox(NULL,txt,APP_NAME,MB_ICONQUESTION|MB_YESNO);
		if (response == IDYES) {
			//Open process
			//process = OpenProcess(PROCESS_ALL_ACCESS,TRUE,pids[i]);
			process = OpenProcess(PROCESS_CREATE_THREAD|PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ,TRUE,pids[i]);
			if (process == NULL) {
				wchar_t txt2[100];
				wsprintf(txt2, L"Could not open process.\npid: %d", pids[i]);
				Error(L"OpenProcess()", txt2 /*L"Could not open process."*/, GetLastError(), TEXT(__FILE__), __LINE__);
				continue;
			}
			
			//Write dll path to process memory
			PVOID memory = VirtualAllocEx(process,NULL,strlen(path)+1,MEM_COMMIT,PAGE_READWRITE);
			if (memory == NULL) {
				Error(L"VirtualAllocEx()", L"Could not allocate memory in process.", GetLastError(), TEXT(__FILE__), __LINE__);
				CloseHandle(process);
				continue;
			}
			if (WriteProcessMemory(process,memory,path,strlen(path)+1,NULL) == 0) {
				Error(L"WriteProcessMemory()", L"Could not write memory to process.", GetLastError(), TEXT(__FILE__), __LINE__);
				CloseHandle(process);
				continue;
			}
			
			//Inject dll
			if (CreateRemoteThread(process,NULL,0,(LPTHREAD_START_ROUTINE)pfnLoadLibrary,memory,0,NULL) == NULL) {
				Error(L"CreateRemoteThread()", L"Could not create remote thread.", GetLastError(), TEXT(__FILE__), __LINE__);
				CloseHandle(process);
			}
			
			//Free memory
			//VirtualFreeEx(process, memory, strlen(path)+1, MEM_RELEASE);
			
			//Close process
			CloseHandle(process);
		}
	}
	
	//Disable SeDebugPrivilege
	if (SeDebugPrivilege) {
		tkp.Privileges[0].Attributes = 0;
		AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0);
	}
}

void ShowContextMenu(HWND hwnd) {
	POINT pt;
	GetCursorPos(&pt);
	HMENU hMenu = CreatePopupMenu();
	
	//Toggle
	InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_TOGGLE, (enabled?l10n->menu_disable:l10n->menu_enable));
	
	//Hide
	InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_HIDE, l10n->menu_hide);
	
	//Check autostart
	int autostart_enabled=0, autostart_hide=0;
	//Registry
	HKEY key;
	wchar_t autostart_value[MAX_PATH+10] = L"";
	DWORD len = sizeof(autostart_value);
	RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &key);
	RegQueryValueEx(key, APP_NAME, NULL, NULL, (LPBYTE)autostart_value, &len);
	RegCloseKey(key);
	//Compare
	wchar_t path[MAX_PATH];
	GetModuleFileName(NULL, path, MAX_PATH);
	swprintf(txt, L"\"%s\"", path);
	if (!wcscmp(txt,autostart_value)) {
		autostart_enabled = 1;
	}
	else {
		swprintf(txt, L"\"%s\" -hide", path);
		if (!wcscmp(txt,autostart_value)) {
			autostart_enabled = 1;
			autostart_hide = 1;
		}
	}
	//Autostart
	HMENU hAutostartMenu = CreatePopupMenu();
	InsertMenu(hAutostartMenu, -1, MF_BYPOSITION|(autostart_enabled?MF_CHECKED:0), (autostart_enabled?SWM_AUTOSTART_OFF:SWM_AUTOSTART_ON), l10n->menu_autostart);
	InsertMenu(hAutostartMenu, -1, MF_BYPOSITION|(autostart_hide?MF_CHECKED:0), (autostart_hide?SWM_AUTOSTART_HIDE_OFF:SWM_AUTOSTART_HIDE_ON), l10n->menu_hide);
	InsertMenu(hMenu, -1, MF_BYPOSITION|MF_POPUP, (UINT)hAutostartMenu, l10n->menu_autostart);
	InsertMenu(hMenu, -1, MF_BYPOSITION|MF_SEPARATOR, 0, NULL);
	
	//Shutdown
	InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_SHUTDOWN, l10n->menu_shutdown);
	InsertMenu(hMenu, -1, MF_BYPOSITION|MF_SEPARATOR, 0, NULL);
	
	//Update
	if (update) {
		InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_UPDATE, l10n->menu_update);
		InsertMenu(hMenu, -1, MF_BYPOSITION|MF_SEPARATOR, 0, NULL);
	}
	
	//About
	InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_ABOUT, l10n->menu_about);
	
	//Exit
	InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_EXIT, l10n->menu_exit);

	//Track menu
	SetForegroundWindow(hwnd);
	TrackPopupMenu(hMenu, TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, NULL);
	DestroyMenu(hMenu);
}

int UpdateTray() {
	wcsncpy(traydata.szTip, (enabled?l10n->tray_enabled:l10n->tray_disabled), sizeof(traydata.szTip)/sizeof(wchar_t));
	traydata.hIcon = icon[enabled];
	
	//Only add or modify if not hidden or if balloon will be displayed
	if (!hide || traydata.uFlags&NIF_INFO) {
		int tries = 0; //Try at least ten times, sleep 100 ms between each attempt
		while (Shell_NotifyIcon((tray_added?NIM_MODIFY:NIM_ADD),&traydata) == FALSE) {
			tries++;
			if (tries >= 10) {
				Error(L"Shell_NotifyIcon(NIM_ADD/NIM_MODIFY)", L"Failed to update tray icon.", GetLastError(), TEXT(__FILE__), __LINE__);
				return 1;
			}
			Sleep(100);
		}
		
		//Success
		tray_added = 1;
	}
	return 0;
}

int RemoveTray() {
	if (!tray_added) {
		//Tray not added
		return 1;
	}
	
	if (Shell_NotifyIcon(NIM_DELETE,&traydata) == FALSE) {
		Error(L"Shell_NotifyIcon(NIM_DELETE)", L"Failed to remove tray icon.", GetLastError(), TEXT(__FILE__), __LINE__);
		return 1;
	}
	
	//Success
	tray_added = 0;
	return 0;
}

void SetAutostart(int on, int hide) {
	//Open key
	HKEY key;
	int error = RegCreateKeyEx(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,NULL,0,KEY_SET_VALUE,NULL,&key,NULL);
	if (error != ERROR_SUCCESS) {
		Error(L"RegOpenKeyEx(HKEY_CURRENT_USER,'Software\\Microsoft\\Windows\\CurrentVersion\\Run')", L"Error opening the registry.", error, TEXT(__FILE__), __LINE__);
		return;
	}
	if (on) {
		//Get path
		wchar_t path[MAX_PATH];
		if (GetModuleFileName(NULL,path,MAX_PATH) == 0) {
			Error(L"GetModuleFileName(NULL)", L"SetAutostart()", GetLastError(), TEXT(__FILE__), __LINE__);
			return;
		}
		//Add
		wchar_t value[MAX_PATH+10];
		swprintf(value, (hide?L"\"%s\" -hide":L"\"%s\""), path);
		error = RegSetValueEx(key,APP_NAME,0,REG_SZ,(LPBYTE)value,(wcslen(value)+1)*sizeof(wchar_t));
		if (error != ERROR_SUCCESS) {
			Error(L"RegSetValueEx('"APP_NAME"')", L"SetAutostart()", error, TEXT(__FILE__), __LINE__);
			return;
		}
	}
	else {
		//Remove
		error = RegDeleteValue(key,APP_NAME);
		if (error != ERROR_SUCCESS) {
			Error(L"RegDeleteValue('"APP_NAME"')", L"SetAutostart()", error, TEXT(__FILE__), __LINE__);
			return;
		}
	}
	//Close key
	RegCloseKey(key);
}

void ToggleState() {
	enabled = !enabled;
	UpdateTray();
	if (enabled) {
		SendMessage(traydata.hWnd, WM_UPDATESETTINGS, 0, 0);
	}
}

//Shutdown dialog
void CenterWindow(HWND wnd) {
	RECT window;
	GetWindowRect(wnd, &window);
	OffsetRect(&window, -window.left, -window.top);
	MoveWindow(wnd, ((GetSystemMetrics(SM_CXSCREEN)-window.right)/2+4)&~7, (GetSystemMetrics(SM_CYSCREEN)-window.bottom)/2, window.right, window.bottom, FALSE);
}

LRESULT CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_CREATE) {
		CenterWindow(hwnd);
		return 0;
	}
	else if (msg == WM_COMMAND) {
		int button = LOWORD(wParam);
		if (button == IDOK || button == IDCLOSE || button == IDRETRY) {
			enabled = 0;
			hide = 0;
			UpdateTray();
			if (button == IDOK) {
				ExitWindowsEx(EWX_LOGOFF, 0);
			}
			else {
				//Get process token
				HANDLE hToken;
				if (OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&hToken) == 0) {
					Error(L"OpenProcessToken()", L"Could not get privilege to shutdown computer. Try shutting down manually.", GetLastError(), TEXT(__FILE__), __LINE__);
					return 1;
				}
				
				//Get LUID for SeShutdownPrivilege
				TOKEN_PRIVILEGES tkp;
				LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);
				tkp.PrivilegeCount = 1;
				tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
				
				//Enable SeShutdownPrivilege
				AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0); 
				if (GetLastError() != ERROR_SUCCESS) {
					Error(L"AdjustTokenPrivileges()", L"Could not get privilege to shutdown computer. Try shutting down manually.", GetLastError(), TEXT(__FILE__), __LINE__);
					return 1;
				}
				
				//Do it!!
				ExitWindowsEx((button==IDCLOSE?EWX_SHUTDOWN:EWX_REBOOT), 0);
				
				//Disable SeShutdownPrivilege
				tkp.Privileges[0].Attributes = 0;
				AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0);
			}
		}
		else if (button == IDCANCEL) {
			DestroyWindow(hwnd);
		}
		else if (button == IDHELP) {
			ShellExecute(NULL, L"open", settings.HelpUrl, NULL, NULL, SW_SHOWNORMAL);
		}
	}
	else if (msg == WM_CLOSE || (msg == WM_KEYDOWN && wParam == VK_ESCAPE)) {
		//ShowWindow(hwnd, SW_HIDE);
		DestroyWindow(hwnd);
		return 0;
	}
	/*else if (msg == WM_SYSCOMMAND && wParam&0xFFF0 == SC_CLOSE) {
		return 0;
	}*/
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void AskShutdown() {
	HINSTANCE hInst = GetModuleHandle(NULL);
	
	//Create dialog
	WNDCLASSEX wnd;
	wnd.cbSize = sizeof(WNDCLASSEX);
	wnd.style = 0;
	wnd.lpfnWndProc = DialogProc;
	wnd.cbClsExtra = 0;
	wnd.cbWndExtra = 0;
	wnd.hInstance = hInst;
	wnd.hIcon = NULL;
	wnd.hIconSm = NULL;
	wnd.hCursor = LoadImage(NULL,IDC_ARROW,IMAGE_CURSOR,0,0,LR_DEFAULTCOLOR|LR_SHARED);
	wnd.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
	wnd.lpszMenuName = NULL;
	wnd.lpszClassName = APP_NAME L"-AskShutdown";
	RegisterClassEx(&wnd);
	
	//Create window
	HWND hwnd = CreateWindowEx(WS_EX_TOPMOST, wnd.lpszClassName, APP_NAME, WS_OVERLAPPEDWINDOW^WS_SIZEBOX^WS_MAXIMIZEBOX^WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, (settings.HelpUrl==NULL?360:450), 126, NULL, NULL, hInst, NULL);
	HWND text = CreateWindowEx(0, L"STATIC", l10n->shutdown_ask, WS_TABSTOP|WS_VISIBLE|WS_CHILD, 90, 22, 200, 25, hwnd, NULL, hInst, NULL);
	HWND logoff = CreateWindowEx(0, L"BUTTON", l10n->shutdown_logoff, BS_PUSHBUTTON|WS_TABSTOP|WS_VISIBLE|WS_CHILD, 15, 60, 75, 23, hwnd, (HMENU)IDOK, hInst, NULL);
	HWND shutdown = CreateWindowEx(0, L"BUTTON", l10n->shutdown_shutdown, BS_DEFPUSHBUTTON|WS_TABSTOP|WS_VISIBLE|WS_CHILD, 100, 60, 75, 23, hwnd, (HMENU)IDCLOSE, hInst, NULL);
	HWND reboot = CreateWindowEx(0, L"BUTTON", l10n->shutdown_reboot, BS_PUSHBUTTON|WS_TABSTOP|WS_VISIBLE|WS_CHILD, 185, 60, 75, 23, hwnd, (HMENU)IDRETRY, hInst, NULL);
	HWND nothing = CreateWindowEx(0, L"BUTTON", l10n->shutdown_nothing, BS_PUSHBUTTON|WS_TABSTOP|WS_VISIBLE|WS_CHILD, 270, 60, 75, 23, hwnd, (HMENU)IDCANCEL, hInst, NULL);
	
	//Set font
	HFONT font = CreateFont(14,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH|FF_SWISS,L"MS Sans Serif");
	SendMessage(text, WM_SETFONT, (WPARAM)font, TRUE);
	SendMessage(logoff, WM_SETFONT, (WPARAM)font, TRUE);
	SendMessage(shutdown, WM_SETFONT, (WPARAM)font, TRUE);
	SendMessage(reboot, WM_SETFONT, (WPARAM)font, TRUE);
	SendMessage(nothing, WM_SETFONT, (WPARAM)font, TRUE);
	
	//Help button
	if (settings.HelpUrl != NULL) {
		HWND help = CreateWindowEx(0, L"BUTTON", l10n->shutdown_help, BS_PUSHBUTTON|WS_TABSTOP|WS_VISIBLE|WS_CHILD, 355, 60, 75, 23, hwnd, (HMENU)IDHELP, hInst, NULL);
		SendMessage(help, WM_SETFONT, (WPARAM)font, TRUE);
	}
	
	//Associate icon with window
	SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)icon[1]);
	
	//Show window
	ShowWindow(hwnd, SW_SHOW);
	SetForegroundWindow(hwnd);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_ICONTRAY) {
		if (lParam == WM_LBUTTONDOWN || lParam == WM_LBUTTONDBLCLK) {
			ToggleState();
		}
		else if (lParam == WM_RBUTTONDOWN) {
			ShowContextMenu(hwnd);
		}
		else if (lParam == WM_MBUTTONDOWN) {
			AskShutdown();
		}
		else if (lParam == NIN_BALLOONTIMEOUT) {
			if (hide) {
				RemoveTray();
			}
		}
		else if (lParam == NIN_BALLOONUSERCLICK) {
			if (!wcscmp(traydata.szInfo,l10n->update_balloon)) {
				hide = 0;
				SendMessage(hwnd, WM_COMMAND, SWM_UPDATE, 0);
			}
			else {
				AskShutdown();
			}
			if (hide) {
				RemoveTray();
			}
		}
	}
	else if (msg == WM_UPDATESETTINGS) {
		//Load settings
		wchar_t path[MAX_PATH];
		GetModuleFileName(NULL, path, sizeof(path)/sizeof(wchar_t));
		PathRenameExtension(path, L".ini");
		//Language
		GetPrivateProfileString(APP_NAME, L"Language", L"en-US", txt,sizeof(txt)/sizeof(wchar_t), path);
		int i;
		for (i=0; i < num_languages; i++) {
			if (!wcscmp(txt,languages[i].code)) {
				l10n = languages[i].strings;
				break;
			}
		}
		//Prevent
		if (settings.Prevent != l10n->prevent) {
			free(settings.Prevent);
			settings.Prevent = l10n->prevent;
		}
		GetPrivateProfileString(APP_NAME, L"Prevent", L"", txt, sizeof(txt)/sizeof(wchar_t), path);
		if (wcslen(txt) != 0) {
			settings.Prevent = malloc((wcslen(txt)+1)*sizeof(wchar_t));
			wcscpy(settings.Prevent, txt);
		}
		//HelpUrl
		if (settings.HelpUrl != NULL) {
			free(settings.HelpUrl);
			settings.HelpUrl = NULL;
		}
		GetPrivateProfileString(APP_NAME, L"HelpUrl", L"", txt, sizeof(txt)/sizeof(wchar_t), path);
		if (wcslen(txt) != 0 && (!wcsncmp(txt,L"http://",7) || !wcsncmp(txt,L"https://",8))) {
			settings.HelpUrl = malloc((wcslen(txt)+1)*sizeof(wchar_t));
			wcscpy(settings.HelpUrl, txt);
		}
		//Silent
		GetPrivateProfileString(APP_NAME, L"Silent", L"0", txt, sizeof(txt)/sizeof(wchar_t), path);
		swscanf(txt, L"%d", &settings.Silent);
	}
	else if (msg == WM_ADDTRAY && (!settings.Silent || GetAsyncKeyState(VK_SHIFT)&0x8000)) {
		hide = 0;
		UpdateTray();
	}
	else if (msg == WM_HIDETRAY) {
		hide = 1;
		RemoveTray();
	}
	else if (msg == WM_TASKBARCREATED) {
		tray_added = 0;
		UpdateTray();
	}
	else if (msg == WM_SHUTDOWNBLOCKED) {
		//Display balloon tip
		wcsncpy(traydata.szInfo, settings.Prevent, (sizeof(traydata.szInfo))/sizeof(wchar_t));
		wcscat(traydata.szInfo, L"\n");
		wcscat(traydata.szInfo, l10n->balloon);
		traydata.uFlags |= NIF_INFO;
		UpdateTray();
		traydata.uFlags ^= NIF_INFO;
	}
	else if (msg == WM_COMMAND) {
		int wmId=LOWORD(wParam), wmEvent=HIWORD(wParam);
		if (wmId == SWM_TOGGLE) {
			ToggleState();
		}
		else if (wmId == SWM_HIDE) {
			hide = 1;
			RemoveTray();
		}
		else if (wmId == SWM_AUTOSTART_ON) {
			SetAutostart(1, 0);
		}
		else if (wmId == SWM_AUTOSTART_OFF) {
			SetAutostart(0, 0);
		}
		else if (wmId == SWM_AUTOSTART_HIDE_ON) {
			SetAutostart(1, 1);
		}
		else if (wmId == SWM_AUTOSTART_HIDE_OFF) {
			SetAutostart(1, 0);
		}
		else if (wmId == SWM_SHUTDOWN) {
			AskShutdown();
		}
		else if (wmId == SWM_UPDATE) {
			if (MessageBox(NULL,l10n->update_dialog,APP_NAME,MB_ICONINFORMATION|MB_YESNO|MB_SYSTEMMODAL) == IDYES) {
				ShellExecute(NULL, L"open", APP_URL, NULL, NULL, SW_SHOWNORMAL);
			}
		}
		else if (wmId == SWM_ABOUT) {
			MessageBox(NULL, l10n->about, l10n->about_title, MB_ICONINFORMATION|MB_OK);
		}
		else if (wmId == SWM_EXIT) {
			DestroyWindow(hwnd);
		}
	}
	else if (msg == WM_DESTROY) {
		showerror = 0;
		RemoveTray();
		if (user32) {
			FreeLibrary(user32);
		}
		PostQuitMessage(0);
		return 0;
	}
	else if (msg == WM_QUERYENDSESSION) {
		if (enabled) {
			//Prevent shutdown
			if (vista) {
				ShutdownBlockReasonCreate(hwnd, settings.Prevent);
				hide = 0;
				UpdateTray();
			}
			else if (!settings.Silent || !hide || GetAsyncKeyState(VK_SHIFT)&0x800) {
				//Show balloon, in vista it would just be automatically dismissed by the shutdown dialog
				wcsncpy(traydata.szInfo, settings.Prevent, (sizeof(traydata.szInfo))/sizeof(wchar_t));
				wcscat(traydata.szInfo, L"\n");
				wcscat(traydata.szInfo, l10n->balloon);
				traydata.uFlags |= NIF_INFO;
				UpdateTray();
				traydata.uFlags ^= NIF_INFO;
			}
			return FALSE;
		}
		else {
			if (vista) {
				ShutdownBlockReasonDestroy(hwnd);
			}
			return TRUE;
		}
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}
