;ShutdownGuard - nn-NO lokalisering av Jon Stødle (jonstodle@gmail.com)
;
;Dette programmet er fri programvare: du kan redistribuere det og/eller
;endre det under GNU General Public License-lisensen, publisert av
;Free Software Foundation, anten versjon 3 av lisensen eller (om du vil)
;ein seinare versjon.

!insertmacro MUI_LANGUAGE "NorwegianNynorsk"
!define LANG ${LANG_NORWEGIANNYNORSK}

LangString L10N_UPGRADE_TITLE     ${LANG} "Allereie Installert"
LangString L10N_UPGRADE_SUBTITLE  ${LANG} "Vel korleis du vil installere ${APP_NAME}."
LangString L10N_UPGRADE_HEADER    ${LANG} "${APP_NAME} er allereie installert på denne maskinen. Vel kva du vil gjere og klikk på Neste for å halde fram."
LangString L10N_UPGRADE_UPGRADE   ${LANG} "&Oppgrader ${APP_NAME} til ${APP_VERSION}."
LangString L10N_UPGRADE_INI       ${LANG} "Dine noverande innstillingar vert kopiert til ${APP_NAME}-old.ini."
LangString L10N_UPGRADE_INSTALL   ${LANG} "&Installer til ny plassering."
LangString L10N_UPGRADE_UNINSTALL ${LANG} "&Avinstaller ${APP_NAME}."
LangString L10N_UPDATE_SECTION    ${LANG} "Sjå etter oppdateringar"
LangString L10N_UPDATE_DIALOG     ${LANG} "Ein ny versjon er tilgjengeleg.$\nAvbryte installasjonen og gå til vevsida?"
LangString L10N_RUNNING           ${LANG} "${APP_NAME} køyrer. Lukke?"
LangString L10N_RUNNING_UNINSTALL ${LANG} "Om du vel Nei, vil ${APP_NAME} verte fullstendig fjerna ved neste omstart."
LangString L10N_SHORTCUT          ${LANG} "Startmeny-snarveg"
LangString L10N_AUTOSTART         ${LANG} "Autostart"
LangString L10N_AUTOSTART_HIDE    ${LANG} "Gøym ikon"

!undef LANG
