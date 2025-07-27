# --- AppImage-Erstellung ---

# Pfade
set(APPDIR ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.AppDir)
set(APPDIR_BIN ${APPDIR}/usr/bin)
set(APPDIR_ICON_DIR ${APPDIR}/usr/share/icons/hicolor/256x256/apps)

# Desktop-Datei-Pfad
set(APP_DESKTOP ${APPDIR}/${PROJECT_NAME}.desktop)

# Icon (nutze deine resources/logo.png als App-Icon)
set(APP_ICON ${CMAKE_BINARY_DIR}/resources/logo.png)
set(APP_SETTINGS ${CMAKE_BINARY_DIR}/local/settings.json)
set(APP_RESOURCES ${CMAKE_BINARY_DIR}/resources/logo.png)

# .desktop-Datei schreiben (direkt beim Konfigurieren)
file(WRITE ${APP_DESKTOP}
    "[Desktop Entry]
Name=${PROJECT_NAME}
Exec=${PROJECT_NAME}
Icon=${PROJECT_NAME}
Type=Application
Categories=Utility;
")

set(LINUXDEPLOY ${CMAKE_SOURCE_DIR}/misc/linuxdeploy-x86_64.AppImage)
set(LINUXDEPLOY_QT_PLUGIN ${CMAKE_SOURCE_DIR}/misc/linuxdeploy-plugin-qt-x86_64.AppImage)


# Verzeichnisstruktur vorbereiten und Dateien kopieren
add_custom_target(appimage_prepare
    DEPENDS ${PROJECT_NAME}
    COMMAND ${CMAKE_COMMAND} -E copy ${APP_ICON} ${APPDIR}/StarCitizen-Launcher.png
    COMMAND ${CMAKE_COMMAND} -E copy ${APP_SETTINGS} ${APPDIR}/local/settings.json
    COMMAND ${CMAKE_COMMAND} -E copy ${APP_RESOURCES} ${APPDIR}/resources/logo.png
    COMMAND ${CMAKE_COMMAND} -E make_directory ${APPDIR_ICON_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${APPDIR_BIN}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${APPDIR_BIN}/
    COMMAND ${CMAKE_COMMAND} -E copy ${APP_ICON} ${APPDIR_ICON_DIR}/${PROJECT_NAME}.png
    COMMENT "Prepare AppDir structure for AppImage"
)

# Pfad zu deinem appimagetool (bitte vorher manuell herunterladen und hier anpassen)
set(APPIMAGETOOL ${CMAKE_SOURCE_DIR}/misc/appimagetool-x86_64.appimage)

# AppImage bauen
add_custom_target(appimage ALL
    DEPENDS appimage_prepare ${PROJECT_NAME}
    COMMAND ${CMAKE_COMMAND} -E env ARCH=x86_64 ${APPIMAGETOOL} ${APPDIR}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Create AppImage"
)
