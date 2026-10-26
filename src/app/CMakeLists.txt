# The Qt application. Everything the cmake_template provides (warning set,
# sanitizers, hardening, linker flags, static analyzers) is reused through the
# QtWatchAllVideos_options / QtWatchAllVideos_warnings interface targets.

# Qt's code generators. The template does not turn these on globally, and the
# Qt classes here (ControlsPanel/VideoWatcher/MyGraphicsView) carry Q_OBJECT.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

qt_add_executable(
  QtWatchAllVideos
  main.cpp
  videowatcher.cpp
  videowatcher.h
  controlspanel.cpp
  controlspanel.h)

target_link_libraries(
  QtWatchAllVideos
  PRIVATE QtWatchAllVideos::QtWatchAllVideos_options
          QtWatchAllVideos::QtWatchAllVideos_warnings
          Qt6::Widgets
          Qt6::Network
          Qt6::Multimedia
          Qt6::MultimediaWidgets
          Qt6::WebEngineWidgets)

QtWatchAllVideos_configure_linker(QtWatchAllVideos)

# CMake-configured headers (project name / version / git sha)
target_include_directories(QtWatchAllVideos PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include")

# A GUI app on Windows (no console window) and a proper .app bundle on macOS,
# so that windeployqt / macdeployqt have something sensible to work on.
set_target_properties(
  QtWatchAllVideos
  PROPERTIES WIN32_EXECUTABLE ON
             MACOSX_BUNDLE ON
             MACOSX_BUNDLE_BUNDLE_NAME "QtWatchAllVideos"
             MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.snoutfirst.QtWatchAllVideos"
             MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
             MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
