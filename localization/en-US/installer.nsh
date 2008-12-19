;ShutdownGuard - en-US localization by Stefan Sundin (recover89@gmail.com)
;Do not localize APP_NAME, it will be automatically replaced.
;Keep this file in UTF–16LE
;
;Copyright (C) 2008  Stefan Sundin (recover89@gmail.com)
;
;This program is free software: you can redistribute it and/or modify
;it under the terms of the GNU General Public License as published by
;the Free Software Foundation, either version 3 of the License, or
;(at your option) any later version.

!if ${L10N_VERSION} == 2

!insertmacro MUI_LANGUAGE "English"
!define LANG ${LANG_ENGLISH}

LangString L10N_UPDATE_SECTION    ${LANG} "Check for update"
LangString L10N_UPDATE_DIALOG     ${LANG} "A new version is available.$\nAbort install and go to website?"
LangString L10N_RUNNING_INSTALL   ${LANG} "${APP_NAME} is running. Close?"
LangString L10N_RUNNING_UNINSTALL ${LANG} "${APP_NAME} is running. Close?$\nIf you choose No, ${APP_NAME} will be completely removed on next reboot."
LangString L10N_CLOSING           ${LANG} "Closing ${APP_NAME}"
LangString L10N_SHORTCUT          ${LANG} "Start Menu Shortcut"
LangString L10N_AUTOSTART         ${LANG} "Put ${APP_NAME} on autostart"
LangString L10N_AUTOSTART_HIDE    ${LANG} "Do you want the tray icon to be hidden on autostart?"

!undef LANG

!else
!warning "Localization out of date!"
!endif
