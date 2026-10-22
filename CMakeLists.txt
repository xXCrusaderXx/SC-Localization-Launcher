cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

message(STATUS "#### StarCitizen-Launcher ####")
project(SC-Deutsch-Launcher LANGUAGES CXX)

set(SC_LAUNCHER_BINARY ${PROJECT_NAME})
set(SC_LAUNCHER_APPIMAGE "${PROJECT_NAME}-AppImage")

option(SC_BUILD_APPIMAGE "Add the 'appimage' target to the default build" OFF)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/release" CACHE PATH "Install path prefix." FORCE)
endif()

# Prefer Qt6, fall back to Qt5
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} 5.15 REQUIRED COMPONENTS Widgets)
message(STATUS "Using Qt ${QT_VERSION}")

find_package(nlohmann_json REQUIRED)
find_package(CURL REQUIRED)

set(SOURCES
  # Headers with Q_OBJECT, listed so AUTOMOC finds them outside of src/
  ${CMAKE_CURRENT_SOURCE_DIR}/include/MainWindow.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/frontend/Update/Update.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/frontend/Settings/Settings.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/frontend/Settings/SettingsWindow.hpp

  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/MainWindow.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/Settings/SettingsWindow.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/Settings/Settings.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/logging/logger/custom_logger/custom_logger.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.qrc
)

#############################################################
# --- BINARY ---
#############################################################

add_executable(${SC_LAUNCHER_BINARY} ${SOURCES})

target_link_libraries(${SC_LAUNCHER_BINARY}
  Qt::Widgets
  nlohmann_json::nlohmann_json
  CURL::libcurl
)

target_include_directories(${SC_LAUNCHER_BINARY} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(RUNTIME_RESOURCES
  ${CMAKE_SOURCE_DIR}/resources/logo.png
  ${CMAKE_SOURCE_DIR}/resources/sc-deutsch.png
  ${CMAKE_SOURCE_DIR}/resources/background2_main.png
  ${CMAKE_SOURCE_DIR}/resources/background2_settings.png
)

foreach(resource ${RUNTIME_RESOURCES})
  get_filename_component(resource_name ${resource} NAME)
  configure_file(${resource} "${CMAKE_BINARY_DIR}/resources/${resource_name}" COPYONLY)
endforeach()

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

install(TARGETS ${SC_LAUNCHER_BINARY} DESTINATION bin)
install(FILES ${RUNTIME_RESOURCES} DESTINATION bin/resources)
install(FILES "${CMAKE_SOURCE_DIR}/misc/settings.json" DESTINATION bin/local)
install(FILES "${CMAKE_SOURCE_DIR}/misc/starcitizen-launcher.desktop" DESTINATION bin)

#############################################################
# --- APPIMAGE ---
#############################################################
# Packs the same binary; at runtime paths.hpp detects the AppImage via $APPDIR.
# appimagetool is downloaded into the build dir on first use (not tracked in git).

set(APPDIR ${CMAKE_BINARY_DIR}/${SC_LAUNCHER_APPIMAGE}.AppDir)
set(APPDIR_BIN ${APPDIR}/usr/bin)
set(APPDIR_ICON_DIR ${APPDIR}/usr/share/icons/hicolor/256x256/apps)
set(APP_DESKTOP ${APPDIR}/${SC_LAUNCHER_APPIMAGE}.desktop)
set(APP_ICON ${CMAKE_SOURCE_DIR}/resources/logo.png)
set(APPIMAGE_FILE ${CMAKE_BINARY_DIR}/${SC_LAUNCHER_APPIMAGE}-x86_64.AppImage)

set(APPIMAGETOOL_URL "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"
    CACHE STRING "Download URL for appimagetool")
set(APPIMAGETOOL ${CMAKE_BINARY_DIR}/tools/appimagetool-x86_64.AppImage)

file(WRITE ${APP_DESKTOP}
  "[Desktop Entry]\n"
  "Name=${SC_LAUNCHER_APPIMAGE}\n"
  "Exec=${SC_LAUNCHER_BINARY}\n"
  "Icon=${SC_LAUNCHER_APPIMAGE}\n"
  "Type=Application\n"
  "Categories=Utility;\n"
)

add_custom_command(
  OUTPUT ${APPIMAGETOOL}
  COMMAND ${CMAKE_COMMAND} -DURL=${APPIMAGETOOL_URL} -DDEST=${APPIMAGETOOL} -P ${CMAKE_SOURCE_DIR}/misc/download.cmake
  COMMENT "Download appimagetool"
)

add_custom_target(appimage_prepare
  DEPENDS ${SC_LAUNCHER_BINARY}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${APPDIR_BIN} ${APPDIR}/resources ${APPDIR}/local ${APPDIR_ICON_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${SC_LAUNCHER_BINARY}> ${APPDIR_BIN}/
  COMMAND ${CMAKE_COMMAND} -E create_symlink usr/bin/${SC_LAUNCHER_BINARY} ${APPDIR}/AppRun
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/misc/settings.json ${APPDIR}/local/settings.json
  COMMAND ${CMAKE_COMMAND} -E copy ${APP_ICON} ${APPDIR_ICON_DIR}/${SC_LAUNCHER_APPIMAGE}.png
  COMMAND ${CMAKE_COMMAND} -E copy ${APP_ICON} ${APPDIR}/${SC_LAUNCHER_APPIMAGE}.png
  COMMAND ${CMAKE_COMMAND} -E copy ${RUNTIME_RESOURCES} ${APPDIR}/resources/
  COMMENT "Prepare AppDir for AppImage"
)

if(SC_BUILD_APPIMAGE)
  set(APPIMAGE_ALL ALL)
endif()

add_custom_target(appimage ${APPIMAGE_ALL}
  DEPENDS appimage_prepare ${APPIMAGETOOL}
  COMMAND ${CMAKE_COMMAND} -E env ARCH=x86_64 DISABLE_STRIP=1 ${APPIMAGETOOL} ${APPDIR} ${APPIMAGE_FILE}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Create AppImage"
)

# The AppImage is only installed when it was built (cmake --build . --target appimage)
install(FILES ${APPIMAGE_FILE} DESTINATION appimage RENAME ${PROJECT_NAME}.AppImage OPTIONAL)
install(FILES
  "${CMAKE_SOURCE_DIR}/misc/AppImageIntegration.sh"
  "${CMAKE_SOURCE_DIR}/misc/install.sh"
  "${CMAKE_SOURCE_DIR}/misc/uninstall.sh"
  "${CMAKE_SOURCE_DIR}/resources/logo.png"
  DESTINATION appimage
)
