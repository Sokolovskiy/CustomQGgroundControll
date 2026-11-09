# Keep the CMake project/target identifier free of spaces.
set(QGC_APP_NAME "HerelinkGCS" CACHE STRING "Application target name" FORCE)
set(QGC_STABLE_BUILD ON CACHE BOOL "Use stable release behavior" FORCE)
set(QGC_WINDOWS_ICON_PATH "${CMAKE_SOURCE_DIR}/${QGC_CUSTOM_DIR}/resources/herelink-gcs.ico"
    CACHE FILEPATH "Windows application icon" FORCE)

# Herelink GCS shares QGroundControl's organization directory.
set(QGC_WINDOWS_PRESERVE_APPDATA ON)

# Retain both standard firmware plugin factories.
set(QGC_DISABLE_APM_PLUGIN_FACTORY OFF CACHE BOOL "Disable ArduPilot plugin factory" FORCE)
set(QGC_DISABLE_PX4_PLUGIN_FACTORY OFF CACHE BOOL "Disable PX4 plugin factory" FORCE)
