set(QT6_MINIMUM_VERSION "6.6.0")

find_package(Qt6 "${QT6_MINIMUM_VERSION}" REQUIRED COMPONENTS Core Gui Widgets Svg)
find_package(Qt6 QUIET COMPONENTS LinguistTools)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                    ${Qt6Core_INCLUDE_DIRS}
                    ${Qt6Widgets_INCLUDE_DIRS}
                    ${Qt6Gui_INCLUDE_DIRS}
                    ${Qt6Svg_INCLUDE_DIRS})

add_definitions(-DDATADIR="${CMAKE_INSTALL_PREFIX}/share")

set(KM_SOURCES
    main.cpp
    KvantumManager.cpp
    KvCommand.cpp
    svgicons.cpp)

set(KM_RESOURCES kvantummanager.qrc)

set(DESKTOP_FILE data/kvantummanager.desktop)

set(COMBO_HEADER combobox.h)

# Translations
if(Qt6LinguistTools_FOUND)
  file(GLOB TS_FILES data/translations/kvantummanager_*.ts)
  qt6_add_translation(QM_FILES ${TS_FILES})
  add_executable(kvantummanager ${COMBO_HEADER} ${KM_SOURCES} ${KM_RESOURCES} ${DESKTOP_FILE} ${QM_FILES})
  install(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/kvantummanager/translations")
else(Qt6LinguistTools_FOUND)
  add_executable(kvantummanager ${COMBO_HEADER} ${KM_SOURCES} ${KM_RESOURCES} ${DESKTOP_FILE})
endif(Qt6LinguistTools_FOUND)

target_link_libraries(kvantummanager Qt6::Widgets Qt6::Core Qt6::Gui Qt6::Svg)

install(TARGETS kvantummanager RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

install(FILES ${DESKTOP_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
install(FILES ${CMAKE_SOURCE_DIR}/kvantumpreview/data/kvantum.svg DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps")
