/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2020 <tsujan2000@gmail.com>
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

  explicit WindowManager (QObject *parent, Drag drag);
  virtual ~WindowManager() {}

  void initialize (const QStringList &blackList = QStringList());

  void registerWidget (QWidget*);
  void unregisterWidget (QWidget*);
  virtual bool eventFilter (QObject*, QEvent*);
protected:
  // timer event,
  /* used to start drag if button is pressed for a long enough time */
  void timerEvent (QTimerEvent*);
  bool mousePressEvent (QObject*, QEvent*);
  bool mouseMoveEvent (QObject*, QEvent*);
  bool mouseDblClickEvent (QObject*, QEvent*);
  bool mouseReleaseEvent (QObject*, QEvent*);
  bool enabled() const {
    return enabled_;
  }
  void setEnabled (bool value) {
    enabled_ = value;
  }

  void initializeBlackList (const QStringList &list);
  bool isBlackListed (QWidget*);
  // returns true if drag can be started from current widget
  bool canDrag (QWidget*);
  void resetDrag();

  void setLocked (bool value) {
    locked_ = value;
  }
  bool isLocked() const {
    return locked_;
  }
private:
  // the value of QT_DEVICE_PIXEL_RATIO
  bool enabled_;
  int dragDistance_;
  int dragDelay_;

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
  ExceptionSet whiteList_;
  ExceptionSet blackList_;

  QPoint winDragPoint_;
  QPoint widgetDragPoint_;
  QPoint globalDragPoint_;
  QBasicTimer dragTimer_;
  QPointer<QWindow> winTarget_;
  QPointer<QWidget> widgetTarget_;
  bool dragAboutToStart_;
  bool dragInProgress_;
  bool locked_;
  Drag drag_;

  // provide application-wise event filter
  /*
    it is used to unlock dragging and make sure event look
    is properly restored after a drag has occurred
  */
  class AppEventFilter: public QObject
  {
  public:
    AppEventFilter (WindowManager *parent) :
                    QObject (parent),
                    parent_ (parent) {}
  virtual bool eventFilter (QObject*, QEvent*);
  protected:
    bool appMouseEvent (QObject*, QEvent*);
  private:
    WindowManager *parent_;
  };

  AppEventFilter *_appEventFilter;
  /* allow access of all private members to the app event filter */
  friend class AppEventFilter;
};
}

#endif
