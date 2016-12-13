find_package (Qt5Widgets REQUIRED)
find_package (Qt5Gui REQUIRED)
find_package (Qt5Core REQUIRED)
find_package (Qt5Svg REQUIRED)
find_package (Qt5X11Extras REQUIRED)
find_package (X11)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

#if (NOT QT_CMAKE_PREFIX_PATH)
#set(QT_CMAKE_PREFIX_PATH $ENV{QT_CMAKE_PREFIX_PATH})
#endif ()
#set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${QT_CMAKE_PREFIX_PATH})
#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/")

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

add_definitions(-DDATADIR="${CMAKE_INSTALL_PREFIX}/share")

set(kvantum_SOURCES
    Kvantum.cpp
    KvantumPlugin.cpp
    shortcuthandler.cpp
    blur/blurhelper.cpp
    drag/windowmanager.cpp
    drag/x11wmmove.cpp
    themeconfig/ThemeConfig.cpp)

set(kvantum_RESOURCES themeconfig/defaulttheme.qrc)
#set(COLOR_DIR "/usr/share/kde4/apps/color-schemes/")
set(KF5_COLOR_DIR "${CMAKE_INSTALL_PREFIX}/share/color-schemes/")

qt5_add_resources(RESOURCES ${kvantum_RESOURCES})

add_library(kvantum SHARED ${kvantum_SOURCES} ${RESOURCES})

#target_link_libraries(kvantum ${Qt5Core_LIBRARIES} ${Qt5Gui_LIBRARY} ${Qt5Svg_LIBRARY} ${X11_LIBRARIES} ${Qt5X11Extras_LIBRARIES})

target_link_libraries(kvantum ${Qt5Core_LIBRARIES} ${Qt5Gui_LIBRARIES} ${Qt5Widgets_LIBRARIES} ${Qt5Svg_LIBRARIES} ${Qt5X11Extras_LIBRARIES} ${X11_LIBRARIES})

install(TARGETS kvantum LIBRARY DESTINATION ${KVANTUM_STYLE_DIR})
install(FILES "${CMAKE_SOURCE_DIR}/color/Kvantum.colors" DESTINATION ${KF5_COLOR_DIR})
