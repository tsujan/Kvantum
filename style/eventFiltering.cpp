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

#include "Kvantum.h"

#include <QLibrary> // only for setGtkVariant()
#include <QPainter>
#include <QTimer>
#include <QApplication>
#include <QToolBar>
#include <QMainWindow>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QProgressBar>
#include <QMenu>
#include <QGroupBox>
#include <QAbstractScrollArea>
#include <QScrollBar>
#include <QDoubleSpinBox>
#include <QDateTimeEdit>
#include <QPaintEvent>
#include <QMenuBar>
#include <QDialog>
#include <QDesktopWidget> // for positioning menus

namespace Kvantum
{

static void setGtkVariant(QWidget *widget, bool dark) // by Craig Drummond
{
  if (!widget || QLatin1String("xcb")!=qApp->platformName()) {
    return;
  }
  static const char *_GTK_THEME_VARIANT="_GTK_THEME_VARIANT";

  // Check if already set
  QByteArray styleVar = dark ? "dark" : "light";
  QVariant var=widget->property("_GTK_THEME_VARIANT");
  if (var.isValid() && var.toByteArray()==styleVar) {
    return;
  }

  // Typedef's from xcb/xcb.h - copied so that there is no
  // direct xcb dependency
  typedef quint32 XcbAtom;

  struct XcbInternAtomCookie {
    unsigned int sequence;
  };

  struct XcbInternAtomReply {
    quint8  response_type;
    quint8  pad0;
    quint16 sequence;
    quint32 length;
    XcbAtom atom;
  };

  typedef void * (*XcbConnectFn)(int, int);
  typedef XcbInternAtomCookie (*XcbInternAtomFn)(void *, int, int, const char *);
  typedef XcbInternAtomReply * (*XcbInternAtomReplyFn)(void *, XcbInternAtomCookie, int);
  typedef int (*XcbChangePropertyFn)(void *, int, int, XcbAtom, XcbAtom, int, int, const void *);
  typedef int (*XcbFlushFn)(void *);

  static QLibrary *lib = 0;
  static XcbAtom variantAtom = 0;
  static XcbAtom utf8TypeAtom = 0;
  static void *xcbConn = 0;
  static XcbChangePropertyFn XcbChangePropertyFnPtr = 0;
  static XcbFlushFn XcbFlushFnPtr = 0;

  if (!lib) {
    lib = new QLibrary("libxcb", qApp);

    if (lib->load()) {
      XcbConnectFn XcbConnectFnPtr=(XcbConnectFn)lib->resolve("xcb_connect");
      XcbInternAtomFn XcbInternAtomFnPtr=(XcbInternAtomFn)lib->resolve("xcb_intern_atom");
      XcbInternAtomReplyFn XcbInternAtomReplyFnPtr=(XcbInternAtomReplyFn)lib->resolve("xcb_intern_atom_reply");

      XcbChangePropertyFnPtr=(XcbChangePropertyFn)lib->resolve("xcb_change_property");
      XcbFlushFnPtr=(XcbFlushFn)lib->resolve("xcb_flush");
      if (XcbConnectFnPtr && XcbInternAtomFnPtr && XcbInternAtomReplyFnPtr && XcbChangePropertyFnPtr && XcbFlushFnPtr) {
        xcbConn=(*XcbConnectFnPtr)(0, 0);
        if (xcbConn) {
          XcbInternAtomReply *typeReply = (*XcbInternAtomReplyFnPtr)(xcbConn, (*XcbInternAtomFnPtr)(xcbConn, 0, 11, "UTF8_STRING"), 0);

          if (typeReply) {
            XcbInternAtomReply *gtkVarReply = (*XcbInternAtomReplyFnPtr)(xcbConn, (*XcbInternAtomFnPtr)(xcbConn, 0, strlen(_GTK_THEME_VARIANT),
                                                                                                        _GTK_THEME_VARIANT), 0);
            if (gtkVarReply) {
               utf8TypeAtom = typeReply->atom;
               variantAtom = gtkVarReply->atom;
               free(gtkVarReply);
            }
            free(typeReply);
          }
        }
      }
    }
  }

  if (0!=variantAtom) {
    (*XcbChangePropertyFnPtr)(xcbConn, 0, widget->effectiveWinId(), variantAtom, utf8TypeAtom, 8,
                              styleVar.length(), (const void *)styleVar.constData());
    (*XcbFlushFnPtr)(xcbConn);
    widget->setProperty(_GTK_THEME_VARIANT, styleVar);
  }
}

void Style::drawBg(QPainter *p, const QWidget *widget) const
{
  if (widget->palette().color(widget->backgroundRole()) == Qt::transparent)
    return; // Plasma FIXME: needed?
  QRect bgndRect(widget->rect());
  interior_spec ispec = getInteriorSpec("DialogTranslucent");
  size_spec sspec = getSizeSpec("DialogTranslucent");
  if (ispec.element.isEmpty())
  {
    ispec = getInteriorSpec("Dialog");
    sspec = getSizeSpec("Dialog");
  }
  if (!ispec.element.isEmpty()
      && !widget->windowFlags().testFlag(Qt::FramelessWindowHint)) // not a panel
  {
    if (QWidget *child = widget->childAt(0,0))
    { // even dialogs may have menubar or toolbar (as in Qt Designer)
      if (qobject_cast<QMenuBar*>(child) || qobject_cast<QToolBar*>(child))
      {
        ispec = getInteriorSpec("WindowTranslucent");
        sspec = getSizeSpec("WindowTranslucent");
        if (ispec.element.isEmpty())
        {
          ispec = getInteriorSpec("Window");
          sspec = getSizeSpec("Window");
        }
      }
    }
  }
  else
  {
    ispec = getInteriorSpec("WindowTranslucent");
    sspec = getSizeSpec("WindowTranslucent");
    if (ispec.element.isEmpty())
    {
      ispec = getInteriorSpec("Window");
      sspec = getSizeSpec("Window");
    }
  }
  frame_spec fspec;
  default_frame_spec(fspec);

  QString suffix = "-normal";
  if (isWidgetInactive(widget))
    suffix = "-normal-inactive";

  if (tspec_.no_window_pattern && (ispec.px > 0 || ispec.py > 0))
    ispec.px = -2; // no tiling pattern with translucency

  p->setClipRegion(bgndRect, Qt::IntersectClip);
  int ro = tspec_.reduce_window_opacity;
  if (ro > 0)
  {
    p->save();
    p->setOpacity(1.0 - static_cast<qreal>(tspec_.reduce_window_opacity)/100.0);
  }
  int dh = sspec.incrementH ? sspec.minH : qMax(sspec.minH - bgndRect.height(), 0);
  int dw = sspec.incrementW ? sspec.minW : qMax(sspec.minW - bgndRect.width(), 0);
  if (!renderInterior(p,bgndRect.adjusted(0,0,dw,dh),fspec,ispec,ispec.element+suffix))
  { // no window interior element but with reduced translucency
    p->fillRect(bgndRect, QApplication::palette().color(suffix.contains("-inactive")
                                                          ? QPalette::Inactive
                                                          : QPalette::Active,
                                                        QPalette::Window));
  }
  if (ro > 0)
    p->restore();
}

bool Style::eventFilter(QObject *o, QEvent *e)
{
  QWidget *w = qobject_cast<QWidget*>(o);

  switch (e->type()) {
  case QEvent::Paint:
    if (w)
    {
      if (w->inherits("KisAbstractSliderSpinBox") || w->inherits("Digikam::DAbstractSliderSpinBox"))
        isKisSlider_ = true;
      else if (QProgressBar *pb = qobject_cast<QProgressBar*>(o))
      {
        if (pb->maximum() == 0 && pb->minimum() == 0)
        { // add the busy progress bar to the list
          if (!progressbars_.contains(w))
          {
            progressbars_.insert(w, 0);
            if (!progressTimer_->isActive())
              progressTimer_->start(50);
          }
        }
        else if (!progressbars_.isEmpty())
        {
          progressbars_.remove(w);
          if (progressbars_.size() == 0)
            progressTimer_->stop();
        }
        isKisSlider_ = false;
      }
      else if (w->isWindow()
               && w->testAttribute(Qt::WA_StyledBackground)
               && w->testAttribute(Qt::WA_TranslucentBackground)
               && !isPlasma_ && !isOpaque_ && !subApp_ && !isLibreoffice_
               /*&& tspec_.translucent_windows*/ // this could have weird effects with style or settings change
              )
      {
        switch (w->windowFlags() & Qt::WindowType_Mask) {
          case Qt::Window:
          case Qt::Dialog:
          case Qt::Popup:
          case Qt::ToolTip:
          case Qt::Sheet: {
            if (qobject_cast<QMenu*>(w)) break;
            QPainter p(w);
            p.setClipRegion(static_cast<QPaintEvent*>(e)->region());
            drawBg(&p,w);
            break;
          }
          default: break;
        }
      }
      else if (!w->underMouse() && w->inherits("KMultiTabBarTab"))
      {
        if (QPushButton *pb = qobject_cast<QPushButton*>(o))
        {
          if (!pb->isChecked())
          {
            QPainter p(w);
            QStyleOptionToolButton opt;
            opt.initFrom(w);
            opt.state |= QStyle::State_AutoRaise;
            drawPrimitive(QStyle::PE_PanelButtonTool,&opt,&p,w);
          }
        }
      }
    }
    break;

  case QEvent::Enter:
    if (w && hspec_.scroll_jump_workaround)
    {
      if (qobject_cast<QLineEdit*>(o))
      { // consider the special case of a spin box
        if (QAbstractSpinBox *sb = qobject_cast<QAbstractSpinBox*>(w->parentWidget()))
        {
          enteredWidget_ = sb;
          break;
        }
      }
      /* In the case of Dolphin, first KItemListContainerViewport is entered
         and then only a QWidget, which ruins our workaround. Fortunately,
         KItemListContainerViewport is the parent of that QWidget. */
      if (enteredWidget_ && enteredWidget_.data()->inherits("KItemListContainerViewport")
          && w->parentWidget() == enteredWidget_)
      {
        break;
      }
      enteredWidget_ = w;
    }
    break;

  case QEvent::HoverMove:
    if (QTabBar *tabbar = qobject_cast<QTabBar*>(o))
    { // see QEvent::HoverEnter below
      QHoverEvent *he = static_cast<QHoverEvent*>(e);
      int indx = tabbar->tabAt(he->pos());
      if (indx > -1)
      {
        int diff = qAbs(indx - tabbar->currentIndex());
        if (tabHoverRect_.isNull()
            && diff == 1)
        {
          /* the cursor has moved to a tab adjacent to the active tab */
          QRect r = tabbar->tabRect(indx);
          const frame_spec fspec = getFrameSpec("Tab");
          int overlap = tspec_.active_tab_overlap;
          int exp = qMin(fspec.expansion, qMin(r.width(), r.height())) / 2 + 1;
          overlap = qMin(overlap, qMax(exp, qMax(fspec.left, fspec.right)));
          if (tabbar->shape() == QTabBar::RoundedWest
              || tabbar->shape() == QTabBar::RoundedEast
              || tabbar->shape() == QTabBar::TriangularWest
              || tabbar->shape() == QTabBar::TriangularEast)
          {
            tabHoverRect_ = r.adjusted(0,-overlap,0,overlap);
          }
          else
            tabHoverRect_ = r.adjusted(-overlap,0,overlap,0);
          tabbar->update(tabHoverRect_);
        }
        else if (!tabHoverRect_.isNull()
                 && (diff == 0 || diff == 2))
        {
          /* the cursor has left a tab adjacent to the active tab
             and moved to the active tab or the next inactive tab */
          tabbar->update(tabHoverRect_);
          tabHoverRect_ = QRect();
        }
      }
      else if (!tabHoverRect_.isNull())
      {
        /* the cursor has left a tab adjacent to the active tab
           and moved to an empty place on the tabbar */
        tabbar->update(tabHoverRect_);
        tabHoverRect_ = QRect();
      }

    }
    break;

  case QEvent::HoverEnter:
    if (QTabBar *tabbar = qobject_cast<QTabBar*>(o))
    {
      /* In qtabbar.cpp -> QTabBar::event(), Qt updates only the tab rect
         when the cursor moves between the tab widget and the tab, which
         results in an ugly hover effect with overlapping. So, we update
         the extended tab rect when there's an overlapping. */
      QHoverEvent *he = static_cast<QHoverEvent*>(e);
      int indx = tabbar->tabAt(he->pos());
      if (indx > -1 && qAbs(indx - tabbar->currentIndex()) == 1)
      {
        QRect r = tabbar->tabRect(indx);
        const frame_spec fspec = getFrameSpec("Tab");
        int overlap = tspec_.active_tab_overlap;
        int exp = qMin(fspec.expansion, qMin(r.width(), r.height())) / 2 + 1;
        overlap = qMin(overlap, qMax(exp, qMax(fspec.left, fspec.right)));
        if (tabbar->shape() == QTabBar::RoundedWest
            || tabbar->shape() == QTabBar::RoundedEast
            || tabbar->shape() == QTabBar::TriangularWest
            || tabbar->shape() == QTabBar::TriangularEast)
        {
          tabHoverRect_ = r.adjusted(0,-overlap,0,overlap);
        }
        else
          tabHoverRect_ = r.adjusted(-overlap,0,overlap,0);
        tabbar->update(tabHoverRect_);
      }
      else
        tabHoverRect_ = QRect();
    }
    else if (w && w->isEnabled() && tspec_.animate_states
             && !w->isWindow() // WARNING: Translucent (Qt5) windows have enter event!
             && !qobject_cast<QAbstractSpinBox*>(o) && !qobject_cast<QProgressBar*>(o)
             && !qobject_cast<QLineEdit*>(o) && !qobject_cast<QAbstractScrollArea*>(o)
             && !((tspec_.combo_as_lineedit || tspec_.square_combo_button)
                  && qobject_cast<QComboBox*>(o) && qobject_cast<QComboBox*>(o)->lineEdit()))
    {
      /* if another animation is in progress, end it */
      if (animatedWidget_ && animatedWidget_ != w
          && !w->inherits("QComboBoxPrivateContainer")) // Qt4
      {
        if (opacityTimer_->isActive())
        {
          opacityTimer_->stop();
          animationOpacity_ = 100;
          animatedWidget_->update();
        }
        animatedWidget_ = nullptr;
      }
      if (qobject_cast<QAbstractButton*>(o) || qobject_cast<QGroupBox*>(o))
      {
        /* the animations are started/stopped inside
           the drawing functions by using styleObject */
        animatedWidget_ = w;
      }
      else if (qobject_cast<QComboBox*>(o))
      {
        if (!w->hasFocus())
          animationStartState_ = "normal";
        else if (!animationStartState_.startsWith("c-toggled")) // the popup may have been closed (with Qt5)
          animationStartState_ = "pressed";
        if (isWidgetInactive(w))
          animationStartState_.append("-inactive");
        animatedWidget_ = w;
        animationOpacity_ = 0;
        opacityTimer_->start(ANIMATION_FRAME);
      }
      else if (qobject_cast<QScrollBar*>(o) || qobject_cast<QSlider*>(o))
      {
        animationStartState_ = "normal";
        if (isWidgetInactive(w))
          animationStartState_.append("-inactive");
        animatedWidget_ = w;
        animationOpacity_ = 0;
        opacityTimer_->start(ANIMATION_FRAME);
      }
    }
    break;

  case QEvent::FocusIn:
    if (w && w->isEnabled() && tspec_.animate_states)
    {
      if (qobject_cast<QComboBox*>(o)
          && !((tspec_.combo_as_lineedit || tspec_.square_combo_button) && qobject_cast<QComboBox*>(o)->lineEdit()))
      { // QEvent::MouseButtonPress may follow this
        if (animatedWidget_ // the cusror may have been on the popup scrollbar
            && opacityTimer_->isActive())
        {
          opacityTimer_->stop();
          animationOpacity_ = 100;
          animatedWidget_->update();
        }
        if (!animationStartState_.startsWith("c-toggled")
            && !animationStartState_.startsWith("normal"))
        { // it was hidden or another widget was interacted with  -- there's no other possibility
          animationStartState_ = "normal";
          if (isWidgetInactive(w))
            animationStartState_.append("-inactive");
        }
        animatedWidget_ = w;
        animationOpacity_ = 0;
        opacityTimer_->start(ANIMATION_FRAME);
      }
      else
      {
        QAbstractScrollArea *sa = qobject_cast<QAbstractScrollArea*>(o);
        if ((sa && !w->inherits("QComboBoxListView")) // exclude combo popups
            || (qobject_cast<QLineEdit*>(o)
                // this is only needed for Qt5 -- Qt4 combo lineedits don't have FocusIn event
                && !qobject_cast<QComboBox*>(w->parentWidget()))
            || qobject_cast<QAbstractSpinBox*>(o)
            || ((tspec_.combo_as_lineedit || tspec_.square_combo_button)
                && qobject_cast<QComboBox*>(o) && qobject_cast<QComboBox*>(o)->lineEdit()))
        {
          /* disable animation if focus-in happens immediately after focus-out
             for exactly the same area to prevent flashing */
          if (opacityTimerOut_->isActive()
              && (animatedWidgetOut_ == w
                  || (animatedWidgetOut_ != nullptr
                      && !animatedWidgetOut_->isVisible()
                      && animatedWidgetOut_->size() == w->size()
                      && animatedWidgetOut_->mapToGlobal(QPoint(0,0)) == w->mapToGlobal(QPoint(0,0)))))
          {
            opacityTimerOut_->stop();
            animationOpacityOut_ = 100;
            animatedWidgetOut_ = nullptr;
            /* although there will be no animation after this, animationStartStateOut_ doesn't need
               to be set here because it's only used with QEvent::FocusOut and will be set there */
            break;
          }
          if (animatedWidget_ && animatedWidget_ != w)
          {
            if (sa)
            { // no animation when a scrollbar is going to be animated
              if ((animatedWidget_ == sa->verticalScrollBar() || animatedWidget_ == sa->horizontalScrollBar())
                  && animatedWidget_->rect().contains(animatedWidget_->mapFromGlobal(QCursor::pos())))
              {
                break;
              }
            }
            if (opacityTimer_->isActive())
            {
              opacityTimer_->stop();
              animationOpacity_ = 100;
              animatedWidget_->update();
            }
          }
          animationStartState_ = "normal";
          animatedWidget_ = w;
          animationOpacity_ = 0;
          opacityTimer_->start(ANIMATION_FRAME);
        }
      }
    }
    break;

  case QEvent::FocusOut:
    if (w && w->isEnabled() && tspec_.animate_states)
    {
      QWidget *popup = QApplication::activePopupWidget();
      if (popup && !popup->isAncestorOf(w))
        break; // not due to a popup widget
      if (qobject_cast<QComboBox*>(o)
          || qobject_cast<QLineEdit*>(o)
          || qobject_cast<QAbstractSpinBox*>(o)
          || (qobject_cast<QAbstractScrollArea*>(o)
              && !w->inherits("QComboBoxListView"))) // exclude combo popups
      {
        /* disable animation if focus-out happens immediately after focus-in
           for exactly the same area to prevent flashing */
        if (opacityTimer_->isActive()
            && (animatedWidget_ == w
                || (animatedWidget_ != nullptr
                    && !animatedWidget_->isVisible()
                    && animatedWidget_->size() == w->size()
                    && animatedWidget_->mapToGlobal(QPoint(0,0)) == w->mapToGlobal(QPoint(0,0)))))
        {
          opacityTimer_->stop();
          animationOpacity_ = 100;
          animatedWidget_ = nullptr;
          animationStartState_ = "normal"; // should be set; no animation after this
          break;
        }
        if (animatedWidgetOut_ && opacityTimerOut_->isActive())
        {
          opacityTimerOut_->stop();
          animationOpacityOut_ = 100;
          animatedWidgetOut_->update();
        }
        if (qobject_cast<QComboBox*>(o)
            && !((tspec_.combo_as_lineedit || tspec_.square_combo_button) && qobject_cast<QComboBox*>(o)->lineEdit()))
        {
          animationStartStateOut_ = "pressed";
        }
        else
          animationStartStateOut_ = "focused";
        animatedWidgetOut_ = w;
        animationOpacityOut_ = 0;
        opacityTimerOut_->start(ANIMATION_FRAME);
      }
    }
    break;

  case QEvent::HoverLeave:
    if (QTabBar *tabbar = qobject_cast<QTabBar*>(o))
    { // see QEvent::HoverEnter above
      if (!tabHoverRect_.isNull())
      {
        tabbar->update(tabHoverRect_);
        tabHoverRect_ = QRect();
      }
    }
    // we use HoverLeave and not Leave because of popups of comboxes
    else if (w && w->isEnabled() && tspec_.animate_states && animatedWidget_ == w
             && !qobject_cast<QAbstractSpinBox*>(o)
             && !qobject_cast<QLineEdit*>(o) && !qobject_cast<QAbstractScrollArea*>(o)
             && !((tspec_.combo_as_lineedit || tspec_.square_combo_button)
                  && qobject_cast<QComboBox*>(o) && qobject_cast<QComboBox*>(o)->lineEdit()))
    {
      if (qobject_cast<QAbstractButton*>(o) || qobject_cast<QGroupBox*>(o))
      {
        /* button animations are started/stopped inside their drawing functions
           by using styleObject (groupboxes are always checkable here) */
        animatedWidget_ = w;
      }
      else if (!opacityTimer_->isActive())
      {
        animatedWidget_ = w;
        animationOpacity_ = 0;
        opacityTimer_->start(ANIMATION_FRAME);
      }
    }
    break;

  case QEvent::MouseButtonRelease: {
    QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(e);
    if (!mouseEvent || mouseEvent->button() != Qt::LeftButton)
      break;
    if (w && w->isEnabled() && tspec_.animate_states)
    {
      if (qobject_cast<QAbstractButton*>(o) || qobject_cast<QGroupBox*>(o))
      {
        if (opacityTimer_->isActive() && animatedWidget_)
        { // finish the previous animation, whether in this widget or not
          if (animatedWidget_ == w)
          {
            animationOpacity_ = 100;
            animatedWidget_->repaint();
          }
          else
          {
            opacityTimer_->stop();
            animationOpacity_ = 100;
            animatedWidget_->update();
          }
        }
        animatedWidget_ = w;
        animationOpacity_ = 0;
      }
      else if ((qobject_cast<QComboBox*>(o) // impossible because of popup
                && !((tspec_.combo_as_lineedit || tspec_.square_combo_button) && qobject_cast<QComboBox*>(o)->lineEdit()))
               || qobject_cast<QScrollBar*>(o) || qobject_cast<QSlider*>(o))
      {
        if (animatedWidget_ && animatedWidget_ != w
            && opacityTimer_->isActive())
        {
          opacityTimer_->stop();
          animationOpacity_ = 100;
          animatedWidget_->update();
        }
        animatedWidget_ = w;
        animationOpacity_ = 0;
        opacityTimer_->start(ANIMATION_FRAME);
      }
    }
    break;
  }

  case QEvent::MouseButtonPress: {
    QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(e);
    if (!mouseEvent || mouseEvent->button() != Qt::LeftButton)
      break;
    if (w && w->isEnabled() && tspec_.animate_states
        && (qobject_cast<QAbstractButton*>(o)
            || qobject_cast<QGroupBox*>(o)
            || qobject_cast<QSlider*>(o)
            || qobject_cast<QScrollBar*>(o)
            || (qobject_cast<QComboBox*>(o)
                && !((tspec_.combo_as_lineedit || tspec_.square_combo_button) && qobject_cast<QComboBox*>(o)->lineEdit())))
        /* FIXME: Can't we just use the condition "animatedWidget_ == w" and exclude the widgets below? */
        // these widget are dealt with in QEvent::FocusIn
        /*&& !qobject_cast<QAbstractSpinBox*>(o)
        && !qobject_cast<QLineEdit*>(o) && !qobject_cast<QAbstractScrollArea*>(o)*/)
    {
      if (qobject_cast<QAbstractButton*>(o) || qobject_cast<QGroupBox*>(o))
      {
        if (opacityTimer_->isActive() && animatedWidget_)
        {
          if (animatedWidget_ == w)
          {
            animationOpacity_ = 100;
            animatedWidget_->repaint();
          }
          else
          {
            opacityTimer_->stop();
            animationOpacity_ = 100;
            animatedWidget_->update();
          }
        }
        animatedWidget_ = w;
        animationOpacity_ = 0;
      }
      else
      {
        if (animatedWidget_ && animatedWidget_ != w
            && opacityTimer_->isActive())
        {
          opacityTimer_->stop();
          animationOpacity_ = 100;
          animatedWidget_->update();
        }
        animatedWidget_ = w;
        animationOpacity_ = 0;
        opacityTimer_->start(ANIMATION_FRAME);
      }
    }
    break;
  }

  case QEvent::StyleChange:
    if (QComboBox *combo = qobject_cast<QComboBox*>(w))
    {
      if (combo->style() == this // WARNING: Otherwise, the delegate shouldn't be restored.
          && qobject_cast<KvComboItemDelegate*>(combo->itemDelegate()))
      {
        /* QComboBoxPrivate::updateDelegate() won't work correctly
           on style change if the item delegate isn't restored here */
        QList<QItemDelegate*> delegates = combo->findChildren<QItemDelegate*>();
        for (int i = 0; i < delegates.count(); ++i)
        {
          if (delegates.at(i)->inherits("QComboBoxDelegate"))
          {
            combo->setItemDelegate(delegates.at(i));
            /* we shouldn't delete the previous delegate here
               because QComboBox::setItemDelegate() deletes it */
            break;
          }
        }
      }
    }
    else if (gtkDesktop_
             && (!w->parent() || !qobject_cast<QWidget*>(w->parent())
                 || qobject_cast<QDialog*>(w) || qobject_cast<QMainWindow*>(w)))
    {
      setGtkVariant(w, tspec_.dark_titlebar);
    }
    break;

  case QEvent::WindowActivate:
    if (qobject_cast<QAbstractItemView*>(o))
    {
      QPalette palette = w->palette();
      if (palette.color(QPalette::Active, QPalette::Text)
          != QApplication::palette().color(QPalette::Active, QPalette::Text))
      { // Custom text color; don't set palettes! The app is responsible for all colors.
        break;
      }
      const label_spec lspec = getLabelSpec("ItemView");
      /* set the normal inactive text color to the normal active one
         (needed when the app sets it inactive) */
      QColor normalCol = getFromRGBA(lspec.normalColor);
      if (!normalCol.isValid())
        normalCol = QApplication::palette().color(QPalette::Active,QPalette::Text);
      palette.setColor(QPalette::Inactive, QPalette::Text, normalCol);
      if (!hasInactiveSelItemCol_)
      {
        w->setPalette(palette);
        break;
      }
      /* set the toggled inactive text color to the toggled active one
         (the main purpose of installing an event filter on the view) */
      palette.setColor(QPalette::Inactive, QPalette::HighlightedText,
                       getFromRGBA(lspec.toggleColor));
      /* use the active highlight color for the toggled (unfocused) item if there's
         no contrast with the pressed state because some apps (like Qt Designer)
         may not call PE_PanelItemViewItem but highlight the item instead */
      if (!toggledItemHasContrast_)
      {
        palette.setColor(QPalette::Inactive, QPalette::Highlight,
                         QApplication::palette().color(QPalette::Active,QPalette::Highlight));
      }
      w->setPalette(palette);
    }
    break;

  case QEvent::WindowDeactivate:
    if (qobject_cast<QAbstractItemView*>(o))
    {
      QPalette palette = w->palette();
      if (palette.color(QPalette::Active, QPalette::Text)
          != QApplication::palette().color(QPalette::Active, QPalette::Text))
      {
        break;
      }
      const label_spec lspec = getLabelSpec("ItemView");
      /* restore the normal inactive text color (which was changed at QEvent::WindowActivate) */
      QColor normalInactiveCol = getFromRGBA(lspec.normalInactiveColor);
      if (!normalInactiveCol.isValid())
        normalInactiveCol = QApplication::palette().color(QPalette::Inactive,QPalette::Text);
      palette.setColor(QPalette::Inactive, QPalette::Text, normalInactiveCol);
      if (!hasInactiveSelItemCol_)
      { // custom text color
        w->setPalette(palette);
        break;
      }
      /* restore the toggled inactive text color (which was changed at QEvent::WindowActivate) */
      palette.setColor(QPalette::Inactive,QPalette::HighlightedText,
                       getFromRGBA(lspec.toggleInactiveColor));
      /* restore the inactive highlight color (which was changed at QEvent::WindowActivate) */
      if (!toggledItemHasContrast_)
      {
        palette.setColor(QPalette::Inactive, QPalette::Highlight,
                         QApplication::palette().color(QPalette::Inactive,QPalette::Highlight));
      }
      w->setPalette(palette);
    }
    break;

  case QEvent::Show:
    if (w)
    {
      if (animatedWidget_ && (w->windowType() == Qt::Popup
                              || w->windowType() == Qt::ToolTip))
      {
        popupOrigins_.insert(w, animatedWidget_);
      }

      if (QProgressBar *pb = qobject_cast<QProgressBar*>(o))
      {
        if (pb->maximum() == 0 && pb->minimum() == 0)
        {
          if (!progressbars_.contains(w))
            progressbars_.insert(w, 0);
          if (!progressTimer_->isActive())
            progressTimer_->start(50);
        }
      }
#if (QT_VERSION >= QT_VERSION_CHECK(5,11,0))
      else if (QMenu *menu = qobject_cast<QMenu*>(o))
#else
      else if (qobject_cast<QMenu*>(o))
#endif
      {
        /* "magical" condition for a submenu */
        QPoint parentMenuCorner;
        QMenu *parentMenu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (!parentMenu)
        { // search for a detached menu with an active action
          const QWidgetList topLevels = QApplication::topLevelWidgets();
          for (QWidget *topWidget : topLevels)
          {
            if (topWidget->isVisible()
                && qobject_cast<QMenu*>(topWidget)
                && topWidget->testAttribute(Qt::WA_X11NetWmWindowTypeMenu)
                && qobject_cast<QMenu*>(topWidget)->activeAction())
            {
              parentMenu = qobject_cast<QMenu*>(topWidget);
              parentMenuCorner = parentMenu->mapToGlobal(QPoint(0,0));
              break;
            }
          }
        }
        else
          parentMenuCorner = parentMenu->mapToGlobal(QPoint(0,0));
        QMenuBar *parentMenubar = nullptr;
        if (!parentMenu)
        { // search for a menubar with an active action
          if (QMainWindow *mw = qobject_cast<QMainWindow*>(QApplication::activeWindow()))
          {
            if (QMenuBar *mb = qobject_cast<QMenuBar*>(mw->menuWidget()))
            {
              if (mb->activeAction())
                parentMenubar = mb;
            }
          }
        }
        QRect ag(QApplication::desktop()->availableGeometry(w));
        /* this gives the real position AFTER pending movements
           because it's QWidgetData::crect (Qt -> qwidget.h) */
        QRect g(w->geometry());
        int X = g.left();
        int Y = g.top();

        /* WARNING: If compositing is stopped here, we aren't responsible.
                    A check for the state of compositing at this very moment
                    may be CPU-intensive. */
        if (!noComposite_
            && menuShadow_.count() == 4)
        {
          /* compensate for the offset created by the shadow */

          Y -= menuShadow_.at(1); // top shadow

          if (w->layoutDirection() == Qt::RightToLeft)
          { // see explanations for ltr below
            X += menuShadow_.at(2);
            if (parentMenu)
            {
              if (parentMenuCorner.x() < g.left())
                X -= menuShadow_.at(2) + menuShadow_.at(0);
              else
              {
                X += menuShadow_.at(0)
                     - getMenuMargin(true); // workaround for an old Qt bug
              }
            }
            else
            {
              if (parentMenubar)
              {
                QString group = tspec_.merge_menubar_with_toolbar ? "Toolbar" : "MenuBar";
                if (parentMenubar->mapToGlobal(QPoint(0,0)).y() > g.bottom())
                  Y +=  menuShadow_.at(1) + menuShadow_.at(3) + getFrameSpec(group).top;
                else
                  Y -= getFrameSpec(group).bottom;

                QRect activeG = parentMenubar->actionGeometry(parentMenubar->activeAction());
                QPoint activeTopLeft = parentMenubar->mapToGlobal(activeG.topLeft());
                if (g.right() + 1 > activeTopLeft.x() + activeG.width())
                { // Qt positions the menu wrongly in this case but we don't add a workaround
                  X -= menuShadow_.at(2);
                  int delta = menuShadow_.at(2)
                              - (g.right() + 1 - (activeTopLeft.x() + activeG.width()));
                  if (delta > 0)
                    X += delta;
                  else
                    X -= qMin(menuShadow_.at(0), -delta);
                }
              }
              else
              {
                if (!sunkenButton_.isNull())
                {
                  QPoint wTopLeft = sunkenButton_.data()->mapToGlobal(QPoint(0,0));
                  if (wTopLeft.y() >= g.bottom())
                    Y +=  menuShadow_.at(1) + menuShadow_.at(3);
                  if (g.right() + 1 > wTopLeft.x() + sunkenButton_.data()->width())
                  {
                    X -= menuShadow_.at(2);
                    int delta = menuShadow_.at(2)
                                - (g.right() + 1 - (wTopLeft.x() + sunkenButton_.data()->width()));
                    if (delta > 0)
                      X += delta;
                    else
                      X -= qMin(menuShadow_.at(0), -delta);
                  }
                }
                else
                {
                  if (g.bottom() == ag.bottom() && g.top() != ag.top())
                    Y += menuShadow_.at(1) + menuShadow_.at(3);
                  if (g.left() == ag.left() && g.right() != ag.right())
                    X -= menuShadow_.at(2) + menuShadow_.at(0);
                }
              }
            }
          }
          else // ltr
          {
            X -= menuShadow_.at(0); // left shadow
            if (parentMenu)
            {
              if (parentMenuCorner.x() > g.left())
              { // there wasn't enough space to the right of the parent
                X += menuShadow_.at(0) + menuShadow_.at(2);
              }
              else
                X -= menuShadow_.at(2); // right shadow of the left menu
            }
            else
            {
              if (parentMenubar)
              {
                QString group = tspec_.merge_menubar_with_toolbar ? "Toolbar" : "MenuBar";
                if (parentMenubar->mapToGlobal(QPoint(0,0)).y() > g.bottom()) // menu is above menubar
                  Y +=  menuShadow_.at(1) + menuShadow_.at(3) + getFrameSpec(group).top;
                else
                  Y -= getFrameSpec(group).bottom;

                QPoint activeTopLeft = parentMenubar->mapToGlobal(parentMenubar->actionGeometry(
                                                                   parentMenubar->activeAction())
                                                                 .topLeft());
                if (activeTopLeft.x() > g.left()) // because of the right screen border
                {
                  X += menuShadow_.at(0);
                  int delta = menuShadow_.at(0) - (activeTopLeft.x() - g.left());
                  if (delta > 0)
                    X -= delta;
                  else
                    X += qMin(menuShadow_.at(2), -delta);
                }
              }
              else
              {
                if (!sunkenButton_.isNull()) // the menu is triggered by a button
                {
                  QPoint wTopLeft = sunkenButton_.data()->mapToGlobal(QPoint(0,0));
                  if (wTopLeft.y() >= g.bottom()) // above the button (strange! Qt doesn't add 1px)
                    Y +=  menuShadow_.at(1) + menuShadow_.at(3);
                  if (wTopLeft.x() > g.left()) // because of the right screen border
                  {
                    X += menuShadow_.at(0);
                    int delta = menuShadow_.at(0) - (wTopLeft.x() - g.left());
                    if (delta > 0)
                      X -= delta;
                    else
                      X += qMin(menuShadow_.at(2), -delta);
                  }
                }
                else // probably a panel menu
                {
                  /* snap to the screen bottom if possible */
                  if (g.bottom() == ag.bottom() && g.top() != ag.top())
                    Y += menuShadow_.at(1) + menuShadow_.at(3);
                  /* snap to the right screen edge if possible */
                  if (g.right() == ag.right() && g.left() != ag.left())
                    X += menuShadow_.at(0) + menuShadow_.at(2);
                }
              }
            }
          }

#if (QT_VERSION >= QT_VERSION_CHECK(5,11,0))
          /* compensate for an annoyance in Qt 5.11 -> QMenu::internalDelayedPopup() */
          if (parentMenu)
          {
            QAction *activeAct = parentMenu->activeAction();
            if (activeAct && activeAct == menu->menuAction() && activeAct->isEnabled()
                && activeAct->menu() && activeAct->menu()->isEnabled() && activeAct->menu()->isVisible())
            {
              const auto &actions = w->actions();
              if (!actions.isEmpty())
              {
                const auto topActionRect = menu->actionGeometry(actions.first());
                if (g.top() + topActionRect.top() ==  parentMenu->actionGeometry(activeAct).top()
                                                      + parentMenuCorner.y())
                {
                  Y += topActionRect.top();
                }
              }
            }
          }
#endif

          w->move(X,Y);
        }
        else if (!parentMenu && parentMenubar)
        {
          QString group = tspec_.merge_menubar_with_toolbar ? "Toolbar" : "MenuBar";
          if (parentMenubar->mapToGlobal(QPoint(0,0)).y() > g.bottom()) // menu is above menubar
            Y += getFrameSpec(group).top;
          else
            Y -= getFrameSpec(group).bottom;
          w->move(X,Y);
        }
      }
      else if (tspec_.group_toolbar_buttons && qobject_cast<QToolButton*>(o))
      {
        if (QToolBar *toolBar = qobject_cast<QToolBar*>(w->parentWidget()))
          toolBar->update();
      }
      else if (qobject_cast<QAbstractItemView*>(o))
      {
        /* view palettes should also be set when the view is shown
           and not only when its window is activated/deactivated
           (-> QEvent::WindowActivate and QEvent::WindowDeactivate) */
        QPalette palette = w->palette();
        if (palette.color(QPalette::Active, QPalette::Text)
            != QApplication::palette().color(QPalette::Active, QPalette::Text))
        {
          break;
        }
        const label_spec lspec = getLabelSpec("ItemView");
        if (isWidgetInactive(w)) // FIXME: probably not needed with inactive window
        {
          QColor normalInactiveCol = getFromRGBA(lspec.normalInactiveColor);
          if (!normalInactiveCol.isValid())
            normalInactiveCol = QApplication::palette().color(QPalette::Inactive,QPalette::Text);
          palette.setColor(QPalette::Inactive, QPalette::Text, normalInactiveCol);
          if (!hasInactiveSelItemCol_)
          {
            w->setPalette(palette);
            break;
          }
          palette.setColor(QPalette::Inactive,QPalette::HighlightedText,
                           getFromRGBA(lspec.toggleInactiveColor));
          if (!toggledItemHasContrast_)
          {
            palette.setColor(QPalette::Inactive, QPalette::Highlight,
                             QApplication::palette().color(QPalette::Inactive,QPalette::Highlight));
          }
        }
        else
        {
          QColor normalCol = getFromRGBA(lspec.normalColor);
          if (!normalCol.isValid())
            normalCol = QApplication::palette().color(QPalette::Active,QPalette::Text);
          palette.setColor(QPalette::Inactive, QPalette::Text, normalCol);
          if (!hasInactiveSelItemCol_)
          {
            w->setPalette(palette);
            break;
          }
          palette.setColor(QPalette::Inactive, QPalette::HighlightedText,
                           getFromRGBA(lspec.toggleColor));
          if (!toggledItemHasContrast_)
          {
            palette.setColor(QPalette::Inactive, QPalette::Highlight,
                             QApplication::palette().color(QPalette::Active,QPalette::Highlight));
          }
        }
        w->setPalette(palette);
      }
      else if (gtkDesktop_
               && (!w->parent() || !qobject_cast<QWidget *>(w->parent())
                   || qobject_cast<QDialog *>(w) || qobject_cast<QMainWindow *>(w)))
      {
        setGtkVariant(w, tspec_.dark_titlebar);
      }
    }
    break;

  /* FIXME For some reason unknown to me (a Qt5 bug?), the Qt5 spinbox size hint
     is sometimes wrong as if Qt5 spinboxes don't have time to consult CT_SpinBox
     although they should (-> qabstractspinbox.cpp -> QAbstractSpinBox::sizeHint).
     The same thing rarely happens with Qt4 too. Here we force a minimum size by
     using CT_SpinBox when the maximum size isn't set by the app or isn't smaller
     than our size. */
  case QEvent::ShowToParent:
    if (w
        /* not if it's just a QAbstractSpinBox, hoping that
           no one sets the minimum width in normal cases */
        && (qobject_cast<QSpinBox*>(o)
            || qobject_cast<QDoubleSpinBox*>(o)
            || qobject_cast<QDateTimeEdit*>(o)))
    {
      QSize size = sizeFromContents(CT_SpinBox,nullptr,QSize(),w);
      if (w->maximumWidth() > size.width())
        w->setMinimumWidth(size.width());
      if (w->maximumHeight() > size.height())
        w->setMinimumHeight(size.height());
    }
    /* correct line-edit palettes on stylable toolbars if needed */
    else if (qobject_cast<QLineEdit*>(o)
             && (!getFrameSpec("ToolbarLineEdit").element.isEmpty()
                 || !getInteriorSpec("ToolbarLineEdit").element.isEmpty())
             && getStylableToolbarContainer(w, true))
    {
      QPalette palette = w->palette();
      const label_spec tlspec = getLabelSpec("Toolbar");
      QColor col = getFromRGBA(tlspec.normalColor);
      if (col.isValid() && palette.color(QPalette::Active, QPalette::Text) != col)
      {
        palette.setColor(QPalette::Active, QPalette::Text, col);
        QColor col1 = getFromRGBA(tlspec.normalInactiveColor);
        if (!col1.isValid()) col1 = col;
        palette.setColor(QPalette::Inactive, QPalette::Text, col1);
        col.setAlpha(102); // 0.4 * disabledCol.alpha()
        palette.setColor(QPalette::Disabled, QPalette::Text,col);
        w->setPalette(palette);
      }
    }
    break;

  case QEvent::Wheel :
    if (w && enteredWidget_.data() == w/* && hspec_.scroll_jump_workaround*/)
    {
      if (QWheelEvent *we = static_cast<QWheelEvent*>(e))
      {
        enteredWidget_.clear();
        if (we->angleDelta().manhattanLength() >= 240)
          return true;
      }
    }
    break;

  case QEvent::Hide:
    if (qobject_cast<QToolButton*>(o))
    {
      if (tspec_.group_toolbar_buttons)
      {
        if (QToolBar *toolBar = qobject_cast<QToolBar*>(w->parentWidget()))
          toolBar->update();
      }
      //break; // toolbuttons may be animated (see below)
    }
    else if (w && w->isEnabled() && tspec_.animate_states)
    {
      if (w->inherits("QComboBoxPrivateContainer")
          && qobject_cast<QComboBox*>(w->parentWidget()))
      {
        /* start with an appropriate state on closing popup, considering
           that lineedits only have normal and focused states in Kvantum */
        if ((tspec_.combo_as_lineedit || tspec_.square_combo_button)
            && qobject_cast<QComboBox*>(w->parentWidget())->lineEdit())
        {
          animationStartState_ = "normal"; // -> QEvent::FocusIn
        }
        else
          animationStartState_ = "c-toggled"; // distinguish it from a toggled button
        /* ensure that the combobox will be animated on closing popup
           (especially needed if the cursor has been on the popup) */
        animatedWidget_ = w->parentWidget();
        animationOpacity_ = 0;
        break;
      }
      else if (w->windowType() == Qt::Popup
               || w->windowType() == Qt::ToolTip)
      { // let the popup origin have a fade-out animation
        animatedWidget_ = popupOrigins_.value(w);
        popupOrigins_.remove(w);
      }
      /* let the state animation continue (not necessary but useful
         for better flash prevention -- see FocusIn and FocusOut) */
      else if ((animatedWidget_ == w && opacityTimer_->isActive())
               || (animatedWidgetOut_ == w && opacityTimerOut_->isActive()))
      {
        break;
      }
    }
    /* Falls through. */

  case QEvent::Destroy: // FIXME: Isn't QEvent::Hide enough?
    if (w)
    {
      if (!progressbars_.isEmpty() && qobject_cast<QProgressBar*>(o))
      {
        progressbars_.remove(w);
        if (progressbars_.size() == 0)
          progressTimer_->stop();
      }
      else if (tspec_.animate_states)
      {
        if (w->windowType() == Qt::Popup
            || w->windowType() == Qt::ToolTip)
        {
          popupOrigins_.remove(w);
        }
        else // popups aren't animated
        {
          if (animatedWidget_ == w)
          {
            opacityTimer_->stop();
            animatedWidget_ = nullptr;
            animationOpacity_ = 100;
          }
          if (animatedWidgetOut_ == w)
          {
            opacityTimerOut_->stop();
            animatedWidgetOut_ = nullptr;
            animationOpacityOut_ = 100;
          }
        }
      }
    }
    break;

  default:
    return false;
  }

  return false;
}

}
