cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

message(STATUS "#### StarCitizen-Launcher ####")

# Version of a normal build. Release workflows read this line; keep it a plain number.
set(SC_DEFAULT_VERSION 0.1.0)
# CI sets the release version (e.g. 0.6.0 or 0.6.0-rc1), so no version bump commit is needed
set(SC_VERSION_OVERRIDE "" CACHE STRING "Release version set by CI")
if(SC_VERSION_OVERRIDE)
  set(SC_FULL_VERSION "${SC_VERSION_OVERRIDE}")
else()
  set(SC_FULL_VERSION "${SC_DEFAULT_VERSION}")
endif()
# project() only accepts numbers; a suffix like -rc1 stays in SC_FULL_VERSION
string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" SC_NUMERIC_VERSION "${SC_FULL_VERSION}")
if(NOT SC_NUMERIC_VERSION)
  message(FATAL_ERROR "Invalid version '${SC_FULL_VERSION}', expected X.Y.Z or X.Y.Z-suffix")
endif()

project(SC-Localization-Launcher VERSION ${SC_NUMERIC_VERSION} LANGUAGES CXX)

# Lowercase like other Linux programs; also the package name
set(SC_LAUNCHER_BINARY sc-localization-launcher)
set(SC_APP_ID "io.github.xXCrusaderXx.SCLocalizationLauncher")

option(SC_BUILD_APPIMAGE "Add the 'appimage' target to the default build" OFF)
# ON:  FHS layout for distro packages and Flatpak (bin/, share/applications, share/icons, share/metainfo)
# OFF: portable layout in release/ (bin/ with local/settings.json next to the binary)
option(SC_SYSTEM_INSTALL "Install with the FHS layout used by distro packages and Flatpak" OFF)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT AND NOT SC_SYSTEM_INSTALL)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/release" CACHE PATH "Install path prefix." FORCE)
endif()
include(GNUInstallDirs)

# 6.4 is the oldest Qt shipped by current LTS distributions (Ubuntu 24.04, Debian 12)
find_package(Qt6 6.4 REQUIRED COMPONENTS Quick Network)
message(STATUS "Using Qt ${Qt6_VERSION}")
if(QT_KNOWN_POLICY_QTP0004)
  qt_policy(SET QTP0004 NEW)
endif()

# 3.11 brings NLOHMANN_DEFINE_TYPE_*_WITH_DEFAULT; older distros (Ubuntu 22.04) get it downloaded
# Version is checked by hand: some setups only allow find_package() without a version request
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND OR nlohmann_json_VERSION VERSION_LESS 3.11)
  message(STATUS "nlohmann_json >= 3.11 not found, downloading it")
  include(FetchContent)
  FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
  )
  FetchContent_MakeAvailable(nlohmann_json)
endif()
find_package(CURL REQUIRED)

# languages.json is downloaded at runtime; this built-in copy is used on a first start without network
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/languages.json)
file(READ ${CMAKE_SOURCE_DIR}/languages.json SC_BUNDLED_LANGUAGES)
configure_file(${CMAKE_SOURCE_DIR}/include/backend/BundledLanguages.hpp.in ${CMAKE_BINARY_DIR}/generated/BundledLanguages.hpp @ONLY)

#############################################################
# --- BINARY ---
#############################################################

add_executable(${SC_LAUNCHER_BINARY}
  ${CMAKE_CURRENT_SOURCE_DIR}/include/frontend/BackendBridge.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/frontend/SingleInstance.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/logging/logger/custom_logger/custom_logger.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.qrc
)

set(SC_QML_FILES
  qml/Main.qml
  qml/ChannelRow.qml
  qml/LanguagePicker.qml
  qml/LauncherUpdateDialog.qml
  qml/IconButton.qml
  qml/PathRow.qml
  qml/SettingsPanel.qml
  qml/Theme.qml
  qml/Toggle.qml
)
# Drop the qml/ folder from the resource path, so Main.qml is at qrc:/qt/qml/SCLauncher/Main.qml
foreach(qml_file ${SC_QML_FILES})
  get_filename_component(qml_name ${qml_file} NAME)
  set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_name})
endforeach()
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(${SC_LAUNCHER_BINARY}
  URI SCLauncher
  VERSION 1.0
  # Fixed prefix, so main.cpp finds Main.qml on every Qt version
  RESOURCE_PREFIX /qt/qml
  QML_FILES ${SC_QML_FILES}
)

# UI translations, picked by the system language. Without Qt Linguist the UI is English only.
find_package(Qt6 QUIET COMPONENTS LinguistTools)
if(Qt6LinguistTools_FOUND)
  qt_add_translations(${SC_LAUNCHER_BINARY}
    # _en only translates the English plural forms of %n texts; everything else stays unfinished and uses the source text
    TS_FILES translations/sc-localization-launcher_de.ts translations/sc-localization-launcher_en.ts
    RESOURCE_PREFIX /i18n
    # No line numbers in the .ts files, otherwise every QML change touches them and PRs conflict
    LUPDATE_OPTIONS -locations none
  )
else()
  message(STATUS "Qt LinguistTools not found, the UI will be English only")
endif()

target_link_libraries(${SC_LAUNCHER_BINARY} PRIVATE
  Qt6::Quick
  Qt6::Network
  nlohmann_json::nlohmann_json
  CURL::libcurl
)

target_include_directories(${SC_LAUNCHER_BINARY} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_BINARY_DIR}/generated
)

target_compile_definitions(${SC_LAUNCHER_BINARY} PRIVATE
  SC_APP_ID="${SC_APP_ID}"
  SC_VERSION="${SC_FULL_VERSION}"
)

configure_file(
  "${CMAKE_SOURCE_DIR}/misc/settings.json"
  "${CMAKE_BINARY_DIR}/local/settings.json"
  COPYONLY
)

option(SC_BUILD_TESTS "Build the backend and GUI tests (run with ctest)" OFF)
if(SC_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

set(SC_DESKTOP_FILE "${CMAKE_SOURCE_DIR}/packaging/${SC_APP_ID}.desktop")
set(SC_ICON_FILE "${CMAKE_SOURCE_DIR}/resources/icon.png")
set(SC_METAINFO_FILE "${CMAKE_SOURCE_DIR}/packaging/${SC_APP_ID}.metainfo.xml")

#############################################################
# --- INSTALL ---
#############################################################

if(SC_SYSTEM_INSTALL)
  install(TARGETS ${SC_LAUNCHER_BINARY} DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(FILES ${SC_DESKTOP_FILE} DESTINATION ${CMAKE_INSTALL_DATADIR}/applications RENAME ${SC_APP_ID}.desktop)
  install(FILES ${SC_ICON_FILE} DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps RENAME ${SC_APP_ID}.png)
  install(FILES ${SC_METAINFO_FILE} DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
  if(EXISTS "${CMAKE_SOURCE_DIR}/LICENSE")
    install(FILES "${CMAKE_SOURCE_DIR}/LICENSE" DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/${SC_LAUNCHER_BINARY})
  endif()
else()
  install(TARGETS ${SC_LAUNCHER_BINARY} DESTINATION bin)
  install(FILES "${CMAKE_SOURCE_DIR}/misc/settings.json" DESTINATION bin/local)
  install(FILES ${SC_DESKTOP_FILE} DESTINATION bin)
endif()

#############################################################
# --- APPIMAGE ---
#############################################################
# linuxdeploy copies the binary and all libraries it needs, including Qt and its QML modules,
# so the AppImage runs without Qt installed. For broad glibc compatibility build it on an old
# distro (the CI uses Ubuntu 22.04). At runtime paths.hpp finds the AppDir via $APPDIR.

set(APPIMAGE_FILE ${CMAKE_BINARY_DIR}/${PROJECT_NAME}-x86_64.AppImage)

# linuxdeploy-plugin-qt needs qmake; distro builds without it simply get no appimage target
if(NOT TARGET Qt6::qmake)
  message(STATUS "qmake not found, the appimage target is not available")
else()
  set(APPDIR ${CMAKE_BINARY_DIR}/AppDir)
  set(TOOLS_DIR ${CMAKE_BINARY_DIR}/tools)
  set(LINUXDEPLOY ${TOOLS_DIR}/linuxdeploy-x86_64.AppImage)
  set(LINUXDEPLOY_QT ${TOOLS_DIR}/linuxdeploy-plugin-qt-x86_64.AppImage)

  set(LINUXDEPLOY_URL "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
      CACHE STRING "Download URL for linuxdeploy")
  set(LINUXDEPLOY_QT_URL "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
      CACHE STRING "Download URL for linuxdeploy-plugin-qt")

  add_custom_command(
    OUTPUT ${LINUXDEPLOY}
    COMMAND ${CMAKE_COMMAND} -DURL=${LINUXDEPLOY_URL} -DDEST=${LINUXDEPLOY} -P ${CMAKE_SOURCE_DIR}/misc/download.cmake
    COMMENT "Download linuxdeploy"
  )
  add_custom_command(
    OUTPUT ${LINUXDEPLOY_QT}
    COMMAND ${CMAKE_COMMAND} -DURL=${LINUXDEPLOY_QT_URL} -DDEST=${LINUXDEPLOY_QT} -P ${CMAKE_SOURCE_DIR}/misc/download.cmake
    COMMENT "Download linuxdeploy-plugin-qt"
  )

  if(SC_BUILD_APPIMAGE)
    set(APPIMAGE_ALL ALL)
  endif()

  # The Wayland platform plugin is libqwayland.so since Qt 6.10, libqwayland-{generic,egl}.so before
  get_target_property(SC_QMAKE Qt6::qmake IMPORTED_LOCATION)
  execute_process(COMMAND ${SC_QMAKE} -query QT_INSTALL_PLUGINS OUTPUT_VARIABLE SC_QT_PLUGIN_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
  file(GLOB SC_WAYLAND_PLUGINS RELATIVE ${SC_QT_PLUGIN_DIR}/platforms ${SC_QT_PLUGIN_DIR}/platforms/libqwayland*.so)
  string(JOIN "$<SEMICOLON>" SC_WAYLAND_PLUGINS ${SC_WAYLAND_PLUGINS})

  # Qt 6 links the GLVND dispatcher libOpenGL.so.0, which linuxdeploy treats as a system library.
  # Minimal installs often lack it; it only dispatches to the system's GL driver, so bundling it is safe.
  find_library(SC_LIBOPENGL NAMES libOpenGL.so.0 OpenGL)
  if(SC_LIBOPENGL)
    set(SC_LINUXDEPLOY_EXTRA_LIBS --library ${SC_LIBOPENGL})
  endif()

  add_custom_target(appimage ${APPIMAGE_ALL}
    DEPENDS ${SC_LAUNCHER_BINARY} ${LINUXDEPLOY} ${LINUXDEPLOY_QT}
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${APPDIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${APPDIR}/local
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/misc/settings.json ${APPDIR}/local/settings.json
    # linuxdeploy needs the icon file named like the desktop file's Icon= entry
    COMMAND ${CMAKE_COMMAND} -E copy ${SC_ICON_FILE} ${TOOLS_DIR}/${SC_APP_ID}.png
    COMMAND ${CMAKE_COMMAND} -E copy ${SC_DESKTOP_FILE} ${TOOLS_DIR}/${SC_APP_ID}.desktop
    COMMAND ${CMAKE_COMMAND} -E env
              ARCH=x86_64
              APPIMAGE_EXTRACT_AND_RUN=1
              QMAKE=$<TARGET_FILE:Qt6::qmake>
              QML_SOURCES_PATHS=${CMAKE_SOURCE_DIR}/qml
              EXTRA_PLATFORM_PLUGINS=${SC_WAYLAND_PLUGINS}
              EXTRA_QT_MODULES=waylandclient
              LDAI_OUTPUT=${APPIMAGE_FILE}
            ${LINUXDEPLOY}
              --appdir ${APPDIR}
              --executable $<TARGET_FILE:${SC_LAUNCHER_BINARY}>
              --desktop-file ${TOOLS_DIR}/${SC_APP_ID}.desktop
              --icon-file ${TOOLS_DIR}/${SC_APP_ID}.png
              ${SC_LINUXDEPLOY_EXTRA_LIBS}
              --plugin qt
              --output appimage
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Create AppImage"
    VERBATIM
  )
endif()

if(NOT SC_SYSTEM_INSTALL)
  # The AppImage is only installed when it was built (cmake --build . --target appimage)
  install(FILES ${APPIMAGE_FILE} DESTINATION appimage RENAME ${PROJECT_NAME}.AppImage OPTIONAL)
  install(FILES
    "${CMAKE_SOURCE_DIR}/misc/AppImageIntegration.sh"
    "${CMAKE_SOURCE_DIR}/misc/install.sh"
    "${CMAKE_SOURCE_DIR}/misc/uninstall.sh"
    "${CMAKE_SOURCE_DIR}/resources/logo.png"
    DESTINATION appimage
  )
endif()

#############################################################
# --- DEB / RPM (cpack) ---
#############################################################
# cmake -B build -DSC_SYSTEM_INSTALL=ON -DCMAKE_INSTALL_PREFIX=/usr && cmake --build build
# cd build && cpack -G DEB   (or -G RPM)

set(CPACK_PACKAGE_NAME "${SC_LAUNCHER_BINARY}")
set(CPACK_PACKAGE_VENDOR "Marko Klein")
set(CPACK_PACKAGE_CONTACT "Marko Klein <55139200+xXCrusaderXx@users.noreply.github.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Installs community translations for Star Citizen on Linux")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/xXCrusaderXx/SC-Localization-Launcher")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_STRIP_FILES ON)

set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION "games")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# QML modules are loaded at runtime, shlibdeps cannot see them
set(CPACK_DEBIAN_PACKAGE_DEPENDS
  "qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-dialogs, qml6-module-qtquick-layouts, qml6-module-qtquick-templates, qml6-module-qtquick-window, qml6-module-qtqml-workerscript, qt6-wayland")

set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
set(CPACK_RPM_PACKAGE_GROUP "Amusements/Games")
set(CPACK_RPM_PACKAGE_AUTOREQ ON)
set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtdeclarative, qt6-qtwayland")

include(CPack)
