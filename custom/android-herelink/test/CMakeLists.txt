cmake_minimum_required(VERSION 3.25)
project(HerelinkDisplayCheck LANGUAGES CXX)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 6.6 REQUIRED COMPONENTS Quick Quick3D QuickTest)
qt_add_executable(HerelinkDisplayCheck main.cc
                  "${CMAKE_CURRENT_SOURCE_DIR}/../../../.cache/herelink-android-source/custom/custom.qrc"
)
target_link_libraries(HerelinkDisplayCheck PRIVATE Qt6::Quick Qt6::Quick3D Qt6::QuickTest)
