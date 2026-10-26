include(cmake/CPM.cmake)

# Qt is not fetched: it is a system dependency, provided by the distribution
# (qt6-base-dev / qt6-multimedia-dev / qt6-webengine-dev on Debian and Ubuntu),
# by Homebrew on macOS, or by the Qt online installer / aqtinstall on Windows.
# CPM stays available for future third-party dependencies.

# NOTE: this has to run at *directory* scope, not inside a function().
# find_package(Qt6) sets QT_NAMESPACE and creates the plugin targets that
# qt6_finalize_target() later resolves as "${QT_NAMESPACE}::<plugin>". Inside a
# function the variable dies with the function scope, so the deferred finalizer
# at the end of src/app/CMakeLists.txt fails with
#
#   get_target_property() called with non-existent target
#   "::QComposePlatformInputContextPlugin"
#
# which is exactly what the official Qt 6.8.3 does here.
if(NOT TARGET Qt6::Widgets)
  find_package(
    Qt6
    REQUIRED
    COMPONENTS Widgets
               Network
               Multimedia
               MultimediaWidgets
               WebEngineWidgets)
endif()

# Qt6::MultimediaWidgets is only needed for the QVideoWidget fallback path and
# Qt6::WebEngineWidgets for the page scraper; both must be present. Fail loudly
# rather than half-linking.
foreach(module Widgets Network Multimedia MultimediaWidgets WebEngineWidgets)
  if(NOT TARGET Qt6::${module})
    message(FATAL_ERROR "Qt6::${module} is required but was not found")
  endif()
endforeach()
