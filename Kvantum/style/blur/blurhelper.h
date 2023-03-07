// Some functions were adapted from Oxygen-Transparent -> oxygenblurhelper.h

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

#ifndef BLURHELPER_H
#define BLURHELPER_H

#include <QWidget>
#include <QPointer>
#include <QHash>
#include <QBasicTimer>
#include <QTimerEvent>
#include <QRegion>

#ifdef NO_KF
#include <X11/Xdefs.h>
#endif

namespace Kvantum {
/* A class for blurring the region behind a translucent window in KDE. */
class BlurHelper: public QObject
{
  Q_OBJECT

  public:

    BlurHelper (QObject*, QList<qreal> menuS, QList<qreal> tooltipS,
                int menuBlurRadius = 0, int toolTipBlurRadius = 0,
                qreal contrast = static_cast<qreal>(1),
                qreal intensity = static_cast<qreal>(1),
                qreal saturation = static_cast<qreal>(1),
                bool onlyActiveWindow = false);

    virtual ~BlurHelper() {}

    void registerWidget (QWidget*);
    void unregisterWidget (QWidget*);
    virtual bool eventFilter (QObject*, QEvent*);

  protected:

    /* Timer event, used to perform delayed
       update of blur regions of pending widgets. */
    virtual void timerEvent (QTimerEvent* event)
    {
      if (event->timerId() == timer_.timerId())
      {
        timer_.stop();
        update();
      }
      else
        QObject::timerEvent (event);
    }

    /* The blur-behind region for a given widget. */
    QRegion blurRegion (QWidget*) const;

    /* Update blur region for all pending widgets. A timer is
       used to allow some buffering of the update requests. */
    void delayedUpdate()
    {
      if (!timer_.isActive())
        timer_.start (10, this);
    }
    void update()
    {
      for (const WidgetPointer& widget : static_cast<const WidgetSet&>(pendingWidgets_))
      {
        if (widget)
          update (widget.data());
      }
      pendingWidgets_.clear();
    }

    /* Update blur regions for given widget. */
    void update (QWidget*) const;

    /* Clear blur regions for given widget. */
    void clear (QWidget*) const;

  private:

    bool isWidgetActive (const QWidget *widget) const;

    /* List of widgets for which blur region must be updated. */
    typedef QPointer<QWidget> WidgetPointer;
    typedef QHash<QWidget*, WidgetPointer> WidgetSet;
    WidgetSet pendingWidgets_;

    /* Delayed update timer. */
    QBasicTimer timer_;

    /* Dimensions of pure shadows of menus and tooltips.
       (left, top, right, bottom) */
    QList<qreal> menuShadow_;
    QList<qreal> tooltipShadow_;

    int menuBlurRadius_;
    int toolTipBlurRadius_;

    qreal contrast_, intensity_, saturation_;

    bool onlyActiveWindow_;

#ifdef NO_KF
    /* The required atom. */
    Atom atom_blur_;
    bool isX11_;
#endif
};
}

#endif
