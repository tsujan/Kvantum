find_package (Qt5Widgets REQUIRED)
find_package (Qt5Gui REQUIRED)
find_package (Qt5Core REQUIRED)


set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                    ${Qt5Core_INCLUDE_DIRS} ${Qt5Widgets_INCLUDE_DIRS}
                    ${Qt5Gui_INCLUDE_DIRS})

add_definitions(-DDATADIR="${CMAKE_INSTALL_PREFIX}/share")
#get_filename_component(PARENT_DIR ${CMAKE_SOURCE_DIR} DIRECTORY)

set(KM_SOURCES
    main.cpp
    KvantumManager.cpp)

#set(KM_UI kvantummanager.ui)

set(KM_RESOURCES kvantummanager.qrc)

set(DESKTOP_FILE data/kvantummanager.desktop)

#qt5_wrap_ui(KM_UIS_H ${KM_UI})
qt5_wrap_cpp(COMBO_HEADER combobox.h) # needed by "kvantummanager.ui"

add_executable(kvantummanager ${COMBO_HEADER} ${KM_SOURCES} ${KM_RESOURCES} ${DESKTOP_FILE})
target_link_libraries(kvantummanager Qt5::Widgets Qt5::Core Qt5::Gui)

install(TARGETS kvantummanager RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(FILES ${DESKTOP_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
install(FILES ${CMAKE_SOURCE_DIR}/kvantumpreview/kvantum.svg DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps")
