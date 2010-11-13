#include <stdlib.h>
#include <stdio.h>
#include <windows.h>

int main() {
	int button = MessageBox(NULL, "Yes = Shutdown\nNo = Log off\nCancel = Cancel", "Shutdown of doom!", MB_YESNOCANCEL|MB_ICONINFORMATION);
	if (button == IDYES) {
		ExitWindowsEx(EWX_SHUTDOWN, 5);
	}
	else if (button == IDNO) {
		ExitWindowsEx(EWX_LOGOFF, 6);
	}
	return 0;
}
