CONFIG += qt \
          plugin \
          x11 \
          warn_on

QT += svg
greaterThan(QT_MAJOR_VERSION, 4): QT += x11extras

TARGET = kvantum
TEMPLATE = lib
CONFIG += c++11

VERSION = 0.1

SOURCES += themeconfig/ThemeConfig.cpp \
           shortcuthandler.cpp

HEADERS += themeconfig/specs.h \
           themeconfig/ThemeConfig.h \
           shortcuthandler.h

greaterThan(QT_MAJOR_VERSION, 4) {
  SOURCES += Kvantum.cpp \
             KvantumPlugin.cpp \
             drag/x11wmmove.cpp \
             drag/windowmanager.cpp \
             blur/blurhelper.cpp \
             animation/animation.cpp
  HEADERS += Kvantum.h \
             KvantumPlugin.h \
             drag/x11wmmove.h \
             drag/windowmanager.h \
             blur/blurhelper.h \
             animation/animation.h
  OTHER_FILES += kvantum.json
} else {
  SOURCES += qt4/Kvantum4.cpp \
             qt4/KvantumPlugin4.cpp \
             qt4/x11wmmove4.cpp \
             qt4/windowmanager4.cpp \
             qt4/blurhelper4.cpp
  HEADERS += qt4/Kvantum4.h \
             qt4/KvantumPlugin4.h \
             qt4/x11wmmove4.h \
             qt4/windowmanager4.h \
             qt4/blurhelper4.h
}

RESOURCES += themeconfig/defaulttheme.qrc

unix:!macx: LIBS += -lX11

unix {
  #VARIABLES
  isEmpty(PREFIX) {
    PREFIX = /usr
  }
  COLORSDIR =$$PREFIX/share/kde4/apps/color-schemes
  KF5COLORSDIR =$$PREFIX/share/color-schemes
  DATADIR =$$PREFIX/share

  DEFINES += DATADIR=\\\"$$DATADIR\\\"

  #MAKE INSTALL
  target.path = $$[QT_INSTALL_PLUGINS]/styles
  colors.path = $$COLORSDIR
  colors.files += ../color/Kvantum.colors
  kf5colors.path = $$KF5COLORSDIR
  kf5colors.files += ../color/Kvantum.colors
  equals(QT_MAJOR_VERSION, 4): INSTALLS += target colors
  greaterThan(QT_MAJOR_VERSION, 4): INSTALLS += target kf5colors
}
