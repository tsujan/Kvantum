TEMPLATE = aux

unix {
  #VARIABLES
  isEmpty(PREFIX) {
    PREFIX = /usr
  }
  KVDIR = $$PREFIX/share/Kvantum
  KFCOLORSDIR = $$PREFIX/share/color-schemes

  #MAKE INSTALL
  QMAKE_INSTALL_DIR = cp -f -R --no-preserve=mode
  kv.path = $$KVDIR
  kv.files += ./kvthemes/*
  kfcolors.path = $$KFCOLORSDIR
  kfcolors.files += ./colors/*.colors
  INSTALLS += kv kfcolors
}
