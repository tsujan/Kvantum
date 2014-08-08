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

#include <QEvent>
#include <QBasicTimer>
#include <QObject>
#include <QSet>
#include <QString>
#include <QWeakPointer>
#include <QWidget>

class WindowManager: public QObject
{
  Q_OBJECT
public:
  explicit WindowManager (QObject*);
  virtual ~WindowManager (void) {}
  // ! initialize
  /*! read relevant options from OxygenStyleConfigData */
  void initialize (const QStringList &whiteList=QStringList(),
                   const QStringList &blackList=QStringList());
  // ! register widget
  void registerWidget (QWidget*);
  // ! unregister widget
  void unregisterWidget (QWidget*);
  // ! event filter [reimplemented]
  virtual bool eventFilter (QObject*, QEvent*);
protected:
  // ! timer event,
  /*! used to start drag if button is pressed for a long enough time */
  void timerEvent (QTimerEvent*);
  // ! mouse press event
  bool mousePressEvent (QObject*, QEvent*);
  // ! mouse move event
  bool mouseMoveEvent (QObject*, QEvent*);
  // ! mouse release event
  bool mouseReleaseEvent (QObject*, QEvent*);
  // !@name configuration
  // @{
  // ! enable state
  bool enabled() const
  {
    return _enabled;
  }
  // ! enable state
  void setEnabled (bool value)
  {
    _enabled = value;
  }
  // ! returns true if window manager is used for moving
  bool useWMMoveResize() const
  {
    return _useWMMoveResize;
  }
  // ! drag distance (pixels)
  void setDragDistance (int value)
  {
    _dragDistance = value;
  }
  // ! drag delay (msec)
  void setDragDelay (int value)
  {
    _dragDelay = value;
  }

  // ! set list of whiteListed widgets
  /*!
    white list is read from options and is used to adjust
    per-app window dragging issues
  */
  void initializeWhiteList (const QStringList &list);

  // ! set list of blackListed widgets
  /*!
    black list is read from options and is used to adjust
    per-app window dragging issues
  */
  void initializeBlackList (const QStringList &list);
  // @}
  // ! returns true if widget is dragable
  bool isDragable (QWidget*);
  // ! returns true if widget is dragable
  bool isBlackListed (QWidget*);
  // ! returns true if widget is dragable
  bool isWhiteListed (QWidget*) const;
  // ! returns true if drag can be started from current widget
  bool canDrag (QWidget*);
  // ! returns true if drag can be started from current widget and position
  /*! child at given position is passed as second argument */
  bool canDrag (QWidget*, QWidget*, const QPoint&);
  // ! reset drag
  void resetDrag();
  // ! start drag
  void startDrag (QWidget*, const QPoint&);
  // ! utility function
  bool isDockWidgetTitle (const QWidget*) const;
  // !@name lock
  // @{
  void setLocked (bool value)
  {
    _locked = value;
  }
  // ! lock
  bool isLocked() const
  {
    return _locked;
  }
  // @}
private:
  // ! enability
  bool _enabled;
  // ! use WM moveResize
  bool _useWMMoveResize;
  // ! drag distance
  /*! this is copied from kwin::geometry */
  int _dragDistance;
  // ! drag delay
  /*! this is copied from kwin::geometry */
  int _dragDelay;

  // ! wrapper for exception id
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

  // ! exception set
  typedef QSet<ExceptionId> ExceptionSet;
  // ! list of white listed special widgets
  /*!
    it is read from options and is used to adjust
    per-app window dragging issues
  */
  ExceptionSet _whiteList;
  // ! list of black listed special widgets
  /*!
    it is read from options and is used to adjust
    per-app window dragging issues
  */
  ExceptionSet _blackList;
  // ! drag point
  QPoint _dragPoint;
  QPoint _globalDragPoint;
  // ! drag timer
  QBasicTimer _dragTimer;
  // ! target being dragged
  /*! QWeakPointer is used in case the target gets deleted while drag
      is in progress */
  QWeakPointer<QWidget> _target;
  // ! true if drag is about to start
  bool _dragAboutToStart;
  // ! true if drag is in progress
  bool _dragInProgress;
  // ! true if drag is locked
  bool _locked;
  // ! cursor override
  /*! used to keep track of application cursor being overridden when
      dragging in non-WM mode */
  bool _cursorOverride;

  // ! provide application-wise event filter
  /*!
    it us used to unlock dragging and make sure event look is properly
    restored after a drag has occurred
  */
  class AppEventFilter: public QObject
  {
  public:
    AppEventFilter (WindowManager *parent) :
                   QObject (parent),
                   _parent (parent)
    {}
  virtual bool eventFilter (QObject*, QEvent*);
  protected:
    // ! application-wise event.
    /*! needed to catch end of XMoveResize events */
    bool appMouseEvent (QObject*, QEvent*);
  private:
    // ! parent
    WindowManager *_parent;
  };

  // ! application event filter
  AppEventFilter *_appEventFilter;
  // ! allow access of all private members to the app event filter
  friend class AppEventFilter;
};

#endif
