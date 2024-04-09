CONFIG += qt \
          plugin \
          x11 \
          warn_on

QT += svg

greaterThan(QT_MAJOR_VERSION, 4) {
  lessThan(QT_MAJOR_VERSION, 6) {
    lessThan(QT_MINOR_VERSION, 15) {
      error("Kvantum needs at least Qt 5.15.0.")
    }
    QT += x11extras
  } else {
    equals(QT_MAJOR_VERSION, 6) {
      lessThan(QT_MINOR_VERSION, 2) {
        error("Kvantum needs at least Qt 6.2.0.")
      } else {
        QT += widgets
      }
    } else {
      error("Kvantum cannot be compiled against this version of Qt.")
    }
  }
}

TARGET = kvantum
TEMPLATE = lib
CONFIG += c++11

VERSION = 0.1

greaterThan(QT_MAJOR_VERSION, 4) {
  DEFINES += NO_KF
  message("Compiling without KDE Frameworks...")

  SOURCES += Kvantum.cpp \
             eventFiltering.cpp \
             polishing.cpp \
             rendering.cpp \
             standardIcons.cpp \
             viewItems.cpp \
             KvantumPlugin.cpp \
             shortcuthandler.cpp \
             drag/windowmanager.cpp \
             blur/blurhelper.cpp \
             animation/animation.cpp \
             themeconfig/ThemeConfig.cpp
  HEADERS += Kvantum.h \
             KvantumPlugin.h \
             shortcuthandler.h \
             drag/windowmanager.h \
             blur/blurhelper.h \
             animation/animation.h \
             themeconfig/ThemeConfig.h \
             themeconfig/specs.h
  OTHER_FILES += kvantum.json
} else {
  SOURCES += qt4/Kvantum4.cpp \
             qt4/KvantumPlugin4.cpp \
             qt4/shortcuthandler4.cpp \
             qt4/x11wmmove4.cpp \
             qt4/windowmanager4.cpp \
             qt4/blurhelper4.cpp \
             qt4/ThemeConfig4.cpp
  HEADERS += qt4/Kvantum4.h \
             qt4/KvantumPlugin4.h \
             qt4/shortcuthandler4.h \
             qt4/x11wmmove4.h \
             qt4/windowmanager4.h \
             qt4/blurhelper4.h \
             qt4/ThemeConfig4.h \
             qt4/specs4.h
}

RESOURCES += themeconfig/defaulttheme.qrc

unix:!macx: LIBS += -lX11

unix {
  #VARIABLES
  isEmpty(PREFIX) {
    PREFIX = /usr
  }
  COLORSDIR =$$PREFIX/share/kde4/apps/color-schemes
  KFCOLORSDIR =$$PREFIX/share/color-schemes
  DATADIR =$$PREFIX/share

  DEFINES += DATADIR=\\\"$$DATADIR\\\"

  #MAKE INSTALL
  target.path = $$[QT_INSTALL_PLUGINS]/styles
  colors.path = $$COLORSDIR
  colors.files += ../color/Kvantum.colors
  kfcolors.path = $$KFCOLORSDIR
  kfcolors.files += ../color/Kvantum.colors
  lessThan(QT_MAJOR_VERSION, 5) {
    INSTALLS += target colors
  } else {
    lessThan(QT_MAJOR_VERSION, 6) {
      INSTALLS += target
    } else {
      INSTALLS += target kfcolors
    }
  }
}
