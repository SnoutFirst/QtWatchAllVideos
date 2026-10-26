macro(QtWatchAllVideos2_configure_linker project_name)
  set(QtWatchAllVideos2_USER_LINKER_OPTION
    "DEFAULT"
      CACHE STRING "Linker to be used")
    set(QtWatchAllVideos2_USER_LINKER_OPTION_VALUES "DEFAULT" "SYSTEM" "LLD" "GOLD" "BFD" "MOLD" "SOLD" "APPLE_CLASSIC" "MSVC")
  set_property(CACHE QtWatchAllVideos2_USER_LINKER_OPTION PROPERTY STRINGS ${QtWatchAllVideos2_USER_LINKER_OPTION_VALUES})
  list(
    FIND
    QtWatchAllVideos2_USER_LINKER_OPTION_VALUES
    ${QtWatchAllVideos2_USER_LINKER_OPTION}
    QtWatchAllVideos2_USER_LINKER_OPTION_INDEX)

  if(${QtWatchAllVideos2_USER_LINKER_OPTION_INDEX} EQUAL -1)
    message(
      STATUS
        "Using custom linker: '${QtWatchAllVideos2_USER_LINKER_OPTION}', explicitly supported entries are ${QtWatchAllVideos2_USER_LINKER_OPTION_VALUES}")
  endif()

  set_target_properties(${project_name} PROPERTIES LINKER_TYPE "${QtWatchAllVideos2_USER_LINKER_OPTION}")
endmacro()
