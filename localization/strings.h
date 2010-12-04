
struct strings {
	wchar_t *menu_enable;
	wchar_t *menu_disable;
	wchar_t *menu_hide;
	wchar_t *menu_options;
	wchar_t *menu_autostart;
	wchar_t *menu_settings;
	wchar_t *menu_chkupdate;
	wchar_t *menu_update;
	wchar_t *menu_shutdown;
	wchar_t *menu_about;
	wchar_t *menu_exit;
	wchar_t *tray_enabled;
	wchar_t *tray_disabled;
	wchar_t *prevent;
	wchar_t *balloon;
	wchar_t *shutdown_ask;
	wchar_t *shutdown_logoff;
	wchar_t *shutdown_shutdown;
	wchar_t *shutdown_reboot;
	wchar_t *shutdown_nothing;
	wchar_t *shutdown_help;
	wchar_t *update_balloon;
	wchar_t *update_dialog;
	wchar_t *update_nonew;
	wchar_t *about_title;
	wchar_t *about;
};

#include "en-US/strings.h"
#include "es-ES/strings.h"
#include "gl-ES/strings.h"
//#include "lt-LT/strings.h"
#include "nl-NL/strings.h"
#include "nn-NO/strings.h"

struct {
	wchar_t *code;
	struct strings *strings;
} languages[] = {
	{L"en-US", &en_US},
	{L"es-ES", &es_ES},
	{L"gl-ES", &gl_ES},
	//{L"lt-LT", &lt_LT},
	{L"nl-NL", &nl_NL},
	{L"nn-NO", &nn_NO},
	{NULL}
};

struct strings *l10n = &en_US;
