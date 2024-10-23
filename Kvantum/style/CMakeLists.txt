set(QT5_MINIMUM_VERSION "5.15.0")
set(QT6_MINIMUM_VERSION "6.6.0")
set(KF5_MINIMUM_VERSION "5.82.0")
set(KF6_MINIMUM_VERSION "6.0.0")

if(NOT ENABLE_QT4)
  if(NOT ENABLE_QT5)
    set(QT_VERSION_MAJOR 6)
    find_package(Qt6 "${QT6_MINIMUM_VERSION}" REQUIRED COMPONENTS Core Gui Widgets Svg)
    if(NOT WITHOUT_KF)
      find_package(KF6WindowSystem "${KF6_MINIMUM_VERSION}" REQUIRED)
    endif()
  else()
    set(QT_VERSION_MAJOR 5)
    find_package(Qt5 "${QT5_MINIMUM_VERSION}" REQUIRED COMPONENTS Core Gui Widgets Svg X11Extras)
  endif()
else()
  set(QT_VERSION_MAJOR 4)
  find_package(Qt4 4.8.7 REQUIRED)

  include(${QT_USE_FILE})
  add_definitions(${QT_DEFINITIONS})
endif()

find_package(X11 REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(QT_VERSION_MAJOR EQUAL 6)
  get_target_property(REAL_QMAKE_EXECUTABLE Qt6::qmake
                      IMPORTED_LOCATION)
  execute_process(COMMAND "${REAL_QMAKE_EXECUTABLE}" -query QT_INSTALL_PLUGINS
                  OUTPUT_VARIABLE _Qt6_PLUGIN_INSTALL_DIR
                  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
  set(KVANTUM_STYLE_DIR "${_Qt6_PLUGIN_INSTALL_DIR}/styles/")

  include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                      ${Qt6Core_INCLUDE_DIRS} ${Qt6Widgets_INCLUDE_DIRS}
                      ${Qt6Gui_INCLUDE_DIRS} ${Qt6Svg_INCLUDE_DIRS}
                      ${X11_INCLUDE_DIR})
elseif(QT_VERSION_MAJOR EQUAL 5)
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
  add_definitions(-DNO_KF)
else()
  set(KVANTUM_STYLE_DIR "${QT_PLUGINS_DIR}/styles/")

  include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                      ${QT_INCLUDE_DIR} ${QT_QTSVG_INCLUDE_DIR}
                      ${X11_INCLUDE_DIR})
endif()

add_definitions(-DDATADIR="${CMAKE_INSTALL_PREFIX}/share")

if(WITHOUT_KF)
  add_definitions(-DNO_KF)
endif()

if(QT_VERSION_MAJOR EQUAL 6)
  set(kvantum_SOURCES
      Kvantum.cpp
      eventFiltering.cpp
      polishing.cpp
      rendering.cpp
      standardIcons.cpp
      viewItems.cpp
      KvantumPlugin.cpp
      shortcuthandler.cpp
      blur/blurhelper.cpp
      drag/windowmanager.cpp
      themeconfig/ThemeConfig.cpp
      animation/animation.cpp)
elseif(QT_VERSION_MAJOR EQUAL 5)
  set(kvantum_SOURCES
      qt5/Kvantum5.cpp
      qt5/eventFiltering5.cpp
      qt5/polishing5.cpp
      qt5/rendering5.cpp
      qt5/standardIcons5.cpp
      qt5/viewItems5.cpp
      qt5/KvantumPlugin5.cpp
      qt5/shortcuthandler5.cpp
      qt5/blurhelper5.cpp
      qt5/windowmanager5.cpp
      qt5/ThemeConfig5.cpp
      qt5/animation5.cpp)
else()
  set(kvantum_SOURCES
      qt4/Kvantum4.cpp
      qt4/KvantumPlugin4.cpp
      qt4/shortcuthandler4.cpp
      qt4/blurhelper4.cpp
      qt4/windowmanager4.cpp
      qt4/x11wmmove4.cpp
      qt4/ThemeConfig4.cpp)
endif()

set(kvantum_RESOURCES themeconfig/defaulttheme.qrc)

add_library(kvantum SHARED ${kvantum_SOURCES} ${kvantum_RESOURCES})

if(NOT ENABLE_QT4)
  set(KDE_COLOR_DIR "${CMAKE_INSTALL_PREFIX}/share/color-schemes/")
else()
  set(KDE_COLOR_DIR "${CMAKE_INSTALL_PREFIX}/share/kde4/apps/color-schemes/")
endif()

if(QT_VERSION_MAJOR EQUAL 6)
  if(WITHOUT_KF)
    target_link_libraries(kvantum ${Qt6Core_LIBRARIES}
                                  ${Qt6Gui_LIBRARIES}
                                  ${Qt6Widgets_LIBRARIES}
                                  ${Qt6Svg_LIBRARIES}
                                  ${X11_LIBRARIES})
  else()
    target_link_libraries(kvantum ${Qt6Core_LIBRARIES}
                                  ${Qt6Gui_LIBRARIES}
                                  ${Qt6Widgets_LIBRARIES}
                                  ${Qt6Svg_LIBRARIES}
                                  ${X11_LIBRARIES}
                                  KF6::WindowSystem)
  endif()
elseif(QT_VERSION_MAJOR EQUAL 5)
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
if(QT_VERSION_MAJOR EQUAL 6)
  install(FILES "${CMAKE_SOURCE_DIR}/color/Kvantum.colors" DESTINATION ${KDE_COLOR_DIR})
endif()
