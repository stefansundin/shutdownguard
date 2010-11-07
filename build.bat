@echo off

:: For traditional MinGW, set prefix32 to empty string
:: For mingw-w32, set prefix32 to i686-w64-mingw32-
:: For mingw-w64, set prefix64 to x86_64-w64-mingw32-

set prefix32=i686-w64-mingw32-
set prefix64=x86_64-w64-mingw32-

taskkill /IM ShutdownPatcher.exe

if not exist build. mkdir build

if "%1" == "all" (
	%prefix32%windres -o build\patcher.o include\patcher.rc
	
	@echo.
	echo Building binaries
	if not exist "build\ShutdownPatcher". mkdir "build\ShutdownPatcher"
	%prefix32%gcc -o "build\ShutdownPatcher\ShutdownPatcher.exe" patcher.c build\patcher.o -mwindows -lshlwapi -lwininet -lpsapi -O2 -s
	if not exist "build\ShutdownPatcher\ShutdownPatcher.exe". exit /b
	%prefix32%windres -o build\patch.o include\patch.rc
	%prefix32%gcc -o "build\ShutdownPatcher\patch.dll" patch.c build\patch.o -mdll -O2 -s
	if not exist "build\ShutdownPatcher\patch.dll". exit /b
	copy "ShutdownPatcher.ini" "build\ShutdownPatcher"
	
	if "%2" == "x64" (
		if not exist "build\x64\ShutdownPatcher". mkdir "build\x64\ShutdownPatcher"
		%prefix64%windres -o build\x64\patcher.o include\patcher.rc
		%prefix64%gcc -o "build\x64\ShutdownPatcher\ShutdownPatcher.exe" patcher.c build\x64\patcher.o -mwindows -lshlwapi -lwininet -lpsapi -O2 -s
		if not exist "build\x64\ShutdownPatcher\ShutdownPatcher.exe". exit /b
		%prefix64%windres -o build\x64\patch_x64.o include\patch.rc
		%prefix64%gcc -o "build\x64\ShutdownPatcher\patch_x64.dll" patch.c build\x64\patch_x64.o -mdll -O2 -s
		if not exist "build\x64\ShutdownPatcher\patch_x64.dll". exit /b
		copy "ShutdownPatcher.ini" "build\x64\ShutdownPatcher"
		
		copy "build\x64\ShutdownPatcher\patch_x64.dll" "build\ShutdownPatcher\patch_x64.dll"
		copy "build\ShutdownPatcher\patch.dll" "build\x64\ShutdownPatcher\patch.dll"
	)
) else if "%1" == "x64" (
	if not exist "build\x64". mkdir "build\x64"
	%prefix64%windres -o build\x64\patcher.o include\patcher.rc
	%prefix64%gcc -o ShutdownPatcher.exe patcher.c build\x64\patcher.o -mwindows -lshlwapi -lwininet -lpsapi -g -DDEBUG
	%prefix32%windres -o build\patch_x64.o include\patch.rc
	%prefix32%gcc -o patch_x64.dll patch.c build\patch_x64.o -mdll -g -DDEBUG
) else (
	%prefix32%windres -o build\patcher.o include\patcher.rc
	%prefix32%gcc -o ShutdownPatcher.exe patcher.c build\patcher.o -mwindows -lshlwapi -lwininet -lpsapi -g -DDEBUG
	%prefix32%windres -o build\patch.o include\patch.rc
	%prefix32%gcc -o patch.dll patch.c build\patch.o -mdll -g -DDEBUG
	
	if "%1" == "run" (
		start ShutdownPatcher.exe
	)
)
