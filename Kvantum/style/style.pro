CONFIG += qt \
          plugin \
          x11 \
          warn_on

QT += svg
greaterThan(QT_MAJOR_VERSION, 4): QT += x11extras \
                                        core-private \
                                        widgets-private


TARGET = kvantum
TEMPLATE = lib

VERSION = 0.1

SOURCES += themeconfig/ThemeConfig.cpp \
           Kvantum.cpp \
           KvantumPlugin.cpp \
           shortcuthandler.cpp \
           drag/x11wmmove.cpp \
           drag/windowmanager.cpp \
           blur/blurhelper.cpp

HEADERS += themeconfig/specs.h \
           themeconfig/ThemeConfig.h \
           Kvantum.h \
           KvantumPlugin.h \
           shortcuthandler.h \
           drag/windowmanager.h \
           drag/x11wmmove.h \
           blur/blurhelper.h

greaterThan(QT_MAJOR_VERSION, 4): OTHER_FILES += kvantum.json

RESOURCES += themeconfig/defaulttheme.qrc

unix:!macx: LIBS += -lX11

unix {
  #VARIABLES
  isEmpty(PREFIX) {
    PREFIX = /usr
  }
  COLORSDIR =$$PREFIX/share/kde4/apps/color-schemes

  #MAKE INSTALL
  target.path = $$[QT_INSTALL_PLUGINS]/styles
  colors.path = $$COLORSDIR
  colors.files += ../color/Kvantum.colors
  equals(QT_MAJOR_VERSION, 4): INSTALLS += target colors
  greaterThan(QT_MAJOR_VERSION, 4): INSTALLS += target
}
