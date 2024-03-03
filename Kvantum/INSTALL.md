# INSTALL.md

Table of contents
=================
- [Prerequisites](#prerequisites)
- [Distributions](#distributions)
   - [Arch-based distributions](#arch-based-distributions)
   - [Debian-based distributions](#debian-based-distributions)
   - [Gentoo-based distributions](#gentoo-based-distributions)
   - [openSUSE](#opensuse)
     - [Leap](#leap)
     - [Tumbleweed](#tumbleweed)
   - [Red Hat based distributions](#red-hat-based-distributions)
   - [Solus](#solus)
   - [NixOS](#nixos)
- [Compilation](#compilation)
- [Installation](#installation)
- [Usage](#usage)
   - [Desktop environments](#desktop-environments)
       - [KDE](#in-kde)
       - [LXQt](#in-lxqt)
       - [Other DEs](#in-other-des)
   - [Using other themes](#using-other-themes)
   - [Blur effect (KWin)](#blur-effect-kwin)
   - [Notes for theme makers](#notes-for-theme-makers)
- [GTK](#gtk)

## Prerequisites

Before compiling Kvantum, you will need:

 * GCC
 * X11
 * Qt6

**See [Distributions](#distributions) for distro specific information on required packages and direct installation methods.**

## Distributions

### Arch-based distributions

If you want to compile Kvantum from its source, install the following packages:

 * `gcc` (or gcc-multilib for multilib systems)
 * `libx11` and `libxext` (for X11)
 * `qt6-base` and `qt6-svg` (for Qt6)
 * `kwindowsystem`
 * `qt6-tools` (for localization if you need it)

To install Kvantum directly, you have the choice to install the stable package **or**, preferably, the git package. Respectively, execute:

    sudo pacman -S kvantum

Or:

    yay -S kvantum-qt6-git

NOTE: `yay` only serves as an example here.

### Debian-based distributions

If you want to compile Kvantum from its source, install these packages:

 * `g++`
 * `libx11-dev` and `libxext-dev` (for X11)
 * `qt6-base-dev` and `qt6-svg-dev` (for Qt6)
 * `libkf6windowsystem-dev`
 * `qt6-base-dev-tools` (for localization if you need it)

In Ubuntu, you can install Kvantum directly with:

    sudo add-apt-repository ppa:papirus/papirus
    sudo apt update
    sudo apt install qt6-style-kvantum qt6-style-kvantum-themes

Since the PPA splits the package into `qt6-style-kvantum` and `qt6-style-kvantum-themes`, both of them should be installed.

### Gentoo-based distributions

Only if you are using a stable branch (e.g. [`KEYWORD="amd64"`](https://wiki.gentoo.org/wiki/KEYWORDS)), you will have to [unmask](https://wiki.gentoo.org/wiki/Knowledge_Base:Unmasking_a_package) `x11-themes/kvantum`. After that, execute:

    sudo emerge --ask --verbose x11-themes/kvantum

NOTE: All of Kvantum's dependencies will be automatically installed when emerging `x11-themes/kvantum`.

### openSUSE

If you want to compile Kvantum from its source, install these packages:

 * `gcc-c++`
 * `libX11-devel`
 * `libXext-devel`
 * `libqt6-qtbase-devel`
 * `libqt6-qtsvg-devel`
 * `libqt6-qttools-devel`
 * `kwindowsystem-devel`

#### Leap
see [Compilation](#compilation) on how to compile and install Kvantum.

#### Tumbleweed
Thanks to [trmdi](https://github.com/trmdi), you can install Kvantum directly, by executing:

    sudo zypper ar obs://home:trmdi trmdi
    sudo zypper in -r trmdi kvantum

### Red Hat based distributions

If you want to compile Kvantum from its source in Red Hat based distributions like Fedora, you will need these packages:

 * `gcc-c++`
 * `libX11-devel`
 * `libXext-devel`
 * `qt6-qtbase-devel`
 * `qt6-qtsvg-devel`
 * `qt6-qttools-devel`
 * `kf6-kwindowsystem-devel`

To install Kvantum directly, execute:

    sudo dnf install kvantum

### Solus

To compile Kvantum from source on Solus, you would need the `system.devel` component installed:

* `sudo eopkg install -c system.devel`

There are no pre-built Kvantum eopkg installers avaialble, so proceed to [Compilation](#compilation) to compile Kvantum yourself.

### NixOS

To install Qt5-based Kvantum on NixOS, add the following line to `configuration.nix`:

    environment.systemPackages = with pkgs; [ libsForQt5.qtstyleplugin-kvantum ];

If you want to compile Kvantum from its source, copy the Nix expression of Kvantum [here](https://github.com/NixOS/nixpkgs/blob/nixos-22.05/pkgs/development/libraries/qtstyleplugin-kvantum/default.nix), then paste it in a nix file, for example `kvantum.nix`, and put it in `/etc/nixos/`, then add the following line to `configuration.nix`:

    environment.systemPackages = with pkgs; [ (import ./kvantum.nix pkgs) ];


## Compilation

Open a terminal inside this folder and issue the following commands:

    mkdir build && cd build
    cmake ..
    make

In some distros, you may need to be more explicit and use `cmake .. -DCMAKE_INSTALL_PREFIX=/usr` for installing Kvantum under `/usr`, in contrast to `/usr/local`. The latter place may not be good in Linux distros and could cause troubles later.

NOTE 1. Kvantum needs `kwindowsystem` only to support some KWin effects (like blur effect). `kwindowsystem` is a small package which does not depend on KDE itself. However, if you do not need those effects, you could use `cmake .. -DWITHOUT_KF=ON`.

NOTE 2: You can also compile Kvantum's Qt5 plugin by using `cmake .. -DENABLE_QT5=ON`, but you need `qt5-x11extras` (or whatever the name of its dev package is in your distro) in addition to the Qt5 counterparts of the required packages.

## Installation

Use this command for installation:

    sudo make install

To compile Kvantum again (e.g., agaisnt Qt5), first remove the contents of the build directory.

## Usage

### Desktop environments

#### In KDE:

 1. Select Kvantum from *System Settings → Application Style → Widget Style* and apply it.
 2. Select Kvantum from *System Settings → Color → Scheme* and click Apply. You could change the color scheme later if you choose another Kvantum theme with *Kvantum Manager* (see "Using Other Themes" below).

Logging out and in would be good for Plasma to see the new theme.

#### In LXQt:

Just select *kvantum* from *Configuration Center → Appearance → Widget Style*. Kvantum Manager is also shown in Configuration Center for changing the Kvantum theme.

If the Wayland/X11 compositor you use gives shadows to menus and tooltips, make sure to disable that, or if that is not possible, check *Kvantum Manager → Configure Active Theme → Compositing & General Look → Shadowless menus and tooltips* as a workaround.

#### In Other DEs:

*NOTE: It is highly preferable that you install a Qt configuration utility (which provides a Qt plugin) and select Kvantum with it, instead of using the following methods directly.* Otherwise, there will be no guarantee that any style, other than Qt's default style, will work properly with all Qt applications.

That being said, you could try this command to run a Qt application APP with Kvantum:

    APP -style kvantum

To style all native Qt applications with Kvantum, set the environment variable `QT_STYLE_OVERRIDE` to `kvantum`. Most Wayland compositors have ways of setting environment variables. Under X11, you could add this line to your `~/.profile` or `~/.bashrc`:

    export QT_STYLE_OVERRIDE=kvantum

Under GNOME and desktop environments that do not source `~/.profile`, you could make a file like `~/.config/environment.d/qt.conf` with the following line in it and reboot:

    QT_STYLE_OVERRIDE=kvantum

Touching `/etc/environment` is *not* recommended.

### Using other themes

To select or install (as user) Kvantum themes, use *Kvantum Manager*, which is already installed and is available in the start menu as a utility app. It explains each step in a straightforward way. With it, you could not only switch between themes easily but also select themes for specific applications.

For the running parts of KDE/LXQt to recognize the new Kvantum theme, the easiest way is logging out and in again.

### Blur effect (KWin)

The blur effect of any compositor can be used with Kvantum when its active theme has translucent backgrounds. However, Kvantum can control KWin's blur effect with translucent menus and tooltips as well as explicitly translucent windows (like those of QTerminal, Konsole or LXQt Panel). Enabling blur options in *Kvantum Manager* has effect only when KWin's blur effect is enabled; otherwise, there will be no blurring or another compositor will control how translucent backgrounds are blurred.

In the case of compositors other than KWin, it is preferable that menus and tooltips have neither shadow nor blurring because those compositors cannot distinguish the shadows which Kvantum gives to menus and tooltips. But, if disabling menu/tooltip shadow and blurring is not possible with them, *Kvantum Manager → Configure Active Theme → Compositing & General Look → Shadowless menus and tooltips* could be checked as a workaround.

Under X11, make sure that you never have two compositors running together! KWin is highly recommended, whether with KDE or with LXQt. It supports both X11 and Wayland and has a decent blur effect.

### Notes for theme makers

If the KDE color scheme of the theme is inside its folder, *Kvantum Manager* will install it too. So, theme makers might want to put these files in their theme folder: `$THEME_NAME.svg`, `$THEME_NAME.kvconfig` and `$THEME_NAME.colors`.

The contents of theme folders (if valid) can also be installed manually in the user's home. The possible installation paths are `~/.config/Kvantum/$THEME_NAME/`, `~/.themes/$THEME_NAME/Kvantum/` and `~/.local/share/themes/$THEME_NAME/Kvantum/`, each one of which takes priority over the next one, i.e. if a theme is installed in more than one path, only the instance with the highest priority will be used by Kvantum.

Themes can also be packaged as deb, rpm, xz,... packages and installed as root:

  1. The possible root installation paths are `/usr/share/Kvantum/$THEME_NAME/` and `/usr/share/themes/$THEME_NAME/Kvantum/` (if `PREFIX=/usr`). The first path will take priority over the second one if a theme is installed in both.
  2. The KDE color schemes should go to  `/usr/share/color-schemes/`.
  3. If a theme is installed both as root and as user, the latter installation will take priority.

Please see [Theme-Making](doc/Theme-Making.pdf) for more information on theme installation paths and their priorities.

The default Qt installation adds several root themes, that can be selected by using *Kvantum Manager*. Their corresponding KDE color schemes are also installed.

## GTK

Kvantum does not — and will not — have any relation to GTK. However, it includes some themes similar to or matching GTK themes.
