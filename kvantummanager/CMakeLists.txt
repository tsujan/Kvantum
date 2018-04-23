find_package (Qt5Widgets REQUIRED)
find_package (Qt5Gui REQUIRED)
find_package (Qt5Core REQUIRED)
find_package (Qt5LinguistTools QUIET)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

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
if (CMAKE_VERSION VERSION_LESS "3.10.0") # CMP0071 in 3.10: Let AUTOMOC and AUTOUIC process GENERATED files
  qt5_wrap_cpp(COMBO_HEADER combobox.h) # needed by "kvantummanager.ui"
else()
  set(COMBO_HEADER combobox.h)
endif()

# Translations
if(Qt5LinguistTools_FOUND)
  file(GLOB TS_FILES data/translations/kvantummanager_*.ts)
  qt5_add_translation(QM_FILES ${TS_FILES})
  add_executable(kvantummanager ${COMBO_HEADER} ${KM_SOURCES} ${KM_RESOURCES} ${DESKTOP_FILE} ${QM_FILES})
  install(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/kvantummanager/translations")
else(Qt5LinguistTools_FOUND)
  add_executable(kvantummanager ${COMBO_HEADER} ${KM_SOURCES} ${KM_RESOURCES} ${DESKTOP_FILE})
endif(Qt5LinguistTools_FOUND)


target_link_libraries(kvantummanager Qt5::Widgets Qt5::Core Qt5::Gui)

install(TARGETS kvantummanager RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

install(FILES ${DESKTOP_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
install(FILES ${CMAKE_SOURCE_DIR}/kvantumpreview/data/kvantum.svg DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps")
