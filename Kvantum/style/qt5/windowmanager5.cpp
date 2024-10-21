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
#include "windowmanager5.h"

namespace Kvantum {

static bool _WMExists = false;

static inline bool isPrimaryToolBar (QWidget *w)
{
  if (w == nullptr) return false;
  QToolBar *tb = qobject_cast<QToolBar*>(w);
  if (tb || strcmp (w->metaObject()->className(), "ToolBar") == 0)
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

WindowManager::WindowManager (QObject *parent, Drag drag, bool dragFromBtns) :
               QObject (parent),
               enabled_ (true),
               dragDistance_ (qMax (QApplication::startDragDistance(), 10)),
               dragDelay_ (qMax (QApplication::startDragTime(), 500)),
               doubleClickInterval_ (QApplication::doubleClickInterval()),
               isDelayed_ (false),
               dragAboutToStart_ (false),
               dragInProgress_ (false),
               locked_ (false),
               dragFromBtns_ (dragFromBtns),
               drag_ (drag)
{
  _appEventFilter = new AppEventFilter (this);
  qApp->installEventFilter (_appEventFilter);
}
/*************************/
WindowManager::~WindowManager()
{
  _WMExists = false;
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
  if (type != Qt::Window && type != Qt::Dialog
      && type != Qt::Sheet) // a Qt5 bug on Linux
  {
    return;
  }
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
  blackList_.insert (ExceptionId (QStringLiteral ("CustomTrackView@kdenlive")));
  blackList_.insert (ExceptionId (QStringLiteral ("MuseScore")));
  blackList_.insert (ExceptionId (QStringLiteral ("KGameCanvasWidget")));
  blackList_.insert (ExceptionId (QStringLiteral ("QQuickWidget")));
  blackList_.insert (ExceptionId (QStringLiteral ("*@soffice.bin")));
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
        return mouseMoveEvent (event);
      break;

    case QEvent::MouseButtonRelease:
      if (object == winTarget_.data())
        return mouseReleaseEvent (event);
      break;

    case QEvent::WindowBlocked:
    case QEvent::FocusOut: // e.g., a popup is shown
    case QEvent::Leave:
    case QEvent::Hide:
      if (object == winTarget_.data())
        return leavingWindow();
      break;

    case QEvent::WinIdChange: {
      QWidget *widget = qobject_cast<QWidget*>(object);
      if (!widget || !widget->isWindow()) break;
      Qt::WindowType type = widget->windowType();
      if (type != Qt::Window && type != Qt::Dialog && type != Qt::Sheet
          && type != Qt::Tool) // an exception; see WindowManager::canDrag()
      {
        break;
      }
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
      if (qApp->activePopupWidget()
          || !(QGuiApplication::mouseButtons() & Qt::LeftButton))
      {
        /* WARNING: Due to a Qt bug, that exists under X11 and Wayland alike,
                    the drag may be started even when an active popup is shown.
                    As a workaround, we don't start dragging in this case.

                    Also, rarely, the left mouse button may have been released
                    inside a popup, without sending an event to the window. */
        winTarget_.data()->unsetCursor();
        resetDrag();
        unlock();
        isDelayed_ = false;
      }
      else if (isDelayed_)
      {
        /* NOTE: Under X11, if dragging is started with a delay and the left
                 mouse button is released shortly after it, it will continue
                 until a mouse button is pressed or the mouse wheel is turned.
                 As a workaround, we don't start dragging with a delay but
                 only change and restore the window cursor appropriately. */
        winTarget_.data()->setCursor (Qt::OpenHandCursor);
        isDelayed_ = false;
      }
      else
      {
        winTarget_.data()->unsetCursor();
        _WMExists = true;
        if (widgetTarget_)
        {
          /* NOTE: On starting the drag, we release the mouse outside the widget to
                   prevent a click and also counterbalance the mouse press that was
                   sent by WindowManager::mousePressEvent(). In this way, it's also
                   possible to drag from button-like widgets safely (as in GTK). */
          QMouseEvent e (QEvent::MouseButtonRelease,
                         QPoint (-1, -1),
                         Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
          qApp->sendEvent (widgetTarget_.data(), &e);
        }
        if (!_WMExists) return; // see mousePressEvent for the reason
        if (winTarget_)
          dragInProgress_ = winTarget_.data()->startSystemMove();
        resetDrag(); // clear the drag info, showing that the drag is started
      }
    }
  }
  else if (event->timerId() == doubleClickTimer_.timerId())
    doubleClickTimer_.stop();
}
/*************************/
bool WindowManager::mousePressEvent (QObject *object, QEvent *event)
{
  QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
  if (!(mouseEvent->modifiers() == Qt::NoModifier && mouseEvent->button() == Qt::LeftButton))
    return false;

  /* find the window and its drag point */
  QWindow *w = qobject_cast<QWindow*>(object);
  if (!w) return false;
  QPoint winDragPoint = mouseEvent->pos();

  if (doubleClickTimer_.isActive())
  {
    doubleClickTimer_.stop();
    if (lastWin_.data() == w
        && QPoint (winDragPoint - lastWinDragPoint_).manhattanLength() < dragDistance_)
    { // don't drag by double clicking
      resetDrag();
      unlock();
      return false;
    }
  }
  doubleClickTimer_.start (doubleClickInterval_, this);

  if (qApp->activePopupWidget())
  { // -> the workaround in timerEvent()
    resetDrag();
    unlock();
    return false;
  }

  /* check the lock and drag state */
  if (isLocked() || dragInProgress_)
  {
    resetDrag();
    unlock();
    if (lastWin_.data() == w)
      return false;
    /* if the window is changed, start a new drag */
  }

  /* remember the window and its drag point */
  lastWin_ = w;
  lastWinDragPoint_ = winDragPoint;

  /* find the widget */
  QWidget *widget = nullptr;
  /* NOTE: Under Wayland, if the window was inactive before being dragged, it may still
           be inactive when this function is called. Moreover, the app may have multiple
           windows and the mouse may have been pressed on an inactive one. Therefore,
           we can't rely on QApplication::activeWindow() to find the widget.

           On the other hand, QApplication::widgetAt() isn't reliable either because it
           calls QApplication::topLevelAt(), which is useless under Wayland.

           Checking all top level widgets is our only option. it works under X11 too. */
  QWidget *topLevelWidget = nullptr;
  const auto tlws = qApp->topLevelWidgets();
  for (const auto tlw : tlws)
  {
    if (tlw->windowHandle() == w)
    {
      topLevelWidget = tlw;
      break;
    }
  }
  if (!topLevelWidget) return false;
  widget = topLevelWidget->childAt (topLevelWidget->mapFromGlobal (mouseEvent->globalPos()));
  if (!widget)
    widget = topLevelWidget;

  widgetDragPoint_ = widget->mapFromGlobal (mouseEvent->globalPos()); // needed by canDrag()

  /* check if the widget can be dragged */
  dragDistance_ = qMax (QApplication::startDragDistance(), 10);
  dragDelay_ = qMax (QApplication::startDragTime(), 500);
  if (isBlackListed (widget) || !canDrag (widget))
    return false;

  /* save some targets and drag points */
  winTarget_ = w;
  widgetTarget_ = widget;
  globalDragPoint_ = mouseEvent->globalPos();
  dragAboutToStart_ = true;

  /* clear the info about pressing the mouse button */
  pressedWidget_.clear();
  lastPressedWidget_.clear();

  _WMExists = true;

  /* Because the widget may react to mouse press events,
     we first send a press event to it. */
  QMouseEvent mousePress (QEvent::MouseButtonPress, widgetDragPoint_,
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  qApp->sendEvent (widget, &mousePress);

  /* Theoretically, it's possible that the WindowManager instance
     gets deleted as the result of the above mouse press event. */
  if (!_WMExists)
    return true;

  /* If the target window is left, blocked or hidden now, the drag
     is cancelled (-> leavingWindow). Steal the press event! */
  if (winTarget_ == nullptr)
    return true;

  if (widgetTarget_ == nullptr // the widget has been deleted
      || !widget->isVisible()) // as with Fm::PathBar
  {
    resetDrag();
    return true;
  }

  /* don't start dragging if the mouse press is accepted
     but allow dragging from inside some widget types */
  if (mousePress.isAccepted())
  {
    /* the last pressed widget may do something with the press event */
    if (!isDraggable (lastPressedWidget_ ? lastPressedWidget_.data() : widget))
    {
      resetDrag();
      if (qApp->activePopupWidget()) // steals mouse events
        return true;
      /* The event shouldn't be consumed because it may start a DND or click
         but a press event will be sent to the widget immediately after "false"
         is returned here. Therefore, we need to remember the wdget in order to
         prevent a double press in WindowManager::AppEventFilter::eventFilter(). */
      pressedWidget_ = widget;
      return false;
    }
  }

  locked_ = true;

  /* Send a move event to the target window with the same position.
     If received, it is caught to actually start the drag. */
  QMouseEvent mouseMove (QEvent::MouseMove, winDragPoint,
                         Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  qApp->sendEvent (w, &mouseMove);

  /* NOTE: The event should be consumed; otherwise, mouseover effects
           won't work after dragging (until a mouse button is pressed). */
  return true;
}
/*************************/
bool WindowManager::mouseMoveEvent (QEvent *event)
{
  /* make sure that the left mouse button is still pressed */
  if (!(static_cast<QMouseEvent*>(event)->buttons() & Qt::LeftButton))
    return false;

  if (!dragInProgress_)
  {
    QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
    if (dragAboutToStart_)
    {
      dragAboutToStart_ = false;
      if (dragTimer_.isActive())
        dragTimer_.stop();
      if (QPoint (mouseEvent->globalPos() - globalDragPoint_).manhattanLength() < dragDistance_)
      {
        isDelayed_ = true;
        dragTimer_.start (dragDelay_, this);
      }
      else
      {
        /* the cursor moved too fast; perhaps the window was
           inactive before the left mouse button was pressed */
        isDelayed_ = false;
        dragTimer_.start (0, this);
      }
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
bool WindowManager::mouseReleaseEvent (QEvent *event)
{
  /* unlock the window and click the widget if the
     left mouse button is released before dragging */
  if (!dragInProgress_ && widgetTarget_)
  {
    QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
    if (mouseEvent->button() == Qt::LeftButton)
    {
      auto e = new QMouseEvent (QEvent::MouseButtonRelease,
                                widgetDragPoint_,
                                Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
      qApp->postEvent (widgetTarget_.data(), e);
      resetDrag();
      unlock();
    }
    return true; // the press event was consumed
  }
  return false;
}
/*************************/
bool WindowManager::leavingWindow()
{
  if (!dragInProgress_ && widgetTarget_)
  {
    resetDrag(); // left, blocked or hidden before the drag
    unlock();
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
      /* if application name matches and all classes are selected,
         disable the grabbing entirely */
      setEnabled (false);
      return true;
    }
    if (widget->inherits (id.className().toLatin1().data()))
      return true;
  }
  return false;
}
/*************************/
bool WindowManager::canDrag (QWidget *widget)
{
  if (!widget || !enabled()
      || drag_ == DRAG_NONE) // impossible
  {
    return false;
  }

  if (QWidget::mouseGrabber()) return false;

  /* assume that a changed cursor means that some action is in progress
     and should prevent the drag */
  if (widget->cursor().shape() != Qt::ArrowCursor)
    return false;

  QWidget *win = widget->window();
  if (win == widget
      && !qobject_cast<QMainWindow*>(widget)
      && !qobject_cast<QDialog*>(widget))
  {
    return false;
  }
  if (win->testAttribute (Qt::WA_X11NetWmWindowTypeDesktop))
    return false;
  /* the window type may have changed but we accept Qt::Tool (as in Krita) */
  Qt::WindowType type = win->windowType();
  if (type != Qt::Window && type != Qt::Dialog && type != Qt::Sheet
      && type != Qt::Tool)
  {
    return false;
  }
  /* X11BypassWindowManagerHint can be used to fix the position */
  if (win->windowFlags().testFlag (Qt::X11BypassWindowManagerHint)
      || win->windowFlags().testFlag (Qt::WindowDoesNotAcceptFocus))
  {
    return false;
  }

  QWidget *parent = widget;
  while (parent)
  {
    if (qobject_cast<QMdiSubWindow*>(parent))
      return false;
    if (parent->isWindow()) break; // not inside a subwindow
    parent = parent->parentWidget();
  }

  if (qobject_cast<QMdiArea*>(widget->parentWidget()))
    return false;

  if (QMenuBar *menuBar = qobject_cast<QMenuBar*>(widget))
  {
    if (menuBar->activeAction() && menuBar->activeAction()->isEnabled())
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
    bool draggedBtn (dragFromBtns_ && qobject_cast<QAbstractButton*>(widget));
    bool allowedBtn = false;
    if (QToolButton *toolButton = qobject_cast<QToolButton*>(widget))
      allowedBtn = toolButton->autoRaise() && !toolButton->isEnabled();
    if ((!draggedBtn && !allowedBtn && widget->testAttribute (Qt::WA_Hover))
        || widget->testAttribute (Qt::WA_SetCursor))
    {
      return false;
    }
    if (QLabel *label = qobject_cast<QLabel*>(widget))
    {
      if (label->textInteractionFlags().testFlag (Qt::TextSelectableByMouse))
        return false;
    }

    if (!isPrimaryToolBar (tb)) return false;
    if (draggedBtn)
    { // to make pressing buttons easier
      dragDistance_ *= 1.5;
      dragDelay_ *= 2;
    }
    return true;
  }

  if (QTabBar *tabBar = qobject_cast<QTabBar*>(widget))
    return tabBar->tabAt (widgetDragPoint_) == -1;

  if (qobject_cast<QStatusBar*>(widget))
    return true;

  /* pay attention to some details of item views */
  QAbstractItemView *itemView (nullptr);
  bool draggable (false);
  if ((itemView = qobject_cast<QListView*>(widget->parentWidget()))
      || (itemView = qobject_cast<QTreeView*>(widget->parentWidget())))
  {
    if (widget == itemView->viewport())
    {
      if (isBlackListed (itemView))
        return false;
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
      draggable = true;
    }
  }
  else if ((itemView = qobject_cast<QAbstractItemView*>(widget->parentWidget())))
  {
    if (widget == itemView->viewport())
    {
      if (isBlackListed (itemView))
        return false;
      if (itemView->frameShape() != QFrame::NoFrame)
        return false;
      if (itemView->indexAt (widgetDragPoint_).isValid())
        return false;
      draggable = true;
    }
  }
  else if (QGraphicsView *graphicsView = qobject_cast<QGraphicsView*>(widget->parentWidget()))
  {
    if (widget == graphicsView->viewport())
    {
      return false; // can be troublesome for users of apps like kpat
      /*if (isBlackListed (graphicsView))
        return false;
      if (graphicsView->frameShape() != QFrame::NoFrame)
        return false;
      if (graphicsView->dragMode() != QGraphicsView::NoDrag)
        return false;
      if (graphicsView->itemAt (widgetDragPoint_))
        return false;
      draggable = true;*/
    }
  }
  if (draggable)
  {
    if (widget->focusPolicy() > Qt::TabFocus
        || (widget->focusProxy() && widget->focusProxy()->focusPolicy() > Qt::TabFocus))
    { // focus the widget if it's an item view that accepts focus by clicking
      widget->setFocus (Qt::MouseFocusReason);
    }
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
    if (widget->style()->subElementRect (QStyle::SE_CheckBoxFocusRect, &opt, b)
        .united (widget->style()->subElementRect (QStyle::SE_CheckBoxIndicator, &opt, b))
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
    if (widget->style()->subElementRect (QStyle::SE_RadioButtonFocusRect, &opt, b)
        .united (widget->style()->subElementRect (QStyle::SE_RadioButtonIndicator, &opt, b))
        .contains (widgetDragPoint_))
    {
      return false;
    }
    return true;
  }

  /* allow dragging from outside focus rectangles and indicators of group boxes */
  if (QGroupBox *groupBox = qobject_cast<QGroupBox*>(widget))
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

  if (QDockWidget *dw = qobject_cast<QDockWidget*>(widget))
  {
    if (dw->allowedAreas() == (Qt::DockWidgetArea_Mask | Qt::AllDockWidgetAreas))
      return true;
  }
  else if (qobject_cast<QDockWidget*>(widget->parentWidget()))
    return !qobject_cast<QAbstractButton*>(widget); // not a titlebar button

  bool draggedBtn (dragFromBtns_ && qobject_cast<QAbstractButton*>(widget));
  bool allowedBtn = false;
  if (QToolButton *toolButton = qobject_cast<QToolButton*>(widget))
    allowedBtn = toolButton->autoRaise() && !toolButton->isEnabled();
  if ((!draggedBtn && !allowedBtn && widget->testAttribute (Qt::WA_Hover))
      || widget->testAttribute (Qt::WA_SetCursor))
  {
    return false;
  }

  /* interacting labels shouldn't be dragged */
  if (QLabel *label = qobject_cast<QLabel*>(widget))
  {
    if (label->textInteractionFlags().testFlag (Qt::TextSelectableByMouse))
      return false;
  }

  if (widget->focusPolicy() > Qt::TabFocus
      || (widget->focusProxy() && widget->focusProxy()->focusPolicy() > Qt::TabFocus))
  { // like an ordinary mouse press
    widget->setFocus (Qt::MouseFocusReason);
  }

  if (draggedBtn)
  { // to make pressing buttons easier
    dragDistance_ *= 1.5;
    dragDelay_ *= 2;
  }
  return true;
}
/*************************/
bool WindowManager::isDraggable (QWidget *widget)
{
  if (!widget || QWidget::mouseGrabber())
    return false;

  if (qobject_cast<QAbstractButton*>(widget))
  {
    if (dragFromBtns_)
      return true;
    if (QToolButton *toolButton = qobject_cast<QToolButton*>(widget))
    {
      if (toolButton->autoRaise() && !toolButton->isEnabled())
        return true;
    }
  }

  if (widget->isWindow()
      && (qobject_cast<QMainWindow*>(widget)
          || qobject_cast<QDialog*>(widget)))
  {
    return true;
  }

  if (qobject_cast<QMenuBar*>(widget)
      || qobject_cast<QTabBar*>(widget)
      || qobject_cast<QStatusBar*>(widget)
      || qobject_cast<QToolBar*>(widget))
  {
    return true;
  }

  if (QListView *listView = qobject_cast<QListView*>(widget->parentWidget()))
  {
    if (listView->viewport() == widget && !isBlackListed (listView))
      return true;
  }
  else if (QTreeView *treeView = qobject_cast<QTreeView*>(widget->parentWidget()))
  {
    if (treeView->viewport() == widget && !isBlackListed (treeView))
      return true;
  }
  /*else if (QGraphicsView *graphicsView = qobject_cast<QGraphicsView*>(widget->parentWidget()))
  {
    if (graphicsView->viewport() == widget && !isBlackListed (graphicsView))
      return true;
  }*/

  return false;
}
/*************************/
void WindowManager::resetDrag()
{
  if (winTarget_)
    winTarget_.data()->unsetCursor();
  winTarget_.clear();
  widgetTarget_.clear();
  pressedWidget_.clear();
  if (dragTimer_.isActive())
    dragTimer_.stop();
  widgetDragPoint_ = QPoint();
  globalDragPoint_ = QPoint();
  dragAboutToStart_ = false;
}
/*************************/
bool WindowManager::AppEventFilter::eventFilter (QObject *object, QEvent *event)
{
  if (event->type() == QEvent::MouseButtonPress && !parent_->isLocked())
  {
    QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
    if (object == parent_->pressedWidget_.data())
    { // no double press; the widget was pressed in WindowManager::mousePressEvent()
      parent_->pressedWidget_.clear();
      if (mouseEvent->modifiers() == Qt::NoModifier && mouseEvent->button() == Qt::LeftButton)
        return true;
    }
    else if (parent_->dragAboutToStart_)
    { // find the last pressed widget in WindowManager::mousePressEvent()
      if (QWidget *widget = qobject_cast<QWidget*>(object))
      {
        if (mouseEvent->modifiers() == Qt::NoModifier && mouseEvent->button() == Qt::LeftButton)
          parent_->lastPressedWidget_ = widget;
      }
    }
    return false;
  }

  /* If a drag is in progress, no event will be received. Therefore,
     we wait for the first mouse move or press event that is received
     by any object in the application to unclock the window. */
  if (parent_->enabled()
      && parent_->isLocked()
      && !parent_->winTarget_ // drag was started (-> WindowManager::timerEvent)
      && (event->type() == QEvent::MouseMove
          || event->type() == QEvent::MouseButtonPress))
  {
    parent_->unlock();
  }

  return false;
}

}
