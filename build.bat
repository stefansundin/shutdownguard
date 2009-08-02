@echo off

taskkill /IM ShutdownGuard.exe

if not exist build (
	mkdir build
)

windres -o build/resources.o include\resources.rc

if "%1" == "all" (
	gcc -o build\ini.exe include\ini.c -lshlwapi
	
	@echo.
	echo Building binaries
	if not exist "build/en-US/ShutdownGuard" (
		mkdir "build\en-US\ShutdownGuard"
	)
	gcc -o "build/en-US/ShutdownGuard/ShutdownGuard.exe" shutdownguard.c build/resources.o -mwindows -lshlwapi -lwininet -lpsapi
	if not exist "build/en-US/ShutdownGuard/ShutdownGuard.exe" (
		exit /b
	)
	strip "build/en-US/ShutdownGuard/ShutdownGuard.exe"
	
	gcc -o "build/en-US/ShutdownGuard/patch.dll" patch.c -mdll
	if not exist "build/en-US/ShutdownGuard/patch.dll" (
		exit /b
	)
	strip "build/en-US/ShutdownGuard/patch.dll"
	
	for /D %%f in (localization/*) do (
		@echo.
		echo Putting together %%f
		if not %%f == en-US (
			if not exist "build/%%f/ShutdownGuard" (
				mkdir "build\%%f\ShutdownGuard"
			)
			copy "build\en-US\ShutdownGuard\ShutdownGuard.exe" "build/%%f/ShutdownGuard"
			copy "build\en-US\ShutdownGuard\patch.dll" "build/%%f/ShutdownGuard"
		)
		copy "localization\%%f\info.txt" "build/%%f/ShutdownGuard"
		copy ShutdownGuard.ini "build/%%f/ShutdownGuard"
		"build\ini.exe" "build\%%f\ShutdownGuard\ShutdownGuard.ini" ShutdownGuard Language %%f
	)
	
	@echo.
	echo Building installer
	makensis /V2 installer.nsi
) else (
	gcc -o ShutdownGuard.exe shutdownguard.c build/resources.o -mwindows -lshlwapi -lwininet -lpsapi -DDEBUG
	gcc -o patch.dll patch.c -mdll -DDEBUG
	
	if "%1" == "run" (
		start ShutdownGuard.exe
	)
	if "%1" == "hide" (
		start ShutdownGuard.exe -hide
	)
)
