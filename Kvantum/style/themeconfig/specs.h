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

#include <QStringList>
#if defined Q_WS_X11 || defined Q_OS_LINUX
#include "drag/windowmanager.h"
#endif

namespace Kvantum {
/* Generic information about a theme */
typedef struct {
  QString author;
  QString comment;
#if defined Q_WS_X11 || defined Q_OS_LINUX
  /* draggable from menubar, primary toolbar
     or anywhere possible (under x11)? */
  WindowManager::Drag x11drag;
#endif
  /* should some settings of the current DE be respected? */
  bool respect_DE;
  /* show mnemonics only if Alt is pressed? */
  bool alt_mnemonic;
  /* always activate view items on double clicking? */
  bool double_click;
  /* align tabs with the left edge?
     (by default, they are centered) */
  bool left_tabs;
  /* always center tabs in the document mode? */
  bool center_doc_tabs;
  /* join inactive tabs together?
     (by default, they are joined) */
  bool joined_inactive_tabs;
  /* attach the active tab to the tab widget or
     the tabbar base? (by default, it is detached) */
  bool attach_active_tab;
  /* when using tab separators, don't draw them for the active tab */
  bool no_active_tab_separator;
  /* number of pixels inactive tabs overlap the active one */
  int active_tab_overlap;
  /* mirror the top/left tab to draw the bottom/right tab also in
     the document mode? (By default, the bottom and right tabs are
     always the mirror images of the top and left tabs, respectively */
  bool mirror_doc_tabs;
  /* if tabs have frmae expansion, should only
     the frames of the active tab be expanded? */
  bool no_inactive_tab_expansion;
  /* raise and group neighbor
     toolbar buttons */
  bool group_toolbar_buttons;
  /* the space between toolbar items */
  int toolbar_item_spacing;
  /* the space between toolbar items and toolbar frame */
  int toolbar_interior_spacing;
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
  /* thin progressbars? */
  int progressbar_thickness;
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
  /* should buttons and comboboxes be animated
     under the cursor when their state changes? */
  bool animate_states;
  /* depth of menu shadows */
  int menu_shadow_depth;
  /* overlap between a submenu and its parent */
  int submenu_overlap;
  /* milliseconds to wait before opening a submenu
     (-1 means no popup, 0 means immediately) */
  int submenu_delay;
  /* depth of tooltip shadows */
  int tooltip_shadow_depth;
  /* splitter width */
  int splitter_width;
  /* scrollbar width */
  int scroll_width;
  /* minimum scrollbar extent */
  int scroll_min_extent;
  /* draw scroll arrows? */
  bool scroll_arrows;
  /* draw tree branch lines? */
  bool tree_branch_line;

  /* slider width */
  int slider_width;
  /* width and height of slider handle */
  int slider_handle_width;
  int slider_handle_length;

  /* width and height of checkbox
     and radio button indicators */
  int check_size;
  /* delay before showing tooltip (in milliseconds)*/
  int tooltip_delay;
  /* draw spin indicators vertically
     and inside the spin line-edit? */
  bool vertical_spin_indicators;
  /* draw spin indicators horizontally
     but inside the spin line-edit? */
  bool inline_spin_indicators;
  /* width of a horizontal spin button */
  int spin_button_width;
  /* draw an editable combobox as a lineedit with arrow? */
  bool combo_as_lineedit;
  /* use popup menu for combo popup */
  bool combo_menu;
  /* should popup menus support scrolling? */
  bool scrollable_menu;
  /* fill rubber band rectangle with highlight color? */
  bool fill_rubberband;
  /* put the groupbox label above the frame?
     (it's on the frame by default) */
  bool groupbox_top_label;
  /* shift the contents of a pushbutton when it's down?
     (the contennts are shifted by default) */
  bool button_contents_shift;
  /* draw scrollbars within view */
  bool scrollbar_in_view;

  int layout_spacing;
  int layout_margin;

  int small_icon_size;
  int large_icon_size;
  int button_icon_size;
  int toolbar_icon_size;
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
  QString tooltipBasetColor;
  QString textColor;
  QString windowTextColor;
  QString buttonTextColor;
  QString disabledTextColor;
  QString tooltipTextColor;
  QString highlightTextColor;
  QString linkColor;
  QString linkVisitedColor;
  QString progressIndicatorTextColor;
} color_spec;

/* Hacks */
typedef struct {
  /* don't draw any background or frame for Dolphin's
     view (nice when the window bg has a gradient) */
  bool transparent_dolphin_view;
  /* don't draw any background for PCManFM-qt's side pane */
  bool transparent_pcmanfm_sidepane;
  /* don't draw any background or frame for PCManFM-qt's view */
  bool transparent_pcmanfm_view;
  /* separate icon size for LXQT's main menus? */
  int lxqtmainmenu_iconsize;
  /* blur the region behind a window background that is
     explicitly made translucent by its app? */
  bool blur_translucent;
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
  /* show size grips as far as possible? */
  bool forceSizeGrip;
  /* tint icons with the highlight color
     on mouseover by this percentage */
  int tint_on_mouseover;
  /* don't tint icons with the highlight color on selecting them */
  bool no_selection_tint;
  /* set the opacity of disabled icons by this percentage */
  int disabled_icon_opacity;
  /* don't use bold font for default pushbuttons? */
  bool normal_default_pushbutton;
  /* no icon for pushbuttons as far as possible? */
  bool iconless_pushbutton;
  /* no icon for menuitems? */
  bool iconless_menu;
  /* only style the top toolbar? */
  bool single_top_toolbar;
} hacks_spec;

/* Generic information about a frame */
typedef struct {
  /* Element name */
  QString element;
  /* Element name for frame expansion */
  QString expandedElement;
  /* has frame? */
  bool hasFrame;
  /* Allow capsule grouping ? (used internally) */
  bool hasCapsule;
  /* frame size */
  int top,bottom,left,right;
  /* expanded frame size */
  int topExpanded,bottomExpanded,leftExpanded,rightExpanded;
  /* widget position in a capsule (used internally) */
  int capsuleH,capsuleV; // 0 -> middle, -1 -> left,top, 1 -> right,bottom, 2 -> left+right,top+bottom
  /* pattern size */
  int ps;
  /* if a widget's smallest dimension isn't greater than this,
     its frames (corners) will be expanded as far as possible */
  int expansion;
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
  fspec.expandedElement = QString();
  fspec.top = fspec.bottom = fspec.left = fspec.right = 0;
  fspec.topExpanded = fspec.bottomExpanded = fspec.leftExpanded = fspec.rightExpanded = 0;
  fspec.capsuleH = fspec.capsuleV = 0;
  fspec.ps = 0;
  fspec.expansion = 0;
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
#if defined Q_WS_X11 || defined Q_OS_LINUX
  tspec.x11drag = WindowManager::DRAG_ALL;
#endif
  tspec.respect_DE = true;
  tspec.alt_mnemonic = true;
  tspec.double_click = false;
  tspec.left_tabs = false;
  tspec.center_doc_tabs = false;
  tspec.joined_inactive_tabs = true;
  tspec.attach_active_tab = false;
  tspec.no_active_tab_separator = false;
  tspec.active_tab_overlap = 0;
  tspec.mirror_doc_tabs = true;
  tspec.no_inactive_tab_expansion = false;
  tspec.group_toolbar_buttons = false;
  tspec.toolbar_item_spacing = 0;
  tspec.toolbar_interior_spacing = 0;
  tspec.center_toolbar_handle = false;
  tspec.slim_toolbars = false;
  tspec.merge_menubar_with_toolbar = false;
  tspec.toolbutton_style = 0;
  tspec.spread_progressbar = false;
  tspec.progressbar_thickness = 0;
  tspec.menubar_mouse_tracking = true;
  tspec.composite = false;
  tspec.translucent_windows = false;
  tspec.opaque = QStringList() << "kscreenlocker" << "wine";
  tspec.blurring = false;
  tspec.popup_blurring = false;
  tspec.animate_states = false;
  tspec.menu_shadow_depth = 0;
  tspec.submenu_overlap = -1;
  tspec.submenu_delay = 250;
  tspec.tooltip_shadow_depth = 0;
  tspec.splitter_width = 7;
  tspec.scroll_width = 12;
  tspec.scroll_min_extent = 36;
  tspec.scroll_arrows=true;
  tspec.tree_branch_line=false;
  tspec.slider_width = 8;
  tspec.slider_handle_width = 16;
  tspec.slider_handle_length = 16;
  tspec.check_size = 13;
  tspec.tooltip_delay = -1;
  tspec.vertical_spin_indicators = false;
  tspec.inline_spin_indicators = false;
  tspec.spin_button_width = 16;
  tspec.combo_as_lineedit = false;
  tspec.combo_menu = false;
  tspec.scrollable_menu = false;
  tspec.fill_rubberband = false;
  tspec.groupbox_top_label = false;
  tspec.button_contents_shift = true;
  tspec.scrollbar_in_view = false;
  tspec.layout_spacing = 2;
  tspec.layout_margin = 4;
  tspec.small_icon_size = 16;
  tspec.large_icon_size = 32;
  tspec.button_icon_size = 16;
  tspec.toolbar_icon_size = 22;
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
  cspec.tooltipBasetColor = QString();
  cspec.textColor = QString();
  cspec.windowTextColor = QString();
  cspec.buttonTextColor = QString();
  cspec.disabledTextColor = QString();
  cspec.tooltipTextColor = QString();
  cspec.highlightTextColor = QString();
  cspec.linkColor = QString();
  cspec.linkVisitedColor = QString();
  cspec.progressIndicatorTextColor = QString();
}

/* Fills the hacks spec with default values */
static inline void default_hacks_spec(hacks_spec &hspec) {
  hspec.transparent_dolphin_view = false;
  hspec.transparent_pcmanfm_sidepane = false;
  hspec.transparent_pcmanfm_view = false;
  hspec.lxqtmainmenu_iconsize = 0;
  hspec.blur_translucent = false;
  hspec.transparent_ktitle_label = false;
  hspec.transparent_menutitle = false;
  hspec.kcapacitybar_as_progressbar = false;
  hspec.respect_darkness = false;
  hspec.forceSizeGrip = false;
  hspec.tint_on_mouseover = 0;
  hspec.no_selection_tint = false;
  hspec.disabled_icon_opacity = 100;
  hspec.normal_default_pushbutton = false;
  hspec.iconless_pushbutton = false;
  hspec.iconless_menu = false;
  hspec.single_top_toolbar = false;
}
}

#endif
