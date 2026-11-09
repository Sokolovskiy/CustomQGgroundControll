# Git.cmake generates this header before project(). Regenerate it in this
# directory scope to give runtime metadata a display name containing spaces
# while retaining the valid HerelinkGCS CMake project and executable target.
set(QGC_APP_NAME "Herelink GCS")
configure_file(
    "${CMAKE_SOURCE_DIR}/src/qgc_version.h.in"
    "${CMAKE_BINARY_DIR}/qgc_version.h"
    @ONLY
)

list(APPEND QGC_RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/custom.qrc")
list(REMOVE_DUPLICATES QGC_RESOURCES)
set(QGC_RESOURCES "${QGC_RESOURCES}" CACHE STRING "Application resources" FORCE)
set(CUSTOM_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/HerelinkCorePlugin.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/HerelinkCorePlugin.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/HerelinkOsd.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/HerelinkOsd.h"
    CACHE INTERNAL "Custom sources" FORCE
)
set(CUSTOM_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/src" CACHE INTERNAL "Custom includes" FORCE)
set(CUSTOM_DEFINITIONS
    QGC_CUSTOM_BUILD
    CUSTOMHEADER="HerelinkCorePlugin.h"
    CUSTOMCLASS=HerelinkCorePlugin
    CACHE INTERNAL "Custom plugin definitions" FORCE
)

if(WIN32)
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/USER_GUIDE.uk.md"
        "${CMAKE_SOURCE_DIR}/LICENSE-APACHE"
        "${CMAKE_SOURCE_DIR}/LICENSE-GPL"
        DESTINATION .
    )
endif()

if(QGC_BUILD_TESTING)
    set(CUSTOM_SOURCES ${CUSTOM_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/test/HerelinkFlyViewTest.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/test/HerelinkFlyViewTest.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/test/HerelinkOsdTest.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/test/HerelinkOsdTest.h"
        CACHE INTERNAL "Custom sources" FORCE
    )
    function(herelink_add_ui_test)
        if(WIN32)
            # Full-window tests use the native display on Windows; the Qt 6.11
            # offscreen backend asserts during full Fly View layout here.
            set(QGC_TEST_ONSCREEN ON)
            add_qgc_test(HerelinkFlyViewTest LABELS Integration
                ENV_MODIFICATION "QT_QPA_PLATFORM=set:windows")
        else()
            add_qgc_test(HerelinkFlyViewTest LABELS Integration)
        endif()
        add_qgc_test(HerelinkOsdTest LABELS Unit)
    endfunction()
    cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL herelink_add_ui_test)
endif()
