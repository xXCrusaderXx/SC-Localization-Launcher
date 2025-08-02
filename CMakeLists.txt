cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

message(STATUS "#### StarCitizen-Launcher ####")
project(StarCitizen-Launcher)

find_package(Qt5 COMPONENTS Widgets REQUIRED)
find_package(libgit2 REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(CURL REQUIRED)

set(INCLUDES
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

qt_wrap_cpp(MOC_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/include/MainWindow.hpp)
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

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME}
  Qt5::Widgets
  git2
  nlohmann_json
  CURL::libcurl
)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${INCLUDES}
)

qt5_add_resources(RESOURCES resources/resources.qrc)
target_sources(${PROJECT_NAME} PRIVATE ${RESOURCES})

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
  "${CMAKE_SOURCE_DIR}/resources/background2.png"
  "${CMAKE_BINARY_DIR}/resources/background2.png"
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

#add_subdirectory(AppImageDeploy)
