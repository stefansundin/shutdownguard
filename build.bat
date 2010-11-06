@echo off

:: For traditional MinGW, set prefix32 to empty string
:: For mingw-w32, set prefix32 to i686-w64-mingw32-
:: For mingw-w64, set prefix64 to x86_64-w64-mingw32-

set prefix32=i686-w64-mingw32-
set prefix64=x86_64-w64-mingw32-
set l10n=en-US es-ES gl-ES

taskkill /IM ShutdownGuard.exe

if not exist build. mkdir build

if "%1" == "all" (
	%prefix32%windres -o build\shutdownguard.o include\shutdownguard.rc
	%prefix32%gcc -o build\ini.exe include\ini.c -lshlwapi
	
	@echo.
	echo Building binaries
	%prefix32%gcc -o "build\ShutdownGuard.exe" shutdownguard.c build\shutdownguard.o -mwindows -lshlwapi -lwininet -O2 -s
	if not exist "build\ShutdownGuard.exe". exit /b
	
	if "%2" == "x64" (
		if not exist "build\x64". mkdir "build\x64"
		%prefix64%windres -o build\x64\shutdownguard.o include\shutdownguard.rc
		%prefix64%gcc -o "build\x64\ShutdownGuard.exe" shutdownguard.c build\x64\shutdownguard.o -mwindows -lshlwapi -lwininet -O2 -s
		if not exist "build\x64\KillKeys.exe". exit /b
	)
	
	for %%f in (%l10n%) do (
		@echo.
		echo Putting together %%f
		if not exist "build\%%f\ShutdownGuard". mkdir "build\%%f\ShutdownGuard"
		copy "build\ShutdownGuard.exe" "build\%%f\ShutdownGuard"
		copy "localization\%%f\info.txt" "build\%%f\ShutdownGuard"
		copy "ShutdownGuard.ini" "build\%%f\ShutdownGuard"
		"build\ini.exe" "build\%%f\ShutdownGuard\ShutdownGuard.ini" ShutdownGuard Language %%f
		if "%2" == "x64" (
			if not exist "build\x64\%%f\ShutdownGuard". mkdir "build\x64\%%f\ShutdownGuard"
			copy "build\x64\ShutdownGuard.exe" "build\x64\%%f\ShutdownGuard"
			copy "build\%%f\ShutdownGuard\info.txt" "build\x64\%%f\ShutdownGuard"
			copy "build\%%f\ShutdownGuard\ShutdownGuard.ini" "build\x64\%%f\ShutdownGuard"
		)
	)
	
	@echo.
	echo Building installer
	if "%2" == "x64" (
		makensis /V2 /Dx64 installer.nsi
	) else (
		makensis /V2 installer.nsi
	)
) else if "%1" == "x64" (
	if not exist "build\x64". mkdir "build\x64"
	%prefix64%windres -o build\x64\shutdownguard.o include\shutdownguard.rc
	%prefix64%gcc -o ShutdownGuard.exe shutdownguard.c build\x64\shutdownguard.o -mwindows -lshlwapi -lwininet -g -DDEBUG
) else (
	%prefix32%gcc -o ShutdownGuard.exe shutdownguard.c build\shutdownguard.o -mwindows -lshlwapi -lwininet -g -DDEBUG
	
	if "%1" == "run" (
		start ShutdownGuard.exe
	)
)
