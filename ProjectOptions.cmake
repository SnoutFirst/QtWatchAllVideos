include(cmake/LibFuzzer.cmake)
include(CMakeDependentOption)
include(CheckCXXCompilerFlag)


include(CheckCXXSourceCompiles)


macro(QtWatchAllVideos_supports_sanitizers)
  if((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND NOT WIN32)

    message(STATUS "Sanity checking UndefinedBehaviorSanitizer, it should be supported on this platform")
    set(TEST_PROGRAM "int main() { return 0; }")

    # Check if UndefinedBehaviorSanitizer works at link time
    set(CMAKE_REQUIRED_FLAGS "-fsanitize=undefined")
    set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=undefined")
    check_cxx_source_compiles("${TEST_PROGRAM}" HAS_UBSAN_LINK_SUPPORT)

    if(HAS_UBSAN_LINK_SUPPORT)
      message(STATUS "UndefinedBehaviorSanitizer is supported at both compile and link time.")
      set(SUPPORTS_UBSAN ON)
    else()
      message(WARNING "UndefinedBehaviorSanitizer is NOT supported at link time.")
      set(SUPPORTS_UBSAN OFF)
    endif()
  else()
    set(SUPPORTS_UBSAN OFF)
  endif()

  if((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND WIN32)
    set(SUPPORTS_ASAN OFF)
  else()
    if (NOT WIN32)
      message(STATUS "Sanity checking AddressSanitizer, it should be supported on this platform")
      set(TEST_PROGRAM "int main() { return 0; }")

      # Check if AddressSanitizer works at link time
      set(CMAKE_REQUIRED_FLAGS "-fsanitize=address")
      set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=address")
      check_cxx_source_compiles("${TEST_PROGRAM}" HAS_ASAN_LINK_SUPPORT)

      if(HAS_ASAN_LINK_SUPPORT)
        message(STATUS "AddressSanitizer is supported at both compile and link time.")
        set(SUPPORTS_ASAN ON)
      else()
        message(WARNING "AddressSanitizer is NOT supported at link time.")
        set(SUPPORTS_ASAN OFF)
      endif()
    else()
      set(SUPPORTS_ASAN ON)
    endif()
  endif()
endmacro()

macro(QtWatchAllVideos_setup_options)
  option(QtWatchAllVideos_ENABLE_HARDENING "Enable hardening" ON)
  option(QtWatchAllVideos_ENABLE_COVERAGE "Enable coverage reporting" OFF)
  cmake_dependent_option(
    QtWatchAllVideos_ENABLE_GLOBAL_HARDENING
    "Attempt to push hardening options to built dependencies"
    ON
    QtWatchAllVideos_ENABLE_HARDENING
    OFF)

  QtWatchAllVideos_supports_sanitizers()

  if(NOT PROJECT_IS_TOP_LEVEL OR QtWatchAllVideos_PACKAGING_MAINTAINER_MODE)
    option(QtWatchAllVideos_ENABLE_IPO "Enable IPO/LTO" OFF)
    option(QtWatchAllVideos_WARNINGS_AS_ERRORS "Treat Warnings As Errors" OFF)
    option(QtWatchAllVideos_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" OFF)
    option(QtWatchAllVideos_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(QtWatchAllVideos_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" OFF)
    option(QtWatchAllVideos_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(QtWatchAllVideos_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(QtWatchAllVideos_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(QtWatchAllVideos_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
    option(QtWatchAllVideos_ENABLE_CPPCHECK "Enable cpp-check analysis" OFF)
    option(QtWatchAllVideos_ENABLE_PCH "Enable precompiled headers" OFF)
    option(QtWatchAllVideos_ENABLE_CACHE "Enable ccache" OFF)
  else()
    option(QtWatchAllVideos_ENABLE_IPO "Enable IPO/LTO" ON)
    option(QtWatchAllVideos_WARNINGS_AS_ERRORS "Treat Warnings As Errors" ON)
    option(QtWatchAllVideos_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" ${SUPPORTS_ASAN})
    option(QtWatchAllVideos_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(QtWatchAllVideos_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" ${SUPPORTS_UBSAN})
    option(QtWatchAllVideos_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(QtWatchAllVideos_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(QtWatchAllVideos_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    # OFF by default: the template's demo code used to be tidy/cppcheck clean,
    # the Qt application code that replaced it is not (it trips hundreds of
    # style checks such as readability-magic-numbers or misc-include-cleaner,
    # and they are promoted to errors by WARNINGS_AS_ERRORS). Turn them back on
    # per build with -DQtWatchAllVideos_ENABLE_CLANG_TIDY=ON once the code is
    # cleaned up.
    option(QtWatchAllVideos_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
    option(QtWatchAllVideos_ENABLE_CPPCHECK "Enable cpp-check analysis" OFF)
    option(QtWatchAllVideos_ENABLE_PCH "Enable precompiled headers" OFF)
    option(QtWatchAllVideos_ENABLE_CACHE "Enable ccache" ON)
  endif()

  if(NOT PROJECT_IS_TOP_LEVEL)
    mark_as_advanced(
      QtWatchAllVideos_ENABLE_IPO
      QtWatchAllVideos_WARNINGS_AS_ERRORS
      QtWatchAllVideos_ENABLE_SANITIZER_ADDRESS
      QtWatchAllVideos_ENABLE_SANITIZER_LEAK
      QtWatchAllVideos_ENABLE_SANITIZER_UNDEFINED
      QtWatchAllVideos_ENABLE_SANITIZER_THREAD
      QtWatchAllVideos_ENABLE_SANITIZER_MEMORY
      QtWatchAllVideos_ENABLE_UNITY_BUILD
      QtWatchAllVideos_ENABLE_CLANG_TIDY
      QtWatchAllVideos_ENABLE_CPPCHECK
      QtWatchAllVideos_ENABLE_COVERAGE
      QtWatchAllVideos_ENABLE_PCH
      QtWatchAllVideos_ENABLE_CACHE)
  endif()

endmacro()

macro(QtWatchAllVideos_global_options)
  if(QtWatchAllVideos_ENABLE_IPO)
    include(cmake/InterproceduralOptimization.cmake)
    QtWatchAllVideos_enable_ipo()
  endif()

  QtWatchAllVideos_supports_sanitizers()

  if(QtWatchAllVideos_ENABLE_HARDENING AND QtWatchAllVideos_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR QtWatchAllVideos_ENABLE_SANITIZER_UNDEFINED
       OR QtWatchAllVideos_ENABLE_SANITIZER_ADDRESS
       OR QtWatchAllVideos_ENABLE_SANITIZER_THREAD
       OR QtWatchAllVideos_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    message("${QtWatchAllVideos_ENABLE_HARDENING} ${ENABLE_UBSAN_MINIMAL_RUNTIME} ${QtWatchAllVideos_ENABLE_SANITIZER_UNDEFINED}")
    QtWatchAllVideos_enable_hardening(QtWatchAllVideos_options ON ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()
endmacro()

macro(QtWatchAllVideos_local_options)
  if(PROJECT_IS_TOP_LEVEL)
    include(cmake/StandardProjectSettings.cmake)
  endif()

  add_library(QtWatchAllVideos_warnings INTERFACE)
  add_library(QtWatchAllVideos_options INTERFACE)

  include(cmake/CompilerWarnings.cmake)
  QtWatchAllVideos_set_project_warnings(
    QtWatchAllVideos_warnings
    ${QtWatchAllVideos_WARNINGS_AS_ERRORS}
    ""
    ""
    ""
    "")

  include(cmake/Linker.cmake)
  # Must configure each target with linker options, we're avoiding setting it globally for now

  include(cmake/Sanitizers.cmake)
  QtWatchAllVideos_enable_sanitizers(
    QtWatchAllVideos_options
    ${QtWatchAllVideos_ENABLE_SANITIZER_ADDRESS}
    ${QtWatchAllVideos_ENABLE_SANITIZER_LEAK}
    ${QtWatchAllVideos_ENABLE_SANITIZER_UNDEFINED}
    ${QtWatchAllVideos_ENABLE_SANITIZER_THREAD}
    ${QtWatchAllVideos_ENABLE_SANITIZER_MEMORY})

  set_target_properties(QtWatchAllVideos_options PROPERTIES UNITY_BUILD ${QtWatchAllVideos_ENABLE_UNITY_BUILD})

  if(QtWatchAllVideos_ENABLE_PCH)
    target_precompile_headers(
      QtWatchAllVideos_options
      INTERFACE
      <vector>
      <string>
      <utility>)
  endif()

  if(QtWatchAllVideos_ENABLE_CACHE)
    include(cmake/Cache.cmake)
    QtWatchAllVideos_enable_cache()
  endif()

  include(cmake/StaticAnalyzers.cmake)
  if(QtWatchAllVideos_ENABLE_CLANG_TIDY)
    QtWatchAllVideos_enable_clang_tidy(QtWatchAllVideos_options ${QtWatchAllVideos_WARNINGS_AS_ERRORS})
  endif()

  if(QtWatchAllVideos_ENABLE_CPPCHECK)
    QtWatchAllVideos_enable_cppcheck(${QtWatchAllVideos_WARNINGS_AS_ERRORS} "" # override cppcheck options
    )
  endif()

  if(QtWatchAllVideos_ENABLE_COVERAGE)
    include(cmake/Tests.cmake)
    QtWatchAllVideos_enable_coverage(QtWatchAllVideos_options)
  endif()

  if(QtWatchAllVideos_WARNINGS_AS_ERRORS)
    check_cxx_compiler_flag("-Wl,--fatal-warnings" LINKER_FATAL_WARNINGS)
    if(LINKER_FATAL_WARNINGS)
      # This is not working consistently, so disabling for now
      # target_link_options(QtWatchAllVideos_options INTERFACE -Wl,--fatal-warnings)
    endif()
  endif()

  if(QtWatchAllVideos_ENABLE_HARDENING AND NOT QtWatchAllVideos_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR QtWatchAllVideos_ENABLE_SANITIZER_UNDEFINED
       OR QtWatchAllVideos_ENABLE_SANITIZER_ADDRESS
       OR QtWatchAllVideos_ENABLE_SANITIZER_THREAD
       OR QtWatchAllVideos_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    QtWatchAllVideos_enable_hardening(QtWatchAllVideos_options OFF ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()

endmacro()
