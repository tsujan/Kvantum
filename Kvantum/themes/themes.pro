TEMPLATE = aux

unix {
  #VARIABLES
  isEmpty(PREFIX) {
    PREFIX = /usr
  }
  KVDIR = $$PREFIX/share/Kvantum
  COLORSDIR = $$PREFIX/share/kde4/apps/color-schemes
  KF5COLORSDIR = $$PREFIX/share/color-schemes
  OBDIR = $$PREFIX/share/themes

  #MAKE INSTALL
  QMAKE_INSTALL_DIR = cp -f -R --no-preserve=mode
  kv.path = $$KVDIR
  kv.files += ./kvthemes/*
  colors.path = $$COLORSDIR
  colors.files += ./colors/*.colors
  kf5colors.path = $$KF5COLORSDIR
  kf5colors.files += ./colors/*.colors
  ob.path = $$OBDIR
  ob.files += ./openbox/*
  INSTALLS += kv colors kf5colors ob
}
