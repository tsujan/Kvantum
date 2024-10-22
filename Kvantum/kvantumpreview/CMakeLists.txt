set(QT6_MINIMUM_VERSION "6.6.0")

set(QT_VERSION_MAJOR 6)
find_package(Qt6 "${QT6_MINIMUM_VERSION}" REQUIRED COMPONENTS Core Gui Widgets)

find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS LinguistTools)

set(CMAKE_AUTOMOC ON)
#set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                    ${Qt${QT_VERSION_MAJOR}Core_INCLUDE_DIRS}
                    ${Qt${QT_VERSION_MAJOR}Widgets_INCLUDE_DIRS}
                    ${Qt${QT_VERSION_MAJOR}Gui_INCLUDE_DIRS})

add_definitions(-DDATADIR="${CMAKE_INSTALL_PREFIX}/share")

set(KP_SOURCES main.cpp)
set(KP_UI KvantumPreviewBase.ui)
set(KP_RESOURCES KvantumPreviewResources.qrc)

qt6_wrap_ui(KP_UIS_H ${KP_UI})
set(KP_HEADER KvantumPreview.h)

# Translations
if(Qt${QT_VERSION_MAJOR}LinguistTools_FOUND)
  file(GLOB TS_FILES data/translations/kvantumpreview_*.ts)
  qt6_add_translation(QM_FILES ${TS_FILES})
  add_executable(kvantumpreview ${KP_SOURCES} ${KP_HEADER} ${KP_RESOURCES} ${KP_UI} ${DESKTOP_FILE} ${QM_FILES})
  install(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/kvantumpreview/translations")
else(Qt${QT_VERSION_MAJOR}LinguistTools_FOUND)
  add_executable(kvantumpreview ${KP_SOURCES} ${KP_HEADER} ${KP_RESOURCES} ${KP_UI} ${DESKTOP_FILE})
endif(Qt${QT_VERSION_MAJOR}LinguistTools_FOUND)

target_link_libraries(kvantumpreview Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui)

install(TARGETS kvantumpreview RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
