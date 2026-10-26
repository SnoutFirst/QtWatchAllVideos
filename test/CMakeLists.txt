cmake_minimum_required(VERSION 3.29...3.31)

project(CmakeConfigPackageTests
  VERSION ${PROJECT_VERSION}
  LANGUAGES CXX)

# ---- Test as standalone project the exported config package ----

if(PROJECT_IS_TOP_LEVEL OR TEST_INSTALLED_VERSION)
  enable_testing()

  find_package(QtWatchAllVideos2 CONFIG REQUIRED) # for options/warnings targets

  if(NOT TARGET QtWatchAllVideos2_options)
    message(FATAL_ERROR "Required config package not found!")
  endif()
endif()

# ---- Tests ----

# CMake-script regression tests for cmake/Sanitizers.cmake (issue #141).
add_test(
  NAME cmake.sanitizers.apple_excludes_leak
  COMMAND ${CMAKE_COMMAND} -P
          ${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_sanitizers_apple_excludes_leak.cmake)
add_test(
  NAME cmake.sanitizers.non_apple_includes_leak
  COMMAND ${CMAKE_COMMAND} -P
          ${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_sanitizers_non_apple_includes_leak.cmake)
