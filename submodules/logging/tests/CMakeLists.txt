cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE ON)


if(NOT TARGET GTest::gtest)
    find_package(GTest 1.17.0 REQUIRED)
endif()

set(TEST_SOURCES "")

if(LOGGER_FRAMEWORK_WITH_MOCK OR LOGGER_FRAMEWORK_IS_TOPLEVEL)
    message(STATUS "Configuring with Tests Logger<Mockup>")
    file(GLOB_RECURSE MOCK_TESTS
        "${CMAKE_CURRENT_LIST_DIR}/mock_logger/*.cpp"
    )
    list(APPEND TEST_SOURCES ${MOCK_TESTS})
endif()

if(LOGGER_FRAMEWORK_WITH_TERMINAL OR LOGGER_FRAMEWORK_IS_TOPLEVEL)
    message(STATUS "Configuring with Tests Logger<Terminal>")
    file(GLOB_RECURSE TERMINAL_TESTS
        "${CMAKE_CURRENT_LIST_DIR}/terminal_logger/*.cpp"
    )
    list(APPEND TEST_SOURCES ${TERMINAL_TESTS})
endif()

if(LOGGER_FRAMEWORK_WITH_BOOST OR LOGGER_FRAMEWORK_IS_TOPLEVEL)
    message(STATUS "Configuring with Tests Logger<Boost>")
    file(GLOB_RECURSE BOOST_TESTS
        "${CMAKE_CURRENT_LIST_DIR}/boost_logger/*.cpp"
    )
    list(APPEND TEST_SOURCES ${BOOST_TESTS})
endif()

if(NOT "${TEST_SOURCES}" STREQUAL "")
    message(STATUS "found test files:")
    foreach(test_file ${TEST_SOURCES})
        message(STATUS "  ${test_file}")
    endforeach()
else()
    message(FATAL_ERROR "no tests found !!!")
endif()

add_executable(logger_framework_tests
    ${TEST_SOURCES}
)

target_include_directories(logger_framework_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    # ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

enable_testing()
include(GoogleTest)
gtest_discover_tests(logger_framework_tests)

target_include_directories(logger_framework_tests PUBLIC
    ${ABS_FIXTURE_DIRS}
)

target_link_libraries(logger_framework_tests
    GTest::gtest
    GTest::gtest_main
    LoggerFramework::LoggerFramework
)

target_compile_definitions(logger_framework_tests PRIVATE LOGGER_EXPORTS)

#if(UNIX AND NOT APPLE)
#    target_link_options(logger_framework_tests PRIVATE "-Wl,--export-dynamic")
#endif()

project(TestDLL)

set(DLLSOURCES
    test_dll/TestDLL.cpp
)

add_library(${PROJECT_NAME} SHARED ${DLLSOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_dll
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    LoggerFramework::LoggerFramework
)

target_compile_definitions(${PROJECT_NAME} PRIVATE TESTDLL_API)
