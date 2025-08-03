cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

message(STATUS "#### StarCitizen-Launcher ####")
project(SC-Deutsch-Launcher)

set(SC_LAUNCHER_BINARY ${PROJECT_NAME})
set(SC_LAUNCHER_APPIMAGE "${PROJECT_NAME}-AppImage")
set(SC_LAUNCHER_FLATPAK "${PROJECT_NAME}-Flatpak")

set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/release" CACHE PATH "Install path prefix." FORCE)

find_package(Qt5 COMPONENTS Widgets REQUIRED)
find_package(libgit2 REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(CURL REQUIRED)

set(INCLUDES
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

qt_wrap_cpp(MOC_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/include/MainWindow.hpp)
qt_wrap_cpp(MOC_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/include/frontend/Update/Update.hpp)
qt_wrap_cpp(MOC_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/include/frontend/Settings/Settings.hpp)
qt_wrap_cpp(MOC_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/include/frontend/Settings/SettingsWindow.hpp)

set(SOURCES
  ${MOC_SRCS}
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/MainWindow.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/Settings/SettingsWindow.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/Settings/Settings.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.qrc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/logging/logger/custom_logger/custom_logger.cpp

)

#############################################################
# --- BINARY ---
#############################################################

add_executable(${SC_LAUNCHER_BINARY} ${SOURCES})

target_link_libraries(${SC_LAUNCHER_BINARY}
  Qt5::Widgets
  git2
  nlohmann_json
  CURL::libcurl
)

target_include_directories(${SC_LAUNCHER_BINARY} PRIVATE
  ${INCLUDES}
)

qt5_add_resources(RESOURCES resources/resources.qrc)
target_sources(${SC_LAUNCHER_BINARY} PRIVATE ${RESOURCES})

configure_file(
  "${CMAKE_SOURCE_DIR}/resources/logo.png"
  "${CMAKE_BINARY_DIR}/resources/logo.png"
  COPYONLY
)

configure_file(
  "${CMAKE_SOURCE_DIR}/resources/sc-deutsch.png"
  "${CMAKE_BINARY_DIR}/resources/sc-deutsch.png"
  COPYONLY
)

configure_file(
  "${CMAKE_SOURCE_DIR}/resources/background2_main.png"
  "${CMAKE_BINARY_DIR}/resources/background2_main.png"
  COPYONLY
)
configure_file(
  "${CMAKE_SOURCE_DIR}/resources/background2_settings.png"
  "${CMAKE_BINARY_DIR}/resources/background2_settings.png"
  COPYONLY
)

configure_file(
  "${CMAKE_SOURCE_DIR}/misc/settings.json"
  "${CMAKE_BINARY_DIR}/local/settings.json"
  COPYONLY
)

configure_file(
  "${CMAKE_SOURCE_DIR}/misc/starcitizen-launcher.desktop"
  "${CMAKE_BINARY_DIR}/starcitizen-launcher.desktop"
  COPYONLY
)

install(TARGETS ${SC_LAUNCHER_BINARY} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

install(FILES
  "${CMAKE_SOURCE_DIR}/resources/logo.png"
  "${CMAKE_SOURCE_DIR}/resources/sc-deutsch.png"
  "${CMAKE_SOURCE_DIR}/resources/background2_main.png"
  "${CMAKE_SOURCE_DIR}/resources/background2_settings.png"
  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/resources
)

install(FILES
  "${CMAKE_SOURCE_DIR}/misc/settings.json"
  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/local
)

install(FILES
  "${CMAKE_SOURCE_DIR}/misc/starcitizen-launcher.desktop"
  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
)
install(FILES
  "${CMAKE_SOURCE_DIR}/misc/create_desktop_file.sh"
  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
)

#############################################################
# --- APPIMAGE ---
#############################################################

add_executable(${SC_LAUNCHER_APPIMAGE} ${SOURCES})
target_link_libraries(${SC_LAUNCHER_APPIMAGE}
  Qt5::Widgets
  git2
  nlohmann_json
  CURL::libcurl
)

target_include_directories(${SC_LAUNCHER_APPIMAGE} PRIVATE
  ${INCLUDES}
)

qt5_add_resources(RESOURCES resources/resources.qrc)
target_sources(${SC_LAUNCHER_APPIMAGE} PRIVATE ${RESOURCES})

target_compile_definitions(${SC_LAUNCHER_APPIMAGE} PRIVATE APPIMAGE)

# AppDir Pfade
set(APPDIR ${CMAKE_BINARY_DIR}/${SC_LAUNCHER_APPIMAGE}.AppDir)
set(APPDIR_BIN ${APPDIR}/usr/bin)
set(APPDIR_ICON_DIR ${APPDIR}/usr/share/icons/hicolor/256x256/apps)
set(APP_DESKTOP ${APPDIR}/${SC_LAUNCHER_APPIMAGE}.desktop)
set(APP_ICON ${CMAKE_BINARY_DIR}/resources/logo.png)
set(APP_SETTINGS ${CMAKE_BINARY_DIR}/local/settings.json)

# Ressourcenliste
list(APPEND APP_RESOURCES
  ${CMAKE_BINARY_DIR}/resources/logo.png
  ${CMAKE_BINARY_DIR}/resources/sc-deutsch.png
  ${CMAKE_BINARY_DIR}/resources/background2_main.png
  ${CMAKE_BINARY_DIR}/resources/background2_settings.png
)

file(WRITE ${APP_DESKTOP}
  "[Desktop Entry]\n"
  "Name=${SC_LAUNCHER_APPIMAGE}\n"
  "Exec=${SC_LAUNCHER_APPIMAGE}\n"
  "Icon=${SC_LAUNCHER_APPIMAGE}\n"
  "Type=Application\n"
  "Categories=Utility;\n"
)

set(LINUXDEPLOY ${CMAKE_SOURCE_DIR}/misc/linuxdeploy-x86_64.appimage)
set(LINUXDEPLOY_QT_PLUGIN ${CMAKE_SOURCE_DIR}/misc/linuxdeploy-plugin-qt-x86_64.appimage)
set(APPIMAGETOOL ${CMAKE_SOURCE_DIR}/misc/appimagetool-x86_64.appimage)

add_custom_target(appimage_prepare
  DEPENDS ${SC_LAUNCHER_APPIMAGE}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${APPDIR_BIN}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${APPDIR}/resources
  COMMAND ${CMAKE_COMMAND} -E make_directory ${APPDIR}/local
  COMMAND ${CMAKE_COMMAND} -E make_directory ${APPDIR_ICON_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${SC_LAUNCHER_APPIMAGE}> ${APPDIR_BIN}/
  COMMAND ${CMAKE_COMMAND} -E create_symlink usr/bin/${SC_LAUNCHER_APPIMAGE} ${APPDIR}/AppRun
  COMMAND ${CMAKE_COMMAND} -E copy ${APP_SETTINGS} ${APPDIR}/local/settings.json
  COMMAND ${CMAKE_COMMAND} -E copy ${APP_ICON} ${APPDIR_ICON_DIR}/${SC_LAUNCHER_APPIMAGE}.png
  COMMAND ${CMAKE_COMMAND} -E copy ${APP_ICON} ${APPDIR}/${SC_LAUNCHER_APPIMAGE}.png
  COMMAND ${CMAKE_COMMAND} -E copy ${APP_RESOURCES} ${APPDIR}/resources/
  COMMENT "Prepare AppDir for AppImage"
)

add_custom_target(appimage ALL
  DEPENDS appimage_prepare ${SC_LAUNCHER_APPIMAGE}
  COMMAND chmod +x ${APPIMAGETOOL}
  COMMAND ${CMAKE_COMMAND} -E env ARCH=x86_64 DISABLE_STRIP=1 ${APPIMAGETOOL} ${APPDIR}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Create AppImage"
)

install(FILES
  "${CMAKE_BINARY_DIR}/${SC_LAUNCHER_APPIMAGE}-x86_64.AppImage"
  DESTINATION ${CMAKE_INSTALL_PREFIX}/appimage
)
install(CODE "
  file(RENAME
    \"${CMAKE_INSTALL_PREFIX}/appimage/${SC_LAUNCHER_APPIMAGE}-x86_64.AppImage\"
    \"${CMAKE_INSTALL_PREFIX}/appimage/${PROJECT_NAME}.AppImage\"
  )
")

install(FILES
  "${CMAKE_SOURCE_DIR}/misc/AppImageIntegration.sh"
  DESTINATION ${CMAKE_INSTALL_PREFIX}/appimage
)
