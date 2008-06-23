
taskkill /IM ShutdownGuard.exe

windres -o resources.o resources.rc
gcc -o ShutdownGuard shutdownguard.c resources.o -mwindows

strip ShutdownGuard.exe

upx --best -qq ShutdownGuard.exe
