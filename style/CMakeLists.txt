if(NOT ENABLE_QT4)
  find_package (Qt5Widgets REQUIRED)
  find_package (Qt5Gui REQUIRED)
  find_package (Qt5Core REQUIRED)
  find_package (Qt5Svg REQUIRED)
  find_package (Qt5X11Extras REQUIRED)
else()
  find_package(Qt4 4.8.7 REQUIRED)

  include(${QT_USE_FILE})
  add_definitions(${QT_DEFINITIONS})
endif()
find_package (X11)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

#if (NOT QT_CMAKE_PREFIX_PATH)
#set(QT_CMAKE_PREFIX_PATH $ENV{QT_CMAKE_PREFIX_PATH})
#endif ()
#set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${QT_CMAKE_PREFIX_PATH})
#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/")

if(NOT ENABLE_QT4)
  get_target_property(REAL_QMAKE_EXECUTABLE ${Qt5Core_QMAKE_EXECUTABLE}
                      IMPORTED_LOCATION)
  execute_process(COMMAND "${REAL_QMAKE_EXECUTABLE}" -query QT_INSTALL_PLUGINS
                  OUTPUT_VARIABLE _Qt5_PLUGIN_INSTALL_DIR
                  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
  set(KVANTUM_STYLE_DIR "${_Qt5_PLUGIN_INSTALL_DIR}/styles/")

  include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                      ${Qt5Core_INCLUDE_DIRS} ${Qt5Widgets_INCLUDE_DIRS}
                      ${Qt5Gui_INCLUDE_DIRS} ${Qt5Svg_INCLUDE_DIRS}
                      ${Qt5X11Extras_INCLUDE_DIRS}
                      ${X11_INCLUDE_DIR})
else()
  set(KVANTUM_STYLE_DIR "${QT_PLUGINS_DIR}/styles/")

  include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                      ${QT_INCLUDE_DIR} ${QT_QTSVG_INCLUDE_DIR}
                      ${X11_INCLUDE_DIR})
endif()

add_definitions(-DDATADIR="${CMAKE_INSTALL_PREFIX}/share")

if(NOT ENABLE_QT4)
  set(kvantum_SOURCES
      Kvantum.cpp
      KvantumPlugin.cpp
      shortcuthandler.cpp
      blur/blurhelper.cpp
      drag/windowmanager.cpp
      drag/x11wmmove.cpp
      themeconfig/ThemeConfig.cpp
      animation/animation.cpp)
else()
  set(kvantum_SOURCES
      qt4/Kvantum4.cpp
      qt4/KvantumPlugin4.cpp
      shortcuthandler.cpp
      qt4/blurhelper4.cpp
      qt4/windowmanager4.cpp
      qt4/x11wmmove4.cpp
      themeconfig/ThemeConfig.cpp)
endif()

set(kvantum_RESOURCES themeconfig/defaulttheme.qrc)

add_library(kvantum SHARED ${kvantum_SOURCES} ${kvantum_RESOURCES})

if(NOT ENABLE_QT4)
  set(KDE_COLOR_DIR "${CMAKE_INSTALL_PREFIX}/share/color-schemes/")
else()
  set(KDE_COLOR_DIR "${CMAKE_INSTALL_PREFIX}/share/kde4/apps/color-schemes/")
endif()

if(NOT ENABLE_QT4)
 target_link_libraries(kvantum ${Qt5Core_LIBRARIES}
                               ${Qt5Gui_LIBRARIES}
                               ${Qt5Widgets_LIBRARIES}
                               ${Qt5Svg_LIBRARIES}
                               ${Qt5X11Extras_LIBRARIES}
                               ${X11_LIBRARIES})
else()
 target_link_libraries(kvantum ${QT_QTCORE_LIBRARY}
                               ${QT_QTGUI_LIBRARY}
                               ${QT_QTSVG_LIBRARY}
                               ${X11_LIBRARIES})
endif()

install(TARGETS kvantum LIBRARY DESTINATION ${KVANTUM_STYLE_DIR})
install(FILES "${CMAKE_SOURCE_DIR}/color/Kvantum.colors" DESTINATION ${KDE_COLOR_DIR})
