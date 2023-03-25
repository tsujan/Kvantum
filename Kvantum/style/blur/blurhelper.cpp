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

#include "blurhelper.h"

#include <QEvent>
#include <QMenu>
#include <QFrame>
#include <QWindow>

#ifdef NO_KF
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
#include <QVector>
#include <QX11Info>
#endif
#include <QApplication>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#else
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
#include <KWindowEffects>
#endif
#endif

namespace Kvantum {
BlurHelper::BlurHelper (QObject* parent, QList<qreal> menuS, QList<qreal> tooltipS,
                        int menuBlurRadius, int toolTipBlurRadius,
                        qreal contrast, qreal intensity, qreal saturation,
                        bool onlyActiveWindow) : QObject (parent)
{
#ifdef NO_KF
  isX11_ = (QString::compare(QGuiApplication::platformName(), "xcb", Qt::CaseInsensitive) == 0);
  if (isX11_)
  {
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
    atom_blur_ = XInternAtom (QX11Info::display(), "_KDE_NET_WM_BLUR_BEHIND_REGION", False);
#else
    if (auto x11NativeInterfce = qApp->nativeInterface<QNativeInterface::QX11Application>())
      atom_blur_ = XInternAtom (x11NativeInterfce->display(), "_KDE_NET_WM_BLUR_BEHIND_REGION", False);
    else
      atom_blur_ = None;
#endif
  }
  else
    atom_blur_ = None;
#endif

  contrast_ = qBound (static_cast<qreal>(0), contrast, static_cast<qreal>(2));
  intensity_ = qBound (static_cast<qreal>(0), intensity, static_cast<qreal>(2));
  saturation_ = qBound (static_cast<qreal>(0), saturation, static_cast<qreal>(2));

  menuBlurRadius_ = menuBlurRadius;
  toolTipBlurRadius_ = toolTipBlurRadius;

  if (!menuS.isEmpty() && menuS.size() >= 4)
    menuShadow_ = menuS;
  if (!tooltipS.isEmpty() && tooltipS.size() >= 4)
    tooltipShadow_ = tooltipS;

  onlyActiveWindow_ = onlyActiveWindow;
}
/*************************/
void BlurHelper::registerWidget (QWidget* widget)
{
  /* these conditions are taken care of in polishing.cpp -> polish(QWidget *widget) */
  /*if (widget->isWindow()
      && widget->testAttribute (Qt::WA_TranslucentBackground)
      && widget->windowType() != Qt::Desktop
      && !widget->testAttribute (Qt::WA_X11NetWmWindowTypeDesktop)
      && !widget->testAttribute (Qt::WA_PaintOnScreen)
      && !widget->inherits ("KScreenSaver")
      && !widget->inherits ("QTipLabel")
      && !widget->inherits ("QSplashScreen")
      && !widget->windowFlags().testFlag(Qt::FramelessWindowHint))*/

    widget->installEventFilter (this);
}
/*************************/
void BlurHelper::unregisterWidget (QWidget* widget)
{
  if (widget)
  {
    widget->removeEventFilter (this);
    clear (widget);
  }
}
/*************************/
bool BlurHelper::isWidgetActive (const QWidget *widget) const
{
  return (widget->window()->windowFlags().testFlag(Qt::WindowDoesNotAcceptFocus)
          || widget->window()->windowFlags().testFlag(Qt::X11BypassWindowManagerHint)
          || widget->isActiveWindow()
          // make exception for tooltips
          || widget->inherits("QTipLabel")
          || ((widget->windowFlags() & Qt::WindowType_Mask) == Qt::ToolTip
              && !qobject_cast<const QFrame*>(widget)));
}
/*************************/
bool BlurHelper::eventFilter (QObject* object, QEvent* event)
{
  switch (event->type())
  {
    case QEvent::Show:
    case QEvent::Hide:
    case QEvent::Resize:
    /* the theme may change from
       Kvantum and to it again */
    case QEvent::StyleChange: {
      QWidget* widget (qobject_cast<QWidget*>(object));
      /* take precautions */
      if (!widget || !widget->isWindow()) break;
      if (!onlyActiveWindow_ || isWidgetActive (widget))
      {
        pendingWidgets_.insert (widget, widget);
        delayedUpdate();
      }
      break;
    }

    case QEvent::WindowActivate:
    case QEvent::WindowDeactivate: {
      if (onlyActiveWindow_)
      {
        QWidget* widget (qobject_cast<QWidget*>(object));
        if (!widget || !widget->isWindow()) break;
        if (event->type() == QEvent::WindowDeactivate)
          update (widget); // otherwise artifacts are possible
        else
        {
          pendingWidgets_.insert (widget, widget);
          delayedUpdate();
        }
      }
      break;
    }

    default: break;
  }

  // never eat events
  return false;
}
/*************************/
static inline int ceilingInt (const qreal r)
{ // to prevent 1-px blurred strips outside menu/tooltip borders
  int res = qRound(r);
  if (r - static_cast<qreal>(res) > static_cast<qreal>(0.1))
    res += 1;
  return res;
}

QRegion BlurHelper::blurRegion (QWidget* widget) const
{
  if (!widget->isVisible())
    return QRegion();

  if (onlyActiveWindow_ && !isWidgetActive (widget))
    return QRegion();

  QRect rect = widget->rect();
  QRegion wMask = widget->mask();

  qreal dpr = 1;
#ifdef NO_KF
  if (isX11_)
  {
    QWindow *win = widget->window()->windowHandle();
    dpr = win ? win->devicePixelRatio() : qApp->devicePixelRatio();
  }
#endif

  /* blurring may not be suitable when the available
     painting area is restricted by a widget mask */
  if (!wMask.isEmpty())
  {
    if (wMask != QRegion(rect))
      return QRegion();
#ifdef NO_KF
    QRect mr = wMask.boundingRect();
    if (dpr > static_cast<qreal>(1))
      mr.setSize (QSizeF(mr.size() * dpr).toSize());
    return mr;
#else
    return wMask;
#endif
  }

  QList<qreal> r;
  int radius = 0;
  if ((qobject_cast<QMenu*>(widget)
       && !widget->testAttribute(Qt::WA_X11NetWmWindowTypeMenu)) // not a detached menu
      || widget->inherits("QComboBoxPrivateContainer"))
  {
    if (!widget->testAttribute(Qt::WA_StyleSheetTarget))
      r = menuShadow_;
    radius = menuBlurRadius_;
  }
  else if (widget->inherits("QTipLabel")
           /* unusual tooltips (like in KDE system settings) */
           || ((widget->windowFlags() & Qt::WindowType_Mask) == Qt::ToolTip
               && !qobject_cast<QFrame*>(widget)))
  {
    if (!widget->testAttribute(Qt::WA_StyleSheetTarget))
      r = tooltipShadow_;
    radius = toolTipBlurRadius_;
  }

#ifdef NO_KF
  if (dpr > static_cast<qreal>(1))
  {
    rect.setSize (QSizeF(rect.size() * dpr).toSize());
    radius *= qRound (dpr * 2);
  }
#endif

  if (!r.isEmpty())
  {
    rect.adjust (ceilingInt (dpr * r.at (0)),
                 ceilingInt (dpr * r.at (1)),
                 -ceilingInt (dpr * r.at (2)),
                 -ceilingInt (dpr * r.at (3)));
  }

  if (radius > 0)
  {
    radius = qMin (radius, qMin (rect.width(), rect.height()) / 2);
    QSize rSize (radius, radius);
    QRegion topLeft (QRect (rect.topLeft(), 2 * rSize), QRegion::Ellipse);
    QRegion topRight (QRect (rect.topLeft() + QPoint(rect.width() - 2 * radius, 0),
                             2 * rSize),
                      QRegion::Ellipse);
    QRegion bottomLeft (QRect (rect.topLeft() + QPoint(0, rect.height() - 2 * radius),
                               2 * rSize),
                        QRegion::Ellipse);
    QRegion bottomRight (QRect (rect.topLeft() + QPoint (rect.width() - 2 * radius,
                                                         rect.height() - 2 * radius),
                                2 * rSize),
                         QRegion::Ellipse);
    return topLeft.united (topRight).united (bottomLeft).united (bottomRight)
           .united (QRect (rect.topLeft() + QPoint (radius, 0),
                           QSize (rect.width() - 2 * radius, rect.height())))
           .united (QRect (rect.topLeft() + QPoint (0, radius),
                           QSize (rect.width(), rect.height() - 2 * radius)));
  }
  else
    return rect;
}
/*************************/
void BlurHelper::update (QWidget* widget) const
{
#ifdef NO_KF
  if (!isX11_)
    return;
#elif (QT_VERSION >= QT_VERSION_CHECK(6,0,0))
  return;
#endif

  QWindow *win = widget->windowHandle();
  if (win == nullptr)
    return;

  const QRegion region (blurRegion (widget));
  if (region.isEmpty())
    clear (widget);
  else
  {
#ifdef NO_KF
    if (!widget->internalWinId())
      return;
    Display *display = nullptr;
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
    QVector<unsigned long> data;
    display = QX11Info::display();
#else
    QList<unsigned long> data;
    if (auto x11NativeInterfce = qApp->nativeInterface<QNativeInterface::QX11Application>())
      display = x11NativeInterfce->display();
#endif
    if (display == nullptr)
      return;
    for (QRegion::const_iterator it = region.begin(); it != region.end(); ++ it)
    {
      data << (*it).x() << (*it).y() << (*it).width() << (*it).height();
    }
    XChangeProperty (display, widget->internalWinId(),
                     atom_blur_, XA_CARDINAL, 32, PropModeReplace,
                     reinterpret_cast<const unsigned char*>(data.constData()),
                     data.size());
#elif (QT_VERSION < QT_VERSION_CHECK(6,0,0))
    KWindowEffects::enableBlurBehind (win, true, region);
    /* NOTE: The contrast effect isn't used with menus and tooltips
             because their borders may be anti-aliased. */
    if ((contrast_ != static_cast<qreal>(1)
         || intensity_ != static_cast<qreal>(1)
         || saturation_ != static_cast<qreal>(1))
        && !qobject_cast<QMenu*>(widget)
        && !widget->inherits("QTipLabel")
        && ((widget->windowFlags() & Qt::WindowType_Mask) != Qt::ToolTip
            || qobject_cast<QFrame*>(widget)))
    {
      KWindowEffects::enableBackgroundContrast (win, true,
                                                contrast_, intensity_, saturation_,
                                                region);
    }
#endif
  }
  // force update
  if (widget->isVisible())
    widget->update();
}
/*************************/
void BlurHelper::clear (QWidget* widget) const
{
#ifdef NO_KF
  if (!isX11_)
    return;
#elif (QT_VERSION >= QT_VERSION_CHECK(6,0,0))
  Q_UNUSED (widget)
  return;
#endif

#ifdef NO_KF
  Display *display = nullptr;
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
  display = QX11Info::display();
#else
  if (auto x11NativeInterfce = qApp->nativeInterface<QNativeInterface::QX11Application>())
    display = x11NativeInterfce->display();
#endif
  if (display && widget->internalWinId())
    XDeleteProperty (display, widget->internalWinId(), atom_blur_);
#elif (QT_VERSION < QT_VERSION_CHECK(6,0,0))
  QWindow *win = widget->windowHandle();
  if (win != nullptr)
  {
    KWindowEffects::enableBlurBehind (win, false);
    if ((contrast_ != static_cast<qreal>(1)
         || intensity_ != static_cast<qreal>(1)
         || saturation_ != static_cast<qreal>(1))
        && !qobject_cast<QMenu*>(widget)
        && !widget->inherits("QTipLabel")
        && ((widget->windowFlags() & Qt::WindowType_Mask) != Qt::ToolTip
            || qobject_cast<QFrame*>(widget)))
    {
      KWindowEffects::enableBackgroundContrast (win, false);
    }
  }
#endif
}

}
