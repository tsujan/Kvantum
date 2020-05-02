TEMPLATE = aux

unix {
  #VARIABLES
  isEmpty(PREFIX) {
    PREFIX = /usr
  }
  KVDIR = $$PREFIX/share/Kvantum
  KF5COLORSDIR = $$PREFIX/share/color-schemes
  OBDIR = $$PREFIX/share/themes

  #MAKE INSTALL
  QMAKE_INSTALL_DIR = cp -f -R --no-preserve=mode
  kv.path = $$KVDIR
  kv.files += ./kvthemes/*
  kf5colors.path = $$KF5COLORSDIR
  kf5colors.files += ./colors/*.colors
  ob.path = $$OBDIR
  ob.files += ./openbox/*
  INSTALLS += kv kf5colors ob
}
