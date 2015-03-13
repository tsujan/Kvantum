// Adapted from QuantumStyle (C) Saïd LANKRI (not developed anymore but
// continued as QSvgStyle <https://github.com/DexterMagnific/QSvgStyle>)

/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014 <tsujan2000@gmail.com>
 * 
 * Kvantum is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Kvantum is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SPEC_H
#define SPEC_H

//#include <QString>
#include <QStringList>
//#include <QDebug>

/* Generic information about a theme */
typedef struct {
  QString author;
  QString comment;
  /* draggable from anywhere possible (under x11) */
  bool x11drag;
  /* show mnemonics only if Alt is pressed? */
  bool alt_mnemonic;
  /* always activate view items on double clicking? */
  bool double_click;
  /* align tabs with the left edge?
     (by default, they are centered) */
  bool left_tabs;
  /* join tabs together?
     (by default, they are detached) */
  bool joined_tabs;
  /* attach the active tab to the tab widget?
     (by default, it is detached) */
  bool attach_active_tab;
  /* raise and group neighbor
     toolbar buttons */
  bool group_toolbar_buttons;
  /* center toolbar handle
     instead of scaling it vertically? */
  bool center_toolbar_handle;
  /* 16-px icons for toolbars by default? */
  bool slim_toolbars;
  /* merge the background of menubar with
     that of toolbar when they're adjacent? */
  bool merge_menubar_with_toolbar;
  /* 0 -> follow, 1-> icon only 2 -> text only,
     3 -> text beside icon, 4 -> text under icon */
  int toolbutton_style;
  /* let progress indicator spread across the whole
     progress groove and not just its interior */
  bool spread_progressbar;
  /* no percentage label for progressbars? */
  bool textless_progressbar;
  /* enable mouse tracking in menubars? */
  bool menubar_mouse_tracking;
  /* use composite effects
     (for now, only translucent menus
     or tooltips and their shadows) */
  bool composite;
  /* do we have translucent windows and dialogs? */
  bool translucent_windows;
  /* list of apps that shouldn't have window translucency */
  QStringList opaque;
  /* blur what is behind translucent windows if possible? */
  bool blurring;
  /* blur what is behind menus/tooltips if possible? */
  bool popup_blurring;
  /* depth of menu shadows */
  int menu_shadow_depth;
  /* depth of tooltip shadows */
  int tooltip_shadow_depth;
  /* splitter width */
  int splitter_width;
  /* scrollbar width */
  int scroll_width;

  /* slider width */
  int slider_width;
  /* width and height of slider handle */
  int slider_handle_width;
  int slider_handle_length;

  /* width and height of checkbox
     and radio button indicators */
  int check_size;
  /* draw spin indicators vertically
     and inside the spin line-edit? */
  bool vertical_spin_indicators;
} theme_spec;

/* General colors */
typedef struct {
  QString windowColor;
  QString baseColor;
  QString altBaseColor;
  QString buttonColor;
  QString lightColor;
  QString midLightColor;
  QString darkColor;
  QString midColor;
  QString shadowColor;
  QString highlightColor;
  QString inactiveHighlightColor;
  QString textColor;
  QString windowTextColor;
  QString buttonTextColor;
  QString disabledTextColor;
  QString tooltipTextColor;
  QString highlightTextColor;
  QString linkColor;
  QString linkVisitedColor;
} color_spec;

/* Hacks */
typedef struct {
  /* don't draw any background or frame for Dolphin's
     view (nice when the window bg has a gradient) */
  bool transparent_dolphin_view;
  /* blur the region behind Konsole's transparent background? */
  bool blur_konsole;
  /* transparent background for the label of KTitleWidget
     (nice when the window bg has a gradient) */
  bool transparent_ktitle_label;
  /* transparent background for (KDE) menu titles */
  bool transparent_menutitle;
  /* draw KCapacityBars as progressbars? */
  bool kcapacitybar_as_progressbar;
  /* Some apps don't respect dark themes.
     Fix that as far as possible! */
  bool respect_darkness;
} hacks_spec;

/* Generic information about a frame */
typedef struct {
  /* Element name */
  QString element;
  /* has frame? */
  bool hasFrame;
  /* Allow capsule grouping ? (used internally) */
  bool hasCapsule;
  /* frame size */
  int top,bottom,left,right;
  /* widget position in a capsule (used internally) */
  int capsuleH,capsuleV; // 0 -> middle, -1 -> left,top, 1 -> right,bottom, 2 -> left+right,top+bottom
} frame_spec;

/* Generic information about a frame interior */
typedef struct {
  /* Element name */
  QString element;
  /* has interior */
  bool hasInterior;
  /* pattern size */
  int px,py;
} interior_spec;

/* Generic information about widget indicators */
typedef struct {
  /* Element name */
  QString element;
  /* size */
  int size;
} indicator_spec;

/* Generic information about the size of a widget */
typedef struct {
  /* min width or height. <= 0 : does not apply */
  int minH;
  int minW;
} size_spec;

/* Generic information about text and icons (labels) */
typedef struct {
  /* normal text color */
  QString normalColor;
  /* focused text color */
  QString focusColor;
  /* pressed text color */
  QString pressColor;
  /* toggled text color */
  QString toggleColor;
  /* use bold font? */
  bool boldFont;
  /* use italic font? */
  bool italicFont;
  /* has shadow */
  bool hasShadow;
  /* shadow shift */
  int xshift,yshift;
  /* shadow color */
  QString shadowColor;
  /* shadow alpha */
  int a;
  /* shadow depth */
  int depth;
  /* has margins ? */
  bool hasMargin;
  /* text margins */
  int top,bottom,left,right;
  /* text-icon spacing */
  int tispace;
} label_spec;

/* Fills the frame spec with default values */
static inline void default_frame_spec(frame_spec &fspec) {
  fspec.hasFrame = false;
  fspec.hasCapsule = false; // may change to true in Kvantum.cpp
  fspec.element = QString();
  fspec.top = fspec.bottom = fspec.left = fspec.right = 0;
  fspec.capsuleH = fspec.capsuleV = 0;
}

/* Fills the interior with default values */
static inline void default_interior_spec(interior_spec &ispec) {
  ispec.hasInterior = true;
  ispec.element = QString();
  ispec.px = ispec.py = 0;
}

/* Fills the indicator spec with default values */
static inline void default_indicator_spec(indicator_spec &dspec) {
  dspec.element = QString();
  dspec.size = 15;
}

/* Fills the label spec with default values */
static inline void default_label_spec(label_spec &lspec) {
  lspec.normalColor = QString();
  lspec.focusColor = QString();
  lspec.pressColor = QString();
  lspec.toggleColor = QString();
  lspec.boldFont = false;
  lspec.italicFont = false;
  lspec.hasShadow = false;
  lspec.xshift = 0;
  lspec.yshift = 1;
  lspec.shadowColor = QString("#000000");
  lspec.a = 255;
  lspec.depth = 1;
  lspec.hasMargin = false;
  lspec.top = lspec.bottom = lspec.left = lspec.right = 0;
  lspec.tispace = 0;
}

/* Fills the size spec with default values */
static inline void default_size_spec(size_spec &sspec) {
  sspec.minH = sspec.minW = -1;
}

/* Fills the widget spec with default values */
static inline void default_theme_spec(theme_spec &tspec) {
  tspec.author = QString();
  tspec.comment = QString();
  tspec.x11drag = false;
  tspec.alt_mnemonic = false;
  tspec.double_click = false;
  tspec.left_tabs = false;
  tspec.joined_tabs = false;
  tspec.attach_active_tab = false;
  tspec.group_toolbar_buttons = false;
  tspec.center_toolbar_handle = false;
  tspec.slim_toolbars = false;
  tspec.merge_menubar_with_toolbar = false;
  tspec.toolbutton_style = 0;
  tspec.spread_progressbar = false;
  tspec.textless_progressbar = false;
  tspec.menubar_mouse_tracking = true;
  tspec.composite = false;
  tspec.translucent_windows = false;
  tspec.opaque = QStringList() << "kscreenlocker" << "wine";
  tspec.blurring = false;
  tspec.popup_blurring = false;
  tspec.menu_shadow_depth = 0;
  tspec.tooltip_shadow_depth = 0;
  tspec.splitter_width = 7;
  tspec.scroll_width = 12;
  tspec.slider_width = 8;
  tspec.slider_handle_width = 16;
  tspec.slider_handle_length = 16;
  tspec.check_size = 13;
  tspec.vertical_spin_indicators = false;
}

static inline void default_color_spec(color_spec &cspec) {
  cspec.windowColor = QString();
  cspec.baseColor = QString();
  cspec.altBaseColor = QString();
  cspec.buttonColor = QString();
  cspec.lightColor = QString();
  cspec.midLightColor = QString();
  cspec.darkColor = QString();
  cspec.midColor = QString();
  cspec.shadowColor = QString("#000000");
  cspec.highlightColor = QString();
  cspec.inactiveHighlightColor = QString();
  cspec.textColor = QString();
  cspec.windowTextColor = QString();
  cspec.buttonTextColor = QString();
  cspec.disabledTextColor = QString();
  cspec.tooltipTextColor = QString();
  cspec.highlightTextColor = QString();
  cspec.linkColor = QString();
  cspec.linkVisitedColor = QString();
}

/* Fills the hacks spec with default values */
static inline void default_hacks_spec(hacks_spec &hspec) {
  hspec.transparent_dolphin_view = false;
  hspec.blur_konsole = false;
  hspec.transparent_ktitle_label = false;
  hspec.transparent_menutitle = false;
  hspec.kcapacitybar_as_progressbar = false;
  hspec.respect_darkness = false;
}

#endif
