;ShutdownGuard - lt-LT localization by p|b (pasmane@gmail.com)
;Do not localize APP_NAME, it will be automatically replaced.
;I have converted the file from Unicode to ANSI, and some diacritics have disappeared. This should be fixed when NSIS understands Unicode.
;
;Copyright (C) 2009  Stefan Sundin (recover89@gmail.com)
;
;This program is free software: you can redistribute it and/or modify
;it under the terms of the GNU General Public License as published by
;the Free Software Foundation, either version 3 of the License, or
;(at your option) any later version.

!if ${L10N_VERSION} == 2

!insertmacro MUI_LANGUAGE "Lithuanian"
!define LANG ${LANG_LITHUANIAN}

LangString L10N_UPDATE_SECTION    ${LANG} "Ieškoti atnaujinimo"
LangString L10N_UPDATE_DIALOG     ${LANG} "Rasta nauja versija.$\nNutraukti diegima ir eiti i tinklapi?"
LangString L10N_RUNNING           ${LANG} "${APP_NAME} šiuo metu veikia. Uždaryti?"
LangString L10N_RUNNING_UNINSTALL ${LANG} "Jei pasirinksite No, ${APP_NAME} bus visiškai pašalinta tik po sistemos perkrovimo."
LangString L10N_SHORTCUT          ${LANG} "Start Meniu nuoroda"
LangString L10N_AUTOSTART         ${LANG} "Autopaleidimas"
LangString L10N_AUTOSTART_HIDE    ${LANG} "Slepti deklo ikona"

!undef LANG

!else
!warning "Localization out of date!" ;Don't localize this
!endif
