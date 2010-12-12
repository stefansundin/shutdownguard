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

//App
#define APP_NAME            L"ShutdownGuard"
#define APP_VERSION         "1.0"
#define APP_URL             L"http://code.google.com/p/shutdownguard/"
#define APP_UPDATE_STABLE   L"http://shutdownguard.googlecode.com/svn/wiki/latest-stable.txt"
#define APP_UPDATE_UNSTABLE L"http://shutdownguard.googlecode.com/svn/wiki/latest-unstable.txt"

//Messages
#define WM_TRAY                WM_USER+1
#define SWM_TOGGLE             WM_APP+1
#define SWM_HIDE               WM_APP+2
#define SWM_AUTOSTART_ON       WM_APP+3
#define SWM_AUTOSTART_OFF      WM_APP+4
#define SWM_AUTOSTART_HIDE_ON  WM_APP+5
#define SWM_AUTOSTART_HIDE_OFF WM_APP+6
#define SWM_SETTINGS           WM_APP+7
#define SWM_CHECKFORUPDATE     WM_APP+8
#define SWM_SHUTDOWN           WM_APP+9
#define SWM_UPDATE             WM_APP+10
#define SWM_ABOUT              WM_APP+11
#define SWM_EXIT               WM_APP+12

//Stuff missing in MinGW
#ifndef NIIF_USER
#define NIIF_USER 4
#define NIN_BALLOONSHOW        WM_USER+2
#define NIN_BALLOONHIDE        WM_USER+3
#define NIN_BALLOONTIMEOUT     WM_USER+4
#define NIN_BALLOONUSERCLICK   WM_USER+5
#endif
//Vista shutdown stuff
typedef BOOL WINAPI (*_ShutdownBlockReasonCreate)(HWND, LPCWSTR);
typedef BOOL WINAPI (*_ShutdownBlockReasonDestroy)(HWND);
_ShutdownBlockReasonCreate ShutdownBlockReasonCreate;
_ShutdownBlockReasonDestroy ShutdownBlockReasonDestroy;

//Boring stuff
LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
HINSTANCE g_hinst = NULL;
HWND g_hwnd = NULL;
UINT WM_TASKBARCREATED = 0;
UINT WM_UPDATESETTINGS = 0;
UINT WM_ADDTRAY = 0;
UINT WM_HIDETRAY = 0;
struct {
	wchar_t *PreventMessage;
	int Silent;
	wchar_t *HelpUrl;
} settings = {NULL, 0, NULL};

//Cool stuff
int enabled = 1;
int vista = 0;
HWND hwnd_text, hwnd_logoff, hwnd_shutdown, hwnd_reboot, hwnd_nothing, hwnd_help;

//Include stuff
#include "localization/strings.h"
#include "include/error.c"
#include "include/autostart.c"
#include "include/tray.c"
#include "include/update.c"

//Entry point
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInstance, LPSTR szCmdLine, int iCmdShow) {
	g_hinst = hInst;
	
	//Check command line
	if (!strcmp(szCmdLine,"-hide")) {
		hide = 1;
	}
	
	//Look for previous instance
	WM_UPDATESETTINGS = RegisterWindowMessage(L"UpdateSettings");
	WM_ADDTRAY = RegisterWindowMessage(L"AddTray");
	WM_HIDETRAY = RegisterWindowMessage(L"HideTray");
	HWND previnst = FindWindow(APP_NAME, NULL);
	if (previnst != NULL) {
		PostMessage(previnst, WM_UPDATESETTINGS, 0, 0);
		PostMessage(previnst, (hide?WM_HIDETRAY:WM_ADDTRAY), 0, 0);
		return 0;
	}
	
	//Create window
	WNDCLASSEX wnd = {sizeof(WNDCLASSEX), 0, WindowProc, 0, 0, hInst, NULL, NULL, NULL, NULL, APP_NAME, NULL};
	wnd.hCursor = LoadImage(NULL, IDC_ARROW, IMAGE_CURSOR, 0, 0, LR_DEFAULTCOLOR|LR_SHARED);
	wnd.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
	RegisterClassEx(&wnd);
	g_hwnd = CreateWindowEx(WS_EX_TOPMOST, wnd.lpszClassName, APP_NAME, WS_OVERLAPPEDWINDOW^WS_SIZEBOX^WS_MAXIMIZEBOX^WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 360, 126, NULL, NULL, hInst, NULL);
	
	//Load settings
	wchar_t path[MAX_PATH];
	GetModuleFileName(NULL, path, sizeof(path)/sizeof(wchar_t));
	PathRemoveFileSpec(path);
	wcscat(path, L"\\"APP_NAME".ini");
	wchar_t txt[100];
	//Language
	GetPrivateProfileString(APP_NAME, L"Language", L"en-US", txt, sizeof(txt)/sizeof(wchar_t), path);
	int i;
	for (i=0; languages[i].code != NULL; i++) {
		if (!wcsicmp(txt,languages[i].code)) {
			l10n = languages[i].strings;
			break;
		}
	}
	//PreventMessage
	settings.PreventMessage = l10n->prevent;
	GetPrivateProfileString(APP_NAME, L"PreventMessage", L"", txt, sizeof(txt)/sizeof(wchar_t), path);
	if (wcslen(txt) != 0) {
		settings.PreventMessage = malloc((wcslen(txt)+1)*sizeof(wchar_t));
		wcscpy(settings.PreventMessage, txt);
	}
	//Silent
	GetPrivateProfileString(APP_NAME, L"Silent", L"0", txt, sizeof(txt)/sizeof(wchar_t), path);
	swscanf(txt, L"%d", &settings.Silent);
	//HelpUrl
	GetPrivateProfileString(APP_NAME, L"HelpUrl", L"", txt, sizeof(txt)/sizeof(wchar_t), path);
	if (wcslen(txt) != 0 && (!wcsnicmp(txt,L"http://",7) || !wcsnicmp(txt,L"https://",8))) {
		settings.HelpUrl = malloc((wcslen(txt)+1)*sizeof(wchar_t));
		wcscpy(settings.HelpUrl, txt);
	}
	
	//Create dialog
	CreateWindowEx(0, L"STATIC", L"app_icon", SS_ICON|WS_VISIBLE|WS_CHILD, 55, 12, 32, 32, g_hwnd, NULL, hInst, NULL);
	hwnd_text = CreateWindowEx(0, L"STATIC", l10n->shutdown_ask, WS_TABSTOP|WS_VISIBLE|WS_CHILD, 100, 22, 200, 25, g_hwnd, NULL, hInst, NULL);
	hwnd_logoff = CreateWindowEx(0, L"BUTTON", l10n->shutdown_logoff, BS_PUSHBUTTON|WS_TABSTOP|WS_VISIBLE|WS_CHILD, 15, 60, 75, 23, g_hwnd, (HMENU)IDOK, hInst, NULL);
	hwnd_shutdown = CreateWindowEx(0, L"BUTTON", l10n->shutdown_shutdown, BS_DEFPUSHBUTTON|WS_TABSTOP|WS_VISIBLE|WS_CHILD, 100, 60, 75, 23, g_hwnd, (HMENU)IDCLOSE, hInst, NULL);
	hwnd_reboot = CreateWindowEx(0, L"BUTTON", l10n->shutdown_reboot, BS_PUSHBUTTON|WS_TABSTOP|WS_VISIBLE|WS_CHILD, 185, 60, 75, 23, g_hwnd, (HMENU)IDRETRY, hInst, NULL);
	hwnd_nothing = CreateWindowEx(0, L"BUTTON", l10n->shutdown_nothing, BS_PUSHBUTTON|WS_TABSTOP|WS_VISIBLE|WS_CHILD, 270, 60, 75, 23, g_hwnd, (HMENU)IDCANCEL, hInst, NULL);
	hwnd_help = CreateWindowEx(0, L"BUTTON", l10n->shutdown_help, BS_PUSHBUTTON|WS_TABSTOP|WS_VISIBLE|WS_CHILD, 355, 60, 75, 23, g_hwnd, (HMENU)IDHELP, hInst, NULL);
	//Set font
	HFONT font = CreateFont(14, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH|FF_SWISS, L"MS Sans Serif");
	SendMessage(hwnd_text, WM_SETFONT, (WPARAM)font, TRUE);
	SendMessage(hwnd_logoff, WM_SETFONT, (WPARAM)font, TRUE);
	SendMessage(hwnd_shutdown, WM_SETFONT, (WPARAM)font, TRUE);
	SendMessage(hwnd_reboot, WM_SETFONT, (WPARAM)font, TRUE);
	SendMessage(hwnd_nothing, WM_SETFONT, (WPARAM)font, TRUE);
	SendMessage(hwnd_help, WM_SETFONT, (WPARAM)font, TRUE);
	
	//Tray icon
	InitTray();
	UpdateTray();
	
	//Associate icon to make it appear in the Vista+ shutdown dialog
	SendMessage(g_hwnd, WM_SETICON, ICON_BIG, (LPARAM)icon[1]);
	
	//Check if we are in Vista+ and load functions if we are
	OSVERSIONINFO vi;
	vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&vi);
	if (vi.dwMajorVersion >= 6) {
		//Load user32.dll
		HINSTANCE user32 = GetModuleHandle(L"user32.dll");
		if (user32 == NULL) {
			Error(L"GetModuleHandle('user32.dll')", L"Failed to load user32.dll.", GetLastError(), TEXT(__FILE__), __LINE__);
		}
		else {
			//Get address to ShutdownBlockReasonCreate()
			ShutdownBlockReasonCreate = (_ShutdownBlockReasonCreate)GetProcAddress(user32, "ShutdownBlockReasonCreate");
			if (ShutdownBlockReasonCreate == NULL) {
				Error(L"GetProcAddress('ShutdownBlockReasonCreate')", L"Failed to load Vista+ specific function.", GetLastError(), TEXT(__FILE__), __LINE__);
			}
			//ShutdownBlockReasonDestroy()
			ShutdownBlockReasonDestroy = (_ShutdownBlockReasonDestroy)GetProcAddress(user32, "ShutdownBlockReasonDestroy");
			if (ShutdownBlockReasonDestroy == NULL) {
				Error(L"GetProcAddress('ShutdownBlockReasonDestroy')", L"Failed to load Vista+ specific function.", GetLastError(), TEXT(__FILE__), __LINE__);
			}
			vista = 1;
		}
	}
	
	//Make Windows query this program first
	if (SetProcessShutdownParameters(0x4FF,0) == 0) {
		Error(L"SetProcessShutdownParameters(0x4FF)", L"This means that programs started before "APP_NAME" will be closed before the shutdown can be stopped.", GetLastError(), TEXT(__FILE__), __LINE__);
	}
	
	//Check for update
	GetPrivateProfileString(L"Update", L"CheckOnStartup", L"0", txt, sizeof(txt)/sizeof(wchar_t), path);
	int checkforupdate = _wtoi(txt);
	if (checkforupdate) {
		CheckForUpdate(0);
	}
	
	//Message loop
	MSG msg;
	while (GetMessage(&msg,NULL,0,0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return msg.wParam;
}

void ToggleState() {
	enabled = !enabled;
	if (enabled) {
		SendMessage(g_hwnd, WM_UPDATESETTINGS, 0, 0);
	}
	UpdateTray();
}

//Shutdown dialog
void CenterWindow(HWND wnd) {
	RECT window;
	GetWindowRect(wnd, &window);
	OffsetRect(&window, -window.left, -window.top);
	MoveWindow(wnd, ((GetSystemMetrics(SM_CXSCREEN)-window.right)/2+4)&~7, (GetSystemMetrics(SM_CYSCREEN)-window.bottom)/2, window.right, window.bottom, FALSE);
}

void AskShutdown() {
	//Help button
	if (settings.HelpUrl == NULL) {
		MoveWindow(g_hwnd, 0, 0, 360, 126, FALSE);
		ShowWindow(hwnd_help, SW_HIDE);
	}
	else {
		MoveWindow(g_hwnd, 0, 0, 450, 126, FALSE);
		ShowWindow(hwnd_help, SW_SHOW);
	}
	
	//Show window
	CenterWindow(g_hwnd);
	ShowWindow(g_hwnd, SW_SHOW);
	SetForegroundWindow(g_hwnd);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_TRAY) {
		if (lParam == WM_LBUTTONDOWN || lParam == WM_LBUTTONDBLCLK) {
			ToggleState();
		}
		else if (lParam == WM_RBUTTONDOWN) {
			ShowContextMenu(hwnd);
		}
		else if (lParam == WM_MBUTTONDOWN) {
			AskShutdown();
		}
		else if (lParam == NIN_BALLOONUSERCLICK) {
			if (!wcscmp(tray.szInfo,l10n->update_balloon)) {
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
		else if (lParam == NIN_BALLOONTIMEOUT) {
			if (hide) {
				RemoveTray();
			}
		}
	}
	else if (msg == WM_UPDATESETTINGS) {
		//Load settings
		wchar_t path[MAX_PATH];
		GetModuleFileName(NULL, path, sizeof(path)/sizeof(wchar_t));
		PathRemoveFileSpec(path);
		wcscat(path, L"\\"APP_NAME".ini");
		wchar_t txt[1000];
		//Language
		GetPrivateProfileString(APP_NAME, L"Language", L"en-US", txt, sizeof(txt)/sizeof(wchar_t), path);
		int i;
		for (i=0; languages[i].code != NULL; i++) {
			if (!wcsicmp(txt,languages[i].code)) {
				l10n = languages[i].strings;
				//Update shutdown dialog text
				SendMessage(hwnd_text, WM_SETTEXT, 0, (LPARAM)l10n->shutdown_ask);
				SendMessage(hwnd_logoff, WM_SETTEXT, 0, (LPARAM)l10n->shutdown_logoff);
				SendMessage(hwnd_shutdown, WM_SETTEXT, 0, (LPARAM)l10n->shutdown_shutdown);
				SendMessage(hwnd_reboot, WM_SETTEXT, 0, (LPARAM)l10n->shutdown_reboot);
				SendMessage(hwnd_nothing, WM_SETTEXT, 0, (LPARAM)l10n->shutdown_nothing);
				SendMessage(hwnd_help, WM_SETTEXT, 0, (LPARAM)l10n->shutdown_help);
				break;
			}
		}
		//PreventMessage
		if (settings.PreventMessage != l10n->prevent) {
			free(settings.PreventMessage);
			settings.PreventMessage = l10n->prevent;
		}
		GetPrivateProfileString(APP_NAME, L"PreventMessage", L"", txt, sizeof(txt)/sizeof(wchar_t), path);
		if (wcslen(txt) != 0) {
			settings.PreventMessage = malloc((wcslen(txt)+1)*sizeof(wchar_t));
			wcscpy(settings.PreventMessage, txt);
		}
		//Silent
		GetPrivateProfileString(APP_NAME, L"Silent", L"0", txt, sizeof(txt)/sizeof(wchar_t), path);
		swscanf(txt, L"%d", &settings.Silent);
		//HelpUrl
		if (settings.HelpUrl != NULL) {
			free(settings.HelpUrl);
			settings.HelpUrl = NULL;
		}
		GetPrivateProfileString(APP_NAME, L"HelpUrl", L"", txt, sizeof(txt)/sizeof(wchar_t), path);
		if (wcslen(txt) != 0 && (!wcsnicmp(txt,L"http://",7) || !wcsnicmp(txt,L"https://",8))) {
			settings.HelpUrl = malloc((wcslen(txt)+1)*sizeof(wchar_t));
			wcscpy(settings.HelpUrl, txt);
		}
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
		else if (wmId == SWM_SETTINGS) {
			wchar_t path[MAX_PATH];
			GetModuleFileName(NULL, path, sizeof(path)/sizeof(wchar_t));
			PathRemoveFileSpec(path);
			wcscat(path, L"\\"APP_NAME".ini");
			ShellExecute(NULL, L"open", path, NULL, NULL, SW_SHOWNORMAL);
		}
		else if (wmId == SWM_CHECKFORUPDATE) {
			CheckForUpdate(1);
		}
		else if (wmId == SWM_UPDATE) {
			if (MessageBox(NULL,l10n->update_dialog,APP_NAME,MB_ICONINFORMATION|MB_YESNO|MB_SYSTEMMODAL) == IDYES) {
				ShellExecute(NULL, L"open", APP_URL, NULL, NULL, SW_SHOWNORMAL);
			}
		}
		else if (wmId == SWM_SHUTDOWN) {
			AskShutdown();
		}
		else if (wmId == SWM_ABOUT) {
			MessageBox(NULL, l10n->about, l10n->about_title, MB_ICONINFORMATION|MB_OK);
		}
		else if (wmId == SWM_EXIT) {
			DestroyWindow(hwnd);
		}
		else if (wmId == IDOK || wmId == IDCLOSE || wmId == IDRETRY) {
			enabled = 0;
			hide = 0;
			UpdateTray();
			if (wmId == IDOK) {
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
				ExitWindowsEx((wmId==IDCLOSE?EWX_SHUTDOWN:EWX_REBOOT), 0);
				
				//Disable SeShutdownPrivilege
				tkp.Privileges[0].Attributes = 0;
				AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0);
			}
			ShowWindow(hwnd, SW_HIDE);
		}
		else if (wmId == IDCANCEL) {
			ShowWindow(hwnd, SW_HIDE);
		}
		else if (wmId == IDHELP) {
			ShellExecute(NULL, L"open", settings.HelpUrl, NULL, NULL, SW_SHOWNORMAL);
		}
	}
	else if ((msg == WM_CLOSE && IsWindowVisible(hwnd)) || (msg == WM_KEYDOWN && wParam == VK_ESCAPE)) {
		ShowWindow(hwnd, SW_HIDE);
		return 0;
	}
	else if (msg == WM_DESTROY) {
		showerror = 0;
		RemoveTray();
		PostQuitMessage(0);
		return 0;
	}
	else if (msg == WM_QUERYENDSESSION) {
		if (enabled) {
			//Prevent shutdown
			if (vista) {
				ShutdownBlockReasonCreate(hwnd, settings.PreventMessage);
				hide = 0;
				UpdateTray();
			}
			else if (!settings.Silent || !hide || GetAsyncKeyState(VK_SHIFT)&0x800) {
				//Show balloon, in vista it would just be automatically dismissed by the shutdown dialog
				wcsncpy(tray.szInfo, settings.PreventMessage, (sizeof(tray.szInfo))/sizeof(wchar_t));
				wcscat(tray.szInfo, L"\n");
				wcscat(tray.szInfo, l10n->balloon);
				tray.uFlags |= NIF_INFO;
				UpdateTray();
				tray.uFlags ^= NIF_INFO;
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
