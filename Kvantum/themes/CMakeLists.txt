set(KVANTUM_THEME_DIR "${CMAKE_INSTALL_PREFIX}/share/Kvantum/")
set(KF5_COLOR_DIR "${CMAKE_INSTALL_PREFIX}/share/color-schemes/")
set(OPENBOX_DIR "${CMAKE_INSTALL_PREFIX}/share/themes/")

# CMAKE_CURRENT_LIST_DIR can be used too. The trailing slash means the contents.
install(DIRECTORY "${CMAKE_SOURCE_DIR}/themes/kvthemes/" DESTINATION ${KVANTUM_THEME_DIR})
install(DIRECTORY "${CMAKE_SOURCE_DIR}/themes/colors/" DESTINATION ${KF5_COLOR_DIR})
install(DIRECTORY "${CMAKE_SOURCE_DIR}/themes/openbox/" DESTINATION ${OPENBOX_DIR})
