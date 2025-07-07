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

set(INCLUDES
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

qt_wrap_cpp(MOC_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/include/MainWindow.hpp)
set(SOURCES
  ${MOC_SRCS}
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.qrc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/GitRepoHandler/GitRepoHandler.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME}
  Qt5::Widgets
  git2
)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${INCLUDES}
)


file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/LIVE")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/LIVE/data")
file(WRITE "${CMAKE_BINARY_DIR}/LIVE/user.cfg" "")
file(WRITE "${CMAKE_BINARY_DIR}/LIVE/data/global.ini" "")

configure_file(
  "${CMAKE_SOURCE_DIR}/resources/logo.png"
  "${CMAKE_BINARY_DIR}/resources/logo.png"
  COPYONLY
)
