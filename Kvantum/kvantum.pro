SUBDIRS += style

equals(QT_MAJOR_VERSION, 6) {
  SUBDIRS += kvantumpreview \
             kvantummanager \
             themes
}

TEMPLATE = subdirs

CONFIG += qt \
          warn_on
