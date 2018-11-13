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

#include <QApplication>
#include <QProgressBar>
#include <QComboBox>
#include <QDialog>
#include <QDockWidget>
#include <QGroupBox>
#include <QLabel>
#include <QListView>
#include <QMainWindow>
#include <QMenuBar>
#include <QMouseEvent>
#include <QStatusBar>
#include <QStyleOptionGroupBox>
#include <QTabBar>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QGraphicsView>

#include "windowmanager4.h"
#include "x11wmmove4.h"

// WindowManager can be called on wayland only for Qt >= 5.2,
// in which case, it'll use QWidget::move() for dragging.
// However, the code is commented out for now.
namespace Kvantum {

static inline bool isPrimaryToolBar(QWidget *w)
{
  if (!w) return false;
  QToolBar *tb = qobject_cast<QToolBar*>(w);
  if (tb || 0 == strcmp(w->metaObject()->className(), "ToolBar"))
  {
    if (!tb || Qt::Horizontal == tb->orientation())
    {
      if (0 == w->pos().y())
        return true;

      if (QMainWindow *mw = qobject_cast<QMainWindow *>(w->window()))
      {
        if (QWidget *menuW = mw->menuWidget())
          return menuW->isVisible() && w->pos().y() <= menuW->height()+1;
      }
    }
  }
  return false;
}

WindowManager::WindowManager (QObject* parent, Drag drag) :
               QObject (parent),
               enabled_ (true),
               dragDistance_ (QApplication::startDragDistance()),
               dragDelay_ (QApplication::startDragTime()),
               dragAboutToStart_ (false),
               dragInProgress_ (false),
               locked_ (false),
               drag_ (drag)
{
  _appEventFilter = new AppEventFilter( this );
  qApp->installEventFilter (_appEventFilter);
}
/*************************/
void WindowManager::initialize (const QStringList &whiteList, const QStringList &blackList)
{
  setEnabled (true);
  setDragDelay (QApplication::startDragTime());
  initializeWhiteList (whiteList);
  initializeBlackList (blackList);
}
/*************************/
void WindowManager::registerWidget (QWidget* widget)
{
  /*
    also install filter for blacklisted widgets
    to be able to catch the relevant events and prevent
    the drag to happen
  */
  if (isBlackListed (widget) || isDragable (widget))
  {
    widget->removeEventFilter (this);
    widget->installEventFilter (this);
  }

}
/*************************/
void WindowManager::unregisterWidget (QWidget* widget)
{
  if (widget)
    widget->removeEventFilter (this);
}
/*************************/
void WindowManager::initializeWhiteList (const QStringList &list)
{
  whiteList_.clear();

  // add user specified whitelisted classnames
  whiteList_.insert (ExceptionId ("MplayerWindow"));
  whiteList_.insert (ExceptionId ("Screen@smplayer"));
  whiteList_.insert (ExceptionId ("ViewSliders@kmix"));
  whiteList_.insert (ExceptionId ("Sidebar_Widget@konqueror"));

  foreach (const QString& exception, list)
  {
    ExceptionId id (exception);
    if (!id.className().isEmpty())
      whiteList_.insert (exception);
  }
}
/*************************/
void WindowManager::initializeBlackList (const QStringList &list)
{

  blackList_.clear();
  blackList_.insert (ExceptionId ("CustomTrackView@kdenlive"));
  blackList_.insert (ExceptionId ("MuseScore"));
  foreach (const QString& exception, list)
  {
    ExceptionId id (exception);
    if (!id.className().isEmpty())
      blackList_.insert (exception);
  }

}
/*************************/
bool WindowManager::eventFilter (QObject* object, QEvent* event)
{
  if (!enabled()) return false;

  switch (event->type())
  {
    case QEvent::MouseButtonPress:
      return mousePressEvent (object, event);
      break;

    case QEvent::MouseMove:
      if (object == target_.data())
        return mouseMoveEvent (object, event);
      break;

    case QEvent::MouseButtonRelease:
      if (target_) return mouseReleaseEvent (object, event);
      break;

    default:
      break;
  }

  return false;
}
/*************************/
void WindowManager::timerEvent (QTimerEvent* event)
{

  if (event->timerId() == dragTimer_.timerId())
  {
    dragTimer_.stop();
    if (target_)
      startDrag (target_.data(), globalDragPoint_);
  }
  else
    return QObject::timerEvent (event);
}
/*************************/
bool WindowManager::mousePressEvent (QObject* object, QEvent* event)
{
  // cast event and check buttons/modifiers
  QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
  if (!(mouseEvent->modifiers() == Qt::NoModifier && mouseEvent->button() == Qt::LeftButton))
    return false;

  // check lock
  if (isLocked())
    return false;
  else
    setLocked (true);

  // cast to widget
  QWidget *widget = static_cast<QWidget*>(object);
  if (!widget)
    return false;

  // check if widget can be dragged from current position
  if (isBlackListed (widget) || !canDrag (widget))
    return false;

  // retrieve widget's child at event position
  QPoint position = mouseEvent->pos();

  QWidget* child = widget->childAt (position);
  if(!canDrag (widget, child, position))
    return false;

  // save target and drag point
  target_ = widget;
  dragPoint_ = position;
  globalDragPoint_ = mouseEvent->globalPos();
  dragAboutToStart_ = true;

  // send a move event to the current child with same position
  // if received, it is caught to actually start the drag
  QPoint localPoint (dragPoint_);
  if (child)
    localPoint = child->mapFrom (widget, localPoint);
  else
    child = widget;
  QMouseEvent localMouseEvent (QEvent::MouseMove, localPoint, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  qApp->sendEvent (child, &localMouseEvent);

  // never eat event
  return false;

}
/*************************/
bool WindowManager::mouseMoveEvent (QObject* object, QEvent* event)
{
  Q_UNUSED (object);

  // stop timer
  if (dragTimer_.isActive())
    dragTimer_.stop();

  // cast event and check drag distance
  QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
  if (!dragInProgress_)
  {
    if (dragAboutToStart_)
    {
      if (mouseEvent->globalPos() == globalDragPoint_)
      {
        // start timer,
        dragAboutToStart_ = false;
        if (dragTimer_.isActive())
          dragTimer_.stop();
        dragTimer_.start (dragDelay_, this);

      }
      else resetDrag();

    }
    else if (QPoint (mouseEvent->globalPos() - globalDragPoint_).manhattanLength() >= dragDistance_)
      dragTimer_.start (0, this);

    return true;
  }
  else
    return false;
}
/*************************/
bool WindowManager::mouseReleaseEvent (QObject* object, QEvent* event)
{
  Q_UNUSED (object);
  Q_UNUSED (event);
  resetDrag();
  return false;
}
/*************************/
bool WindowManager::isDragable (QWidget* widget)
{
  // check widget
  if (!widget) return false;

  // accepted default types
  if ((qobject_cast<QDialog*>(widget) && widget->isWindow())
      || (qobject_cast<QMainWindow*>(widget) && widget->isWindow())
      || qobject_cast<QGroupBox*>(widget))
  {
    return true;
  }

  // more accepted types, provided they are not dock widget titles
  if ((qobject_cast<QMenuBar*>(widget)
       || qobject_cast<QTabBar*>(widget)
       || qobject_cast<QStatusBar*>(widget)
       || qobject_cast<QToolBar*>(widget))
      && !isDockWidgetTitle (widget))
  {
    return true;
  }

  /*if (widget->inherits ("KScreenSaver") && widget->inherits ("KCModule"))
    return true;*/

  if (isWhiteListed (widget))
    return true;

  // flat toolbuttons
  if (QToolButton* toolButton = qobject_cast<QToolButton*>(widget))
  {
    if (toolButton->autoRaise()) return true;
  }

  // viewports
  /*
    one needs to check that
    1/ the widget parent is a scrollarea
    2/ it matches its parent viewport
    3/ the parent is not blacklisted
  */
  if (QListView* listView = qobject_cast<QListView*>(widget->parentWidget()))
  {
    if (listView->viewport() == widget && !isBlackListed (listView))
      return true;
  }

  if (QTreeView* treeView = qobject_cast<QTreeView*>(widget->parentWidget()))
  {
    if (treeView->viewport() == widget && !isBlackListed (treeView))
      return true;
  }

  //if( QGraphicsView* graphicsView = qobject_cast<QGraphicsView*>( widget->parentWidget() ) )
  //{ if( graphicsView->viewport() == widget && !isBlackListed( graphicsView ) ) return true; }

  /*
    catch labels in status bars.
    this is because of kstatusbar
    who captures buttonPress/release events
  */
  if (QLabel* label = qobject_cast<QLabel*>(widget))
  {
    if (label->textInteractionFlags().testFlag (Qt::TextSelectableByMouse))
      return false;

    QWidget* parent = label->parentWidget();
    while (parent)
    {
      if (qobject_cast<QStatusBar*>(parent))
        return true;
      parent = parent->parentWidget();
    }
  }

  return false;
}
/*************************/
bool WindowManager::isBlackListed (QWidget* widget)
{

  // check against noAnimations propery
  QVariant propertyValue (widget->property ("_kde_no_window_grab"));
  if (propertyValue.isValid() && propertyValue.toBool())
    return true;

  // list-based blacklisted widgets
  QString appName (qApp->applicationName());
  foreach (const ExceptionId &id, blackList_)
  {
    if (!id.appName().isEmpty() && id.appName() != appName)
      continue;
    if (id.className() == "*" && !id.appName().isEmpty())
    {
      // if application name matches and all classes are selected
      // disable the grabbing entirely
      setEnabled (false);
      return true;
    }
    if (widget->inherits (id.className().toLatin1()))
      return true;
  }
  return false;
}
/*************************/
bool WindowManager::isWhiteListed (QWidget* widget) const
{

  QString appName (qApp->applicationName());
  foreach (const ExceptionId &id, whiteList_)
  {
    if (!id.appName().isEmpty() && id.appName() != appName)
      continue;
    if (widget->inherits (id.className().toLatin1()))
      return true;
  }
  return false;
}
/*************************/
bool WindowManager::canDrag (QWidget* widget)
{
  // check if enabled
  if (!widget || !enabled())
    return false;

  // assume isDragable widget is already passed
  // check some special cases where drag should not be effective

  // check mouse grabber
  if (QWidget::mouseGrabber()) return false;

  /*
    check cursor shape.
    Assume that a changed cursor means that some action is in progress
    and should prevent the drag
  */
  if (widget->cursor().shape() != Qt::ArrowCursor)
    return false;

  // accept
  return true;
}
/*************************/
bool WindowManager::canDrag (QWidget* widget, QWidget* child, const QPoint& position)
{
  // retrieve child at given position and check cursor again
  if (!widget || (child && child->cursor().shape() != Qt::ArrowCursor))
    return false;

  /*
    check against children from which drag should never be enabled,
    even if mousePress/Move has been passed to the parent
    (FIXME: Should dragging from inside QAbstractScrollArea be disabled?)
  */
  if (child
     && (qobject_cast<QComboBox*>(child)
         || qobject_cast<QProgressBar*>(child)))
  {
    return false;
  }

  // tool buttons

  if (QToolButton *toolButton = qobject_cast<QToolButton*>(widget)) {
    if (drag_ < DRAG_ALL && !isPrimaryToolBar(widget->parentWidget()))
      return false;
    return toolButton->autoRaise() && !toolButton->isEnabled();
  }

  // check menubar
  if (QMenuBar* menuBar = qobject_cast<QMenuBar*>(widget))
  {

    // check if there is an active action
    if(menuBar->activeAction() && menuBar->activeAction()->isEnabled())
      return false;

    // check if action at position exists and is enabled
    if (QAction* action = menuBar->actionAt(position))
    {
      if(action->isSeparator()) return true;
      if(action->isEnabled()) return false;
    }

    // return true in all other cases
    return true;
  }

  bool isToolbar = isPrimaryToolBar(widget);
  if (drag_< DRAG_MENUBAR_AND_PRIMARY_TOOLBAR && isToolbar)
    return false;

  /*
      in MINIMAL mode, anything that has not been already accepted
      and does not come from a toolbar is rejected
      */
  if (drag_ < DRAG_ALL)
    return isToolbar;

  /* following checks are relevant only for WD_FULL mode */

  // tabbar. Make sure no tab is under the cursor
  if (QTabBar* tabBar = qobject_cast<QTabBar*>(widget))
    return tabBar->tabAt( position ) == -1;

  /*
    check groupboxes
    prevent drag if unchecking grouboxes
  */
  if(QGroupBox *groupBox = qobject_cast<QGroupBox*>(widget))
  {
    // non checkable group boxes are always ok
    if (!groupBox->isCheckable()) return true;

    // gather options to retrieve checkbox subcontrol rect
    QStyleOptionGroupBox opt;
    opt.initFrom (groupBox);
    if (groupBox->isFlat())
      opt.features |= QStyleOptionFrameV2::Flat;
    opt.lineWidth = 1;
    opt.midLineWidth = 0;
    opt.text = groupBox->title();
    opt.textAlignment = groupBox->alignment();
    opt.subControls = (QStyle::SC_GroupBoxFrame | QStyle::SC_GroupBoxCheckBox);
    if (!groupBox->title().isEmpty())
      opt.subControls |= QStyle::SC_GroupBoxLabel;

    opt.state |= (groupBox->isChecked() ? QStyle::State_On : QStyle::State_Off);

    // check against groupbox checkbox
    if (groupBox->style()->subControlRect (QStyle::CC_GroupBox, &opt, QStyle::SC_GroupBoxCheckBox, groupBox )
                                          .contains (position))
    {
      return false;
    }

    // check against groupbox label
    if (!groupBox->title().isEmpty()
        && groupBox->style()->subControlRect (QStyle::CC_GroupBox, &opt, QStyle::SC_GroupBoxLabel, groupBox)
                                             .contains (position ))
    {
      return false;
    }

    return true;

  }

  // labels
  if (QLabel* label = qobject_cast<QLabel*>(widget))
  {
    if (label->textInteractionFlags().testFlag (Qt::TextSelectableByMouse))
      return false;
  }

  // abstract item views
  QAbstractItemView* itemView (NULL);
  if ((itemView = qobject_cast<QListView*>( widget->parentWidget()))
      || (itemView = qobject_cast<QTreeView*>( widget->parentWidget())))
  {
    if (widget == itemView->viewport())
    {
      // QListView
      if (itemView->frameShape() != QFrame::NoFrame)
        return false;
      else if (itemView->selectionMode() != QAbstractItemView::NoSelection
               && itemView->selectionMode() != QAbstractItemView::SingleSelection
               && itemView->model() && itemView->model()->rowCount())
      {
        return false;
      }
      else if (itemView->model() && itemView->indexAt (position).isValid())
        return false;
    }

  }
  else if (( itemView = qobject_cast<QAbstractItemView*>(widget->parentWidget())))
  {
    if (widget == itemView->viewport())
    {
      // QAbstractItemView
      if (itemView->frameShape() != QFrame::NoFrame)
        return false;
      else if (itemView->indexAt (position).isValid())
        return false;
    }

  }
  else if (QGraphicsView* graphicsView = qobject_cast<QGraphicsView*>(widget->parentWidget()))
  {
    if (widget == graphicsView->viewport())
    {
      // QGraphicsView
      if (graphicsView->frameShape() != QFrame::NoFrame)
        return false;
      else if (graphicsView->dragMode() != QGraphicsView::NoDrag)
        return false;
      else if (graphicsView->itemAt (position))
        return false;
    }

  }

  return true;
}
/*************************/
void WindowManager::resetDrag (void)
{
  target_.clear();
  if (dragTimer_.isActive())
    dragTimer_.stop();
  dragPoint_ = QPoint();
  globalDragPoint_ = QPoint();
  dragAboutToStart_ = false;
  dragInProgress_ = false;
}
/*************************/
void WindowManager::startDrag (QWidget *widget, const QPoint &position)
{
  if (!(enabled() && widget) || QWidget::mouseGrabber())
    return;

  X11MoveTrigger (widget->window()->internalWinId(),
                  position.x(), position.y());

  dragInProgress_ = true;
}
/*************************/
bool WindowManager::isDockWidgetTitle (const QWidget* widget) const
{
  if (!widget) return false;
  if (const QDockWidget* dockWidget = qobject_cast<const QDockWidget*>(widget->parent()))
    return widget == dockWidget->titleBarWidget();
  else
    return false;

}
/*************************/
bool WindowManager::AppEventFilter::eventFilter (QObject* object, QEvent* event)
{
  if (event->type() == QEvent::MouseButtonRelease)
  {
    // stop drag timer
    if (parent_->dragTimer_.isActive())
      parent_->resetDrag();

    // unlock
    if (parent_->isLocked())
      parent_->setLocked (false);
  }

  if (!parent_->enabled()) return false;

  /*
    If a drag is in progress, the widget will not receive any event.
    We trigger on the first MouseMove or MousePress events that are received
    by any widget in the application to detect that the drag is finished.
  */
  if (parent_->dragInProgress_
      && parent_->target_
      && (event->type() == QEvent::MouseMove
          || event->type() == QEvent::MouseButtonPress))
  {
    return appMouseEvent (object, event);
  }

  return false;
}
/*************************/
bool WindowManager::AppEventFilter::appMouseEvent (QObject* object, QEvent* event)
{
  Q_UNUSED(object);

  // store target window (see later)
  QWidget* window (parent_->target_.data()->window());

  /*
    Post some mouseRelease event to the target, in order to counter balance
    the mouse press that triggered the drag. Note that it triggers resetDrag()!
  */
  QMouseEvent mouseEvent (QEvent::MouseButtonRelease,
                          parent_->dragPoint_,
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  qApp->sendEvent (parent_->target_.data(), &mouseEvent);

  if (event->type() == QEvent::MouseMove)
  {
    /*
      HACK: Quickly move the main cursor out of the window and back.
      This is needed to get the focus right for the window children.
      The origin of this issue is unknown at the moment.
    */
    const QPoint cursor = QCursor::pos();
    QCursor::setPos (window->mapToGlobal (window->rect().topRight()) + QPoint (1, 0));
    QCursor::setPos (cursor);
  }

  return true;
}
}
