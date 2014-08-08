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
#include <QX11Info>

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
      && key != "text.normal.color" && key != "text.focus.color")
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

  // titlebars and indicator arrows never have frame
  if (elementName == "TitleBar" || elementName == "IndicatorArrow")
    return r;

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
      r.top = v.toInt();
      v = getValue(elementName,"frame.bottom", i);
      r.bottom = v.toInt();
      v = getValue(elementName,"frame.left", i);
      r.left = v.toInt();
      v = getValue(elementName,"frame.right", i);
      r.right = v.toInt();

      v = getValue(elementName,"frame.repeat.top.patternsize", i);
      r.ptop = v.toInt();
      v = getValue(elementName,"frame.repeat.bottom.patternsize", i);
      r.pbottom = v.toInt();
      v = getValue(elementName,"frame.repeat.left.patternsize", i);
      r.pleft = v.toInt();
      v = getValue(elementName,"frame.repeat.right.patternsize", i);
      r.pright = v.toInt();
    }
  }

  return r;
}

interior_spec ThemeConfig::getInteriorSpec(const QString &elementName) const
{
  interior_spec r;
  default_interior_spec(r);

  // indicator arrows never have interior
  if (elementName == "IndicatorArrow")
    return r;

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

      v = getValue(elementName,"interior.repeat.x.patternsize", i);
      r.px = v.toInt();
      v = getValue(elementName,"interior.repeat.y.patternsize", i);
      r.py = v.toInt();
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
    r.size = v.toInt();
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

  if (r.hasShadow)
  {
    v = getValue(elementName,"text.shadow.xshift", i);
    r.xshift = v.toInt();
    v = getValue(elementName,"text.shadow.yshift", i);
    r.yshift = v.toInt();
    v = getValue(elementName,"text.shadow.color", i);
    r.shadowColor = v.toString();
    v = getValue(elementName,"text.shadow.alpha", i);
    r.a = v.toInt();
    v = getValue(elementName,"text.shadow.depth", i);
    r.depth = v.toInt();
  }

  v = getValue(elementName,"text.margin", i);
  r.hasMargin = v.toBool();
  if (r.hasMargin)
  {
    v = getValue(elementName,"text.margin.top", i);
    r.top = v.toInt();
    v = getValue(elementName,"text.margin.bottom", i);
    r.bottom = v.toInt();
    v = getValue(elementName,"text.margin.left", i);
    r.left = v.toInt();
    v = getValue(elementName,"text.margin.right", i);
    r.right = v.toInt();
  }

  v = getValue(elementName,"text.iconspacing", i);
  r.tispace = v.toInt();

  return r;
}

size_spec ThemeConfig::getSizeSpec(const QString& elementName) const
{
  size_spec r;
  default_size_spec(r);

  QVariant v = getValue(elementName, "inherits");
  QString i = v.toString();

  v = getValue(elementName,"size.minheight", i);
  r.minH = v.toInt();
  v = getValue(elementName,"size.fixedheight", i);
  r.fixedH = v.toInt();

  v = getValue(elementName,"size.minwidth", i);
  r.minW = v.toInt();
  v = getValue(elementName,"size.fixedwidth", i);
  r.fixedW = v.toInt();

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

  v = getValue("General","x11drag", empty);
  r.x11drag = v.toBool();

  v = getValue("General","alt_mnemonic", empty);
  r.alt_mnemonic = v.toBool();

  v = getValue("General","left_tabs", empty);
  r.left_tabs = v.toBool();

  v = getValue("General","joined_tabs", empty);
  r.joined_tabs = v.toBool();

  v = getValue("General","attach_active_tab", empty);
  r.attach_active_tab = v.toBool();

  v = getValue("General","group_toolbar_buttons", empty);
  r.group_toolbar_buttons = v.toBool();

  /* set to false if no compositing manager is running */
  if (QX11Info::isCompositingManagerRunning())
  {
    v = getValue("General","composite");
    r.composite = v.toBool();
  }

  /* no menu or tooltip shadow without compositing */

  v = getValue("General","menu_shadow_depth");
  if (v.isValid() && r.composite)
    r.menu_shadow_depth = v.toInt();

  v = getValue("General","tooltip_shadow_depth");
  if (v.isValid() && r.composite)
    r.tooltip_shadow_depth = v.toInt();

  v = getValue("General","splitter_width", empty);
  if (v.isValid())
    r.splitter_width = v.toInt();

  v = getValue("General","scroll_width", empty);
  if (v.isValid())
    r.scroll_width = v.toInt();

  v = getValue("General","check_size", empty);
  if (v.isValid())
    r.check_size = v.toInt();

  return r;
}

/*QStringList ThemeConfig::getManagedElements()
{
  return
    QStringList()
      << "PanelButtonCommand"
      << "PanelButtonTool"
      << "Dock"
      << "RadioButton"
      << "CheckBox"
      << "Focus"
      << "GenericFrame"
      << "TabFrame"
      << "GroupBox"
      << "LineEdit"
      << "IndicatorSpinBox"
      << "DropDownButton"
      << "IndicatorArrow"
      << "ToolboxTab"
      << "Tab"
      << "TreeExpander"
      << "HeaderSection"
      << "SizeGrip"
      << "Toolbar"
      << "ScrollbarSlider"
      << "Slider"
      << "SliderCursor"
      << "Progressbar"
      << "ProgressbarContents"
      << "ItemView"
      << "Splitter"
      << "Scrollbar"
      << "ScrollbarGroove"
      << "Widget"
      << "MenuItem"
      << "Menu"
      << "MenuBarItem"
      << "MenuBar"
      << "TitleBar"
      << "ComboBox"
      << "ToolTip"
  ;
}*/
