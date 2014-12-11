#-------------------------------------------------
#
# Project created by QtCreator 2014-10-12T19:55:24
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = kvantummanager
TEMPLATE = app


SOURCES += main.cpp \
    KvantumManager.cpp

RESOURCES += kvantummanager.qrc

HEADERS  += \
    KvantumManager.h

FORMS += \
    kvantummanager.ui

unix {
  #VARIABLES
  isEmpty(PREFIX) {
    PREFIX = /usr
  }
  BINDIR = $$PREFIX/bin
  DATADIR =$$PREFIX/share

  DEFINES += DATADIR=\\\"$$DATADIR\\\"

  #MAKE INSTALL
  iconsvg.path = $$DATADIR/pixmaps
  iconsvg.files += ../kvantumpreview/kvantum.svg

  desktop.path = $$DATADIR/applications
  desktop.files += ./data/$${TARGET}.desktop

  target.path =$$BINDIR
  INSTALLS += target desktop iconsvg
}
