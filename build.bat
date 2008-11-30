@echo off

taskkill /IM ShutdownGuard.exe

if not exist build (
	mkdir build
)

windres -o build/resources.o resources.rc

if "%1" == "all" (
	echo Building all
	
	for /D %%f in (localization/*) do (
		@echo.
		echo Building %%f
		if not exist "build/%%f/ShutdownGuard" (
			mkdir "build\%%f\ShutdownGuard"
		)
		copy "localization\%%f\info.txt" "build/%%f/ShutdownGuard/info.txt"
		
		gcc -o "build/%%f/ShutdownGuard/ShutdownGuard.exe" shutdownguard.c build/resources.o -mwindows -DL10N_FILE=\"localization/%%f/strings.h\"
		if exist "build/%%f/ShutdownGuard/ShutdownGuard.exe" (
			strip "build/%%f/ShutdownGuard/ShutdownGuard.exe"
			upx --best -qq "build/%%f/ShutdownGuard/ShutdownGuard.exe"
		)
	)
	
	@echo.
	echo Building installer
	makensis /V2 installer.nsi
) else (
	gcc -o ShutdownGuard.exe shutdownguard.c build/resources.o -mwindows
	
	if "%1" == "run" (
		start ShutdownGuard.exe
	)
)
