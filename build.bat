@echo off

taskkill /IM ShutdownGuard.exe

if not exist build (
	mkdir build
)

windres -o build/resources.o resources.rc

if "%1" == "all" (
	@echo.
	echo Building binaries
	if not exist "build/en-US/ShutdownGuard" (
		mkdir "build\en-US\ShutdownGuard"
	)
	gcc -o "build/en-US/ShutdownGuard/ShutdownGuard.exe" shutdownguard.c build/resources.o -mwindows -lshlwapi -lwininet
	if exist "build/en-US/ShutdownGuard/ShutdownGuard.exe" (
		strip "build/en-US/ShutdownGuard/ShutdownGuard.exe"
	)
	
	for /D %%f in (localization/*) do (
		@echo.
		echo Putting together %%f
		if not %%f == en-US (
			if not exist "build/%%f/ShutdownGuard" (
				mkdir "build\%%f\ShutdownGuard"
			)
			copy "build\en-US\ShutdownGuard\ShutdownGuard.exe" "build/%%f/ShutdownGuard"
		)
		copy "localization\%%f\info.txt" "build/%%f/ShutdownGuard"
		copy "ShutdownGuard.ini" "build/%%f/ShutdownGuard"
	)
	
	@echo.
	echo Building installer
	makensis /V2 installer.nsi
) else (
	gcc -o ShutdownGuard.exe shutdownguard.c build/resources.o -mwindows -lshlwapi -lwininet -DDEBUG
	
	if "%1" == "run" (
		start ShutdownGuard.exe
	)
	if "%1" == "hide" (
		start ShutdownGuard.exe -hide
	)
)
