// Adapted from oxygenwindowmanager.cpp svnversion: 1139230
// and QtCurve's windowmanager.cpp v1.8.17

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

#ifndef WINDOWMANAGER_H
#define WINDOWMANAGER_H

#include <QBasicTimer>
#include <QSet>
#include <QWeakPointer>
#include <QWidget>

namespace Kvantum {
class WindowManager: public QObject
{
  Q_OBJECT
public:
  enum Drag {
    DRAG_NONE,
    DRAG_MENUBAR_ONLY,
    DRAG_MENUBAR_AND_PRIMARY_TOOLBAR,
    DRAG_ALL,

    DRAG_COUNT
  };
  static Drag toDrag (const QString &str)
  {
    for (int i = 0; i < DRAG_COUNT; ++i)
    {
      if (toStr ((Drag)i) == str)
        return (Drag)i;
    }
    return DRAG_NONE;
  }

  static QString toStr (Drag drag)
  {
    switch (drag)
    {
      default:
      case DRAG_ALL: return "all";
      case DRAG_NONE: return "none";
      case DRAG_MENUBAR_ONLY: return "menubar";
      case DRAG_MENUBAR_AND_PRIMARY_TOOLBAR: return "menubar_and_primary_toolbar";
    }
  }

  explicit WindowManager (QObject *parent, Drag drag);
  virtual ~WindowManager (void) {}
  // initialize
  /* read relevant options from OxygenStyleConfigData */
  void initialize (const QStringList &whiteList=QStringList(),
                   const QStringList &blackList=QStringList());
  // register widget
  void registerWidget (QWidget*);
  // unregister widget
  void unregisterWidget (QWidget*);
  // event filter [reimplemented]
  virtual bool eventFilter (QObject*, QEvent*);
protected:
  // timer event,
  /* used to start drag if button is pressed for a long enough time */
  void timerEvent (QTimerEvent*);
  // mouse press event
  bool mousePressEvent (QObject*, QEvent*);
  // mouse move event
  bool mouseMoveEvent (QObject*, QEvent*);
  // mouse release event
  bool mouseReleaseEvent (QObject*, QEvent*);
  // @name configuration
  // @{
  // enable state
  bool enabled() const
  {
    return enabled_;
  }
  // enable state
  void setEnabled (bool value)
  {
    enabled_ = value;
  }
  // drag distance (pixels)
  void setDragDistance (int value)
  {
    dragDistance_ = value;
  }
  // drag delay (msec)
  void setDragDelay (int value)
  {
    dragDelay_ = value;
  }

  // set list of whiteListed widgets
  /*
    white list is read from options and is used to adjust
    per-app window dragging issues
  */
  void initializeWhiteList (const QStringList &list);

  // set list of blackListed widgets
  /*
    black list is read from options and is used to adjust
    per-app window dragging issues
  */
  void initializeBlackList (const QStringList &list);
  // @}
  // returns true if widget is dragable
  bool isDragable (QWidget*);
  // returns true if widget is dragable
  bool isBlackListed (QWidget*);
  // returns true if widget is dragable
  bool isWhiteListed (QWidget*) const;
  // returns true if drag can be started from current widget
  bool canDrag (QWidget*);
  // returns true if drag can be started from current widget and position
  /* child at given position is passed as second argument */
  bool canDrag (QWidget*, QWidget*, const QPoint&);
  // reset drag
  void resetDrag();
  // start drag
  void startDrag (QWidget*, const QPoint&);
  // utility function
  bool isDockWidgetTitle (const QWidget*) const;
  // @name lock
  // @{
  void setLocked (bool value)
  {
    locked_ = value;
  }
  // lock
  bool isLocked() const
  {
    return locked_;
  }
  // @}
private:
  // enability
  bool enabled_;
  // drag distance
  /* this is copied from kwin::geometry */
  int dragDistance_;
  // drag delay
  /* this is copied from kwin::geometry */
  int dragDelay_;

  // wrapper for exception id
  class ExceptionId: public QPair<QString, QString>
  {
  public:
    ExceptionId (const QString &value)
    {
      const QStringList args (value.split ("@"));
      if (args.isEmpty())
        return;
      second = args[0].trimmed();
      if (args.size() > 1)
        first = args[1].trimmed();
    }
    const QString& appName() const
    {
            return first;
    }
    const QString& className() const
    {
            return second;
    }
  };

  // exception set
  typedef QSet<ExceptionId> ExceptionSet;
  // list of white listed special widgets
  /*
    it is read from options and is used to adjust
    per-app window dragging issues
  */
  ExceptionSet whiteList_;
  // list of black listed special widgets
  /*
    it is read from options and is used to adjust
    per-app window dragging issues
  */
  ExceptionSet blackList_;
  // ! drag point
  QPoint dragPoint_;
  QPoint globalDragPoint_;
  // drag timer
  QBasicTimer dragTimer_;
  // target being dragged
  /* QWeakPointer is used in case the target
     gets deleted while drag is in progress */
  QWeakPointer<QWidget> target_;
  // true if drag is about to start
  bool dragAboutToStart_;
  // true if drag is in progress
  bool dragInProgress_;
  // true if drag is locked
  bool locked_;
  Drag drag_;

  // provide application-wise event filter
  /*
    it us used to unlock dragging and make sure event look
    is properly restored after a drag has occurred
  */
  class AppEventFilter: public QObject
  {
  public:
    AppEventFilter (WindowManager *parent) :
                   QObject (parent),
                   parent_ (parent)
    {}
  virtual bool eventFilter (QObject*, QEvent*);
  protected:
    // application-wise event.
    /* needed to catch end of XMoveResize events */
    bool appMouseEvent (QObject*, QEvent*);
  private:
    // parent
    WindowManager *parent_;
  };

  // application event filter
  AppEventFilter *_appEventFilter;
  // allow access of all private members to the app event filter
  friend class AppEventFilter;
};
}

#endif
