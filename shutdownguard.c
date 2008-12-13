/*
	ShutdownGuard - Prevent Windows shutdown
	Copyright (C) 2008  Stefan Sundin (recover89@gmail.com)
	
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

//Localization
#define L10N_NAME    L"ShutdownGuard"
#define L10N_VERSION L"0.2"
#ifndef L10N_FILE
#define L10N_FILE "localization/en-US/strings.h"
#endif
#include L10N_FILE
#if L10N_FILE_VERSION != 1
#error Localization out of date!
#endif

//Messages
#define WM_ICONTRAY            WM_USER+1
#define WM_ADDTRAY             WM_USER+2 //This value has to remain constant through versions
#define SWM_TOGGLE             WM_APP+1
#define SWM_HIDE               WM_APP+2
#define SWM_AUTOSTART_ON       WM_APP+3
#define SWM_AUTOSTART_OFF      WM_APP+4
#define SWM_AUTOSTART_HIDE_ON  WM_APP+5
#define SWM_AUTOSTART_HIDE_OFF WM_APP+6
#define SWM_ABOUT              WM_APP+7
#define SWM_SHUTDOWN           WM_APP+8
#define SWM_EXIT               WM_APP+9

//Balloon stuff missing in MinGW
#define NIIF_USER 4
#define NIN_BALLOONSHOW        WM_USER+2
#define NIN_BALLOONHIDE        WM_USER+3
#define NIN_BALLOONTIMEOUT     WM_USER+4
#define NIN_BALLOONUSERCLICK   WM_USER+5

//Vista shutdown stuff missing in MinGW
static HINSTANCE user32=NULL;
BOOL WINAPI (*ShutdownBlockReasonCreate)(HWND, LPCWSTR)=NULL;
BOOL WINAPI (*ShutdownBlockReasonDestroy)(HWND)=NULL;

//Boring stuff
LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
static HICON icon[2];
static NOTIFYICONDATA traydata;
static UINT WM_TASKBARCREATED=0;
static int tray_added=0;
static int hide=0;
static wchar_t txt[100];

//Cool stuff
static int enabled=1;
static int vista=0;

//Error message handling
static int showerror=1;

LRESULT CALLBACK ErrorMsgProc(INT nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode == HCBT_ACTIVATE) {
		//Edit the caption of the buttons
		SetDlgItemText((HWND)wParam,IDYES,L"Copy error");
		SetDlgItemText((HWND)wParam,IDNO,L"OK");
	}
	return 0;
}

void Error(wchar_t *func, wchar_t *info, int errorcode, int line) {
	if (showerror) {
		//Format message
		wchar_t errormsg[100];
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,NULL,errorcode,0,errormsg,sizeof(errormsg)/sizeof(wchar_t),NULL);
		errormsg[wcslen(errormsg)-2]='\0'; //Remove that damn newline at the end of the formatted error message
		swprintf(txt,L"%s failed in file %s, line %d.\nError: %s (%d)\n\n%s", func, TEXT(__FILE__), line, errormsg, errorcode, info);
		//Display message
		HHOOK hhk=SetWindowsHookEx(WH_CBT, &ErrorMsgProc, 0, GetCurrentThreadId());
		int response=MessageBox(NULL, txt, L10N_NAME" Error", MB_ICONERROR|MB_YESNO|MB_DEFBUTTON2);
		UnhookWindowsHookEx(hhk);
		if (response == IDYES) {
			//Copy message to clipboard
			OpenClipboard(NULL);
			EmptyClipboard();
			wchar_t *data=LocalAlloc(LMEM_FIXED,(wcslen(txt)+1)*sizeof(wchar_t));
			memcpy(data,txt,(wcslen(txt)+1)*sizeof(wchar_t));
			SetClipboardData(CF_UNICODETEXT,data);
			CloseClipboard();
		}
	}
}

//Entry point
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInstance, LPSTR szCmdLine, int iCmdShow) {
	//Look for previous instance
	HWND previnst;
	if ((previnst=FindWindow(L10N_NAME,NULL)) != NULL) {
		SendMessage(previnst,WM_ADDTRAY,0,0);
		return 0;
	}

	//Check command line
	if (!strcmp(szCmdLine,"-hide")) {
		hide=1;
	}
	
	//Create window class
	WNDCLASSEX wnd;
	wnd.cbSize=sizeof(WNDCLASSEX);
	wnd.style=0;
	wnd.lpfnWndProc=WindowProc;
	wnd.cbClsExtra=0;
	wnd.cbWndExtra=0;
	wnd.hInstance=hInst;
	wnd.hIcon=NULL;
	wnd.hIconSm=NULL;
	wnd.hCursor=LoadImage(NULL, IDC_ARROW, IMAGE_CURSOR, 0, 0, LR_DEFAULTCOLOR|LR_SHARED);
	wnd.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
	wnd.lpszMenuName=NULL;
	wnd.lpszClassName=L10N_NAME;
	
	//Register class
	RegisterClassEx(&wnd);
	
	//Create window
	HWND hwnd=CreateWindowEx(0,wnd.lpszClassName, L10N_NAME, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInst, NULL);
	
	//Register TaskbarCreated so we can re-add the tray icon if explorer.exe crashes
	if ((WM_TASKBARCREATED=RegisterWindowMessage(L"TaskbarCreated")) == 0) {
		Error(L"RegisterWindowMessage('TaskbarCreated')",L"This means the tray icon won't be added if (or should I say when) explorer.exe crashes.",GetLastError(),__LINE__);
	}
	
	//Load tray icons
	if ((icon[0] = LoadImage(hInst, L"tray-disabled", IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR)) == NULL) {
		Error(L"LoadImage('tray-disabled')",L"",GetLastError(),__LINE__);
		PostQuitMessage(1);
	}
	if ((icon[1] = LoadImage(hInst, L"tray-enabled", IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR)) == NULL) {
		Error(L"LoadImage('tray-enabled')",L"",GetLastError(),__LINE__);
		PostQuitMessage(1);
	}
	
	//Create icondata
	traydata.cbSize=sizeof(NOTIFYICONDATA);
	traydata.uID=0;
	traydata.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;
	traydata.hWnd=hwnd;
	traydata.uCallbackMessage=WM_ICONTRAY;
	//Balloon tooltip
	traydata.uTimeout=10000;
	wcsncpy(traydata.szInfoTitle,L10N_NAME,sizeof(traydata.szInfoTitle));
	wcsncpy(traydata.szInfo,L10N_BALLOON,sizeof(traydata.szInfo));
	traydata.dwInfoFlags=NIIF_USER;
	
	//Update tray icon
	UpdateTray();
	
	//Associate icon to hwnd to make it appear in the Vista shutdown dialog
	SendMessage(hwnd,WM_SETICON,ICON_BIG,(LPARAM)icon[1]);
	
	//Check if we are in Vista and load vista-specific shutdown functions if we are
	OSVERSIONINFO vi;
	vi.dwOSVersionInfoSize=sizeof(OSVERSIONINFO);
	GetVersionEx(&vi);
	if (vi.dwMajorVersion >= 6) {
		//Load user32.dll
		if ((user32=LoadLibraryEx(L"user32.dll",NULL,0)) == NULL) {
			Error(L"LoadLibrary('user32.dll')",L"This really shouldn't have happened.\nGo check the "L10N_NAME" website for an update. If the latest version doesn't fix this, please report it.",GetLastError(),__LINE__);
		}
		else {
			//Get address to ShutdownBlockReasonCreate
			if ((ShutdownBlockReasonCreate=GetProcAddress(user32,"ShutdownBlockReasonCreate")) == NULL) {
				Error(L"GetProcAddress('ShutdownBlockReasonCreate')",L"Failed to load Vista specific function.\nGo check the "L10N_NAME" website for an update. If the latest version doesn't fix this, please report it.",GetLastError(),__LINE__);
			}
			//ShutdownBlockReasonDestroy
			if ((ShutdownBlockReasonDestroy=GetProcAddress(user32,"ShutdownBlockReasonDestroy")) == NULL) {
				Error(L"GetProcAddress('ShutdownBlockReasonDestroy')",L"Failed to load Vista specific function.\nGo check the "L10N_NAME" website for an update. If the latest version doesn't fix this, please report it.",GetLastError(),__LINE__);
			}
			vista=1;
		}
	}
	
	//Make Windows query this program first
	if (SetProcessShutdownParameters(0x4FF,0) == 0) {
		Error(L"SetProcessShutdownParameters(0x4FF)",L"This means that programs started before "L10N_NAME" will probably be closed before the shutdown can be stopped.",GetLastError(),__LINE__);
	}
	
	//Message loop
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return msg.wParam;
}

void ShowContextMenu(HWND hwnd) {
	POINT pt;
	GetCursorPos(&pt);
	HMENU hMenu=CreatePopupMenu();
	
	//Toggle
	InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_TOGGLE, (enabled?L10N_MENU_DISABLE:L10N_MENU_ENABLE));
	
	//Hide
	InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_HIDE, L10N_MENU_HIDE);
	
	//Check autostart
	int autostart_enabled=0, autostart_hide=0;
	//Open key
	HKEY key;
	RegOpenKeyEx(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_QUERY_VALUE,&key);
	//Read value
	wchar_t autostart_value[MAX_PATH+10];
	DWORD len=sizeof(autostart_value);
	RegQueryValueEx(key,L10N_NAME,NULL,NULL,(LPBYTE)autostart_value,&len);
	//Close key
	RegCloseKey(key);
	//Get path
	wchar_t path[MAX_PATH];
	GetModuleFileName(NULL,path,MAX_PATH);
	//Compare
	wchar_t pathcmp[MAX_PATH+10];
	swprintf(pathcmp,L"\"%s\"",path);
	if (!wcscmp(pathcmp,autostart_value)) {
		autostart_enabled=1;
	}
	else {
		swprintf(pathcmp,L"\"%s\" -hide",path);
		if (!wcscmp(pathcmp,autostart_value)) {
			autostart_enabled=1;
			autostart_hide=1;
		}
	}
	//Autostart
	HMENU hAutostartMenu=CreatePopupMenu();
	InsertMenu(hAutostartMenu, -1, MF_BYPOSITION|(autostart_enabled?MF_CHECKED:0), (autostart_enabled?SWM_AUTOSTART_OFF:SWM_AUTOSTART_ON), L10N_MENU_AUTOSTART);
	InsertMenu(hAutostartMenu, -1, MF_BYPOSITION|(autostart_hide?MF_CHECKED:0), (autostart_hide?SWM_AUTOSTART_HIDE_OFF:SWM_AUTOSTART_HIDE_ON), L10N_MENU_HIDE);
	InsertMenu(hMenu, -1, MF_BYPOSITION|MF_POPUP, (UINT)hAutostartMenu, L10N_MENU_AUTOSTART);
	InsertMenu(hMenu, -1, MF_BYPOSITION|MF_SEPARATOR, 0, NULL);
	
	//Shutdown
	InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_SHUTDOWN, L10N_MENU_SHUTDOWN);
	InsertMenu(hMenu, -1, MF_BYPOSITION|MF_SEPARATOR, 0, NULL);
	
	//About
	InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_ABOUT, L10N_MENU_ABOUT);
	
	//Exit
	InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_EXIT, L10N_MENU_EXIT);

	//Track menu
	SetForegroundWindow(hwnd);
	TrackPopupMenu(hMenu, TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, NULL);
	DestroyMenu(hMenu);
}

int UpdateTray() {
	wcsncpy(traydata.szTip,(enabled?L10N_TRAY_ENABLED:L10N_TRAY_DISABLED),sizeof(traydata.szTip));
	traydata.hIcon=icon[enabled];
	
	//Only add or modify if not hidden or if balloon will be displayed
	if (!hide || traydata.uFlags&NIF_INFO) {
		if (Shell_NotifyIcon((tray_added?NIM_MODIFY:NIM_ADD),&traydata) == FALSE) {
			Error(L"Shell_NotifyIcon(NIM_ADD/NIM_MODIFY)",L"Failed to add tray icon.",GetLastError(),__LINE__);
			return 1;
		}
		
		//Success
		tray_added=1;
	}
	return 0;
}

int RemoveTray() {
	if (!tray_added) {
		//Tray not added
		return 1;
	}
	
	if (Shell_NotifyIcon(NIM_DELETE,&traydata) == FALSE) {
		Error(L"Shell_NotifyIcon(NIM_DELETE)",L"Failed to remove tray icon.",GetLastError(),__LINE__);
		return 1;
	}
	
	//Success
	tray_added=0;
	return 0;
}

void SetAutostart(int on, int hide) {
	//Open key
	HKEY key;
	int error;
	if ((error=RegOpenKeyEx(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_SET_VALUE,&key)) != ERROR_SUCCESS) {
		Error(L"RegOpenKeyEx(HKEY_CURRENT_USER,'Software\\Microsoft\\Windows\\CurrentVersion\\Run')",L"Error opening the registry.",error,__LINE__);
		return;
	}
	if (on) {
		//Get path
		wchar_t path[MAX_PATH];
		if (GetModuleFileName(NULL,path,MAX_PATH) == 0) {
			Error(L"GetModuleFileName(NULL)",L"",GetLastError(),__LINE__);
			return;
		}
		//Add
		wchar_t value[MAX_PATH+10];
		swprintf(value,(hide?L"\"%s\" -hide":L"\"%s\""),path);
		if ((error=RegSetValueEx(key,L10N_NAME,0,REG_SZ,(LPBYTE)value,(wcslen(value)+1)*sizeof(wchar_t))) != ERROR_SUCCESS) {
			Error(L"RegSetValueEx('"L10N_NAME"')",L"",error,__LINE__);
			return;
		}
	}
	else {
		//Remove
		if ((error=RegDeleteValue(key,L10N_NAME)) != ERROR_SUCCESS) {
			Error(L"RegDeleteValue('"L10N_NAME"')",L"",error,__LINE__);
			return;
		}
	}
	//Close key
	RegCloseKey(key);
}

void ToggleState() {
	enabled=!enabled;
	UpdateTray();
}

LRESULT CALLBACK ShutdownDialogProc(INT nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode == HCBT_ACTIVATE) {
		//Edit the caption of the buttons
		SetDlgItemText((HWND)wParam,IDYES,L10N_SHUTDOWN_LOGOFF);
		SetDlgItemText((HWND)wParam,IDNO,L10N_SHUTDOWN_SHUTDOWN);
		SetDlgItemText((HWND)wParam,IDCANCEL,L10N_SHUTDOWN_NOTHING);
	}
	return 0;
}

void AskShutdown() {
	HHOOK hhk=SetWindowsHookEx(WH_CBT, &ShutdownDialogProc, 0, GetCurrentThreadId());
	int response=MessageBox(NULL, L10N_SHUTDOWN_ASK, L10N_NAME, MB_ICONQUESTION|MB_YESNOCANCEL|MB_DEFBUTTON2|MB_SYSTEMMODAL);
	UnhookWindowsHookEx(hhk);
	if (response == IDYES || response == IDNO) {
		enabled=0;
		hide=0;
		UpdateTray();
		if (response == IDYES) {
			ExitWindowsEx(EWX_LOGOFF,0);
		}
		else {
			//Get process token
			HANDLE hToken;
			if (OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&hToken) == 0) {
				Error(L"OpenProcessToken()",L"Could not get privilege to shutdown computer. Try shutting down manually.",GetLastError(),__LINE__);
				return;
			}
			
			//Get LUID for SeShutdownPrivilege
			TOKEN_PRIVILEGES tkp;
			LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);
			tkp.PrivilegeCount=1;
			tkp.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED;
			
			//Enable SeShutdownPrivilege
			AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0); 
			if (GetLastError() != ERROR_SUCCESS) {
				Error(L"AdjustTokenPrivileges()",L"Could not get privilege to shutdown computer. Try shutting down manually.",GetLastError(),__LINE__);
				return;
			}
			
			//Do it!!
			ExitWindowsEx(EWX_SHUTDOWN,0);

			//Disable SeShutdownPrivilege
			tkp.Privileges[0].Attributes=0;
			AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0);
		}
		
	}
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_ICONTRAY) {
		if (lParam == WM_LBUTTONDOWN) {
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
			hide=0;
			AskShutdown();
		}
	}
	else if (msg == WM_ADDTRAY) {
		hide=0;
		UpdateTray();
	}
	else if (msg == WM_TASKBARCREATED) {
		tray_added=0;
		UpdateTray();
	}
	else if (msg == WM_COMMAND) {
		int wmId=LOWORD(wParam), wmEvent=HIWORD(wParam);
		if (wmId == SWM_TOGGLE) {
			ToggleState();
		}
		else if (wmId == SWM_HIDE) {
			hide=1;
			RemoveTray();
		}
		else if (wmId == SWM_AUTOSTART_ON) {
			SetAutostart(1,0);
		}
		else if (wmId == SWM_AUTOSTART_OFF) {
			SetAutostart(0,0);
		}
		else if (wmId == SWM_AUTOSTART_HIDE_ON) {
			SetAutostart(1,1);
		}
		else if (wmId == SWM_AUTOSTART_HIDE_OFF) {
			SetAutostart(1,0);
		}
		else if (wmId == SWM_ABOUT) {
			MessageBox(NULL, L10N_ABOUT, L10N_ABOUT_TITLE, MB_ICONINFORMATION|MB_OK);
		}
		else if (wmId == SWM_SHUTDOWN) {
			AskShutdown();
		}
		else if (wmId == SWM_EXIT) {
			DestroyWindow(hwnd);
		}
	}
	else if (msg == WM_DESTROY) {
		showerror=0;
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
				ShutdownBlockReasonCreate(hwnd,L10N_VISTA_REASON);
				hide=0;
				UpdateTray();
			}
			else {
				//Show balloon, in vista it would just be automatically dismissed by the shutdown dialog
				traydata.uFlags|=NIF_INFO;
				UpdateTray();
				traydata.uFlags^=NIF_INFO;
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
