// Adapted from Oxygen-Transparent -> oxygenblurhelper.h

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

#ifndef BLURHELPER_H
#define BLURHELPER_H

#include <QObject>
#include <QPointer>
#include <QHash>
#include <QBasicTimer>
#include <QTimerEvent>
#include <QDockWidget>
#include <QMenu>
#include <QRegion>
#include <QToolBar>

#if defined Q_WS_X11 || defined Q_OS_LINUX
#include <X11/Xdefs.h>
#endif

/* A class for blurring the region behind a translucent window in KDE. */
class BlurHelper: public QObject
{
  Q_OBJECT

  public:

    BlurHelper (QObject*, QList<int> menuS, QList<int> tooltipS);

    virtual ~BlurHelper (void) {}

    void registerWidget (QWidget*);
    void unregisterWidget (QWidget*);
    virtual bool eventFilter (QObject*, QEvent*);

  protected:

    /* Timer event, used to perform delayed 
       update of blur regions of pending widgets. */
    virtual void timerEvent (QTimerEvent* event)
    {
      if (event->timerId() == _timer.timerId())
      {
        _timer.stop();
        update();
      }
      else
        QObject::timerEvent (event);
    }

    /* The blur-behind region for a given widget. */
    QRegion blurRegion (QWidget*) const;

    /* Update blur region for all pending widgets. A timer is
       used to allow some buffering of the update requests. */
    void delayedUpdate (void)
    {
      if (!_timer.isActive())
        _timer.start (10, this);
    }
    void update (void)
    {
      foreach (const WidgetPointer& widget, _pendingWidgets)
      {
        if (widget)
          update (widget.data());
      }
      _pendingWidgets.clear();
    }

    /* Update blur regions for given widget. */
    void update (QWidget*) const;

    /* Clear blur regions for given widget. */
    void clear (QWidget*) const;

  private:

    /* List of widgets for which blur region must be updated. */
    typedef QPointer<QWidget> WidgetPointer;
    typedef QHash<QWidget*, WidgetPointer> WidgetSet;
    WidgetSet _pendingWidgets;

    /* Delayed update timer. */
    QBasicTimer _timer;

    /* Dimensions of pure shadows of menus and tooltips.
       (left, top, right, bottom) */
    QList<int> menuShadow;
    QList<int> tooltipShadow;

#if defined Q_WS_X11 || defined Q_OS_LINUX
    /* The required atom. */
    Atom _atom_blur;
#endif
};

#endif
