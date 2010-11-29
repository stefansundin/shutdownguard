;ShutdownGuard - nl-NL localization by Laurens van Dam (laurens94@gmail.com)
;
;This program is free software: you can redistribute it and/or modify
;it under the terms of the GNU General Public License as published by
;the Free Software Foundation, either version 3 of the License, or
;(at your option) any later version.

!insertmacro MUI_LANGUAGE "Dutch"
!define LANG ${LANG_DUTCH}

LangString L10N_UPGRADE_TITLE     ${LANG} "Al geïnstalleerd"
LangString L10N_UPGRADE_SUBTITLE  ${LANG} "Kies hoe je ${APP_NAME} wilt installeren."
LangString L10N_UPGRADE_HEADER    ${LANG} "${APP_NAME} is al geïnstalleerd op dit systeem. Selecteer de bewerking die je uit wilt voeren en klik op Volgende om verder te gaan."
LangString L10N_UPGRADE_UPGRADE   ${LANG} "&Upgrade ${APP_NAME} naar ${APP_VERSION}."
LangString L10N_UPGRADE_INI       ${LANG} "Je huidige instellingen zullen worden gekopiëerd naar ${APP_NAME}-old.ini."
LangString L10N_UPGRADE_INSTALL   ${LANG} "&Installeren op een nieuwe locatie"
;LangString L10N_UPGRADE_UNINSTALL ${LANG} "&Uninstall ${APP_NAME}."
LangString L10N_UPDATE_SECTION    ${LANG} "Controleer op updates voor het installeren"
LangString L10N_UPDATE_DIALOG     ${LANG} "Een nieuwe versie is beschikbaar.$\nInstallatie afbreken en naar de website gaan?"
LangString L10N_RUNNING           ${LANG} "${APP_NAME} is actief. Afsluiten?"
LangString L10N_RUNNING_UNINSTALL ${LANG} "Als je Nee kiest, wordt ${APP_NAME} compleet verwijderd bij de volgende reboot."
LangString L10N_SHORTCUT          ${LANG} "Start Menu Snelkoppeling"
LangString L10N_AUTOSTART         ${LANG} "Automatisch starten"
LangString L10N_AUTOSTART_HIDE    ${LANG} "In taakbalk verbergen"

!undef LANG
