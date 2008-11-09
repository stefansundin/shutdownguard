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
typedef void (*pfunc)();
pfunc ShutdownBlockReasonCreate;  //BOOL WINAPI ShutdownBlockReasonCreate(HWND, LPCWSTR);
pfunc ShutdownBlockReasonDestroy; //BOOL WINAPI ShutdownBlockReasonDestroy(HWND);

//Stuff
LRESULT CALLBACK MyWndProc(HWND, UINT, WPARAM, LPARAM);

static HICON icon[2];
static NOTIFYICONDATA traydata;
static UINT WM_TASKBARCREATED;
static int tray_added=0;
static int hide=0;

static int enabled=1;
static int vista=0;
static HINSTANCE hinstDLL=NULL;

static char txt[100];

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInstance, LPSTR szCmdLine, int iCmdShow) {
	//Look for previous instance
	HWND previnst;
	if ((previnst=FindWindow("ShutdownGuard",NULL)) != NULL) {
		SendMessage(previnst,WM_ADDTRAY,0,0);
		return 0;
	}

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
	wnd.lpszClassName="ShutdownGuard";
	
	//Register class
	if (RegisterClass(&wnd) == 0) {
		sprintf(txt,"RegisterClass() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "ShutdownGuard Error", MB_ICONERROR|MB_OK);
		return 1;
	}
	
	//Create window
	HWND hwnd=CreateWindow(wnd.lpszClassName, "ShutdownGuard", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInst, NULL);
	
	//Register TaskbarCreated so we can re-add the tray icon if explorer.exe crashes
	if ((WM_TASKBARCREATED=RegisterWindowMessage("TaskbarCreated")) == 0) {
		sprintf(txt,"RegisterWindowMessage() failed (error code: %d) in file %s, line %d.\nThis means the tray icon won't be added if (or should I say when) explorer.exe crashes.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
	}
	
	//Load tray icons
	if ((icon[0] = LoadImage(hInst, "tray-disabled", IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR)) == NULL) {
		sprintf(txt,"LoadImage() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "ShutdownGuard Error", MB_ICONERROR|MB_OK);
		PostQuitMessage(1);
	}
	if ((icon[1] = LoadImage(hInst, "tray-enabled", IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR)) == NULL) {
		sprintf(txt,"LoadImage() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "ShutdownGuard Error", MB_ICONERROR|MB_OK);
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
	strncpy(traydata.szInfoTitle,"ShutdownGuard",sizeof(traydata.szInfoTitle));
	strncpy(traydata.szInfo,"Prevented Windows shutdown.\nPress me to continue shutdown.",sizeof(traydata.szInfo));
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
		vista=1;
		//Load dll
		if ((hinstDLL=LoadLibrary((LPCTSTR)"user32.dll")) == NULL) {
			sprintf(txt,"Failed to load user32.dll. This really shouldn't have happened.\nGo check the ShutdownGuard website for an update, if the latest version doesn't fix this, please report it.\n\nError message:\nLoadLibrary() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
			MessageBox(NULL, txt, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
		}
		else {
			//Get address to ShutdownBlockReasonCreate
			if ((ShutdownBlockReasonCreate=(pfunc)GetProcAddress(hinstDLL,"ShutdownBlockReasonCreate")) == NULL) {
				sprintf(txt,"Failed to load Vista specific function.\nGo check the ShutdownGuard website for an update, if the latest version doesn't fix this, please report it.\n\nError message:\nGetProcAddress() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
				MessageBox(NULL, txt, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
			}
			//ShutdownBlockReasonDestroy
			if ((ShutdownBlockReasonDestroy=(pfunc)GetProcAddress(hinstDLL,"ShutdownBlockReasonDestroy")) == NULL) {
				sprintf(txt,"Failed to load Vista specific function.\nGo check the ShutdownGuard website for an update, if the latest version doesn't fix this, please report it.\n\nError message:\nGetProcAddress() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
				MessageBox(NULL, txt, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
			}
		}
	}
	
	//Make windows query this program first
	if (SetProcessShutdownParameters(0x4FF,0) == 0) {
		sprintf(txt,"SetProcessShutdownParameters() failed (error code: %d) in file %s, line %d.\nThis means that programs started before ShutdownGuard will be closed before the shutdown is stopped.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
	}
	
	//Message loop
	MSG msg;
	while(GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return msg.wParam;
}

void ShowContextMenu(HWND hwnd) {
	POINT pt;
	GetCursorPos(&pt);
	HMENU hMenu, hAutostartMenu;
	if ((hMenu = CreatePopupMenu()) == NULL) {
		sprintf(txt,"CreatePopupMenu() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
	}
	
	//Toggle
	InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_TOGGLE, (enabled?"Disable":"Enable"));
	
	//Hide
	InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_HIDE, "Hide tray");
	
	//Autostart
	//Check registry
	int autostart_enabled=0, autostart_hide=0;
	//Open key
	HKEY key;
	if (RegOpenKeyEx(HKEY_CURRENT_USER,"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_QUERY_VALUE,&key) != ERROR_SUCCESS) {
		sprintf(txt,"RegOpenKeyEx() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
	}
	//Read value
	char autostart_value[MAX_PATH+10];
	DWORD len=sizeof(autostart_value);
	DWORD res=RegQueryValueEx(key,"ShutdownGuard",NULL,NULL,(LPBYTE)autostart_value,&len);
	if (res != ERROR_FILE_NOT_FOUND && res != ERROR_SUCCESS) {
		sprintf(txt,"RegQueryValueEx() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
	}
	//Close key
	if (RegCloseKey(key) != ERROR_SUCCESS) {
		sprintf(txt,"RegCloseKey() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
	}
	//Get path
	char path[MAX_PATH];
	if (GetModuleFileName(NULL,path,sizeof(path)) == 0) {
		sprintf(txt,"GetModuleFileName() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
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
		sprintf(txt,"CreatePopupMenu() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
	}
	InsertMenu(hAutostartMenu, -1, MF_BYPOSITION|(autostart_enabled?MF_CHECKED:0), (autostart_enabled?SWM_AUTOSTART_OFF:SWM_AUTOSTART_ON), "Autostart");
	InsertMenu(hAutostartMenu, -1, MF_BYPOSITION|(autostart_hide?MF_CHECKED:0), (autostart_hide?SWM_AUTOSTART_HIDE_OFF:SWM_AUTOSTART_HIDE_ON), "Hide tray");
	InsertMenu(hMenu, -1, MF_BYPOSITION|MF_POPUP, (UINT)hAutostartMenu, "Autostart");
	InsertMenu(hMenu, -1, MF_BYPOSITION|MF_SEPARATOR, 0, NULL);
	
	//Shutdown
	InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_SHUTDOWN, "Shutdown");
	InsertMenu(hMenu, -1, MF_BYPOSITION|MF_SEPARATOR, 0, NULL);
	
	//About
	InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_ABOUT, "About");
	
	//Exit
	InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_EXIT, "Exit");

	//Must set window to the foreground, or else the menu won't disappear when clicking outside it
	SetForegroundWindow(hwnd);

	TrackPopupMenu(hMenu, TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, NULL );
	DestroyMenu(hMenu);
}

int UpdateTray() {
	strncpy(traydata.szTip,(enabled?"ShutdownGuard (enabled)":"ShutdownGuard (disabled)"),sizeof(traydata.szTip));
	traydata.hIcon=icon[enabled];
	
	//Only add or modify if not hidden or if balloon will be displayed
	if (!hide || traydata.uFlags&NIF_INFO) {
		if (Shell_NotifyIcon((tray_added?NIM_MODIFY:NIM_ADD),&traydata) == FALSE) {
			sprintf(txt,"Failed to add tray icon.\n\nError message:\nShell_NotifyIcon() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
			MessageBox(NULL, txt, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
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
		sprintf(txt,"Failed to remove tray icon.\n\nShell_NotifyIcon() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
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
		sprintf(txt,"RegOpenKeyEx() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
		return;
	}
	if (on) {
		//Get path
		char path[MAX_PATH];
		if (GetModuleFileName(NULL,path,sizeof(path)) == 0) {
			sprintf(txt,"GetModuleFileName() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
			MessageBox(NULL, txt, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
			return;
		}
		//Add
		char value[MAX_PATH+10];
		sprintf(value,(hide?"\"%s\" -hide":"\"%s\""),path);
		if (RegSetValueEx(key,"ShutdownGuard",0,REG_SZ,value,strlen(value)+1) != ERROR_SUCCESS) {
			sprintf(txt,"RegSetValueEx() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
			MessageBox(NULL, txt, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
			return;
		}
	}
	else {
		//Remove
		if (RegDeleteValue(key,"ShutdownGuard") != ERROR_SUCCESS) {
			sprintf(txt,"RegDeleteValue() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
			MessageBox(NULL, txt, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
			return;
		}
	}
	//Close key
	if (RegCloseKey(key) != ERROR_SUCCESS) {
		sprintf(txt,"RegCloseKey() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
		return;
	}
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

void AskShutdown() {
	HHOOK hhk=SetWindowsHookEx(WH_CBT, &CBTProc, 0, GetCurrentThreadId());
	int response=MessageBox(NULL, "What do you want to do?", "ShutdownGuard", MB_ICONQUESTION|MB_YESNOCANCEL|MB_DEFBUTTON2|MB_SYSTEMMODAL);
	UnhookWindowsHookEx(hhk);
	if (response == IDYES || response == IDNO) {
		enabled=0;
		UpdateTray();
		if (response == IDYES) {
			ExitWindowsEx(EWX_LOGOFF,0);
		}
		else {
			//Get process token
			HANDLE hToken;
			if (OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY, &hToken) == 0) {
				sprintf(txt,"Could not get privilege to shutdown computer. Try shutting down manually.\n\nError message:\nOpenProcessToken() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
				MessageBox(NULL, txt, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
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
				sprintf(txt,"Could not get privilege to shutdown computer. Try shutting down manually.\n\nError message:\nAdjustTokenPrivileges() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
				MessageBox(NULL, txt, "ShutdownGuard Warning", MB_ICONWARNING|MB_OK);
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

LRESULT CALLBACK MyWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_COMMAND) {
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
			MessageBox(NULL, "ShutdownGuard - 0.2\n\
http://shutdownguard.googlecode.com/\n\
recover89@gmail.com\n\
\n\
Prevent Windows shutdown.\n\
\n\
You can use -hide as a parameter to hide the tray icon.\n\
\n\
Send feedback to recover89@gmail.com", "About ShutdownGuard", MB_ICONINFORMATION|MB_OK);
		}
		else if (wmId == SWM_SHUTDOWN) {
			AskShutdown();
		}
		else if (wmId == SWM_EXIT) {
			DestroyWindow(hwnd);
		}
	}
	else if (msg == WM_ICONTRAY) {
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
		if (vista && hinstDLL) {
			FreeLibrary(hinstDLL);
		}
		PostQuitMessage(0);
		return 0;
	}
	else if (msg == WM_QUERYENDSESSION) {
		if (enabled) {
			//Prevent shutdown
			if (vista) {
				ShutdownBlockReasonCreate(hwnd,L"ShutdownGuard prevented Windows shutdown.");
				hide=0;
				UpdateTray();
			}
			else {
				//Show balloon, in vista it will just be automatically dismissed by the shutdown dialog
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
	else if (msg == WM_ADDTRAY) {
		hide=0;
		UpdateTray();
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}
