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

#include <QSettings>
#include <QFile>
#include <QApplication>
#include "ThemeConfig.h"
#if defined Q_WS_X11 || defined Q_OS_LINUX
#include <QX11Info>
#if QT_VERSION >= 0x050000
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif
#endif

namespace Kvantum {
ThemeConfig::ThemeConfig(const QString& theme) :
  settings_(NULL),
  parentConfig_(NULL)
{
  /* For now, the lack of x11 means wayland.
     Later, a better method should be found. */
#if defined Q_WS_X11 || defined Q_OS_LINUX
#if QT_VERSION < 0x050200
  isX11_ = true;
#else
  isX11_ = QX11Info::isPlatformX11();
#endif
#else
  isX11_ = false;
#endif

  load(theme);
}

ThemeConfig::~ThemeConfig()
{
  if (settings_)
    delete settings_;
}

void ThemeConfig::load(const QString& theme)
{
  if (settings_)
  {
    delete settings_;
    settings_ = NULL;
  }

  if (!QFile::exists(theme))
    return;

  settings_ = new QSettings(theme,QSettings::NativeFormat);
}

QVariant ThemeConfig::getValue(const QString& group, const QString& key) const
{
  QVariant r;

  if (group.isNull() || group.isEmpty() || key.isNull() || key.isEmpty())
    return r;

  if (settings_)
  {
    settings_->beginGroup(group);
    r = settings_->value(key);
    settings_->endGroup();
  }

  return r;
}

QVariant ThemeConfig::getValue(const QString& group, const QString& key, const QString &inherits) const
{
  QVariant r;

  r = getValue(group, key);
  if (r.isValid())
    return r;

  QString i = inherits;
  QStringList l;
  while (!i.isEmpty())
  {
    r = getValue(i, key);
    if (r.isValid())
      return r;
    l << i;
    i = getValue(i, "inherits").toString();
    // no infinite loop
    if (l.contains(i))
      break;
  }

  /* go to the parent config if this key isn't found here
     but leave the text color to be set by the color scheme */
  if (parentConfig_
      && !key.contains(".normal.") && !key.contains(".focus.") && !key.contains(".press.") && !key.contains(".toggle.")
      && key != "text.bold" && key != "text.italic")
  {
    i = parentConfig_->getValue(group, "inherits").toString();
    r = parentConfig_->getValue(group, key, i);
  }

  return r;
}

frame_spec ThemeConfig::getFrameSpec(const QString &elementName)
{
  if (fSpecs_.contains(elementName))
    return fSpecs_[elementName];

  frame_spec r;
  default_frame_spec(r);

  QVariant v = getValue(elementName, "inherits");
  QString i = v.toString();

  /* except for text colors and indicator, frame and interior elements,
     ToolbarButton gets all of its variables from PanelButtonTool */
  QString name = elementName;
  if (name == "ToolbarButton")
    name = "PanelButtonTool";

  v = getValue(name,"frame", i);
  r.hasFrame = v.toBool();
  if (r.hasFrame)
  {
    v = getValue(name,"focusFrame", i);
    r.hasFocusFrame = v.toBool();

    v = getValue(elementName, "frame.element", i);
    if (!v.toString().isEmpty())
    {
      r.element = v.toString();

      if (elementName == "ToolbarButton")
        i = getValue(name, "inherits").toString();

      v = getValue(name,"frame.top", i);
      r.top = qMax(v.toInt(),0);
      v = getValue(name,"frame.bottom", i);
      r.bottom = qMax(v.toInt(),0);
      v = getValue(name,"frame.left", i);
      r.left = qMax(v.toInt(),0);
      v = getValue(name,"frame.right", i);
      r.right = qMax(v.toInt(),0);

      v = getValue(name,"frame.patternsize", i);
      r.ps = qMax(v.toInt(),0);

      if (r.top || r.bottom || r.left || r.right)
      {
        v = getValue(name,"frame.expansion", i);
        if (v.isValid())
        {
          QString value = v.toString();
          if (value.endsWith("font"))
          { // multiply by the app font height
            r.expansion = qMax(value.left(value.length()-4).toFloat(), 0.0f)
                          * QFontMetrics(QApplication::font()).height();
          }
          else
            r.expansion = qMax(v.toInt(),0);
        }

        if (r.expansion > 0)
        {
          v = getValue(name,"frame.expanded.top", i);
          if (v.isValid())
          {
            r.topExpanded = qMin(v.toInt(),r.top);
            if (r.topExpanded < 0) r.topExpanded = r.top;
          }
          else r.topExpanded = r.top;
          v = getValue(name,"frame.expanded.bottom", i);
          if (v.isValid())
          {
            r.bottomExpanded = qMin(v.toInt(),r.bottom);
            if (r.bottomExpanded < 0) r.bottomExpanded = r.bottom;
          }
          else r.bottomExpanded = r.bottom;
          v = getValue(name,"frame.expanded.left", i);
          if (v.isValid())
          {
            r.leftExpanded = qMin(v.toInt(),r.left);
            if (r.leftExpanded < 0) r.leftExpanded = r.left;
          }
          else r.leftExpanded = r.left;
          v = getValue(name,"frame.expanded.right", i);
          if (v.isValid())
          {
            r.rightExpanded = qMin(v.toInt(),r.right);
            if (r.rightExpanded < 0) r.rightExpanded = r.right;
          }
          else r.rightExpanded = r.right;
        }
      }
    }
    v = getValue(elementName, "frame.expandedElement");
    r.expandedElement = v.toString();
  }

  fSpecs_[elementName] = r;
  return r;
}

interior_spec ThemeConfig::getInteriorSpec(const QString &elementName)
{
  if (iSpecs_.contains(elementName))
    return iSpecs_[elementName];

  interior_spec r;
  default_interior_spec(r);

  QVariant v = getValue(elementName, "inherits");
  QString i = v.toString();

  QString name = elementName;
  if (name == "ToolbarButton")
    name = "PanelButtonTool";

  v = getValue(name,"interior", i);
  r.hasInterior = v.toBool();

  if (r.hasInterior)
  {
    v = getValue(name,"focusInterior", i);
    r.hasFocusInterior = v.toBool();

    v = getValue(elementName, "interior.element", i);
    if (!v.toString().isEmpty())
    {
      r.element = v.toString();

      if (elementName == "ToolbarButton")
        i = getValue(name, "inherits").toString();

      v = getValue(name,"interior.x.patternsize", i);
      r.px = qMax(v.toInt(),0);
      v = getValue(name,"interior.y.patternsize", i);
      r.py = qMax(v.toInt(),0);
    }
  }

  iSpecs_[elementName] = r;
  return r;
}

indicator_spec ThemeConfig::getIndicatorSpec(const QString &elementName)
{
  if (dSpecs_.contains(elementName))
    return dSpecs_[elementName];

  indicator_spec r;
  default_indicator_spec(r);

  QVariant v = getValue(elementName, "inherits");
  QString i = v.toString();

  v = getValue(elementName, "indicator.element", i);
  if (!v.toString().isEmpty())
    r.element = v.toString();

  /* ToolbarButton gets its indicator size from PanelButtonTool */
  QString name = elementName;
  if (name == "ToolbarButton")
  {
    name = "PanelButtonTool";
    i = getValue(name, "inherits").toString();
  }
  v = getValue(name,"indicator.size", i);
  if (v.isValid()) // 15 by default
    r.size = qMax(v.toInt(),0);

  dSpecs_[elementName] = r;
  return r;
}

label_spec ThemeConfig::getLabelSpec(const QString &elementName)
{
  if (lSpecs_.contains(elementName))
    return lSpecs_[elementName];

  label_spec r;
  default_label_spec(r);

  QVariant v = getValue(elementName, "inherits");
  QString i = v.toString();

  /* LineEdit is excluded for its size calculation to be correct */
  if (elementName != "LineEdit")
  {
    v = getValue(elementName,"text.shadow", i);
    r.hasShadow = v.toBool();

    v = getValue(elementName,"text.normal.color", i);
    r.normalColor = v.toString();

    v = getValue(elementName,"text.normal.inactive.color", i);
    r.normalInactiveColor = v.toString();

    v = getValue(elementName,"text.focus.color", i);
    r.focusColor = v.toString();

    v = getValue(elementName,"text.focus.inactive.color", i);
    r.focusInactiveColor = v.toString();

    if (elementName == "MenuItem" || elementName == "MenuBarItem")
    { // no inheritance because the (fallback) focus color seems more natural
      v = getValue(elementName,"text.press.color");
      r.pressColor = v.toString();

      v = getValue(elementName,"text.toggle.color");
      r.toggleColor = v.toString();
    }
    else
    {
      v = getValue(elementName,"text.press.color", i);
      r.pressColor = v.toString();

      v = getValue(elementName,"text.press.inactive.color", i);
      r.pressInactiveColor = v.toString();

      v = getValue(elementName,"text.toggle.color", i);
      r.toggleColor = v.toString();

      v = getValue(elementName,"text.toggle.inactive.color", i);
      r.toggleInactiveColor = v.toString();
    }

    v = getValue(elementName,"text.bold", i);
    r.boldFont = v.toBool();

#if QT_VERSION >= 0x050000
    v = getValue(elementName,"text.boldness", i);
    if (v.isValid()) // QFont::Bold by default
    {
      int b = qMin(qMax(v.toInt(),1),5);
      switch (b) {
        case 1:
          r.boldness = QFont::Medium;
          break;
        case 2:
          r.boldness = QFont::DemiBold;
          break;
        case 3:
          r.boldness = QFont::Bold;
          break;
        case 4:
          r.boldness = QFont::ExtraBold;
          break;
        case 5:
          r.boldness = QFont::Black;
          break;
        default:
          r.boldness = QFont::Bold;
      }
    }
#endif

    v = getValue(elementName,"text.italic", i);
    r.italicFont = v.toBool();

    if (r.hasShadow)
    {
      v = getValue(elementName,"text.shadow.xshift", i);
      r.xshift = v.toInt();
      v = getValue(elementName,"text.shadow.yshift", i);
      if (v.isValid())
        r.yshift = v.toInt();
      v = getValue(elementName,"text.shadow.color", i);
      if (v.isValid())
        r.shadowColor = v.toString();
      v = getValue(elementName,"text.inactive.shadow.color", i);
      if (v.isValid())
        r.inactiveShadowColor = v.toString();
      v = getValue(elementName,"text.shadow.alpha", i);
      if (v.isValid())
        r.a = qMax(v.toInt(),0);
      v = getValue(elementName,"text.shadow.depth", i);
      if (v.isValid())
        r.depth = qMax(v.toInt(),0);
    }
  }

  QString name = elementName;
  if (name == "ToolbarButton")
  {
    name = "PanelButtonTool";
    i = getValue(name, "inherits").toString();
  }

  v = getValue(name,"text.margin", i);
  r.hasMargin = v.toBool();
  if (r.hasMargin)
  {
    v = getValue(name,"text.margin.top", i);
    r.top = qMax(v.toInt(),0);
    v = getValue(name,"text.margin.bottom", i);
    r.bottom = qMax(v.toInt(),0);
    v = getValue(name,"text.margin.left", i);
    r.left = qMax(v.toInt(),0);
    v = getValue(name,"text.margin.right", i);
    r.right = qMax(v.toInt(),0);

    /* let's be more precise */
    if(name == "LineEdit")
    {
      r.top = qMax(0,r.top-1);
      r.bottom = qMax(0,r.bottom-1);
    }
    else if (name == "PanelButtonCommand"
             || name == "PanelButtonTool"
             || name == "ComboBox")
    {
      r.left = qMax(0,r.left-1);
      r.right = qMax(0,r.right-1);
      r.top = qMax(0,r.top-1);
      r.bottom = qMax(0,r.bottom-1);
    }
  }

  v = getValue(name,"text.iconspacing", i);
  r.tispace = qMax(v.toInt(),0);

  lSpecs_[elementName] = r;
  return r;
}

size_spec ThemeConfig::getSizeSpec(const QString& elementName)
{
  if (sSpecs_.contains(elementName))
    return sSpecs_[elementName];

  size_spec r;
  default_size_spec(r);

  QString name = elementName;
  if (name == "ToolbarButton")
    name = "PanelButtonTool";

  QVariant v = getValue(name, "inherits");
  QString i = v.toString();

  v = getValue(name,"min_height", i);
  if (v.isValid())
  {
    QString value = v.toString();
    if (value.startsWith("+"))
      r.incrementH = true;
    if (value.endsWith("font"))
    { // multiply by the app font height
      r.minH = qMax(value.left(value.length()-4).toFloat(), 0.0f)
               * QFontMetrics(QApplication::font()).height();
    }
    else
      r.minH = qMax(v.toInt(),0);
  }

  v = getValue(name,"min_width", i);
  if (v.isValid())
  {
    QString value = v.toString();
    if (value.startsWith("+"))
      r.incrementW = true;
    if (value.endsWith("font"))
    { // multiply by the app font height
      r.minW = qMax(value.left(value.length()-4).toFloat(), 0.0f)
               * QFontMetrics(QApplication::font()).height();
    }
    else
      r.minW = qMax(v.toInt(),0);
  }

  sSpecs_[elementName] = r;
  return r;
}

theme_spec ThemeConfig::getCompositeSpec()
{
  theme_spec r;
  default_theme_spec(r);
  QVariant v;

#if defined Q_WS_X11 || defined Q_OS_LINUX
  /* set to false if no compositing manager is running */
#if QT_VERSION < 0x050200
  if (QX11Info::isCompositingManagerRunning())
#else
  bool compositing = false;
  if (isX11_)
  {
    Atom atom = XInternAtom (QX11Info::display(), "_NET_WM_CM_S0", False);
    if (XGetSelectionOwner(QX11Info::display(), atom))
      compositing = true;
  }
  else
    compositing = true; // wayland is always composited
  if (compositing)
#endif
  {
    v = getValue("General","composite");
    r.composite = v.toBool();
  }
#endif

  /* no blurring or window translucency without compositing */
  if (isX11_ && r.composite)
  {
    /* no window translucency or blurring without
       window interior element or reduced opacity */

    interior_spec ispec = getInteriorSpec("WindowTranslucent");
    if (ispec.element.isEmpty())
      ispec = getInteriorSpec("Window");

    if (ispec.hasInterior
        || getValue("General","reduce_window_opacity").toInt() > 0)
    {
      v = getValue("General","translucent_windows");
      if (v.isValid())
        r.translucent_windows = v.toBool();

      /* no window blurring without window translucency */
      if (r.translucent_windows)
      {
        v = getValue("General","blurring");
        if (v.isValid())
          r.blurring = v.toBool();
      }
    }

    /* "blurring" is sufficient but not necessary for "popup_blurring" */
    if (r.blurring)
      r.popup_blurring = true;
    else
    {
      interior_spec ispecM = getInteriorSpec("Menu");
      interior_spec ispecT = getInteriorSpec("ToolTip");
      if (ispecM.hasInterior || ispecT.hasInterior)
      {
        v = getValue("General","popup_blurring");
        if (v.isValid())
          r.popup_blurring = v.toBool();
      }
    }
  }

  /* no menu/tooltip shadow without compositing */
  v = getValue("General","menu_shadow_depth");
  if (v.isValid() && r.composite)
    r.menu_shadow_depth = qMax(v.toInt(),0);

  v = getValue("General","tooltip_shadow_depth");
  if (v.isValid() && r.composite)
    r.tooltip_shadow_depth = qMax(v.toInt(),0);

  return r;
}

theme_spec ThemeConfig::getThemeSpec()
{
  /* start with compositing */
  theme_spec r = getCompositeSpec();

  r.isX11 = isX11_;

  QVariant v = getValue("General","author");
  if (!v.toString().isEmpty())
    r.author = v.toString();

  v = getValue("General","comment");
  if (!v.toString().isEmpty())
    r.comment = v.toString();

  v = getValue("General","reduce_window_opacity");
  if (v.isValid())
    r.reduce_window_opacity = qMin(qMax(v.toInt(),0),90);

  v = getValue("General","x11drag");
  if (v.isValid()) // "WindowManager::DRAG_ALL" by default
  {
    // backward compatibility
    if (!(v.toString() == "true" || v.toInt() == 1))
      r.x11drag = WindowManager::toDrag(v.toString());
  }

  v = getValue("General","respect_DE");
  if (v.isValid()) // true by default
    r.respect_DE = v.toBool();

  v = getValue("General","alt_mnemonic");
  if (v.isValid()) // true by default
    r.alt_mnemonic = v.toBool();

  v = getValue("General","double_click");
  r.double_click = v.toBool();

  v = getValue("General","left_tabs");
  r.left_tabs = v.toBool();

  v = getValue("General","center_doc_tabs");
  r.center_doc_tabs = v.toBool();

  v = getValue("General","joined_inactive_tabs");
  if (v.isValid()) // true by default
    r.joined_inactive_tabs = v.toBool();
  else // backward compatibility
  {
    v = getValue("General","joined_tabs");
    if (v.isValid())
      r.joined_inactive_tabs = v.toBool();
  }

  v = getValue("General","attach_active_tab");
  r.attach_active_tab = v.toBool();

  if (!r.attach_active_tab)
  {
    v = getValue("General","embedded_tabs");
    r.embedded_tabs = v.toBool();
  }

  v = getValue("General","no_active_tab_separator");
  r.no_active_tab_separator = v.toBool();

  v = getValue("General","active_tab_overlap");
  if (v.isValid()) // 0 by default
  {
    QString value = v.toString();
    if (value.endsWith("font"))
    { // multiply by the app font height
      r.active_tab_overlap = qMax(value.left(value.length()-4).toFloat(), 0.0f)
                             * QFontMetrics(QApplication::font()).height();
    }
    else
      r.active_tab_overlap = qMax(v.toInt(),0);
  }

  v = getValue("General","mirror_doc_tabs");
  if (v.isValid()) // true by default
    r.mirror_doc_tabs = v.toBool();

  v = getValue("General","no_inactive_tab_expansion");
  r.no_inactive_tab_expansion = v.toBool();

  v = getValue("General","tab_button_extra_margin");
  if (v.isValid()) // 0 by default
  {
    int max = QFontMetrics(QApplication::font()).height();
    QString value = v.toString();
    if (value.endsWith("font"))
    { // multiply by the app font height
      r.tab_button_extra_margin = qMin(qMax(value.left(value.length()-4).toFloat(), 0.0f),1.0f)
                                  * max;
    }
    else
      r.tab_button_extra_margin = qMin(qMax(v.toInt(),0),max);
  }

  v = getValue("General","bold_active_tab");
  r.bold_active_tab = v.toBool();

  v = getValue("General","group_toolbar_buttons");
  r.group_toolbar_buttons = v.toBool();

  if (!r.group_toolbar_buttons)
  {
    v = getValue("General","toolbar_item_spacing");
    if (v.isValid()) // 0 by default
      r.toolbar_item_spacing = qMax(v.toInt(),0);
  }

  v = getValue("General","toolbar_interior_spacing");
  if (v.isValid()) // 0 by default
    r.toolbar_interior_spacing = qMax(v.toInt(),0);

  v = getValue("General","center_toolbar_handle");
  r.center_toolbar_handle = v.toBool();

  v = getValue("General","slim_toolbars");
  r.slim_toolbars = v.toBool();

  v = getValue("General","merge_menubar_with_toolbar");
  r.merge_menubar_with_toolbar = v.toBool();

  v = getValue("General","toolbutton_style");
  if (v.isValid()) // 0 by default
    r.toolbutton_style = v.toInt();

  v = getValue("General","spread_progressbar");
  r.spread_progressbar = v.toBool();

  v = getValue("General","progressbar_thickness");
  if (v.isValid()) // 0 by default
  {
    QString value = v.toString();
    if (value.endsWith("font"))
    { // multiply by the app font height
      r.progressbar_thickness = qMax(value.left(value.length()-4).toFloat(), 0.0f)
                                * QFontMetrics(QApplication::font()).height();
    }
    else
      r.progressbar_thickness = qMax(v.toInt(),0);
  }

  v = getValue("General","menubar_mouse_tracking");
  if (v.isValid()) //true by default
    r.menubar_mouse_tracking = v.toBool();

  v = getValue("General","opaque", QString()); // for going to the parent
  if (v.isValid())
    r.opaque << v.toStringList();

  v = getValue("General","submenu_overlap");
  if (v.isValid()) // -1 by default
    r.submenu_overlap = qMin(qMax(v.toInt(),0),16);

  v = getValue("General","submenu_delay");
  if (v.isValid()) // 250 by default
    r.submenu_delay = qMin(qMax(v.toInt(),-1),1000);

  v = getValue("General","splitter_width");
  if (v.isValid()) // 7 by default
    r.splitter_width = qMin(qMax(v.toInt(),0),32);

  v = getValue("General","scroll_width");
  if (v.isValid()) // 12 by default
    r.scroll_width = qMin(qMax(v.toInt(),0),32);

  v = getValue("General","scroll_min_extent");
  if (v.isValid()) // 36 by default
    r.scroll_min_extent = qMin(qMax(v.toInt(),16),100);

  v = getValue("General","tree_branch_line");
  r.tree_branch_line = v.toBool();

  v = getValue("General","slider_width");
  if (v.isValid()) // 8 by default
    r.slider_width = qMin(qMax(v.toInt(),0),48);

  v = getValue("General","slider_handle_width");
  if (v.isValid()) // 16 by default
    r.slider_handle_width = qMin(qMax(v.toInt(),0),48);

  v = getValue("General","slider_handle_length");
  if (v.isValid()) // 16 by default
    r.slider_handle_length = qMin(qMax(v.toInt(),0),48);

  v = getValue("General","tickless_slider_handle_size");
  r.tickless_slider_handle_size = qMin(qMax(v.toInt(),0),r.slider_handle_width);

  v = getValue("General","check_size");
  if (v.isValid()) //13 by default
    r.check_size = qMax(v.toInt(),0);

  v = getValue("General","tooltip_delay");
  if (v.isValid()) // -1 by default
    r.tooltip_delay = v.toInt();

  v = getValue("General","vertical_spin_indicators");
  r.vertical_spin_indicators = v.toBool();

  v = getValue("General","inline_spin_indicators");
  r.inline_spin_indicators = v.toBool();

  v = getValue("General","spin_button_width");
  if (v.isValid()) // 16 by default
    r.spin_button_width = qMin(qMax(v.toInt(),16), 32);

  v = getValue("General","combo_as_lineedit");
  r.combo_as_lineedit = v.toBool();
  if (!r.combo_as_lineedit)
  {
    v = getValue("General","square_combo_button");
    r.square_combo_button = v.toBool();
  }

  v = getValue("General","combo_menu");
  r.combo_menu = v.toBool();

  v = getValue("General","hide_combo_checkboxes");
  r.hide_combo_checkboxes = v.toBool();

  v = getValue("General","combo_focus_rect");
  r.combo_focus_rect = v.toBool();

  v = getValue("General","scrollable_menu");
  r.scrollable_menu = v.toBool();

  v = getValue("General","fill_rubberband");
  r.fill_rubberband = v.toBool();

  v = getValue("General","groupbox_top_label");
  r.groupbox_top_label = v.toBool();

  v = getValue("General","button_contents_shift");
  if (v.isValid()) // true by default
    r.button_contents_shift = v.toBool();

#if QT_VERSION < 0x050000
  r.transient_scrollbar=false;
#else
  v = getValue("General","transient_scrollbar");
  r.transient_scrollbar = v.toBool();
  v = getValue("General","transient_groove");
  r.transient_groove = v.toBool();
#endif

  /* for technical reasons, we always set scrollbar_in_view
     to false with transient scrollbars and (try to) put
     them inside their scroll contents in another way */
  if (!r.transient_scrollbar)
  {
    v = getValue("General","scrollbar_in_view");
    r.scrollbar_in_view = v.toBool(); // false by default

    v = getValue("General","scroll_arrows");
    if (v.isValid()) // true by default
      r.scroll_arrows = v.toBool();
  }
  else
    r.scroll_arrows = false;

  v = getValue("General","layout_spacing");
  if (v.isValid()) // 2 by default
    r.layout_spacing = qMin(qMax(v.toInt(),2), 16);

  v = getValue("General","layout_margin");
  if (v.isValid()) // 4 by default
    r.layout_margin = qMin(qMax(v.toInt(),2), 16);

  v = getValue("General","small_icon_size");
  if (v.isValid()) // 16 by default
    r.small_icon_size = qMin(qMax(v.toInt(),16), 48);

  v = getValue("General","large_icon_size");
  if (v.isValid()) // 32 by default
    r.large_icon_size = qMin(qMax(v.toInt(),24), 128);

  v = getValue("General","button_icon_size");
  if (v.isValid()) // 16 by default
    r.button_icon_size = qMin(qMax(v.toInt(),16), 64);

  v = getValue("General","toolbar_icon_size");
  if (v.isValid()) // 22 by default
    r.toolbar_icon_size = qMin(qMax(v.toInt(),16), 64);
  else if (r.slim_toolbars)
    r.toolbar_icon_size = 16;

  v = getValue("General","animate_states");
  r.animate_states = v.toBool();

  v = getValue("General","no_window_pattern");
  r.no_window_pattern = v.toBool();

  v = getValue("General", "dark_titlebar");
  r.dark_titlebar = v.toBool();

  return r;
}

color_spec ThemeConfig::getColorSpec() const
{
  color_spec r;
  default_color_spec(r);

  QVariant v = getValue("GeneralColors","window.color");
  r.windowColor = v.toString();

  v = getValue("GeneralColors","inactive.window.color");
  r.inactiveWindowColor = v.toString();

  v = getValue("GeneralColors","base.color");
  r.baseColor = v.toString();

  v = getValue("GeneralColors","inactive.base.color");
  r.inactiveBaseColor = v.toString();

  v = getValue("GeneralColors","alt.base.color");
  r.altBaseColor = v.toString();

  v = getValue("GeneralColors","button.color");
  r.buttonColor = v.toString();

  v = getValue("GeneralColors","light.color");
  r.lightColor = v.toString();

  v = getValue("GeneralColors","mid.light.color");
  r.midLightColor = v.toString();

  v = getValue("GeneralColors","dark.color");
  r.darkColor = v.toString();

  v = getValue("GeneralColors","mid.color");
  r.midColor = v.toString();

  v = getValue("GeneralColors","shadow.color");
  if (v.isValid())
    r.shadowColor = v.toString();

  v = getValue("GeneralColors","highlight.color");
  r.highlightColor = v.toString();

  v = getValue("GeneralColors","inactive.highlight.color");
  r.inactiveHighlightColor = v.toString();

  v = getValue("GeneralColors","tooltip.base.color");
  r.tooltipBaseColor = v.toString();

  v = getValue("GeneralColors","text.color");
  r.textColor = v.toString();

  v = getValue("GeneralColors","inactive.text.color");
  r.inactiveTextColor = v.toString();

  v = getValue("GeneralColors","window.text.color");
  r.windowTextColor = v.toString();

  v = getValue("GeneralColors","inactive.window.text.color");
  r.inactiveWindowTextColor = v.toString();

  v = getValue("GeneralColors","button.text.color");
  r.buttonTextColor = v.toString();

  v = getValue("GeneralColors","disabled.text.color");
  r.disabledTextColor = v.toString();

  v = getValue("GeneralColors","tooltip.text.color");
  r.tooltipTextColor = v.toString();

  v = getValue("GeneralColors","highlight.text.color");
  r.highlightTextColor = v.toString();

  v = getValue("GeneralColors","inactive.highlight.text.color");
  r.inactiveHighlightTextColor = v.toString();

  v = getValue("GeneralColors","link.color");
  r.linkColor = v.toString();

  v = getValue("GeneralColors","link.visited.color");
  r.linkVisitedColor = v.toString();

  v = getValue("GeneralColors","progress.indicator.text.color");
  r.progressIndicatorTextColor = v.toString();

  v = getValue("GeneralColors","progress.inactive.indicator.text.color");
  r.progressInactiveIndicatorTextColor = v.toString();

  return r;
}

hacks_spec ThemeConfig::getHacksSpec() const
{
  hacks_spec r;
  default_hacks_spec(r);

  QVariant v = getValue("Hacks","transparent_dolphin_view");
  r.transparent_dolphin_view = v.toBool();

  v = getValue("Hacks","transparent_pcmanfm_sidepane");
  r.transparent_pcmanfm_sidepane = v.toBool();

  v = getValue("Hacks","transparent_pcmanfm_view");
  r.transparent_pcmanfm_view = v.toBool();

  v = getValue("Hacks","lxqtmainmenu_iconsize");
  if (v.isValid())
    r.lxqtmainmenu_iconsize = qMin(qMax(v.toInt(),0),32);

  if (isX11_)
  {
    v = getValue("Hacks","blur_translucent");
    if (v.isValid())
      r.blur_translucent = v.toBool();
    else // backward compatibility
    {
      v = getValue("Hacks","blur_konsole");
      r.blur_translucent = v.toBool();
    }
  }

  v = getValue("Hacks","transparent_ktitle_label");
  r.transparent_ktitle_label = v.toBool();

  v = getValue("Hacks","transparent_menutitle");
  r.transparent_menutitle = v.toBool();

  v = getValue("Hacks","kcapacitybar_as_progressbar");
  r.kcapacitybar_as_progressbar = v.toBool();

  v = getValue("Hacks","respect_darkness");
  r.respect_darkness = v.toBool();

  v = getValue("Hacks","force_size_grip");
  r.forceSizeGrip = v.toBool();

  v = getValue("Hacks","tint_on_mouseover");
  if (v.isValid())
    r.tint_on_mouseover = qMin(qMax(v.toInt(),0),100);

  v = getValue("Hacks","no_selection_tint");
  r.no_selection_tint = v.toBool();

  v = getValue("Hacks","disabled_icon_opacity");
  if (v.isValid())
    r.disabled_icon_opacity = qMin(qMax(v.toInt(),0),100);

  v = getValue("Hacks","normal_default_pushbutton");
  r.normal_default_pushbutton = v.toBool();

  v = getValue("Hacks","iconless_pushbutton");
  r.iconless_pushbutton = v.toBool();

  v = getValue("Hacks","transparent_arrow_button");
  r.transparent_arrow_button = v.toBool();

  v = getValue("Hacks","iconless_menu");
  r.iconless_menu = v.toBool();

  v = getValue("Hacks","single_top_toolbar");
  r.single_top_toolbar = v.toBool();

  v = getValue("Hacks","middle_click_scroll");
  r.middle_click_scroll = v.toBool();

  return r;
}
}
