# The Qt application. Everything the cmake_template provides (warning set,
# sanitizers, hardening, linker flags, static analyzers) is reused through the
# QtWatchAllVideos2_options / QtWatchAllVideos2_warnings interface targets.

# Qt's code generators. The template does not turn these on globally, and the
# Qt classes here (ControlsPanel/VideoWatcher/MyGraphicsView) carry Q_OBJECT.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

qt_add_executable(
  QtWatchAllVideos2
  main.cpp
  videowatcher.cpp
  videowatcher.h
  controlspanel.cpp
  controlspanel.h)

target_link_libraries(
  QtWatchAllVideos2
  PRIVATE QtWatchAllVideos2::QtWatchAllVideos2_options
          QtWatchAllVideos2::QtWatchAllVideos2_warnings
          Qt6::Widgets
          Qt6::Network
          Qt6::Multimedia
          Qt6::MultimediaWidgets
          Qt6::WebEngineWidgets)

QtWatchAllVideos2_configure_linker(QtWatchAllVideos2)

# CMake-configured headers (project name / version / git sha)
target_include_directories(QtWatchAllVideos2 PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include")

# A GUI app on Windows (no console window) and a proper .app bundle on macOS,
# so that windeployqt / macdeployqt have something sensible to work on.
set_target_properties(
  QtWatchAllVideos2
  PROPERTIES WIN32_EXECUTABLE ON
             MACOSX_BUNDLE ON
             MACOSX_BUNDLE_BUNDLE_NAME "QtWatchAllVideos2"
             MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.snoutfirst.QtWatchAllVideos2"
             MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
             MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
