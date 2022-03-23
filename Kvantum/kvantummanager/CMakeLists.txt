set(QT5_MINIMUM_VERSION "5.15.0")
set(QT6_MINIMUM_VERSION "6.2.0")

if(NOT ENABLE_QT5)
  set(QT_VERSION_MAJOR 6)
  find_package(Qt6Core "${QT6_MINIMUM_VERSION}" REQUIRED)
else()
  set(QT_VERSION_MAJOR 5)
  find_package(Qt5Core "${QT5_MINIMUM_VERSION}" REQUIRED)
endif()

find_package(Qt${QT_VERSION_MAJOR}Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR}Gui REQUIRED)
find_package(Qt${QT_VERSION_MAJOR}Svg REQUIRED)
find_package(Qt${QT_VERSION_MAJOR}LinguistTools QUIET)

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

if(QT_VERSION_MAJOR EQUAL 6)
  #qt6_wrap_ui(KM_UIS_H ${KM_UI})
  set(COMBO_HEADER combobox.h)
else()
   #qt5_wrap_ui(KM_UIS_H ${KM_UI})
  if(CMAKE_VERSION VERSION_LESS "3.10.0") # CMP0071 in 3.10: Let AUTOMOC and AUTOUIC process GENERATED files
    qt5_wrap_cpp(COMBO_HEADER combobox.h) # needed by "kvantummanager.ui"
  else()
    set(COMBO_HEADER combobox.h)
  endif()
endif()

# Translations
if(Qt${QT_VERSION_MAJOR}LinguistTools_FOUND)
  file(GLOB TS_FILES data/translations/kvantummanager_*.ts)
  if(QT_VERSION_MAJOR EQUAL 6)
    qt6_add_translation(QM_FILES ${TS_FILES})
  else()
    qt5_add_translation(QM_FILES ${TS_FILES})
  endif()
  add_executable(kvantummanager ${COMBO_HEADER} ${KM_SOURCES} ${KM_RESOURCES} ${DESKTOP_FILE} ${QM_FILES})
  install(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/kvantummanager/translations")
else(Qt${QT_VERSION_MAJOR}LinguistTools_FOUND)
  add_executable(kvantummanager ${COMBO_HEADER} ${KM_SOURCES} ${KM_RESOURCES} ${DESKTOP_FILE})
endif(Qt${QT_VERSION_MAJOR}LinguistTools_FOUND)

target_link_libraries(kvantummanager Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui Qt${QT_VERSION_MAJOR}::Svg)

install(TARGETS kvantummanager RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

install(FILES ${DESKTOP_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
install(FILES ${CMAKE_SOURCE_DIR}/kvantumpreview/data/kvantum.svg DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps")
