find_package (Qt5Widgets REQUIRED)
find_package (Qt5Gui REQUIRED)
find_package (Qt5Core REQUIRED)


set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
#set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                    ${Qt5Core_INCLUDE_DIRS} ${Qt5Widgets_INCLUDE_DIRS}
                    ${Qt5Gui_INCLUDE_DIRS})

set(KP_SOURCES main.cpp)
set(KP_UI KvantumPreviewBase.ui)
set(KP_RESOURCES KvantumPreviewResources.qrc)


qt5_wrap_ui(KP_UIS_H ${KP_UI})
qt5_wrap_cpp(KP_HEADER KvantumPreview.h) # needed by "main.cpp"

add_executable(kvantumpreview ${KP_SOURCES} ${KP_HEADER} ${KP_RESOURCES} ${KP_UI} ${DESKTOP_FILE})
target_link_libraries(kvantumpreview Qt5::Widgets Qt5::Core Qt5::Gui)

install(TARGETS kvantumpreview RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
