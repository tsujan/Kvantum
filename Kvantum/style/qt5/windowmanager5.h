/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2022 <tsujan2000@gmail.com>
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
#include <QPointer>
#include <QWindow>
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

  static Drag toDrag (const QString &str) {
    for (int i = 0; i < DRAG_COUNT; ++i)
    {
      if (toStr (static_cast<Drag>(i)) == str)
        return static_cast<Drag>(i);
    }
    return DRAG_NONE;
  }

  static QString toStr (Drag drag) {
    switch (drag)
    {
      default:
      case DRAG_ALL: return "all";
      case DRAG_NONE: return "none";
      case DRAG_MENUBAR_ONLY: return "menubar";
      case DRAG_MENUBAR_AND_PRIMARY_TOOLBAR: return "menubar_and_primary_toolbar";
    }
  }

  explicit WindowManager (QObject *parent, Drag drag, bool dragFromBtns);
  ~WindowManager();

  void initialize (const QStringList &blackList = QStringList());

  void registerWidget (QWidget *widget);
  void unregisterWidget (QWidget *widget);
  virtual bool eventFilter (QObject *object, QEvent *event);

  /* needed in "Kvantum.cpp" for animations
     and also for the workaround of hover bug */
  bool dragInProgress() const {
    return dragInProgress_;
  }

protected:
  void timerEvent (QTimerEvent *event);
  bool mousePressEvent (QObject *object, QEvent *event);
  bool mouseMoveEvent (QEvent *event);
  bool mouseReleaseEvent (QEvent *event);
  bool leavingWindow();

  bool enabled() const {
    return enabled_;
  }
  void setEnabled (bool value) {
    enabled_ = value;
  }

  void initializeBlackList (const QStringList &list);
  bool isBlackListed (QWidget *widget);
  bool canDrag (QWidget *widget);
  bool isDraggable (QWidget *widget);
  void resetDrag();

  bool isLocked() const {
    return locked_;
  }
  void unlock() {
    locked_ = false;
    dragInProgress_ = false;
  }

private:
  bool enabled_;
  int dragDistance_;
  int dragDelay_;
  int doubleClickInterval_;
  bool isDelayed_;

  // wrapper for exception id
  class ExceptionId: public QPair<QString, QString>
  {
  public:
    ExceptionId (const QString &value) {
      const QStringList args (value.split (QStringLiteral("@")));
      if (args.isEmpty())
        return;
      second = args[0].trimmed();
      if (args.size() > 1)
        first = args[1].trimmed();
    }
    const QString& appName() const {
      return first;
    }
    const QString& className() const {
      return second;
    }
  };

  typedef QSet<ExceptionId> ExceptionSet;
  ExceptionSet blackList_;

  QPoint widgetDragPoint_;
  QPoint globalDragPoint_;
  QPoint lastWinDragPoint_; // used to find double clicks
  QBasicTimer dragTimer_;
  QBasicTimer doubleClickTimer_;
  QPointer<QWindow> winTarget_;
  QPointer<QWindow> lastWin_;
  QPointer<QWidget> widgetTarget_;
  QPointer<QWidget> pressedWidget_;
  QPointer<QWidget> lastPressedWidget_;
  bool dragAboutToStart_;
  bool dragInProgress_;
  bool locked_;
  bool dragFromBtns_;
  Drag drag_;

  /*
     provide application-wise event filter
     (used to unlock dragging and checking some mouse events)
  */
  class AppEventFilter: public QObject
  {
  public:
    AppEventFilter (WindowManager *parent) :
                    QObject (parent),
                    parent_ (parent) {}
    virtual bool eventFilter (QObject *object, QEvent *event);

  private:
    WindowManager *parent_;
  };

  AppEventFilter *_appEventFilter;
  /* allow access of all private members to the app event filter */
  friend class AppEventFilter;
};

}

#endif
