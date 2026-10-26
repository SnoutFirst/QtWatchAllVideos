include(cmake/CPM.cmake)

# Qt is not fetched: it is a system dependency, provided by the distribution
# (qt6-base-dev / qt6-multimedia-dev / qt6-webengine-dev on Debian and Ubuntu),
# by Homebrew on macOS, or by the Qt online installer / aqtinstall on Windows.
# CPM stays available for future third-party dependencies.
function(QtWatchAllVideos2_setup_dependencies)

  # Allow a parent project to provide Qt itself
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

endfunction()
