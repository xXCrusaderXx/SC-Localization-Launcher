# Run with: cmake -B build -DSC_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build --output-on-failure
# The tests need no internet: channels.json and the translation come from a local server (TestSupport.hpp).

find_package(Qt6 REQUIRED COMPONENTS Test Network)

# Backend headers, logger and test helpers for both tests
add_library(sc_test_support INTERFACE)
target_sources(sc_test_support INTERFACE ${CMAKE_SOURCE_DIR}/src/logging/logger/custom_logger/custom_logger.cpp)
target_include_directories(sc_test_support INTERFACE ${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/generated ${CMAKE_CURRENT_SOURCE_DIR})
# Enables the SC_LANGUAGES_URL override and plain HTTP to 127.0.0.1, which the app itself never has
target_compile_definitions(sc_test_support INTERFACE SC_TESTING)
target_link_libraries(sc_test_support INTERFACE Qt6::Test Qt6::Network nlohmann_json::nlohmann_json CURL::libcurl)

add_executable(BackendTest BackendTest.cpp)
target_link_libraries(BackendTest PRIVATE sc_test_support)

add_executable(DirectoryFinderTest DirectoryFinderTest.cpp)
target_link_libraries(DirectoryFinderTest PRIVATE sc_test_support)

add_executable(SingleInstanceTest SingleInstanceTest.cpp ${CMAKE_SOURCE_DIR}/include/frontend/SingleInstance.hpp)
target_link_libraries(SingleInstanceTest PRIVATE sc_test_support)

add_executable(GuiTest
  GuiTest.cpp
  ${CMAKE_SOURCE_DIR}/include/frontend/BackendBridge.hpp
  ${CMAKE_SOURCE_DIR}/resources/resources.qrc
)
target_link_libraries(GuiTest PRIVATE sc_test_support Qt6::Quick)

# The same QML module as the app. Source file properties are per directory, so they are set again here.
list(TRANSFORM SC_QML_FILES PREPEND ${CMAKE_SOURCE_DIR}/ OUTPUT_VARIABLE SC_TEST_QML_FILES)
foreach(qml_file ${SC_TEST_QML_FILES})
  get_filename_component(qml_name ${qml_file} NAME)
  set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_name})
endforeach()
set_source_files_properties(${CMAKE_SOURCE_DIR}/qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(GuiTest
  URI SCLauncher
  VERSION 1.0
  RESOURCE_PREFIX /qt/qml
  QML_FILES ${SC_TEST_QML_FILES}
)

foreach(test BackendTest DirectoryFinderTest SingleInstanceTest GuiTest)
  add_test(NAME ${test} COMMAND ${test})
  # Own HOME and XDG dirs, so the user's settings are never touched. Empty PATH, so the game can never be started.
  set(test_home ${CMAKE_CURRENT_BINARY_DIR}/home-${test})
  set_tests_properties(${test} PROPERTIES
    TIMEOUT 120
    ENVIRONMENT "SC_TEST_HOME=${test_home};HOME=${test_home};XDG_CONFIG_HOME=${test_home}/.config;XDG_CACHE_HOME=${test_home}/.cache;XDG_STATE_HOME=${test_home}/.local/state;PATH=/nonexistent;QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1;LANGUAGE=en;LC_ALL=C.UTF-8"
  )
endforeach()
