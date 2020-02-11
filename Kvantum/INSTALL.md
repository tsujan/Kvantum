# INSTALL.md

Table of contents
=================
 * [Prerequisites](#prerequisites)
 * [Distributions](#distributions)
    * [Arch-based distributions](#arch-based-distributions)
    * [Debian-based distributions](#debian-based-distributions)
    * [Gentoo-based distributions](#gentoo-based-distributions)
    * [openSUSE](#opensuse)
    * [Red Hat based distributions](#red-hat-based-distributions)

 * [Compilation](#compilation)
    * [qmake](#with-qmake)
    * [cmake](#with-cmake)
 * [Installation](#installation)
 * [Usage](#usage)
    * [Desktop environments](#desktop-environments)
        * [KDE](#in-kde)
        * [LXQt](#in-lxqt)
        * [Other DEs](#in-other-des)
    * [Using other themes](#using-other-themes)
    * [Blur effect (KWin)](#blur-effect-kwin)
    * [Notes for theme makers](#notes-for-theme-makers)
 * [GTK](#gtk)

## Prerequisites

Before installing Kvantum, you will need:

 * GCC
 * X11
 * Qt5 development packages

**See in [#Distributions](#distributions) to install those packages.**

## Distributions

### Arch-based distributions

Before installing Kvantum, install the required packages:

 * `gcc` (or gcc-multilib for multilib systems)
 * `libx11` and `libxext` (for X11)
 * `qt5-base`, `qt5-svg` and `qt5-x11extras` (for Qt5)
 * `kwindowsystem` (required with Qt >= 5.11)
 * `qt5-tools` (for localization if you need it)

Then, to install Kvantum, you have the choice to install the stable package **or** the git package. Respectively, execute:

    sudo pacman -S kvantum-qt5

Or:

    yay -S kvantum-qt5-git

NOTE: `yay` only serves as an example, as it is the most common used pacman wrapper.

### Debian-based distributions

Before installing Kvantum, install the required packages:

 * `g++`
 * `libx11-dev` and `libxext-dev` (for X11)
 * `qtbase5-dev`, `libqt5svg5-dev` and `libqt5x11extras5-dev` (for Qt5)
 * `libkf5windowsystem-dev` (required with Qt >= 5.11)
 * `qttools5-dev-tools` (for localization if you need it)

Then, to install Kvantum, execute:

    sudo apt install qt5-style-kvantum

### Gentoo-based distributions

If using the stable branch (e.g. `amd64`), you will have to [unmask](https://wiki.gentoo.org/wiki/Knowledge_Base:Unmasking_a_package) `x11-themes/kvantum`, and then execute:

    sudo emerge --ask --verbose x11-themes/kvantum

### openSUSE

Before installing Kvantum, install the required packages:

 * `gcc-c++`
 * `libX11-devel`
 * `libXext-devel`
 * `libqt5-qtx11extras-devel`
 * `libqt5-qtbase-devel`
 * `libqt5-qtsvg-devel`
 * `libqt5-qttools-devel`
 * `kwindowsystem-devel` (required with Qt >= 5.11)

**Only for Tumblweed users!** Thanks to [trmdi](https://github.com/trmdi), you can install Kvantum, by executing:

    sudo zypper ar obs://home:trmdi trmdi
    sudo zypper in -r trmdi kvantum

For Leap users, see [#Compilation](#compilation) on how to compile and install Kvantum.

### Red Hat based distributions

Before installing Kvantum in Red Hat based distributions like Fedora, install the required packages:

 * `gcc-c++`
 * `libX11-devel`
 * `libXext-devel`
 * `qt5-qtx11extras-devel`
 * `qt5-qtbase-devel`
 * `qt5-qtsvg-devel`
 * `qt5-qttools-devel`
 * `kf5-kwindowsystem-devel` (required with Qt >= 5.11)

Then, to install Kvantum, execute:

    sudo dnf install kvantum

## Compilation

There are two ways to compile Kvantum: with `qmake` or with `cmake`.

### With qmake

Just open a terminal inside this folder and issue the following command:

    qmake && make

With some distros, you might need to put the full path of `qmake` in the above command.

### With cmake

Open a terminal inside this folder and issue the following commands:

    mkdir build && cd build
    cmake ..
    make

If you want to install Kvantum in a nonstandard path (which is not recommended), you could add the option `-DCMAKE_INSTALL_PREFIX=YOUR_SELECTED_PATH` to the `cmake` command.

## Installation

Then, use this command for installation:

    sudo make install

If you have compiled Kvantum with `qmake`, the following command cleans the source completely and makes it ready for another compilation:

    make distclean

If you have used `cmake` for compilation, to compile Kvantum again, first remove the contents of the build directory.

## Usage

### Desktop environments

#### In KDE:

 1. Select Kvantum from *System Settings → Application Style → Widget Style* and apply it.
 2. Select Kvantum from *System Settings → Color → Scheme* and click Apply. You could change the color scheme later if you choose another Kvantum theme with *Kvantum Manager* (see "Using Other Themes" below).

Logging out and in would be good for Plasma to see the new theme.

#### In LXQt:

Just select *kvantum* from *Configuration Center → Appearance → Widget Style*. Kvantum Manager is also shown in Configuration Center for changing the Kvantum theme.

In case you use Compton as the X compositor (not recommended), be sure to disable its shadow and blurring for composited Qt menus with lines like these in `~/.config/compton.conf`:

    shadow-exclude = [ "argb && (_NET_WM_WINDOW_TYPE@:a *= 'MENU' || _NET_WM_WINDOW_TYPE@:a *= 'COMBO')" ];
    blur-background-exclude = [ "(_NET_WM_WINDOW_TYPE@:a *= 'MENU' || _NET_WM_WINDOW_TYPE@:a *= 'COMBO')" ];

#### In Other DEs:

In desktop environments that do not have a Qt5 configuration utility, you could use this command to run a Qt5 application APP:

    APP -style kvantum

Or, better, set the environment variable `QT_STYLE_OVERRIDE` to `kvantum`. For example, you could add this line to your `~/.profile` or `~/.bashrc`:

    export QT_STYLE_OVERRIDE=kvantum

If the display manager you use does not source `~/.profile`, you could probably add the above line to `~/.xsessionrc`. The last resort is `/etc/environment` but most desktop environments have GUI tools for setting environment variables.

### Using other themes

To select or install (as user) Kvantum themes, use *Kvantum Manager*, which is already installed and is available in the start menu as a utility app. It explains each step in a straightforward way. With it, you could not only switch between themes easily but also select themes for specific applications.

For the running parts of KDE/LXQt to recognize the new Kvantum theme, the easiest way is logging out and in again.

### Blur effect (KWin)

The blur effect of any compositor can be used with Kvantum when its active theme has translucent backgrounds. However, Kvantum can control KWin's blur effect with translucent menus and tooltips as well as explicitly translucent windows (like those of QTerminal, Konsole or LXQt Panel). Enabling blur options in *Kvantum Manager* has effect only when KWin's blur effect is enabled; otherwise, there will be no blurring or another compositor will control how translucent backgrounds are blurred.

In the case of compositors other than KWin, it is preferable that menus and tooltips have neither shadow nor blurring because those compositors cannot distinguish the shadows that Kvantum gives to menus and tooltips. But, if disabling menu/tooltip shadow and blurring is not possible with them, *Kvantum Manager → Compositing & General Look → Shadowless menus and tooltips* could be checked as a workaround.

Make sure that you never have two compositors running together! KWin is highly recommended, whether with KDE or with LXQt. It supports both X11 and Wayland and has a decent blur effect.

### Notes for theme makers

If the KDE color scheme of the theme is inside its folder, *Kvantum Manager* will install it too. So, theme makers might want to put these files in their theme folder: `$THEME_NAME.svg`, `$THEME_NAME.kvconfig` and `$THEME_NAME.colors`.

The contents of theme folders (if valid) can also be installed manually in the user's home. The possible installation paths are `~/.config/Kvantum/$THEME_NAME/`, `~/.themes/$THEME_NAME/Kvantum/` and `~/.local/share/themes/$THEME_NAME/Kvantum/`, each one of which takes priority over the next one, i.e. if a theme is installed in more than one path, only the instance with the highest priority will be used by Kvantum.

Themes can also be packaged as deb, rpm, xz,... packages and installed as root:

  1. The possible root installation paths are `/usr/share/Kvantum/$THEME_NAME/` and `/usr/share/themes/$THEME_NAME/Kvantum/` (if `PREFIX=/usr`). The first path will take priority over the second one if a theme is installed in both.
  2. The KDE color schemes should go to  `/usr/share/color-schemes/`.
  3. If a theme is installed both as root and as user, the latter installation will take priority.

Please see "Theme-Making.pdf" for more information on theme installation paths and their priorities.

The default Qt5 installation adds several root themes, that can be selected by using *Kvantum Manager*. Their corresponding KDE color schemes are also installed.

## GTK

Kvantum does not — and will not — have any relation to GTK. However, it includes some themes similar to or matching GTK themes.
