/*
	ShutdownGuard - Prevent Windows shutdown
	Copyright (C) 2009  Stefan Sundin (recover89@gmail.com)
	
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
*/

#include <stdio.h>
#include <stdlib.h>
#define _WIN32_WINNT 0x0500
#include <windows.h>

//App
#define APP_NAME "ShutdownGuard"

//Cool stuff
HMODULE app = NULL;
PROC fnExitWindowsEx = NULL;
UINT WM_SHUTDOWNBLOCKED = 0;
int patched = 0;

char txt[1000];

#ifdef DEBUG
void Log(char *line) {
	FILE *f = fopen("C:\\shutdownguard-log.txt","ab");
	fprintf(f, "%s\n", line);
	fflush(f);
	fclose(f);
}
void Error(char *func, int errorcode, char *file, int line) {
	//Format message
	char errormsg[100];
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, errorcode, 0, errormsg, sizeof(errormsg), NULL);
	errormsg[strlen(errormsg)-2] = '\0'; //Remove that damn newline at the end of the formatted error message
	//Write error to file
	sprintf(txt, "%s failed in file %s, line %d. Error: %s (%d).\n", func, file, line, errormsg, errorcode);
	Log(txt);
}
#else
#define Log(a)
#define Error(a,b,c,d)
#endif

int PatchIAT(PCSTR pszCalleeModName, PROC pfnCurrent, PROC pfnNew, HMODULE hmodCaller) {
	//Load dbghelp.dll
	HINSTANCE dbghelp = LoadLibrary("dbghelp.dll");
	if (dbghelp == NULL) {
		Error("LoadLibrary('dbghelp.dll')", GetLastError(), TEXT(__FILE__), __LINE__);
		return 1;
	}
	//Get address to ImageDirectoryEntryToData
	PVOID WINAPI (*ImageDirectoryEntryToData)(PVOID,BOOLEAN,USHORT,PULONG)=NULL;
	ImageDirectoryEntryToData = (PVOID)GetProcAddress(dbghelp,"ImageDirectoryEntryToData");
	if (ImageDirectoryEntryToData == NULL) {
		Error("GetProcAddress('ImageDirectoryEntryToData')", GetLastError(), TEXT(__FILE__), __LINE__);
		return 1;
	}
	
	ULONG ulSize;
	PIMAGE_IMPORT_DESCRIPTOR pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)ImageDirectoryEntryToData(hmodCaller, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &ulSize);
	
	//Unload dbghelp
	FreeLibrary(dbghelp);
	
	if (pImportDesc == NULL) {
		//This module has no import section
		return 1;
	}
	
	//Find the import descriptor containing references to callee's functions
	for (; pImportDesc->Name; pImportDesc++) {
		PSTR pszModName = (PSTR)((PBYTE) hmodCaller+pImportDesc->Name);
		if (lstrcmpiA(pszModName,pszCalleeModName) == 0) {
			break;
		}
	}
	
	if (pImportDesc->Name == 0) {
		//This module does not use ExitWindowsEx(), it does not import any functions from user32.dll
		return 1;
	}
	
	//Get the import address table (IAT)
	PIMAGE_THUNK_DATA pThunk = (PIMAGE_THUNK_DATA)((PBYTE) hmodCaller + pImportDesc->FirstThunk);
	
	//Loop through all functions
	for (; pThunk->u1.Function; pThunk++) {
		// Get the address of the function address.
		PROC* ppfn = (PROC*)&pThunk->u1.Function;
		
		if (*ppfn == pfnCurrent) {
			//The function pointers match
			
			//MessageBox(NULL, APP_NAME " patch.dll", "Press OK to patch.", MB_OK);
			
			//Replace function pointer
			WriteProcessMemory(GetCurrentProcess(), ppfn, &pfnNew, sizeof(pfnNew), NULL);
			
			//We did it; get out.
			return 0;
		}
	}
	
	//This module does not use ExitWindowsEx(), the function is not in the caller's import section
	return 1;
}

void ShutdownBlocked(UINT uFlags, DWORD dwReason) {
	#ifdef DEBUG
	sprintf(txt, "ShutdownBlocked(%d,%d)", uFlags, dwReason);
	Log(txt);
	#endif
	//This only works for desktop applications, and not services or system processes
	//Need to find a way to notify ShutdownGuard.exe from a system/service process!
	HWND wnd = FindWindow(APP_NAME,NULL);
	PostMessage(wnd, WM_SHUTDOWNBLOCKED, uFlags, dwReason);
}

BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD reason, LPVOID reserved) {
	if (reason == DLL_PROCESS_ATTACH) {
		WM_SHUTDOWNBLOCKED = RegisterWindowMessage("ShutdownBlocked");
		app = GetModuleHandle(NULL);
		//Get address to ExitWindowsEx()
		HMODULE user32 = LoadLibrary("user32.dll");
		fnExitWindowsEx = (PROC)GetProcAddress(user32,"ExitWindowsEx");
		FreeLibrary(user32);
		//Patch IAT
		if (PatchIAT("user32.dll",fnExitWindowsEx,(PROC)ShutdownBlocked,app) == 0) {
			patched = 1;
			#ifdef DEBUG
			sprintf(txt, "Patching was successful! pid: %d", GetCurrentProcessId());
			Log(txt);
			#endif
		}
		else {
			return FALSE;
		}
	}
	else if (reason == DLL_PROCESS_DETACH && patched) {
		//Unpatch
		if (PatchIAT("user32.dll",(PROC)ShutdownBlocked,fnExitWindowsEx,app) == 0) {
			#ifdef DEBUG
			sprintf(txt, "Unpatching was successful! pid: %d", GetCurrentProcessId());
			Log(txt);
			#endif
		}
	}
	return TRUE;
}
