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

#include "ThemeConfig.h"

#include <QVariant>
#include <QSettings>
#include <QFile>
#include <QStringList>
#if defined Q_WS_X11 || defined Q_OS_LINUX
#include <QX11Info>
#if QT_VERSION >= 0x050000
#include <X11/Xlib.h>
#include <X11/Xatom.h>
static Atom atom = XInternAtom (QX11Info::display(), "_NET_WM_CM_S0", False);
#endif
#endif

ThemeConfig::ThemeConfig(const QString& theme) :
  settings(NULL),
  parentConfig(NULL)
{
  load(theme);
}

ThemeConfig::~ThemeConfig()
{
  if (settings)
    delete settings;
}

void ThemeConfig::load(const QString& theme)
{
  if (settings)
  {
    delete settings;
    settings = NULL;
  }

  if (!QFile::exists(theme))
    return;

  settings = new QSettings(theme,QSettings::NativeFormat);
}

QVariant ThemeConfig::getValue(const QString& group, const QString& key) const
{
  QVariant r;

  if (group.isNull() || group.isEmpty() || key.isNull() || key.isEmpty())
    return r;

  if (settings)
  {
    settings->beginGroup(group);
    r = settings->value(key);
    settings->endGroup();
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
  if (parentConfig
      && key != "text.normal.color" && key != "text.focus.color" && key != "text.press.color" && key != "text.toggle.color"
      && key != "text.bold" && key != "text.italic")
  {
    i = parentConfig->getValue(group, "inherits").toString();
    r = parentConfig->getValue(group, key, i);
  }

  return r;
}

frame_spec ThemeConfig::getFrameSpec(const QString &elementName) const
{
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
      }
    }
  }

  return r;
}

interior_spec ThemeConfig::getInteriorSpec(const QString &elementName) const
{
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
  return r;
}

indicator_spec ThemeConfig::getIndicatorSpec(const QString &elementName) const
{
  indicator_spec r;
  default_indicator_spec(r);

  QVariant v = getValue(elementName, "inherits");
  QString i = v.toString();

  v = getValue(elementName, "indicator.element", i);
  if (!v.toString().isEmpty())
  {
    r.element = v.toString();

    v = getValue(elementName,"indicator.size", i);
    r.size = qMax(v.toInt(),0);
  }

  return r;
}

label_spec ThemeConfig::getLabelSpec(const QString &elementName) const
{
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

  return r;
}

size_spec ThemeConfig::getSizeSpec(const QString& elementName) const
{
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

  return r;
}

theme_spec ThemeConfig::getThemeSpec() const
{
  theme_spec r;
  default_theme_spec(r);

  QString empty; // use this for going to the parent
  QVariant v = getValue("General","author", empty);
  if (!v.toString().isEmpty())
    r.author = v.toString();

  v = getValue("General","comment", empty);
  if (!v.toString().isEmpty())
    r.comment = v.toString();

#if defined Q_WS_X11 || defined Q_OS_LINUX
  v = getValue("General","x11drag", empty);
  r.x11drag = v.toBool();
#endif

  v = getValue("General","alt_mnemonic", empty);
  r.alt_mnemonic = v.toBool();

  v = getValue("General","double_click", empty);
  r.double_click = v.toBool();

  v = getValue("General","left_tabs", empty);
  r.left_tabs = v.toBool();

  v = getValue("General","joined_tabs", empty);
  r.joined_tabs = v.toBool();

  v = getValue("General","attach_active_tab", empty);
  r.attach_active_tab = v.toBool();

  v = getValue("General","mirror_doc_tabs", empty);
  if (v.isValid()) // it's true by default
    r.mirror_doc_tabs = v.toBool();

  v = getValue("General","group_toolbar_buttons", empty);
  r.group_toolbar_buttons = v.toBool();

  v = getValue("General","center_toolbar_handle", empty);
  r.center_toolbar_handle = v.toBool();

  v = getValue("General","slim_toolbars", empty);
  r.slim_toolbars = v.toBool();

  v = getValue("General","merge_menubar_with_toolbar", empty);
  r.merge_menubar_with_toolbar = v.toBool();

  v = getValue("General","toolbutton_style", empty);
  r.toolbutton_style = v.toInt();

  v = getValue("General","spread_progressbar", empty);
  r.spread_progressbar = v.toBool();

  v = getValue("General","textless_progressbar", empty);
  r.textless_progressbar = v.toBool();

  v = getValue("General","progressbar_thickness", empty);
  r.progressbar_thickness = v.toInt();

  v = getValue("General","menubar_mouse_tracking", empty);
  if (v.isValid()) // it's true by default
    r.menubar_mouse_tracking = v.toBool();

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

  v = getValue("General","opaque", empty);
  if (v.isValid())
    r.opaque << v.toStringList();

  v = getValue("General","splitter_width", empty);
  if (v.isValid())
    r.splitter_width = qMax(v.toInt(),0);

  v = getValue("General","scroll_width", empty);
  if (v.isValid())
    r.scroll_width = qMax(v.toInt(),0);

  v = getValue("General","scroll_arrows", empty);
  if (v.isValid()) // it's true by default
    r.scroll_arrows = v.toBool();

  v = getValue("General","slider_width", empty);
  if (v.isValid())
    r.slider_width = qMax(v.toInt(),0);

  v = getValue("General","slider_handle_width", empty);
  if (v.isValid())
    r.slider_handle_width = qMax(v.toInt(),0);

  v = getValue("General","slider_handle_length", empty);
  if (v.isValid())
    r.slider_handle_length = qMax(v.toInt(),0);

  v = getValue("General","check_size", empty);
  if (v.isValid())
    r.check_size = qMax(v.toInt(),0);

  v = getValue("General","vertical_spin_indicators");
  r.vertical_spin_indicators = v.toBool();

  v = getValue("General","fill_rubberband");
  r.fill_rubberband = v.toBool();

  v = getValue("General","small_icon_size", empty);
  if (v.isValid())
    r.small_icon_size = qMin(qMax(v.toInt(),16), 48);

  v = getValue("General","large_icon_size", empty);
  if (v.isValid())
    r.large_icon_size = qMin(qMax(v.toInt(),24), 128);

  v = getValue("General","button_icon_size", empty);
  if (v.isValid())
    r.button_icon_size = qMin(qMax(v.toInt(),16), 64);

  v = getValue("General","toolbar_icon_size", empty);
  if (v.isValid())
    r.toolbar_icon_size = qMin(qMax(v.toInt(),16), 64);
  else if (r.slim_toolbars)
    r.toolbar_icon_size = 16;

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

  return r;
}
