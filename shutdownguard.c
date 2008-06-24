/*
	ShutdownGuard - Prevent Windows shutdown
	Copyright (C) 2008  Stefan Sundin (recover89@gmail.com)
	
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
	
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	
	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>

#define _WIN32_IE 0x0600
#include <windows.h>

//Tray messages
#define WM_ICONTRAY            WM_USER+1
#define SWM_TOGGLE             WM_APP+1
#define SWM_HIDE               WM_APP+2
#define SWM_AUTOSTART_ON       WM_APP+3
#define SWM_AUTOSTART_OFF      WM_APP+4
#define SWM_AUTOSTART_HIDE_ON  WM_APP+5
#define SWM_AUTOSTART_HIDE_OFF WM_APP+6
#define SWM_ABOUT              WM_APP+7
#define SWM_EXIT               WM_APP+8

//Balloon stuff missing in MinGW
#define NIIF_USER 4
#define NIN_BALLOONSHOW        WM_USER+2
#define NIN_BALLOONHIDE        WM_USER+3
#define NIN_BALLOONTIMEOUT     WM_USER+4
#define NIN_BALLOONUSERCLICK   WM_USER+5


//Stuff
LPSTR szClassName="ShutdownGuard";
LRESULT CALLBACK MyWndProc(HWND, UINT, WPARAM, LPARAM);

//Global info
static HICON icon[2];
static NOTIFYICONDATA traydata;
static UINT WM_TASKBARCREATED;
static int tray_added=0;
static int enabled=1;
static int hide=0;

static char msg[100];

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInstance, LPSTR szCmdLine, int iCmdShow) {
	//Check command line
	if (!strcmp(szCmdLine,"-hide")) {
		hide=1;
	}
	
	//Create window class
	WNDCLASS wnd;
	wnd.style=CS_HREDRAW | CS_VREDRAW;
	wnd.lpfnWndProc=MyWndProc;
	wnd.cbClsExtra=0;
	wnd.cbWndExtra=0;
	wnd.hInstance=hInst;
	wnd.hIcon=LoadImage(NULL, IDI_APPLICATION, IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
	wnd.hCursor=LoadImage(NULL, IDC_ARROW, IMAGE_CURSOR, 0, 0, LR_DEFAULTCOLOR);
	wnd.hbrBackground=(HBRUSH)(COLOR_BACKGROUND+1);
	wnd.lpszMenuName=NULL;
	wnd.lpszClassName=szClassName;
	
	//Register class
	if (RegisterClass(&wnd) == 0) {
		sprintf(msg,"RegisterClass() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, msg, "ShutdownGuard Error", MB_ICONERROR|MB_OK);
		return 1;
	}
	
	//Create window
	HWND hWnd;
	hWnd=CreateWindow(szClassName, "ShutdownGuard", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInst, NULL);
	//ShowWindow(hWnd, iCmdShow); //Show
	//UpdateWindow(hWnd); //Update
	
	//Register TaskbarCreated so we can readd the tray icon if explorer.exe crashes
	if ((WM_TASKBARCREATED=RegisterWindowMessage("TaskbarCreated")) == 0) {
		sprintf(msg,"RegisterWindowMessage() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, msg, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
	}
	
	//Load tray icons
	if ((icon[0] = LoadImage(hInst, "tray-disabled", IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR)) == NULL) {
		sprintf(msg,"LoadImage() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, msg, "ShutdownGuard Error", MB_ICONERROR|MB_OK);
		PostQuitMessage(1);
	}
	if ((icon[1] = LoadImage(hInst, "tray-enabled", IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR)) == NULL) {
		sprintf(msg,"LoadImage() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, msg, "ShutdownGuard Error", MB_ICONERROR|MB_OK);
		PostQuitMessage(1);
	}
	
	//Create icondata
	traydata.cbSize=sizeof(NOTIFYICONDATA);
	traydata.uID=0;
	traydata.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;
	traydata.hWnd=hWnd;
	traydata.uCallbackMessage=WM_ICONTRAY;
	//Balloon tooltip
	traydata.uTimeout=10000;
	strncpy(traydata.szInfoTitle,"ShutdownGuard",sizeof(traydata.szInfoTitle));
	strncpy(traydata.szInfo,"Prevented Windows shutdown.\nPress me to continue shutdown.",sizeof(traydata.szInfo));
	traydata.dwInfoFlags=NIIF_USER;
	
	//Add tray icon
	if (!hide) {
		UpdateTray();
	}
	
	//Make windows query this program first
	if (SetProcessShutdownParameters(0x4FF,0) == 0) {
		sprintf(msg,"SetProcessShutdownParameters() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, msg, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
	}
	
	//Enable SeShutdownPrivilege
	HANDLE hToken;
	if (OpenThreadToken(GetCurrentThread(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, FALSE, &hToken) == 0) {
		if (GetLastError() == ERROR_NO_TOKEN) {
			if (ImpersonateSelf(SecurityImpersonation) == 0) {
				sprintf(msg,"ImpersonateSelf() failed (error: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
				MessageBox(NULL, msg, "ShutdownGuard Error", MB_ICONERROR|MB_OK);
				return 0;
			}
			
			if (OpenThreadToken(GetCurrentThread(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, FALSE, &hToken) == 0) {
				sprintf(msg,"OpenThreadToken() failed (error: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
				MessageBox(NULL, msg, "ShutdownGuard Error", MB_ICONERROR|MB_OK);
				return 0;
			}
		}
		else {
			sprintf(msg,"OpenThreadToken() failed (error: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
			MessageBox(NULL, msg, "ShutdownGuard Error", MB_ICONERROR|MB_OK);
			return 0;
		}
	}
	if (SetPrivilege(hToken, SE_SHUTDOWN_NAME, TRUE) == FALSE) {
		sprintf(msg,"SetPrivilege() failed (error: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, msg, "ShutdownGuard Error", MB_ICONERROR|MB_OK);
		CloseHandle(hToken);
		return 0;
	}
	CloseHandle(hToken);
	
	//Message loop
	MSG msg;
	while(GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return msg.wParam;
}

void ShowContextMenu(HWND hWnd) {
	POINT pt;
	GetCursorPos(&pt);
	HMENU hMenu, hAutostartMenu;
	if ((hMenu = CreatePopupMenu()) == NULL) {
		sprintf(msg,"CreatePopupMenu() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, msg, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
	}
	
	//Toggle
	InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_TOGGLE, (enabled?"Disable":"Enable"));
	//InsertMenu(hMenu, -1, MF_BYPOSITION|MF_SEPARATOR, SWM_ABOUT, "");
	
	//Hide
	InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_HIDE, "Hide tray");
	
	//Autostart
	//Check registry
	int autostart_enabled=0, autostart_hide=0;
	//Open key
	HKEY key;
	if (RegOpenKeyEx(HKEY_CURRENT_USER,"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_QUERY_VALUE,&key) != ERROR_SUCCESS) {
		sprintf(msg,"RegOpenKeyEx() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, msg, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
	}
	//Read value
	char autostart_value[MAX_PATH+10];
	DWORD len=sizeof(autostart_value);
	DWORD res=RegQueryValueEx(key,"ShutdownGuard",NULL,NULL,(LPBYTE)autostart_value,&len);
	if (res != ERROR_FILE_NOT_FOUND && res != ERROR_SUCCESS) {
		sprintf(msg,"RegQueryValueEx() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, msg, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
	}
	//Close key
	if (RegCloseKey(key) != ERROR_SUCCESS) {
		sprintf(msg,"RegCloseKey() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, msg, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
	}
	//Get path
	char path[MAX_PATH];
	if (GetModuleFileName(NULL,path,sizeof(path)) == 0) {
		sprintf(msg,"GetModuleFileName() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, msg, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
	}
	//Compare
	char pathcmp[MAX_PATH+10];
	sprintf(pathcmp,"\"%s\"",path);
	if (!strcmp(pathcmp,autostart_value)) {
		autostart_enabled=1;
	}
	sprintf(pathcmp,"\"%s\" -hide",path);
	if (!strcmp(pathcmp,autostart_value)) {
		autostart_enabled=1;
		autostart_hide=1;
	}
	
	if ((hAutostartMenu = CreatePopupMenu()) == NULL) {
		sprintf(msg,"CreatePopupMenu() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, msg, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
	}
	InsertMenu(hAutostartMenu, -1, MF_BYPOSITION|(autostart_enabled?MF_CHECKED:0), (autostart_enabled?SWM_AUTOSTART_OFF:SWM_AUTOSTART_ON), "Autostart");
	InsertMenu(hAutostartMenu, -1, MF_BYPOSITION|(autostart_hide?MF_CHECKED:0), (autostart_hide?SWM_AUTOSTART_HIDE_OFF:SWM_AUTOSTART_HIDE_ON), "Hide tray");
	InsertMenu(hMenu, -1, MF_BYPOSITION|MF_POPUP, (UINT)hAutostartMenu, "Autostart");
	InsertMenu(hMenu, -1, MF_BYPOSITION|MF_SEPARATOR, SWM_ABOUT, "");
	
	//About
	InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_ABOUT, "About");
	
	//Exit
	InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_EXIT, "Exit");

	//Must set window to the foreground, or else the menu won't disappear when clicking outside it
	SetForegroundWindow(hWnd);

	TrackPopupMenu(hMenu, TPM_BOTTOMALIGN, pt.x, pt.y, 0, hWnd, NULL );
	DestroyMenu(hMenu);
}

int UpdateTray() {
	strncpy(traydata.szTip,(enabled?"ShutdownGuard (enabled)":"ShutdownGuard (disabled)"),sizeof(traydata.szTip));
	traydata.hIcon=icon[enabled];
	
	if (Shell_NotifyIcon((tray_added?NIM_MODIFY:NIM_ADD),&traydata) == FALSE) {
		sprintf(msg,"Shell_NotifyIcon() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, msg, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
		return 1;
	}
	
	//Success
	tray_added=1;
	return 0;
}

int RemoveTray() {
	if (!tray_added) {
		//Tray not added
		return 1;
	}
	
	if (Shell_NotifyIcon(NIM_DELETE,&traydata) == FALSE) {
		sprintf(msg,"Shell_NotifyIcon() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, msg, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
		return 1;
	}
	
	//Success
	tray_added=0;
	return 0;
}

void ToggleState() {
	enabled=!enabled;
	UpdateTray();
}

void SetAutostart(int on, int hide) {
	//Open key
	HKEY key;
	if (RegOpenKeyEx(HKEY_CURRENT_USER,"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_SET_VALUE,&key) != ERROR_SUCCESS) {
		sprintf(msg,"RegOpenKeyEx() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, msg, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
		return;
	}
	if (on) {
		//Get path
		char path[MAX_PATH];
		if (GetModuleFileName(NULL,path,sizeof(path)) == 0) {
			sprintf(msg,"GetModuleFileName() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
			MessageBox(NULL, msg, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
			return;
		}
		//Add
		char value[MAX_PATH+10];
		if (hide) {
			sprintf(value,"\"%s\" -hide",path);
			if (RegSetValueEx(key,"ShutdownGuard",0,REG_SZ,value,strlen(value)+1) != ERROR_SUCCESS) {
				sprintf(msg,"RegSetValueEx() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
				MessageBox(NULL, msg, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
				return;
			}
		}
		else {
			sprintf(value,"\"%s\"",path);
			if (RegSetValueEx(key,"ShutdownGuard",0,REG_SZ,value,strlen(value)+1) != ERROR_SUCCESS) {
				sprintf(msg,"RegSetValueEx() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
				MessageBox(NULL, msg, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
				return;
			}
		}
	}
	else {
		//Remove
		if (RegDeleteValue(key,"ShutdownGuard") != ERROR_SUCCESS) {
			sprintf(msg,"RegDeleteValue() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
			MessageBox(NULL, msg, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
			return;
		}
	}
	//Close key
	if (RegCloseKey(key) != ERROR_SUCCESS) {
		sprintf(msg,"RegCloseKey() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, msg, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
		return;
	}
}

BOOL SetPrivilege(HANDLE hToken, LPCTSTR priv, BOOL bEnablePrivilege) {
	TOKEN_PRIVILEGES tp;
	LUID luid;
	TOKEN_PRIVILEGES tpPrevious;
	DWORD cbPrevious=sizeof(TOKEN_PRIVILEGES);

	if (!LookupPrivilegeValue(NULL, priv, &luid)) {
		return FALSE;
	}
	
	//Get current privileges
	tp.PrivilegeCount=1;
	tp.Privileges[0].Luid=luid;
	tp.Privileges[0].Attributes=0;

	if (AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), &tpPrevious, &cbPrevious) != 0 && GetLastError() != ERROR_SUCCESS) {
		return FALSE;
	}

	//Set privileges
	tpPrevious.PrivilegeCount=1;
	tpPrevious.Privileges[0].Luid=luid;

	if(bEnablePrivilege) {
		tpPrevious.Privileges[0].Attributes |= (SE_PRIVILEGE_ENABLED);
	}
	else {
		tpPrevious.Privileges[0].Attributes ^= (SE_PRIVILEGE_ENABLED & tpPrevious.Privileges[0].Attributes);
	}

	if (AdjustTokenPrivileges(hToken, FALSE, &tpPrevious, cbPrevious, NULL, NULL) != 0 && GetLastError() != ERROR_SUCCESS) {
		return FALSE;
	}

	return TRUE;
}

LRESULT CALLBACK CBTProc(INT nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode == HCBT_ACTIVATE) {
		//Edit the caption of the buttons
		SetDlgItemText((HWND)wParam,IDYES,"Log off");
		SetDlgItemText((HWND)wParam,IDNO,"Shutdown");
		SetDlgItemText((HWND)wParam,IDCANCEL,"Nothing");
	}
	return 0;
}


LRESULT CALLBACK MyWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_COMMAND) {
		int wmId=LOWORD(wParam), wmEvent=HIWORD(wParam);
		if (wmId == SWM_TOGGLE) {
			ToggleState();
		}
		if (wmId == SWM_HIDE) {
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
			MessageBox(NULL, "ShutdownGuard - 0.1\nhttp://shutdownguard.googlecode.com/\nrecover89@gmail.com\n\nPrevent Windows shutdown.\n\nYou can use -hide as a parameter to hide the tray icon.\n\nSend feedback to recover89@gmail.com", "About ShutdownGuard", MB_ICONINFORMATION|MB_OK);
		}
		else if (wmId == SWM_EXIT) {
			DestroyWindow(hWnd);
		}
	}
	else if (msg == WM_ICONTRAY) {
		if (lParam == WM_LBUTTONDOWN) {
			ToggleState();
		}
		else if (lParam == WM_RBUTTONDOWN) {
			ShowContextMenu(hWnd);
		}
		else if (lParam == NIN_BALLOONTIMEOUT) {
			if (hide) {
				RemoveTray();
			}
		}
		else if (lParam == NIN_BALLOONUSERCLICK) {
			hide=0;
			HHOOK hhk=SetWindowsHookEx(WH_CBT, &CBTProc, 0, GetCurrentThreadId());
			int response=MessageBox(NULL, "What do you want to do?", "ShutdownGuard", MB_ICONQUESTION|MB_YESNOCANCEL|MB_DEFBUTTON2);
			UnhookWindowsHookEx(hhk);
			if (response == IDYES || response == IDNO) {
				enabled=0;
				UpdateTray();
				ExitWindowsEx((response==IDYES?EWX_LOGOFF:EWX_SHUTDOWN),0);
			}
		}
	}
	else if (msg == WM_TASKBARCREATED) {
		tray_added=0;
		if (!hide) {
			UpdateTray();
		}
	}
	else if (msg == WM_DESTROY) {
		if (tray_added) {
			RemoveTray();
		}
		PostQuitMessage(0);
		return 0;
	}
	else if (msg == WM_QUERYENDSESSION) {
		if (enabled) {
			traydata.uFlags|=NIF_INFO;
			UpdateTray();
			traydata.uFlags^=NIF_INFO;
			return FALSE;
		}
		else {
			return TRUE;
		}
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}
