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

/*
  NOTE:
  This is for Qt >= 5.15, which can call the WM drag with QWindow::startSystemMove().
*/

#include <QMouseEvent>
#include <QApplication>
#include <QMainWindow>
#include <QDialog>
#include <QLabel>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QToolButton>
#include <QCheckBox>
#include <QRadioButton>
#include <QGroupBox>
#include <QTabBar>
#include <QListView>
#include <QTreeView>
#include <QGraphicsView>
#include <QDockWidget>
#include <QMdiArea>
#include <QMdiSubWindow>
#include "windowmanager.h"

namespace Kvantum {

static inline bool isPrimaryToolBar (QWidget *w)
{
  if (w == nullptr) return false;
  QToolBar *tb = qobject_cast<QToolBar*>(w);
  if (tb || strcmp(w->metaObject()->className(), "ToolBar") == 0)
  {
    if (tb == nullptr || Qt::Horizontal == tb->orientation())
    {
      QWidget *p = w->parentWidget();
      if (p != w->window()) return false; // inside a dock

      if (0 == w->pos().y())
        return true;

      if (QMainWindow *mw = qobject_cast<QMainWindow *>(p))
      {
        if (QWidget *menuW = mw->menuWidget())
          return menuW->isVisible() && w->pos().y() <= menuW->height()+1;
      }
    }
  }
  return false;
}

static inline QWidget* toolbarContainer (QWidget *w)
{
  if (w == nullptr || qobject_cast<const QToolBar*>(w))
    return nullptr;
  QWidget *window = w->window();
  if (window == w) return nullptr;
  if (qobject_cast<const QToolBar*>(window)) // detached toolbar
    return window;
  const QList<QToolBar*> toolbars = window->findChildren<QToolBar*>(QString(), Qt::FindDirectChildrenOnly);
  for (QToolBar *tb : toolbars)
  {
    if (tb->isAncestorOf (w))
      return tb;
  }
  return nullptr;
}

WindowManager::WindowManager (QObject *parent, Drag drag) :
               QObject (parent),
               enabled_ (true),
               dragDistance_ (QApplication::startDragDistance()),
               dragDelay_ (QApplication::startDragTime()),
               isDelayed_ (false),
               dragAboutToStart_ (false),
               dragInProgress_ (false),
               locked_ (false),
               drag_ (drag)
{
  _appEventFilter = new AppEventFilter (this);
  qApp->installEventFilter (_appEventFilter);
}
/*************************/
void WindowManager::initialize (const QStringList &blackList)
{
  setEnabled (true);
  initializeBlackList (blackList);
}
/*************************/
void WindowManager::registerWidget (QWidget *widget)
{
  if (!widget || !widget->isWindow()) return;
  Qt::WindowType type = widget->windowType();
  if (type != Qt::Window && type != Qt::Dialog)
    return;
  if (QWindow *w = widget->windowHandle())
  {
    w->removeEventFilter (this);
    w->installEventFilter (this);
  }
  else
  { // wait until the window ID is changed (see WindowManager::eventFilter)
    widget->removeEventFilter (this);
    widget->installEventFilter (this);
  }
}
/*************************/
void WindowManager::unregisterWidget (QWidget *widget)
{
  if (!widget) return;
  widget->removeEventFilter (this);
  if (widget->isWindow())
  {
    if (QWindow *w = widget->windowHandle())
      w->removeEventFilter (this);
  }
}
/*************************/
void WindowManager::initializeBlackList (const QStringList &list)
{
  blackList_.clear();
  blackList_.insert (ExceptionId (QStringLiteral("CustomTrackView@kdenlive")));
  blackList_.insert (ExceptionId (QStringLiteral("MuseScore")));
  for (const QString& exception : list)
  {
    ExceptionId id (exception);
    if (!id.className().isEmpty())
      blackList_.insert (exception);
  }
}
/*************************/
bool WindowManager::eventFilter (QObject *object, QEvent *event)
{
  if (!enabled()) return false;

  switch (event->type())
  {
    case QEvent::MouseButtonPress:
      return mousePressEvent (object, event);
      break;

    case QEvent::MouseMove:
      if (object == winTarget_.data())
        return mouseMoveEvent (object, event);
      break;

    case QEvent::MouseButtonDblClick:
      if (object == winTarget_.data())
        return mouseDblClickEvent (object, event);
      break;

    case QEvent::WinIdChange: {
      QWidget *widget = qobject_cast<QWidget*>(object);
      if (!widget || !widget->isWindow()) break;
      Qt::WindowType type = widget->windowType();
      if (type != Qt::Window && type != Qt::Dialog)
        break;
      if (QWindow *w = widget->windowHandle())
      {
        w->removeEventFilter (this);
        w->installEventFilter (this);
      }
      break;
    }

    default:
      break;
  }

  return false;
}
/*************************/
void WindowManager::timerEvent (QTimerEvent *event)
{
  QObject::timerEvent (event);
  if (event->timerId() == dragTimer_.timerId())
  {
    dragTimer_.stop();
    if (winTarget_)
    {
      /* NOTE: Under X11, if dragging is started with a delay and the left
               mouse button is released short after it, it will continue
               until a mouse button is pressed or the mouse wheel is turned.
               As a workaround, we don't start dragging with a delay but
               only change and restore the window cursor appropriately. */
      if (isDelayed_)
        winTarget_.data()->setCursor (Qt::OpenHandCursor);
      else
      {
        winTarget_.data()->unsetCursor();
        dragInProgress_ = winTarget_.data()->startSystemMove();
      }
    }
    isDelayed_ = false;
  }
}
/*************************/
bool WindowManager::mousePressEvent (QObject *object, QEvent *event)
{
  QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
  if (!(mouseEvent->modifiers() == Qt::NoModifier && mouseEvent->button() == Qt::LeftButton))
    return false;

  /* check the lock */
  if (isLocked())
    return false;

  /* find the window */
  QWindow *w = qobject_cast<QWindow*>(object);
  if (!w) return false;

  /* find the widget */
  QWidget *activeWin = qApp->activeWindow();
  if (!activeWin) return false;
  QWidget *widget = activeWin->childAt (activeWin->mapFromGlobal (mouseEvent->globalPos()));
  if (!widget)
    widget = activeWin;

  widgetDragPoint_ = widget->mapFromGlobal (mouseEvent->globalPos()); // needed by canDrag()

  /* check if widget can be dragged */
  if (isBlackListed (widget) || !canDrag (widget))
    return false;

  /* save target and drag point */
  winTarget_ = w;
  widgetTarget_ = widget;
  globalDragPoint_ = mouseEvent->globalPos();
  dragAboutToStart_ = true;

  QPoint winDragPoint = mouseEvent->pos();

  /* Because the widget may react to mouse press events, we first send a press event to it.
     A release event will be sent in WindowManager::AppEventFilter::eventFilter. */
  QMouseEvent mousePressEvent (QEvent::MouseButtonPress, widgetDragPoint_, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  qApp->sendEvent (widget, &mousePressEvent);

  /* Steal the event if the window is inactive now. The window may become
     inactive here due to the mouse press event that was sent above. */
  if (winTarget_ == nullptr || !w->isActive())
  {
    resetDrag();
    return true;
  }

  setLocked (true);

  /* Send a move event to the target window with the same position.
     If received, it is caught to actually start the drag. */
  QMouseEvent mouseMoveEvent (QEvent::MouseMove, winDragPoint, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  qApp->sendEvent (w, &mouseMoveEvent);

  /* NOTE: The event should be consumed; otherwise, mouseover effects
           won't work after dragging (unless a mouse button is pressed). */
  return true;

}
/*************************/
bool WindowManager::mouseMoveEvent (QObject *object, QEvent *event)
{
  if (!qobject_cast<QWindow*>(object)) return false;

  QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
  if (!dragInProgress_)
  {
    if (dragAboutToStart_)
    {
      if (mouseEvent->globalPos() == globalDragPoint_)
      {
        if (dragTimer_.isActive())
          dragTimer_.stop();
        dragAboutToStart_ = false;
        isDelayed_ = true;
        dragTimer_.start (dragDelay_, this);
      }
      else resetDrag();
    }
    else if (!dragTimer_.isActive() // drag timeout
             || QPoint (mouseEvent->globalPos() - globalDragPoint_).manhattanLength() >= dragDistance_)
    {
      if (dragTimer_.isActive())
        dragTimer_.stop();
      isDelayed_ = false;
      dragTimer_.start (0, this);
    }

    return true;
  }
  return false;
}
/*************************/
bool WindowManager::mouseDblClickEvent (QObject *object, QEvent *event)
{
  Q_UNUSED(event);
  if (!dragInProgress_ && object == winTarget_.data() && widgetTarget_)
  { // don't drag by double clicking
    QMouseEvent mouseEvent (QEvent::MouseButtonRelease,
                            widgetDragPoint_,
                            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    qApp->sendEvent (widgetTarget_.data(), &mouseEvent);
  }
  return false;
}
/*************************/
bool WindowManager::isBlackListed (QWidget *widget)
{
  /* check against noAnimations propery */
  QVariant propertyValue (widget->property ("_kde_no_window_grab"));
  if (propertyValue.isValid() && propertyValue.toBool())
    return true;

  /* list-based blacklisted widgets */
  QString appName (qApp->applicationName());
  for (const ExceptionId& id : static_cast<const ExceptionSet&>(blackList_))
  {
    if (!id.appName().isEmpty() && id.appName() != appName)
      continue;
    if (id.className() == "*" && !id.appName().isEmpty())
    {
      /* if application name matches and all classes are selected
         disable the grabbing entirely */
      setEnabled (false);
      return true;
    }
    if (widget->inherits (id.className().toLatin1()))
      return true;
  }
  return false;
}
/*************************/
bool WindowManager::canDrag (QWidget *widget)
{
  if (!widget || !widget->isEnabled() || !enabled()
      || drag_ == DRAG_NONE) // impossible
  {
    return false;
  }

  if (QWidget::mouseGrabber()) return false;

  /* assume that a changed cursor means that some action is in progress
     and should prevent the drag */
  if (widget->cursor().shape() != Qt::ArrowCursor)
    return false;

  // X11BypassWindowManagerHint can be used to fix the position
  if (widget->window()->windowFlags().testFlag(Qt::X11BypassWindowManagerHint))
    return false;

  QWidget *parent = widget;
  while (parent)
  {
    if (qobject_cast<QMdiSubWindow*>(parent))
      return false;
    parent = parent->parentWidget();
  }

  if (QMenuBar *menuBar = qobject_cast<QMenuBar*>(widget))
  {
    if(menuBar->activeAction() && menuBar->activeAction()->isEnabled())
      return false;
    if (QAction *action = menuBar->actionAt (widgetDragPoint_))
    {
      if (action->isSeparator()) return true;
      if (action->isEnabled()) return false;
    }
    return true;
  }

  /* toolbar */
  if (drag_ == DRAG_ALL && qobject_cast<QToolBar*>(widget))
    return true;
  if (drag_ < DRAG_ALL)
  {
    if (drag_ == DRAG_MENUBAR_ONLY) return false;

    if (isPrimaryToolBar (widget)) return true;

    QWidget *tb = toolbarContainer (widget);
    if (tb == nullptr) return false;

    /* consider some widgets inside toolbars */
    if (widget->focusPolicy() > Qt::TabFocus
        || (widget->focusProxy() && widget->focusProxy()->focusPolicy() > Qt::TabFocus
            && widget->focusProxy()->underMouse()))
    {
      return false;
    }
    if (widget->testAttribute (Qt::WA_Hover) || widget->testAttribute (Qt::WA_SetCursor))
      return false;
    if (QLabel *label = qobject_cast<QLabel*>(widget))
    {
      if (label->textInteractionFlags().testFlag (Qt::TextSelectableByMouse))
        return false;
    }

    return isPrimaryToolBar (tb);
  }

  if (QTabBar *tabBar = qobject_cast<QTabBar*>(widget))
    return tabBar->tabAt (widgetDragPoint_) == -1;

  if (qobject_cast<QStatusBar*>(widget))
    return true;

  /* pay attention to some details of item views */
  QAbstractItemView *itemView (nullptr);
  bool isDraggable (false);
  if ((itemView = qobject_cast<QListView*>(widget->parentWidget()))
      || (itemView = qobject_cast<QTreeView*>(widget->parentWidget())))
  {
    if (widget == itemView->viewport())
    {
      if (itemView->frameShape() != QFrame::NoFrame)
        return false;
      if (itemView->selectionMode() != QAbstractItemView::NoSelection
          && itemView->selectionMode() != QAbstractItemView::SingleSelection
          && itemView->model() && itemView->model()->rowCount())
      {
        return false;
      }
      if (itemView->model() && itemView->indexAt (widgetDragPoint_).isValid())
        return false;
      isDraggable = true;
    }
  }
  else if ((itemView = qobject_cast<QAbstractItemView*>(widget->parentWidget())))
  {
    if (widget == itemView->viewport())
    {
      if (itemView->frameShape() != QFrame::NoFrame)
        return false;
      if (itemView->indexAt (widgetDragPoint_).isValid())
        return false;
      isDraggable = true;
    }
  }
  else if (QGraphicsView *graphicsView = qobject_cast<QGraphicsView*>(widget->parentWidget()))
  {
    if (widget == graphicsView->viewport())
    {
      if (graphicsView->frameShape() != QFrame::NoFrame)
        return false;
      if (graphicsView->dragMode() != QGraphicsView::NoDrag)
        return false;
      if (graphicsView->itemAt (widgetDragPoint_))
        return false;
      isDraggable = true;
    }
  }
  if (isDraggable
      && (widget->focusPolicy() > Qt::TabFocus
          || (widget->focusProxy() && widget->focusProxy()->focusPolicy() > Qt::TabFocus)))
  { // focus the widget if it's an item view that accepts focus by clicking
    widget->setFocus (Qt::MouseFocusReason);
    return true;
  }

  /* allow dragging from outside focus rectangles and indicators
     of check boxes and radio buttons */
  if (QCheckBox *b = qobject_cast<QCheckBox*>(widget))
  {
    QStyleOptionButton opt;
    opt.initFrom (b);
    opt.text = b->text();
    opt.icon = b->icon();
    if (widget->style()->subElementRect(QStyle::SE_CheckBoxFocusRect, &opt, b)
        .united (widget->style()->subElementRect(QStyle::SE_CheckBoxIndicator, &opt, b))
        .contains (widgetDragPoint_))
    {
      return false;
    }
    return true;
  }
  if (QRadioButton *b = qobject_cast<QRadioButton*>(widget))
  {
    QStyleOptionButton opt;
    opt.initFrom (b);
    opt.text = b->text();
    opt.icon = b->icon();
    if (widget->style()->subElementRect(QStyle::SE_RadioButtonFocusRect, &opt, b)
        .united (widget->style()->subElementRect(QStyle::SE_RadioButtonIndicator, &opt, b))
        .contains (widgetDragPoint_))
    {
      return false;
    }
    return true;
  }

  /* allow dragging from outside focus rectangles and indicators of group boxes */
  if(QGroupBox *groupBox = qobject_cast<QGroupBox*>(widget))
  {
    if (!groupBox->isCheckable()) return true;
    QStyleOptionGroupBox opt;
    opt.initFrom (groupBox);
    if (groupBox->isFlat())
      opt.features |= QStyleOptionFrame::Flat;
    opt.lineWidth = 1;
    opt.midLineWidth = 0;
    opt.text = groupBox->title();
    opt.textAlignment = groupBox->alignment();
    opt.subControls = (QStyle::SC_GroupBoxFrame | QStyle::SC_GroupBoxCheckBox);
    if (!groupBox->title().isEmpty())
      opt.subControls |= QStyle::SC_GroupBoxLabel;
    QRect r = groupBox->style()->subControlRect (QStyle::CC_GroupBox, &opt, QStyle::SC_GroupBoxCheckBox, groupBox);
    if (!groupBox->title().isEmpty())
      r = r.united (groupBox->style()->subControlRect (QStyle::CC_GroupBox, &opt, QStyle::SC_GroupBoxLabel, groupBox));
    return !r.contains (widgetDragPoint_);
  }

  if (widget->focusPolicy() > Qt::TabFocus
      || (widget->focusProxy() && widget->focusProxy()->focusPolicy() > Qt::TabFocus
          && widget->focusProxy()->underMouse()))
  {
    return false;
  }

  if (QDockWidget *dw = qobject_cast<QDockWidget*>(widget))
  {
    if(dw->allowedAreas() == (Qt::DockWidgetArea_Mask | Qt::AllDockWidgetAreas))
      return true;
  }
  else if (qobject_cast<QDockWidget*>(widget->parentWidget())
           /* not a titlebar button */
           && !qobject_cast<QAbstractButton*>(widget))
  {
    return true;
  }

  if (widget->testAttribute (Qt::WA_Hover) || widget->testAttribute (Qt::WA_SetCursor))
    return false;

  /* interacting labels shouldn't be dragged */
  if (QLabel *label = qobject_cast<QLabel*>(widget))
  {
    if (label->textInteractionFlags().testFlag (Qt::TextSelectableByMouse))
      return false;
  }

  if (qobject_cast<QMdiArea*>(widget->parentWidget()))
    return false;

  /* this is for QMdi::ControllerWidget, which isn't caught by QObject::inherits() */
  if (QMenuBar *menuBar = qobject_cast<QMenuBar*>(widget->parentWidget()))
  {
    if (widget == menuBar->cornerWidget (Qt::TopLeftCorner)
        || widget == menuBar->cornerWidget (Qt::TopRightCorner))
    {
      return false;
    }
  }

  return true;
}
/*************************/
void WindowManager::resetDrag()
{
  if (winTarget_)
    winTarget_.data()->unsetCursor();
  winTarget_.clear();
  widgetTarget_.clear();
  if (dragTimer_.isActive())
    dragTimer_.stop();
  widgetDragPoint_ = QPoint();
  globalDragPoint_ = QPoint();
  dragAboutToStart_ = false;
  dragInProgress_ = false;
}
/*************************/
bool WindowManager::AppEventFilter::eventFilter (QObject *object, QEvent *event)
{
  Q_UNUSED(object);

  /* unlock the window and cancel dragging if mouse button
     is released before the start of dragging */
  if (event->type() == QEvent::MouseButtonRelease
      && parent_->isLocked())
  {
    if (parent_->widgetTarget_ && object == parent_->winTarget_.data())
      widgetMouseRelease();
    if (parent_->isLocked()) // may have been unlocked above
    {
      parent_->resetDrag();
      parent_->setLocked (false);
    }
  }

  /* If a drag is in progress, no event will be received. Therefore,
     we wait for the first mouse move or press event that is received
     by any object in the application to detect that the drag is finished. */
  if (parent_->enabled()
      && parent_->dragInProgress_
      && parent_->widgetTarget_
      && (event->type() == QEvent::MouseMove
          || event->type() == QEvent::MouseButtonPress))
  {
    widgetMouseRelease();
  }

  return false;
}
/*************************/
void WindowManager::AppEventFilter::widgetMouseRelease()
{
  /* Send a mouse release event to the widget, in order to counterbalance
     the mouse press that was sent by WindowManager::mousePressEvent. */
  QMouseEvent mouseEvent (QEvent::MouseButtonRelease,
                          parent_->widgetDragPoint_,
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  qApp->sendEvent (parent_->widgetTarget_.data(), &mouseEvent);
}

}
