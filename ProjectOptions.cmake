include(cmake/LibFuzzer.cmake)
include(CMakeDependentOption)
include(CheckCXXCompilerFlag)


include(CheckCXXSourceCompiles)


macro(QtWatchAllVideos2_supports_sanitizers)
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

macro(QtWatchAllVideos2_setup_options)
  option(QtWatchAllVideos2_ENABLE_HARDENING "Enable hardening" ON)
  option(QtWatchAllVideos2_ENABLE_COVERAGE "Enable coverage reporting" OFF)
  cmake_dependent_option(
    QtWatchAllVideos2_ENABLE_GLOBAL_HARDENING
    "Attempt to push hardening options to built dependencies"
    ON
    QtWatchAllVideos2_ENABLE_HARDENING
    OFF)

  QtWatchAllVideos2_supports_sanitizers()

  if(NOT PROJECT_IS_TOP_LEVEL OR QtWatchAllVideos2_PACKAGING_MAINTAINER_MODE)
    option(QtWatchAllVideos2_ENABLE_IPO "Enable IPO/LTO" OFF)
    option(QtWatchAllVideos2_WARNINGS_AS_ERRORS "Treat Warnings As Errors" OFF)
    option(QtWatchAllVideos2_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" OFF)
    option(QtWatchAllVideos2_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(QtWatchAllVideos2_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" OFF)
    option(QtWatchAllVideos2_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(QtWatchAllVideos2_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(QtWatchAllVideos2_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(QtWatchAllVideos2_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
    option(QtWatchAllVideos2_ENABLE_CPPCHECK "Enable cpp-check analysis" OFF)
    option(QtWatchAllVideos2_ENABLE_PCH "Enable precompiled headers" OFF)
    option(QtWatchAllVideos2_ENABLE_CACHE "Enable ccache" OFF)
  else()
    option(QtWatchAllVideos2_ENABLE_IPO "Enable IPO/LTO" ON)
    option(QtWatchAllVideos2_WARNINGS_AS_ERRORS "Treat Warnings As Errors" ON)
    option(QtWatchAllVideos2_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" ${SUPPORTS_ASAN})
    option(QtWatchAllVideos2_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(QtWatchAllVideos2_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" ${SUPPORTS_UBSAN})
    option(QtWatchAllVideos2_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(QtWatchAllVideos2_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(QtWatchAllVideos2_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(QtWatchAllVideos2_ENABLE_CLANG_TIDY "Enable clang-tidy" ON)
    option(QtWatchAllVideos2_ENABLE_CPPCHECK "Enable cpp-check analysis" ON)
    option(QtWatchAllVideos2_ENABLE_PCH "Enable precompiled headers" OFF)
    option(QtWatchAllVideos2_ENABLE_CACHE "Enable ccache" ON)
  endif()

  if(NOT PROJECT_IS_TOP_LEVEL)
    mark_as_advanced(
      QtWatchAllVideos2_ENABLE_IPO
      QtWatchAllVideos2_WARNINGS_AS_ERRORS
      QtWatchAllVideos2_ENABLE_SANITIZER_ADDRESS
      QtWatchAllVideos2_ENABLE_SANITIZER_LEAK
      QtWatchAllVideos2_ENABLE_SANITIZER_UNDEFINED
      QtWatchAllVideos2_ENABLE_SANITIZER_THREAD
      QtWatchAllVideos2_ENABLE_SANITIZER_MEMORY
      QtWatchAllVideos2_ENABLE_UNITY_BUILD
      QtWatchAllVideos2_ENABLE_CLANG_TIDY
      QtWatchAllVideos2_ENABLE_CPPCHECK
      QtWatchAllVideos2_ENABLE_COVERAGE
      QtWatchAllVideos2_ENABLE_PCH
      QtWatchAllVideos2_ENABLE_CACHE)
  endif()

endmacro()

macro(QtWatchAllVideos2_global_options)
  if(QtWatchAllVideos2_ENABLE_IPO)
    include(cmake/InterproceduralOptimization.cmake)
    QtWatchAllVideos2_enable_ipo()
  endif()

  QtWatchAllVideos2_supports_sanitizers()

  if(QtWatchAllVideos2_ENABLE_HARDENING AND QtWatchAllVideos2_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR QtWatchAllVideos2_ENABLE_SANITIZER_UNDEFINED
       OR QtWatchAllVideos2_ENABLE_SANITIZER_ADDRESS
       OR QtWatchAllVideos2_ENABLE_SANITIZER_THREAD
       OR QtWatchAllVideos2_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    message("${QtWatchAllVideos2_ENABLE_HARDENING} ${ENABLE_UBSAN_MINIMAL_RUNTIME} ${QtWatchAllVideos2_ENABLE_SANITIZER_UNDEFINED}")
    QtWatchAllVideos2_enable_hardening(QtWatchAllVideos2_options ON ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()
endmacro()

macro(QtWatchAllVideos2_local_options)
  if(PROJECT_IS_TOP_LEVEL)
    include(cmake/StandardProjectSettings.cmake)
  endif()

  add_library(QtWatchAllVideos2_warnings INTERFACE)
  add_library(QtWatchAllVideos2_options INTERFACE)

  include(cmake/CompilerWarnings.cmake)
  QtWatchAllVideos2_set_project_warnings(
    QtWatchAllVideos2_warnings
    ${QtWatchAllVideos2_WARNINGS_AS_ERRORS}
    ""
    ""
    ""
    "")

  include(cmake/Linker.cmake)
  # Must configure each target with linker options, we're avoiding setting it globally for now

  include(cmake/Sanitizers.cmake)
  QtWatchAllVideos2_enable_sanitizers(
    QtWatchAllVideos2_options
    ${QtWatchAllVideos2_ENABLE_SANITIZER_ADDRESS}
    ${QtWatchAllVideos2_ENABLE_SANITIZER_LEAK}
    ${QtWatchAllVideos2_ENABLE_SANITIZER_UNDEFINED}
    ${QtWatchAllVideos2_ENABLE_SANITIZER_THREAD}
    ${QtWatchAllVideos2_ENABLE_SANITIZER_MEMORY})

  set_target_properties(QtWatchAllVideos2_options PROPERTIES UNITY_BUILD ${QtWatchAllVideos2_ENABLE_UNITY_BUILD})

  if(QtWatchAllVideos2_ENABLE_PCH)
    target_precompile_headers(
      QtWatchAllVideos2_options
      INTERFACE
      <vector>
      <string>
      <utility>)
  endif()

  if(QtWatchAllVideos2_ENABLE_CACHE)
    include(cmake/Cache.cmake)
    QtWatchAllVideos2_enable_cache()
  endif()

  include(cmake/StaticAnalyzers.cmake)
  if(QtWatchAllVideos2_ENABLE_CLANG_TIDY)
    QtWatchAllVideos2_enable_clang_tidy(QtWatchAllVideos2_options ${QtWatchAllVideos2_WARNINGS_AS_ERRORS})
  endif()

  if(QtWatchAllVideos2_ENABLE_CPPCHECK)
    QtWatchAllVideos2_enable_cppcheck(${QtWatchAllVideos2_WARNINGS_AS_ERRORS} "" # override cppcheck options
    )
  endif()

  if(QtWatchAllVideos2_ENABLE_COVERAGE)
    include(cmake/Tests.cmake)
    QtWatchAllVideos2_enable_coverage(QtWatchAllVideos2_options)
  endif()

  if(QtWatchAllVideos2_WARNINGS_AS_ERRORS)
    check_cxx_compiler_flag("-Wl,--fatal-warnings" LINKER_FATAL_WARNINGS)
    if(LINKER_FATAL_WARNINGS)
      # This is not working consistently, so disabling for now
      # target_link_options(QtWatchAllVideos2_options INTERFACE -Wl,--fatal-warnings)
    endif()
  endif()

  if(QtWatchAllVideos2_ENABLE_HARDENING AND NOT QtWatchAllVideos2_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR QtWatchAllVideos2_ENABLE_SANITIZER_UNDEFINED
       OR QtWatchAllVideos2_ENABLE_SANITIZER_ADDRESS
       OR QtWatchAllVideos2_ENABLE_SANITIZER_THREAD
       OR QtWatchAllVideos2_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    QtWatchAllVideos2_enable_hardening(QtWatchAllVideos2_options OFF ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()

endmacro()
