TEMPLATE = aux

unix {
  #VARIABLES
  isEmpty(PREFIX) {
    PREFIX = /usr
  }
  KVDIR = $$PREFIX/share/Kvantum
  COLORSDIR = $$PREFIX/share/kde4/apps/color-schemes
  KF5COLORSDIR = $$PREFIX/share/color-schemes

  #MAKE INSTALL
  kv.path = $$KVDIR
  kv.files += ./kvthemes/*
  colors.path = $$COLORSDIR
  colors.files += ./colors/*.colors
  kf5colors.path = $$KF5COLORSDIR
  kf5colors.files += ./colors/*.colors
  INSTALLS += kv colors kf5colors
}
