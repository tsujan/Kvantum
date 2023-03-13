TEMPLATE = aux

unix {
  #VARIABLES
  isEmpty(PREFIX) {
    PREFIX = /usr
  }
  KVDIR = $$PREFIX/share/Kvantum
  KF5COLORSDIR = $$PREFIX/share/color-schemes

  #MAKE INSTALL
  QMAKE_INSTALL_DIR = cp -f -R --no-preserve=mode
  kv.path = $$KVDIR
  kv.files += ./kvthemes/*
  kf5colors.path = $$KF5COLORSDIR
  kf5colors.files += ./colors/*.colors
  INSTALLS += kv kf5colors
}
