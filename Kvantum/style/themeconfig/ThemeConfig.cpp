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

#include <QVariant>
#include <QSettings>
#include <QFile>
#include <QStringList>
#include "ThemeConfig.h"
#if defined Q_WS_X11 || defined Q_OS_LINUX
#include <QX11Info>
#if QT_VERSION >= 0x050000
#include <X11/Xlib.h>
#include <X11/Xatom.h>
static Atom atom = XInternAtom (QX11Info::display(), "_NET_WM_CM_S0", False);
#endif
#endif

namespace Kvantum {
ThemeConfig::ThemeConfig(const QString& theme) :
  settings_(NULL),
  parentConfig_(NULL)
{
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
      && key != "text.normal.color" && key != "text.focus.color" && key != "text.press.color" && key != "text.toggle.color"
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

  v = getValue(elementName,"frame", i);
  r.hasFrame = v.toBool();
  if (r.hasFrame)
  {
    v = getValue(elementName, "frame.element", i);
    if (!v.toString().isEmpty())
    {
      r.element = v.toString();

      v = getValue(elementName,"frame.top", i);
      r.top = qMax(v.toInt(),0);
      v = getValue(elementName,"frame.bottom", i);
      r.bottom = qMax(v.toInt(),0);
      v = getValue(elementName,"frame.left", i);
      r.left = qMax(v.toInt(),0);
      v = getValue(elementName,"frame.right", i);
      r.right = qMax(v.toInt(),0);

      if (r.top && r.bottom && r.left && r.right)
      {
        v = getValue(elementName,"frame.expansion", i);
        r.expansion = qMax(v.toInt(),0);

        if (r.expansion > 0)
        {
          v = getValue(elementName,"frame.expanded.top", i);
          r.topExpanded = qMin(v.toInt(),r.top);
          if (r.topExpanded <= 0) r.topExpanded = r.top;
          v = getValue(elementName,"frame.expanded.bottom", i);
          r.bottomExpanded = qMin(v.toInt(),r.bottom);
          if (r.bottomExpanded <= 0) r.bottomExpanded = r.bottom;
          v = getValue(elementName,"frame.expanded.left", i);
          r.leftExpanded = qMin(v.toInt(),r.left);
          if (r.leftExpanded <= 0) r.leftExpanded = r.left;
          v = getValue(elementName,"frame.expanded.right", i);
          r.rightExpanded = qMin(v.toInt(),r.right);
          if (r.rightExpanded <= 0) r.rightExpanded = r.right;
        }
      }
    }
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

  v = getValue(elementName,"interior", i);
  r.hasInterior = v.toBool();

  if (r.hasInterior)
  {
    v = getValue(elementName, "interior.element", i);
    if (!v.toString().isEmpty())
    {
      r.element = v.toString();

      v = getValue(elementName,"interior.x.patternsize", i);
      r.px = qMax(v.toInt(),0);
      v = getValue(elementName,"interior.y.patternsize", i);
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
  {
    r.element = v.toString();

    v = getValue(elementName,"indicator.size", i);
    if (v.isValid())
      r.size = qMax(v.toInt(),0);
  }

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

  v = getValue(elementName,"text.shadow", i);
  r.hasShadow = v.toBool();

  v = getValue(elementName,"text.normal.color", i);
  r.normalColor = v.toString();
  v = getValue(elementName,"text.focus.color", i);
  r.focusColor = v.toString();
  v = getValue(elementName,"text.press.color", i);
  r.pressColor = v.toString();
  v = getValue(elementName,"text.toggle.color", i);
  r.toggleColor = v.toString();

  v = getValue(elementName,"text.bold", i);
  r.boldFont = v.toBool();
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
    v = getValue(elementName,"text.shadow.alpha", i);
    if (v.isValid())
      r.a = qMax(v.toInt(),0);
    v = getValue(elementName,"text.shadow.depth", i);
    if (v.isValid())
      r.depth = qMax(v.toInt(),0);
  }

  v = getValue(elementName,"text.margin", i);
  r.hasMargin = v.toBool();
  if (r.hasMargin)
  {
    v = getValue(elementName,"text.margin.top", i);
    r.top = qMax(v.toInt(),0);
    v = getValue(elementName,"text.margin.bottom", i);
    r.bottom = qMax(v.toInt(),0);
    v = getValue(elementName,"text.margin.left", i);
    r.left = qMax(v.toInt(),0);
    v = getValue(elementName,"text.margin.right", i);
    r.right = qMax(v.toInt(),0);
  }

  v = getValue(elementName,"text.iconspacing", i);
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

  QVariant v = getValue(elementName, "inherits");
  QString i = v.toString();

  v = getValue(elementName,"size.minheight", i);
  if (v.isValid())
    r.minH = qMax(v.toInt(),0);

  v = getValue(elementName,"size.minwidth", i);
  if (v.isValid())
    r.minW = qMax(v.toInt(),0);

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
#if QT_VERSION < 0x050000
  if (QX11Info::isCompositingManagerRunning())
#else
  if (XGetSelectionOwner(QX11Info::display(), atom))
#endif
  {
    v = getValue("General","composite");
    r.composite = v.toBool();
  }
#endif

  /* no blurring or window translucency without compositing */
  if (r.composite)
  {
    /* no window translucency or blurring
       without window interior elemenet */
    interior_spec ispec = getInteriorSpec("WindowTranslucent");
    if (ispec.element.isEmpty())
      ispec = getInteriorSpec("Window");
    if (ispec.hasInterior)
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

  QVariant v = getValue("General","author");
  if (!v.toString().isEmpty())
    r.author = v.toString();

  v = getValue("General","comment");
  if (!v.toString().isEmpty())
    r.comment = v.toString();

#if defined Q_WS_X11 || defined Q_OS_LINUX
  v = getValue("General","x11drag");
  if (v.isValid()) // "WindowManager::DRAG_ALL" by default
  {
    // backward compatibility
    if (!(v.toString() == "true" || v.toInt() == 1))
      r.x11drag = WindowManager::toDrag(v.toString());
  }
#endif

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

  v = getValue("General","mirror_doc_tabs");
  if (v.isValid()) // true by default
    r.mirror_doc_tabs = v.toBool();

  v = getValue("General","group_toolbar_buttons");
  r.group_toolbar_buttons = v.toBool();

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
    r.progressbar_thickness = v.toInt();

  v = getValue("General","menubar_mouse_tracking");
  if (v.isValid()) //true by default
    r.menubar_mouse_tracking = v.toBool();

  v = getValue("General","opaque", QString()); // for going to the parent
  if (v.isValid())
    r.opaque << v.toStringList();

  v = getValue("General","submenu_overlap");
  if (v.isValid()) // -1 by default
    r.submenu_overlap = qMin(qMax(v.toInt(),-1),16);

  v = getValue("General","splitter_width");
  if (v.isValid()) // 7 by default
    r.splitter_width = qMin(qMax(v.toInt(),0),32);

  v = getValue("General","scroll_width");
  if (v.isValid()) // 12 by default
    r.scroll_width = qMin(qMax(v.toInt(),0),32);

  v = getValue("General","scroll_min_extent");
  if (v.isValid()) // 36 by default
    r.scroll_min_extent = qMin(qMax(v.toInt(),16),100);

  v = getValue("General","scroll_arrows");
  if (v.isValid()) // true by default
    r.scroll_arrows = v.toBool();

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

  v = getValue("General","combo_menu");
  r.combo_menu = v.toBool();

  v = getValue("General","fill_rubberband");
  r.fill_rubberband = v.toBool();

  v = getValue("General","groupbox_top_label");
  r.groupbox_top_label = v.toBool();

  v = getValue("General","button_contents_shift");
  if (v.isValid()) // true by default
    r.button_contents_shift = v.toBool();

  v = getValue("General","scrollbar_in_view");
  r.scrollbar_in_view = v.toBool();

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

  return r;
}

color_spec ThemeConfig::getColorSpec() const
{
  color_spec r;
  default_color_spec(r);

  QVariant v = getValue("GeneralColors","window.color");
  r.windowColor = v.toString();

  v = getValue("GeneralColors","base.color");
  r.baseColor = v.toString();

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
  r.tooltipBasetColor = v.toString();

  v = getValue("GeneralColors","text.color");
  r.textColor = v.toString();

  v = getValue("GeneralColors","window.text.color");
  r.windowTextColor = v.toString();

  v = getValue("GeneralColors","button.text.color");
  r.buttonTextColor = v.toString();

  v = getValue("GeneralColors","disabled.text.color");
  r.disabledTextColor = v.toString();

  v = getValue("GeneralColors","tooltip.text.color");
  r.tooltipTextColor = v.toString();

  v = getValue("GeneralColors","highlight.text.color");
  r.highlightTextColor = v.toString();

  v = getValue("GeneralColors","link.color");
  r.linkColor = v.toString();

  v = getValue("GeneralColors","link.visited.color");
  r.linkVisitedColor = v.toString();

  v = getValue("GeneralColors","progress.indicator.text.color");
  r.progressIndicatorTextColor = v.toString();

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

  v = getValue("Hacks","blur_konsole");
  r.blur_konsole = v.toBool();

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

  v = getValue("Hacks","iconless_menu");
  r.iconless_menu = v.toBool();

  v = getValue("Hacks","single_top_toolbar");
  r.single_top_toolbar = v.toBool();

  return r;
}
}
