set(QT6_MINIMUM_VERSION "6.6.0")

set(QT_VERSION_MAJOR 6)
find_package(Qt6 "${QT6_MINIMUM_VERSION}" REQUIRED COMPONENTS Core Gui Widgets Svg)

find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS LinguistTools)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                    ${Qt${QT_VERSION_MAJOR}Core_INCLUDE_DIRS}
                    ${Qt${QT_VERSION_MAJOR}Widgets_INCLUDE_DIRS}
                    ${Qt${QT_VERSION_MAJOR}Gui_INCLUDE_DIRS}
                    ${Qt${QT_VERSION_MAJOR}Svg_INCLUDE_DIRS})

add_definitions(-DDATADIR="${CMAKE_INSTALL_PREFIX}/share")
#get_filename_component(PARENT_DIR ${CMAKE_SOURCE_DIR} DIRECTORY)

set(KM_SOURCES
    main.cpp
    KvantumManager.cpp
    KvCommand.cpp
    svgicons.cpp)

#set(KM_UI kvantummanager.ui)

set(KM_RESOURCES kvantummanager.qrc)

set(DESKTOP_FILE data/kvantummanager.desktop)

 #qt6_wrap_ui(KM_UIS_H ${KM_UI})
 set(COMBO_HEADER combobox.h)

# Translations
if(Qt${QT_VERSION_MAJOR}LinguistTools_FOUND)
  file(GLOB TS_FILES data/translations/kvantummanager_*.ts)
  qt6_add_translation(QM_FILES ${TS_FILES})
  add_executable(kvantummanager ${COMBO_HEADER} ${KM_SOURCES} ${KM_RESOURCES} ${DESKTOP_FILE} ${QM_FILES})
  install(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/kvantummanager/translations")
else(Qt${QT_VERSION_MAJOR}LinguistTools_FOUND)
  add_executable(kvantummanager ${COMBO_HEADER} ${KM_SOURCES} ${KM_RESOURCES} ${DESKTOP_FILE})
endif(Qt${QT_VERSION_MAJOR}LinguistTools_FOUND)

target_link_libraries(kvantummanager Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui Qt${QT_VERSION_MAJOR}::Svg)

install(TARGETS kvantummanager RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

install(FILES ${DESKTOP_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
install(FILES ${CMAKE_SOURCE_DIR}/kvantumpreview/data/kvantum.svg DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps")
