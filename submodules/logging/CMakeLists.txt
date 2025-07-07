cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(LOGGER_FRAMEWORK_ENABLE_TESTS "Enable tests for FMX Logger" OFF)
option(LOGGER_FRAMEWORK_WITH_MOCK "Enable Mock Logger" ON)
option(LOGGER_FRAMEWORK_WITH_TERMINAL "Enable Terminal Logger" ON)
option(LOGGER_FRAMEWORK_WITH_BOOST "Enable Boost Logger" OFF)

set(LOGGER_FRAMEWORK_IS_TOPLEVEL OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_LIST_DIR)
    set(LOGGER_FRAMEWORK_IS_TOPLEVEL ON)
endif()

message(STATUS "LOGGER_FRAMEWORK_IS_TOPLEVEL = ${LOGGER_FRAMEWORK_IS_TOPLEVEL}")

#------------------------------------------------------------------------------
if(LOGGER_FRAMEWORK_IS_TOPLEVEL)
    set(LOGGER_FRAMEWORK_ENABLE_TESTS ON CACHE BOOL "Build unit tests" FORCE)
endif()

message(STATUS "LOGGER_FRAMEWORK_ENABLE_TESTS = ${LOGGER_FRAMEWORK_ENABLE_TESTS}")

project(LoggerFramework)

set(SOURCES "")
set(LoggerFramework_EXTERNAL_LIBS "")

if(LOGGER_FRAMEWORK_WITH_MOCK OR LOGGER_FRAMEWORK_IS_TOPLEVEL)
    message(STATUS "Configuring with Logger<Mockup>")
    list(APPEND SOURCES
        src/logger/mock_logger/mock_logger.cpp
    )
else()
    message(STATUS "Configuring without Logger<Mockup>")
endif()


if(LOGGER_FRAMEWORK_WITH_TERMINAL OR LOGGER_FRAMEWORK_IS_TOPLEVEL)
    message(STATUS "Configuring with Logger<Terminal>")
    list(APPEND SOURCES
        src/logger/terminal_logger/terminal_logger.cpp
    )
else()
    message(STATUS "Configuring without Logger<Terminal>")
endif()

if(LOGGER_FRAMEWORK_WITH_BOOST OR LOGGER_FRAMEWORK_IS_TOPLEVEL)
    message(STATUS "Configuring with Logger<Boost>")

    if(NOT Boost_FOUND AND NOT TARGET Boost::log)
        message(STATUS "Finding Boost libraries...")
        find_package(Boost QUIET REQUIRED COMPONENTS log log_setup date_time thread filesystem system)
    endif()

    list(APPEND SOURCES src/logger/boost_logger/boost_logger.cpp)
    list(APPEND LoggerFramework_EXTERNAL_LIBS
        Boost::log
        Boost::log_setup
        Boost::headers
        Boost::property_tree
    )
else()
    message(STATUS "Configuring without Logger<Boost>")
endif()



add_library(${PROJECT_NAME} STATIC
    ${SOURCES}
)

add_library(LoggerFramework::LoggerFramework ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    ${LoggerFramework_EXTERNAL_LIBS}
)

target_compile_definitions(${PROJECT_NAME} PUBLIC LOGGER_EXPORTS)

if(UNIX AND NOT APPLE)
    target_link_options(${PROJECT_NAME} PRIVATE "-Wl,--export-dynamic")
endif()

#------------------------------------------------------------------------------
if(LOGGER_FRAMEWORK_ENABLE_TESTS)
    if(NOT TARGET GTest::gtest)
        message(STATUS "Finding googletest...")
        find_package(googletest QUIET REQUIRED)
    endif()
    add_subdirectory(tests)
endif()
