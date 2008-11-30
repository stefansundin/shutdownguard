;ShutdownGuard installer
;NSIS Unicode: http://www.scratchpaper.com/
;
;Copyright (C) 2008  Stefan Sundin (recover89@gmail.com)
;
;This program is free software: you can redistribute it and/or modify
;it under the terms of the GNU General Public License as published by
;the Free Software Foundation, either version 3 of the License, or
;(at your option) any later version.


!define L10N_NAME         "ShutdownGuard"
!define L10N_VERSION      0.2
!define L10N_FILE_VERSION 1

;General

!include "MUI2.nsh"

Name "${L10N_NAME}"
OutFile "build/${L10N_NAME}-${L10N_VERSION} (Installer).exe"
InstallDir "$PROGRAMFILES\${L10N_NAME}"
InstallDirRegKey HKCU "Software\${L10N_NAME}" "Install_Dir"
RequestExecutionLevel user
ShowInstDetails nevershow
ShowUninstDetails show
SetCompressor lzma

;Interface

!define MUI_LANGDLL_REGISTRY_ROOT "HKCU" 
!define MUI_LANGDLL_REGISTRY_KEY "Software\${L10N_NAME}" 
!define MUI_LANGDLL_REGISTRY_VALUENAME "Language"

!define MUI_COMPONENTSPAGE_NODESC

!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
;!define MUI_FINISHPAGE_SHOWREADME_TEXT "Read info.txt"
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\info.txt"
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_FUNCTION "Launch"

;Pages

!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

;Languages

;!insertmacro MUI_LANGUAGE "English"
;!insertmacro MUI_LANGUAGE "Spanish"

!include "localization\installer.nsh"

!insertmacro MUI_RESERVEFILE_LANGDLL

;Installer

Section "${L10N_NAME} (${L10N_VERSION})"
	SectionIn RO

	FindWindow $0 "${L10N_NAME}" ""
	IntCmp $0 0 continue
		MessageBox MB_ICONINFORMATION|MB_YESNO "$(L10N_RUNNING_INSTALL)" IDNO continue
			DetailPrint "$(L10N_CLOSING)"
			SendMessage $0 ${WM_CLOSE} 0 0
			Sleep 100
	continue:

	SetOutPath "$INSTDIR"

	;Store directory and version
	WriteRegStr HKCU "Software\${L10N_NAME}" "Install_Dir" "$INSTDIR"
	WriteRegStr HKCU "Software\${L10N_NAME}" "Version" "${L10N_VERSION}"

	IntCmp $LANGUAGE ${LANG_ENGLISH} english
	IntCmp $LANGUAGE ${LANG_SPANISH} spanish
	english:
		File "build\en-US\${L10N_NAME}\*.*"
		Goto files_installed
	spanish:
		File "build\es-ES\${L10N_NAME}\*.*"
		Goto files_installed

	files_installed:

	;Create uninstaller
	WriteUninstaller "Uninstall.exe"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${L10N_NAME}" "DisplayName" "${L10N_NAME}"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${L10N_NAME}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${L10N_NAME}" "NoModify" 1
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${L10N_NAME}" "NoRepair" 1
SectionEnd

Section "$(L10N_SHORTCUT)"
	CreateShortCut "$SMPROGRAMS\${L10N_NAME}.lnk" "$INSTDIR\${L10N_NAME}.exe" "" "$INSTDIR\${L10N_NAME}.exe" 0
SectionEnd

Section /o "$(L10N_AUTOSTART)"
	MessageBox MB_ICONINFORMATION|MB_YESNO|MB_DEFBUTTON2 "$(L10N_AUTOSTART_HIDE)" IDYES hide
		WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "${L10N_NAME}" '"$INSTDIR\${L10N_NAME}.exe"'
		Goto continue
	hide:
		WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "${L10N_NAME}" '"$INSTDIR\${L10N_NAME}.exe" -hide'
	continue:
SectionEnd

Function Launch
	Exec "$INSTDIR\${L10N_NAME}.exe"
FunctionEnd

Function .onInit
	!insertmacro MUI_LANGDLL_DISPLAY
FunctionEnd

;Uninstaller

Section "Uninstall"
	FindWindow $0 "${L10N_NAME}" ""
	IntCmp $0 0 continue
		MessageBox MB_ICONINFORMATION|MB_YESNO "$(L10N_RUNNING_UNINSTALL)" IDNO continue
			DetailPrint "$(L10N_CLOSING)"
			SendMessage $0 ${WM_CLOSE} 0 0
			Sleep 100
	continue:

	Delete /REBOOTOK "$INSTDIR\${L10N_NAME}.exe"
	Delete /REBOOTOK "$INSTDIR\info.txt"
	Delete /REBOOTOK "$INSTDIR\Uninstall.exe"
	RMDir /REBOOTOK "$INSTDIR"

	Delete /REBOOTOK "$SMPROGRAMS\${L10N_NAME}.lnk"

	DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "${L10N_NAME}"
	DeleteRegKey /ifempty HKCU "Software\${L10N_NAME}"
	DeleteRegKey /ifempty HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${L10N_NAME}"
SectionEnd

Function un.onInit
	!insertmacro MUI_UNGETLANGUAGE
FunctionEnd
