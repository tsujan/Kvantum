project(kvantum)
cmake_minimum_required(VERSION 3.0)
set(CMAKE_BUILD_TYPE Release)
add_definitions (-Wall)

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "/usr")
endif()

option(ENABLE_QT4 "Building Qt4 plugin." OFF)

add_subdirectory(style)
if(NOT ENABLE_QT4)
  add_subdirectory(kvantumpreview)
  add_subdirectory(kvantummanager)
  add_subdirectory(themes)
endif()
