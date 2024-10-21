/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2022 <tsujan2000@gmail.com>
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
#include "ThemeConfig5.h"

#if defined Q_WS_X11 || defined Q_OS_LINUX || defined Q_OS_FREEBSD || defined Q_OS_OPENBSD || defined Q_OS_NETBSD || defined Q_OS_HURD
#include <QX11Info>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif

#define KSL(x) QStringLiteral(x)

namespace Kvantum {

ThemeConfig::ThemeConfig(const QString& theme) :
  settings_(nullptr),
  parentConfig_(nullptr)
{
#if defined Q_WS_X11 || defined Q_OS_LINUX || defined Q_OS_FREEBSD || defined Q_OS_OPENBSD || defined Q_OS_NETBSD || defined Q_OS_HURD
  isX11_ = (QString::compare(QGuiApplication::platformName(), "xcb", Qt::CaseInsensitive) == 0);
#else
  isX11_ = false;
#endif

  load(theme);

  /* WARNING: Qt has a bug that causes drawing problems with a non-integer
     scale factor. Those problems can be seen with Fusion too but they become
     intense with an SVG window gradient and/or window translucency. As a
     workaround, we remove the window interior and translucency in this case. */
  const qreal dpr = qApp->devicePixelRatio();
  nonIntegerScale = (dpr > static_cast<qreal>(1) && static_cast<qreal>(qRound(dpr)) != dpr);
  if (nonIntegerScale)
  {
    nonIntegerScale = !getValue(KSL("Hacks"),KSL("noninteger_translucency")).toBool();
    if (nonIntegerScale)
    {
      interior_spec r;
      default_interior_spec(r);
      r.hasInterior = false;
      iSpecs_[KSL("WindowTranslucent")] = iSpecs_[KSL("Window")] = iSpecs_[KSL("Dialog")] = r;
    }
  }

  default_theme_spec(compositeSpecs_);
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
    settings_ = nullptr;
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
    i = getValue(i, KSL("inherits")).toString();
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
    i = parentConfig_->getValue(group, KSL("inherits")).toString();
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

  QVariant v = getValue(elementName, KSL("inherits"));
  QString i = v.toString();

  /* except for text colors and indicator, frame and interior elements,
     ToolbarButton gets all of its variables from PanelButtonTool */
  QString name = elementName;
  if (name == "ToolbarButton")
    name = "PanelButtonTool";
  /* and the same for ToolbarComboBox */
  else if (name == "ToolbarComboBox")
    name = "ComboBox";
  /* and a similar thing is true for ToolbarLineEdit */
  else if (name == "ToolbarLineEdit")
    name = "LineEdit";

  v = getValue(name,KSL("frame"), i);
  r.hasFrame = v.toBool();
  if (r.hasFrame)
  {
    v = getValue(name,KSL("focusFrame"), i);
    r.hasFocusFrame = v.toBool();

    v = getValue(elementName, KSL("frame.element"), i);
    if (!v.toString().isEmpty())
    {
      r.element = v.toString();

      if (elementName == "ToolbarButton" || elementName == "ToolbarComboBox"
          || elementName == "ToolbarLineEdit")
      {
        i = getValue(name, KSL("inherits")).toString();
      }
      else if (elementName == "ScrollbarTransientSlider")
        i = "ScrollbarSlider"; // to get the frame sizes

      v = getValue(name,KSL("frame.top"), i);
      r.top = qMax(v.toInt(),0);
      v = getValue(name,KSL("frame.bottom"), i);
      r.bottom = qMax(v.toInt(),0);
      v = getValue(name,KSL("frame.left"), i);
      r.left = qMax(v.toInt(),0);
      v = getValue(name,KSL("frame.right"), i);
      r.right = qMax(v.toInt(),0);

      if(name == "ItemView")
      {
        r.left = qMin(r.left,6);
        r.right = qMin(r.right,6);
        r.top = qMin(r.top,6);
        r.bottom = qMin(r.bottom,6);
      }

      v = getValue(name,KSL("frame.patternsize"), i);
      r.ps = qMax(v.toInt(),0);

      if (r.top || r.bottom || r.left || r.right)
      {
        v = getValue(name,KSL("frame.expansion"), i);
        if (v.isValid())
        {
          QString value = v.toString();
          if (value.endsWith(QLatin1String("font")))
          { // multiply by the app font height
            r.expansion = qMax(value.left(value.length()-4).toFloat(), 0.0f)
                          * QFontMetrics(QApplication::font()).boundingRect(QLatin1Char('M')).height()*1.6;
          }
          else
            r.expansion = qMax(v.toInt(),0);
        }

        if (r.expansion > 0)
        {
          v = getValue(name,KSL("frame.expanded.top"), i);
          if (v.isValid())
          {
            r.topExpanded = qMin(v.toInt(),r.top);
            if (r.topExpanded < 0) r.topExpanded = r.top;
          }
          else r.topExpanded = r.top;
          v = getValue(name,KSL("frame.expanded.bottom"), i);
          if (v.isValid())
          {
            r.bottomExpanded = qMin(v.toInt(),r.bottom);
            if (r.bottomExpanded < 0) r.bottomExpanded = r.bottom;
          }
          else r.bottomExpanded = r.bottom;
          v = getValue(name,KSL("frame.expanded.left"), i);
          if (v.isValid())
          {
            r.leftExpanded = qMin(v.toInt(),r.left);
            if (r.leftExpanded < 0) r.leftExpanded = r.left;
          }
          else r.leftExpanded = r.left;
          v = getValue(name,KSL("frame.expanded.right"), i);
          if (v.isValid())
          {
            r.rightExpanded = qMin(v.toInt(),r.right);
            if (r.rightExpanded < 0) r.rightExpanded = r.right;
          }
          else r.rightExpanded = r.right;
        }
      }
    }
    v = getValue(elementName, KSL("frame.expandedElement"));
    r.expandedElement = v.toString();

    v = getValue(elementName, KSL("focusRectElement"));
    r.focusRectElement = v.toString();
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

  QVariant v = getValue(elementName, KSL("inherits"));
  QString i = v.toString();

  QString name = elementName;
  if (name == "ToolbarButton")
    name = "PanelButtonTool";
  else if (name == "ToolbarComboBox")
    name = "ComboBox";
  else if (name == "ToolbarLineEdit")
    name = "LineEdit";

  v = getValue(name,KSL("interior"), i);
  r.hasInterior = v.toBool();

  if (r.hasInterior)
  {
    v = getValue(name,"focusInterior", i);
    r.hasFocusInterior = v.toBool();

    v = getValue(elementName, KSL("interior.element"), i);
    if (!v.toString().isEmpty())
    {
      r.element = v.toString();

      if (elementName == "ToolbarButton" || elementName == "ToolbarComboBox"
          || elementName == "ToolbarLineEdit")
      {
        i = getValue(name, KSL("inherits")).toString();
      }

      v = getValue(name,KSL("interior.x.patternsize"), i);
      r.px = qMax(v.toInt(),0);
      v = getValue(name,KSL("interior.y.patternsize"), i);
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

  QVariant v = getValue(elementName, KSL("inherits"));
  QString i = v.toString();

  v = getValue(elementName, KSL("indicator.element"), i);
  if (!v.toString().isEmpty())
    r.element = v.toString();

  /* ToolbarButton gets its indicator size from PanelButtonTool */
  QString name = elementName;
  if (name == "ToolbarButton")
  {
    name = "PanelButtonTool";
    i = getValue(name, KSL("inherits")).toString();
  }
  /* and the same for ToolbarComboBox */
  else if (name == "ToolbarComboBox")
  {
    name = "ComboBox";
    i = getValue(name, KSL("inherits")).toString();
  }
  /* and the same for ToolbarLineEdit */
  else if (name == "ToolbarLineEdit")
  {
    name = "LineEdit";
    i = getValue(name, KSL("inherits")).toString();
  }
  v = getValue(name,KSL("indicator.size"), i);
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

  QVariant v = getValue(elementName, KSL("inherits"));
  QString i = v.toString();

  /* LineEdit is excluded for its size calculation to be correct */
  if (elementName != "LineEdit")
  {
    v = getValue(elementName,KSL("text.shadow"), i);
    r.hasShadow = v.toBool();

    v = getValue(elementName,KSL("text.normal.color"), i);
    r.normalColor = v.toString();

    v = getValue(elementName,KSL("text.normal.inactive.color"), i);
    r.normalInactiveColor = v.toString();

    v = getValue(elementName,KSL("text.focus.color"), i);
    r.focusColor = v.toString();

    v = getValue(elementName,KSL("text.focus.inactive.color"), i);
    r.focusInactiveColor = v.toString();

    if (elementName == "MenuItem" || elementName == "MenuBarItem")
    { // no inheritance because the (fallback) focus color seems more natural
      v = getValue(elementName,KSL("text.press.color"));
      r.pressColor = v.toString();

      v = getValue(elementName,KSL("text.toggle.color"));
      r.toggleColor = v.toString();
    }
    else
    {
      v = getValue(elementName,KSL("text.press.color"), i);
      r.pressColor = v.toString();

      v = getValue(elementName,KSL("text.press.inactive.color"), i);
      r.pressInactiveColor = v.toString();

      v = getValue(elementName,KSL("text.toggle.color"), i);
      r.toggleColor = v.toString();

      v = getValue(elementName,KSL("text.toggle.inactive.color"), i);
      r.toggleInactiveColor = v.toString();
    }

    /* because finding longest texts of combo boxes isn't CPU-friendly and since
       combos can have menu popups, we don't make menu and combo texts bold or italic */
    if (elementName != "MenuItem"
        && elementName != "ComboBox" && elementName != "ToolbarComboBox")
    {
      v = getValue(elementName,KSL("text.bold"), i);
      r.boldFont = v.toBool();

      v = getValue(elementName,KSL("text.boldness"), i);
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

      v = getValue(elementName,KSL("text.italic"), i);
      r.italicFont = v.toBool();
    }

    if (r.hasShadow)
    {
      v = getValue(elementName,KSL("text.shadow.xshift"), i);
      if (v.isValid())
        r.xshift = v.toInt();
      v = getValue(elementName,KSL("text.shadow.yshift"), i);
      if (v.isValid())
        r.yshift = v.toInt();
      v = getValue(elementName,KSL("text.shadow.color"), i);
      if (v.isValid())
        r.shadowColor = v.toString();
      v = getValue(elementName,KSL("text.inactive.shadow.color"), i);
      if (v.isValid())
        r.inactiveShadowColor = v.toString();
      v = getValue(elementName,KSL("text.shadow.alpha"), i);
      if (v.isValid())
        r.a = qMax(v.toInt(),0);
      v = getValue(elementName,KSL("text.shadow.depth"), i);
      if (v.isValid())
        r.depth = qMin(qMax(v.toInt(),0),1); // drawing more than once would be ugly
    }
  }

  QString name = elementName;
  if (name == "ToolbarButton")
  {
    name = "PanelButtonTool";
    i = getValue(name, KSL("inherits")).toString();
  }
  else if (name == "ToolbarComboBox")
  {
    name = "ComboBox";
    i = getValue(name, KSL("inherits")).toString();
  }
  else if (name == "ToolbarLineEdit")
  {
    name = "LineEdit";
    i = getValue(name, KSL("inherits")).toString();
  }

  v = getValue(name,KSL("text.margin"), i);
  r.hasMargin = v.toBool();
  if (r.hasMargin)
  {
    v = getValue(name,KSL("text.margin.top"), i);
    r.top = qMax(v.toInt(),0);
    v = getValue(name,KSL("text.margin.bottom"), i);
    r.bottom = qMax(v.toInt(),0);
    v = getValue(name,KSL("text.margin.left"), i);
    r.left = qMax(v.toInt(),0);
    v = getValue(name,KSL("text.margin.right"), i);
    r.right = qMax(v.toInt(),0);

    /* let's make button-like widgets a little compact */
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

  v = getValue(name,KSL("text.iconspacing"), i);
  r.tispace = qMax(v.toInt(),0);

  if(name == "ItemView")
  {
    r.tispace = qBound(6, r.tispace, 12);
    /* as in getFrameSpec() */
    r.left = qMin(r.left,6);
    r.right = qMin(r.right,6);
    r.top = qMin(r.top,6);
    r.bottom = qMin(r.bottom,6);
  }

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
  else if (name == "ToolbarComboBox")
    name = "ComboBox";
  else if (name == "ToolbarLineEdit")
    name = "LineEdit";

  QVariant v = getValue(name, KSL("inherits"));
  QString i = v.toString();

  v = getValue(name,KSL("min_height"), i);
  if (v.isValid())
  {
    QString value = v.toString();
    if (value.startsWith(QLatin1String("+")))
      r.incrementH = true;
    if (value.endsWith(QLatin1String("font")))
    { // multiply by the app font height
      r.minH = qMax(value.left(value.length()-4).toFloat(), 0.0f)
               * QFontMetrics(QApplication::font()).boundingRect(QLatin1Char('M')).height()*1.6;
    }
    else
      r.minH = qMax(v.toInt(),0);
    r.minH += r.minH % 2; // for vertical centering
  }

  v = getValue(name,KSL("min_width"), i);
  if (v.isValid())
  {
    QString value = v.toString();
    if (value.startsWith(QLatin1String("+")))
      r.incrementW = true;
    if (value.endsWith(QLatin1String("font")))
    { // multiply by the app font height
      r.minW = qMax(value.left(value.length()-4).toFloat(), 0.0f)
               * QFontMetrics(QApplication::font()).boundingRect(QLatin1Char('M')).height()*1.6;
    }
    else
      r.minW = qMax(v.toInt(),0);
  }

  sSpecs_[elementName] = r;
  return r;
}

theme_spec ThemeConfig::getCompositeSpec()
{
  bool compositing(false);

#if defined Q_WS_X11 || defined Q_OS_LINUX || defined Q_OS_FREEBSD || defined Q_OS_OPENBSD || defined Q_OS_NETBSD || defined Q_OS_HURD
  if (isX11_)
  {
    Atom atom = XInternAtom(QX11Info::display(), "_NET_WM_CM_S0", False);
    if (XGetSelectionOwner(QX11Info::display(), atom))
      compositing = true;
  }
  else if (QString::compare(QGuiApplication::platformName(), "wayland", Qt::CaseInsensitive) == 0)
  {
    /* Wayland is always composited.
       NOTE: Even under Linux, there's at least one situation, where the lack of x11
             doesn't mean Wayland: KWin-Wayland's menus and tooltips are polished
             when the platform name isn't set to "wayland". */
    compositing = true;
  }
#endif

  /* no blurring or window translucency without compositing */
  if (compositing)
  {
    if (compositeSpecs_.hasCompositor)
      return compositeSpecs_;

    compositeSpecs_.hasCompositor = true;

    QVariant v = getValue(KSL("General"),KSL("composite"));
    compositeSpecs_.composite = v.toBool();

    /* no window translucency or blurring without
       window interior element or reduced opacity */
    if (compositeSpecs_.composite)
    {
      interior_spec ispec = getInteriorSpec(KSL("WindowTranslucent"));
      if (ispec.element.isEmpty())
        ispec = getInteriorSpec(KSL("Window"));

      if (ispec.hasInterior
          || (!nonIntegerScale
              && getValue(KSL("General"),KSL("reduce_window_opacity")).toInt() != 0))
      {
        v = getValue(KSL("General"),KSL("translucent_windows"));
        if (v.isValid())
          compositeSpecs_.translucent_windows = v.toBool();

        /* no window blurring without window translucency */
        if (compositeSpecs_.translucent_windows)
        {
          v = getValue(KSL("General"),KSL("blurring"));
          if (v.isValid())
            compositeSpecs_.blurring = v.toBool();
        }
      }

      /* "blurring" is sufficient but not necessary for "popup_blurring" */
      if (compositeSpecs_.blurring)
        compositeSpecs_.popup_blurring = true;
      else
      {
        interior_spec ispecM = getInteriorSpec(KSL("Menu"));
        interior_spec ispecT = getInteriorSpec(KSL("ToolTip"));
        if (ispecM.hasInterior || ispecT.hasInterior)
        {
          v = getValue(KSL("General"),KSL("popup_blurring"));
          if (v.isValid())
            compositeSpecs_.popup_blurring = v.toBool();
        }
      }

      /* no menu/tooltip shadow without compositing */
      v = getValue(KSL("General"),KSL("menu_shadow_depth"));
      if (v.isValid())
        compositeSpecs_.menu_shadow_depth = qMax(v.toInt(),0);

      v = getValue(KSL("General"),KSL("tooltip_shadow_depth"));
      if (v.isValid())
        compositeSpecs_.tooltip_shadow_depth = qMax(v.toInt(),0);
    }

    return compositeSpecs_;
  }

  theme_spec r;
  default_theme_spec(r);
  return r;
}

theme_spec ThemeConfig::getThemeSpec()
{
  /* start with compositing */
  theme_spec r = getCompositeSpec();

  r.isX11 = isX11_;

  QVariant v = getValue(KSL("General"),KSL("author"));
  if (!v.toString().isEmpty())
    r.author = v.toString();

  v = getValue(KSL("General"),KSL("comment"));
  if (!v.toString().isEmpty())
    r.comment = v.toString();

  v = getValue(KSL("General"),KSL("reduce_window_opacity"));
  if (v.isValid()) // compositing will be checked by the code
    r.reduce_window_opacity = qMin(qMax(v.toInt(),-90),90);

  v = getValue(KSL("General"),KSL("reduce_menu_opacity"));
  if (v.isValid()) // compositing will be checked by the code
    r.reduce_menu_opacity = qMin(qMax(v.toInt(),0),90);

  v = getValue(KSL("General"),KSL("menu_separator_height"));
  if (v.isValid())
    r.menu_separator_height = qMin(qMax(v.toInt(),1),16);

  v = getValue(KSL("General"),KSL("spread_menuitems"));
  r.spread_menuitems = v.toBool();

  v = getValue(KSL("General"),KSL("shadowless_popup"));
  r.shadowless_popup = v.toBool();

    /* NOTE: The contrast effect is applied by BlurHelper, so that the following values have
             effect only for windows that can be blurred, whether they are blurred or not. */
  v = getValue(KSL("General"),KSL("contrast"));
  if (v.isValid()) // 1 by default
    r.contrast = qBound (static_cast<qreal>(0), v.toReal(), static_cast<qreal>(2));
  v = getValue(KSL("General"),KSL("intensity"));
  if (v.isValid()) // 1 by default
    r.intensity = qBound (static_cast<qreal>(0), v.toReal(), static_cast<qreal>(2));
  v = getValue(KSL("General"),KSL("saturation"));
  if (v.isValid()) // 1 by default
    r.saturation = qBound (static_cast<qreal>(0), v.toReal(), static_cast<qreal>(2));

  v = getValue(KSL("General"),KSL("x11drag"));
  if (v.isValid()) // "WindowManager::DRAG_ALL" by default
  {
    // backward compatibility
    if (!(v.toString() == "true" || v.toInt() == 1))
      r.x11drag = WindowManager::toDrag(v.toString());
  }

  v = getValue(KSL("General"),KSL("drag_from_buttons"));
  r.drag_from_buttons = v.toBool();

  v = getValue(KSL("General"),KSL("respect_DE"));
  if (v.isValid()) // true by default
    r.respect_DE = v.toBool();

  v = getValue(KSL("General"),KSL("alt_mnemonic"));
  if (v.isValid()) // true by default
    r.alt_mnemonic = v.toBool();

  v = getValue(KSL("General"),KSL("click_behavior"));
  r.click_behavior = v.toInt();

  v = getValue(KSL("General"),KSL("left_tabs"));
  r.left_tabs = v.toBool();

  v = getValue(KSL("General"),KSL("center_doc_tabs"));
  r.center_doc_tabs = v.toBool();

  v = getValue(KSL("General"),KSL("center_normal_tabs"));
  r.center_normal_tabs = v.toBool();

  v = getValue(KSL("General"),KSL("joined_inactive_tabs"));
  if (v.isValid()) // true by default
    r.joined_inactive_tabs = v.toBool();

  v = getValue(KSL("General"),KSL("attach_active_tab"));
  r.attach_active_tab = v.toBool();

  if (!r.attach_active_tab)
  {
    v = getValue(KSL("General"),KSL("embedded_tabs"));
    r.embedded_tabs = v.toBool();
  }

  v = getValue(KSL("General"),KSL("no_active_tab_separator"));
  r.no_active_tab_separator = v.toBool();

  v = getValue(KSL("General"),KSL("active_tab_overlap"));
  if (v.isValid()) // 0 by default
  {
    QString value = v.toString();
    if (value.endsWith(QLatin1String("font")))
    { // multiply by the app font height
      r.active_tab_overlap = qMax(value.left(value.length()-4).toFloat(), 0.0f)
                             * QFontMetrics(QApplication::font()).boundingRect(QLatin1Char('M')).height()*1.6;
    }
    else
      r.active_tab_overlap = qMax(v.toInt(),0);
  }

  v = getValue(KSL("General"),KSL("mirror_doc_tabs"));
  if (v.isValid()) // true by default
    r.mirror_doc_tabs = v.toBool();

  v = getValue(KSL("General"),KSL("no_inactive_tab_expansion"));
  r.no_inactive_tab_expansion = v.toBool();

  v = getValue(KSL("General"),KSL("tab_button_extra_margin"));
  if (v.isValid()) // 0 by default
  {
    int max = QFontMetrics(QApplication::font()).boundingRect(QLatin1Char('M')).height()*1.6;
    QString value = v.toString();
    if (value.endsWith(QLatin1String("font")))
    { // multiply by the app font height
      r.tab_button_extra_margin = qMin(qMax(value.left(value.length()-4).toFloat(), 0.0f),1.0f)
                                  * max;
    }
    else
      r.tab_button_extra_margin = qMin(qMax(v.toInt(),0),max);
  }

  v = getValue(KSL("General"),KSL("bold_active_tab"));
  r.bold_active_tab = v.toBool();

  v = getValue(KSL("General"),KSL("remove_extra_frames"));
  r.remove_extra_frames = v.toBool();

  v = getValue(KSL("General"),KSL("group_toolbar_buttons"));
  r.group_toolbar_buttons = v.toBool();

  if (!r.group_toolbar_buttons)
  {
    v = getValue(KSL("General"),KSL("toolbar_item_spacing"));
    if (v.isValid()) // 0 by default
      r.toolbar_item_spacing = qMax(v.toInt(),0);
  }

  v = getValue(KSL("General"),KSL("toolbar_interior_spacing"));
  if (v.isValid()) // 0 by default
    r.toolbar_interior_spacing = qMax(v.toInt(),0);

  v = getValue(KSL("General"),KSL("toolbar_separator_thickness"));
  if (v.isValid()) // -1 by default
    r.toolbar_separator_thickness = qMax(v.toInt(),0);

  v = getValue(KSL("General"),KSL("center_toolbar_handle"));
  r.center_toolbar_handle = v.toBool();

  v = getValue(KSL("General"),KSL("slim_toolbars"));
  r.slim_toolbars = v.toBool();

  v = getValue(KSL("General"),KSL("merge_menubar_with_toolbar"));
  r.merge_menubar_with_toolbar = v.toBool();

  v = getValue(KSL("General"),KSL("toolbutton_style"));
  if (v.isValid()) // 0 by default
    r.toolbutton_style = v.toInt();

  v = getValue(KSL("General"),KSL("spread_progressbar"));
  r.spread_progressbar = v.toBool();

  v = getValue(KSL("General"),KSL("progressbar_thickness"));
  if (v.isValid()) // 0 by default
  {
    QString value = v.toString();
    if (value.endsWith(QLatin1String("font")))
    { // multiply by the app font height
      r.progressbar_thickness = qMax(value.left(value.length()-4).toFloat(), 0.0f)
                                * QFontMetrics(QApplication::font()).boundingRect(QLatin1Char('M')).height()*1.6;
    }
    else
      r.progressbar_thickness = qMax(v.toInt(),0);
  }

  v = getValue(KSL("General"),KSL("spread_header"));
  r.spread_header = v.toBool();

  v = getValue(KSL("General"),KSL("menubar_mouse_tracking"));
  if (v.isValid()) //true by default
    r.menubar_mouse_tracking = v.toBool();

  v = getValue(KSL("General"),KSL("opaque"), QString()); // for going to the parent
  if (v.isValid())
    r.opaque << v.toStringList();

  v = getValue(KSL("General"),KSL("submenu_overlap"));
  if (v.isValid()) // -1 by default
    r.submenu_overlap = qMin(qMax(v.toInt(),0),16);

  v = getValue(KSL("General"),KSL("submenu_delay"));
  if (v.isValid()) // 250 by default
    r.submenu_delay = qMin(qMax(v.toInt(),-1),1000);

  v = getValue(KSL("General"),KSL("splitter_width"));
  if (v.isValid()) // 7 by default
    r.splitter_width = qMin(qMax(v.toInt(),0),32);

  v = getValue(KSL("General"),KSL("scroll_width"));
  if (v.isValid()) // 12 by default
    r.scroll_width = qMin(qMax(v.toInt(),0),32);

  v = getValue(KSL("General"),KSL("scroll_min_extent"));
  if (v.isValid()) // 36 by default
    r.scroll_min_extent = qMin(qMax(v.toInt(),16),100);

  v = getValue(KSL("General"),KSL("center_scrollbar_indicator"));
  r.center_scrollbar_indicator = v.toBool();

  v = getValue(KSL("General"),KSL("tree_branch_line"));
  r.tree_branch_line = v.toBool();

  v = getValue(KSL("General"),KSL("slider_width"));
  if (v.isValid()) // 8 by default
    r.slider_width = qMin(qMax(v.toInt(),0),48);

  v = getValue(KSL("General"),KSL("slider_handle_width"));
  if (v.isValid()) // 16 by default
    r.slider_handle_width = qMin(qMax(v.toInt(),0),48);

  v = getValue(KSL("General"),KSL("slider_handle_length"));
  if (v.isValid()) // 16 by default
    r.slider_handle_length = qMin(qMax(v.toInt(),0),48);

  v = getValue(KSL("General"),KSL("tickless_slider_handle_size"));
  r.tickless_slider_handle_size = qMin(qMax(v.toInt(),0),r.slider_handle_width);

  v = getValue(KSL("General"),KSL("check_size"));
  if (v.isValid()) //13 by default
    r.check_size = qMax(v.toInt(),0);

  v = getValue(KSL("General"),KSL("tooltip_delay"));
  if (v.isValid()) // -1 by default
    r.tooltip_delay = v.toInt();

  v = getValue(KSL("General"),KSL("vertical_spin_indicators"));
  r.vertical_spin_indicators = v.toBool();

  v = getValue(KSL("General"),KSL("inline_spin_indicators"));
  r.inline_spin_indicators = v.toBool();

  v = getValue(KSL("General"),KSL("inline_spin_separator"));
  r.inline_spin_separator = v.toBool();

  v = getValue(KSL("General"),KSL("spin_button_width"));
  if (v.isValid()) // 16 by default
    r.spin_button_width = qMin(qMax(v.toInt(),16), 32);

  v = getValue(KSL("General"),KSL("combo_as_lineedit"));
  r.combo_as_lineedit = v.toBool();
  if (!r.combo_as_lineedit)
  {
    v = getValue(KSL("General"),KSL("square_combo_button"));
    r.square_combo_button = v.toBool();
  }

  v = getValue(KSL("General"),KSL("combo_menu"));
  r.combo_menu = v.toBool();

  v = getValue(KSL("General"),KSL("hide_combo_checkboxes"));
  r.hide_combo_checkboxes = v.toBool();

  v = getValue(KSL("General"),KSL("combo_focus_rect"));
  r.combo_focus_rect = v.toBool();

  v = getValue(KSL("General"),KSL("scrollable_menu"));
  if (v.isValid()) // true by default
    r.scrollable_menu = v.toBool();

  v = getValue(KSL("General"),KSL("fill_rubberband"));
  r.fill_rubberband = v.toBool();

  v = getValue(KSL("General"),KSL("groupbox_top_label"));
  r.groupbox_top_label = v.toBool();

  /*v = getValue(KSL("General"),KSL("button_contents_shift"));
  if (v.isValid()) // true by default
    r.button_contents_shift = v.toBool();*/

  v = getValue(KSL("General"),KSL("transient_scrollbar"));
  r.transient_scrollbar = v.toBool();
  v = getValue(KSL("General"),KSL("transient_groove"));
  r.transient_groove = v.toBool();

  /* for technical reasons, we always set scrollbar_in_view
     to false with transient scrollbars and (try to) put
     them inside their scroll contents in another way */
  if (!r.transient_scrollbar)
  {
    v = getValue(KSL("General"),KSL("scrollbar_in_view"));
    r.scrollbar_in_view = v.toBool(); // false by default

    v = getValue(KSL("General"),KSL("scroll_arrows"));
    if (v.isValid()) // true by default
      r.scroll_arrows = v.toBool();
  }
  else
    r.scroll_arrows = false;

  v = getValue(KSL("General"),KSL("dialog_button_layout"));
  if (v.isValid()) // 0 by default
    r.dialog_button_layout = qMin(qMax(v.toInt(),0), 5);

  v = getValue(KSL("General"),KSL("layout_spacing"));
  if (v.isValid()) // 2 by default
    r.layout_spacing = qMin(qMax(v.toInt(),2), 16);

  v = getValue(KSL("General"),KSL("layout_margin"));
  if (v.isValid()) // 4 by default
    r.layout_margin = qMin(qMax(v.toInt(),2), 16);

  v = getValue(KSL("General"),KSL("small_icon_size"));
  if (v.isValid()) // 16 by default
    r.small_icon_size = qMin(qMax(v.toInt(),16), 48);

  v = getValue(KSL("General"),KSL("large_icon_size"));
  if (v.isValid()) // 32 by default
    r.large_icon_size = qMin(qMax(v.toInt(),24), 128);

  v = getValue(KSL("General"),KSL("button_icon_size"));
  if (v.isValid()) // 16 by default
    r.button_icon_size = qMin(qMax(v.toInt(),16), 64);

  v = getValue(KSL("General"),KSL("toolbar_icon_size"));
  if (v.isValid()) // 22 by default
  {
    int icnSize;
    if (v.toString() == "font")
      icnSize = QFontMetrics(QApplication::font()).height();
    else
      icnSize = v.toInt();
    r.toolbar_icon_size = qMin(qMax(icnSize,16), 64);
  }
  else if (r.slim_toolbars)
    r.toolbar_icon_size = 16;

  v = getValue(KSL("General"),KSL("animate_states"));
  r.animate_states = v.toBool();

  v = getValue(KSL("General"),KSL("no_inactiveness"));
  r.no_inactiveness = v.toBool();

  v = getValue(KSL("General"),KSL("no_window_pattern"));
  r.no_window_pattern = v.toBool();

  v = getValue(KSL("General"),KSL("dark_titlebar"));
  r.dark_titlebar = v.toBool();

  v = getValue(KSL("General"),KSL("menu_blur_radius"));
  if (v.isValid())
    r.menu_blur_radius = qMin(v.toInt(),10);

  v = getValue(KSL("General"),KSL("tooltip_blur_radius"));
  if (v.isValid())
    r.tooltip_blur_radius = qMin(v.toInt(),10);

  return r;
}

color_spec ThemeConfig::getColorSpec() const
{
  color_spec r;
  default_color_spec(r);

  QVariant v = getValue(KSL("GeneralColors"),KSL("window.color"));
  r.windowColor = v.toString();

  v = getValue(KSL("GeneralColors"),KSL("inactive.window.color"));
  r.inactiveWindowColor = v.toString();

  v = getValue(KSL("GeneralColors"),KSL("base.color"));
  r.baseColor = v.toString();

  v = getValue(KSL("GeneralColors"),KSL("inactive.base.color"));
  r.inactiveBaseColor = v.toString();

  v = getValue(KSL("GeneralColors"),KSL("alt.base.color"));
  r.altBaseColor = v.toString();

  v = getValue(KSL("GeneralColors"),KSL("inactive.alt.base.color"));
  r.inactiveAltBaseColor = v.toString();

  v = getValue(KSL("GeneralColors"),KSL("button.color"));
  r.buttonColor = v.toString();

  v = getValue(KSL("GeneralColors"),KSL("light.color"));
  r.lightColor = v.toString();

  v = getValue(KSL("GeneralColors"),KSL("mid.light.color"));
  r.midLightColor = v.toString();

  v = getValue(KSL("GeneralColors"),KSL("dark.color"));
  r.darkColor = v.toString();

  v = getValue(KSL("GeneralColors"),KSL("mid.color"));
  r.midColor = v.toString();

  v = getValue(KSL("GeneralColors"),KSL("shadow.color"));
  if (v.isValid())
    r.shadowColor = v.toString();

  v = getValue(KSL("GeneralColors"),KSL("highlight.color"));
  r.highlightColor = v.toString();

  v = getValue(KSL("GeneralColors"),KSL("inactive.highlight.color"));
  r.inactiveHighlightColor = v.toString();

  v = getValue(KSL("GeneralColors"),KSL("tooltip.base.color"));
  r.tooltipBaseColor = v.toString();

  v = getValue(KSL("GeneralColors"),KSL("text.color"));
  r.textColor = v.toString();

  v = getValue(KSL("GeneralColors"),KSL("inactive.text.color"));
  r.inactiveTextColor = v.toString();

  v = getValue(KSL("GeneralColors"),KSL("window.text.color"));
  r.windowTextColor = v.toString();

  v = getValue(KSL("GeneralColors"),KSL("inactive.window.text.color"));
  r.inactiveWindowTextColor = v.toString();

  v = getValue(KSL("GeneralColors"),KSL("button.text.color"));
  r.buttonTextColor = v.toString();

  v = getValue(KSL("GeneralColors"),KSL("disabled.text.color"));
  r.disabledTextColor = v.toString();

  v = getValue(KSL("GeneralColors"),KSL("tooltip.text.color"));
  r.tooltipTextColor = v.toString();

  v = getValue(KSL("GeneralColors"),KSL("highlight.text.color"));
  r.highlightTextColor = v.toString();

  v = getValue(KSL("GeneralColors"),KSL("inactive.highlight.text.color"));
  r.inactiveHighlightTextColor = v.toString();

  v = getValue(KSL("GeneralColors"),KSL("link.color"));
  r.linkColor = v.toString();

  v = getValue(KSL("GeneralColors"),KSL("link.visited.color"));
  r.linkVisitedColor = v.toString();

  v = getValue(KSL("GeneralColors"),KSL("progress.indicator.text.color"));
  r.progressIndicatorTextColor = v.toString();

  v = getValue(KSL("GeneralColors"),KSL("progress.inactive.indicator.text.color"));
  r.progressInactiveIndicatorTextColor = v.toString();

  return r;
}

hacks_spec ThemeConfig::getHacksSpec() const
{
  hacks_spec r;
  default_hacks_spec(r);

  QVariant v = getValue(KSL("Hacks"),KSL("transparent_dolphin_view"));
  r.transparent_dolphin_view = v.toBool();

  v = getValue(KSL("Hacks"),KSL("transparent_pcmanfm_sidepane"));
  r.transparent_pcmanfm_sidepane = v.toBool();

  v = getValue(KSL("Hacks"),KSL("transparent_pcmanfm_view"));
  r.transparent_pcmanfm_view = v.toBool();

  v = getValue(KSL("Hacks"),KSL("lxqtmainmenu_iconsize"));
  if (v.isValid())
    r.lxqtmainmenu_iconsize = qMin(qMax(v.toInt(),0),32);

  v = getValue(KSL("Hacks"),KSL("blur_translucent"));
  r.blur_translucent = v.toBool();

  v = getValue(KSL("Hacks"),KSL("transparent_ktitle_label"));
  r.transparent_ktitle_label = v.toBool();

  v = getValue(KSL("Hacks"),KSL("transparent_menutitle"));
  r.transparent_menutitle = v.toBool();

  v = getValue(KSL("Hacks"),KSL("respect_darkness"));
  r.respect_darkness = v.toBool();

  v = getValue(KSL("Hacks"),KSL("force_size_grip"));
  r.forceSizeGrip = v.toBool();

  v = getValue(KSL("Hacks"),KSL("tint_on_mouseover"));
  if (v.isValid())
    r.tint_on_mouseover = qMin(qMax(v.toInt(),0),100);

  v = getValue(KSL("Hacks"),KSL("no_selection_tint"));
  r.no_selection_tint = v.toBool();

  v = getValue(KSL("Hacks"),KSL("disabled_icon_opacity"));
  if (v.isValid())
    r.disabled_icon_opacity = qMin(qMax(v.toInt(),0),100);

  v = getValue(KSL("Hacks"),KSL("normal_default_pushbutton"));
  r.normal_default_pushbutton = v.toBool();

  v = getValue(KSL("Hacks"),KSL("iconless_pushbutton"));
  r.iconless_pushbutton = v.toBool();

  v = getValue(KSL("Hacks"),KSL("transparent_arrow_button"));
  r.transparent_arrow_button = v.toBool();

  v = getValue(KSL("Hacks"),KSL("iconless_menu"));
  r.iconless_menu = v.toBool();

  v = getValue(KSL("Hacks"),KSL("single_top_toolbar"));
  if (v.toBool())
    r.single_top_toolbar = true; // false by default
  else
  { // vertical toolbars could be styled only if all toolbars can
    v = getValue(KSL("Hacks"),KSL("style_vertical_toolbars"));
    r.style_vertical_toolbars = v.toBool();
  }

  v = getValue(KSL("Hacks"),KSL("middle_click_scroll"));
  r.middle_click_scroll = v.toBool();

  v = getValue(KSL("Hacks"),KSL("centered_forms"));
  r.centered_forms = v.toBool();

  v = getValue(KSL("Hacks"),KSL("kinetic_scrolling"));
  r.kinetic_scrolling = v.toBool();

  v = getValue(KSL("Hacks"),KSL("noninteger_translucency"));
  r.noninteger_translucency = v.toBool();

  v = getValue(KSL("Hacks"),KSL("blur_only_active_window"));
  r.blur_only_active_window = v.toBool();

  return r;
}

}
