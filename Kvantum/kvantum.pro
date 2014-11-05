SUBDIRS += style

equals(QT_MAJOR_VERSION, 4): SUBDIRS += kvantumpreview \
                                        kvantummanager

TEMPLATE = subdirs 

CONFIG += qt \
          warn_on
