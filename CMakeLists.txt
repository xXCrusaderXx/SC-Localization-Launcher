cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 17)
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


file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/LIVE")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/LIVE/data")
file(WRITE "${CMAKE_BINARY_DIR}/LIVE/user.cfg" "")
file(WRITE "${CMAKE_BINARY_DIR}/LIVE/data/global.ini" "")

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/PTU")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/PTU/data")

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/EPTU")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/EPTU/data")

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/HOTFIX")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/HOTFIX/data")

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/TECH-PREVIEW")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/TECH-PREVIEW/data")

configure_file(
  "${CMAKE_SOURCE_DIR}/resources/logo.png"
  "${CMAKE_BINARY_DIR}/resources/logo.png"
  COPYONLY
)

configure_file(
  "${CMAKE_SOURCE_DIR}/misc/settings.json"
  "${CMAKE_BINARY_DIR}/local/settings.json"
  COPYONLY
)

#add_subdirectory(AppImageDeploy)
