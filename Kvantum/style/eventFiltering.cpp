/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2024 <tsujan2000@gmail.com>
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

#include <QPainter>
#include <QTimer>
#include <QApplication>
#include <QToolBar>
#include <QMainWindow>
#include <QPushButton>
#include <QCommandLinkButton>
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
#include <QWindow> // for positioning menus
#include <QScreen> // for positioning menus
#include <QStylePainter>

namespace Kvantum
{

void Style::drawBg(QPainter *p, const QWidget *widget) const
{
  if (widget->palette().color(widget->backgroundRole()) == Qt::transparent)
    return; // Plasma FIXME: needed?
  QRect bgndRect(widget->rect());
  interior_spec ispec = getInteriorSpec(KSL("DialogTranslucent"));
  size_spec sspec = getSizeSpec(KSL("DialogTranslucent"));
  if (ispec.element.isEmpty())
  {
    ispec = getInteriorSpec(KSL("Dialog"));
    sspec = getSizeSpec(KSL("Dialog"));
  }
  if (!ispec.element.isEmpty()
      && !widget->windowFlags().testFlag(Qt::FramelessWindowHint)) // not a panel
  {
    if (QWidget *child = widget->childAt(0,0))
    { // even dialogs may have menubar or toolbar (as in Qt Designer)
      if (qobject_cast<QMenuBar*>(child) || qobject_cast<QToolBar*>(child))
      {
        ispec = getInteriorSpec(KSL("WindowTranslucent"));
        sspec = getSizeSpec(KSL("WindowTranslucent"));
        if (ispec.element.isEmpty())
        {
          ispec = getInteriorSpec(KSL("Window"));
          sspec = getSizeSpec(KSL("Window"));
        }
      }
    }
  }
  else
  {
    ispec = getInteriorSpec(KSL("WindowTranslucent"));
    sspec = getSizeSpec(KSL("WindowTranslucent"));
    if (ispec.element.isEmpty())
    {
      ispec = getInteriorSpec(KSL("Window"));
      sspec = getSizeSpec(KSL("Window"));
    }
  }
  frame_spec fspec;
  default_frame_spec(fspec);

  bool isInactive(isWidgetInactive(widget));
  QString suffix(isInactive ? "-normal-inactive" : "-normal");

  if (tspec_.no_window_pattern && (ispec.px > 0 || ispec.py > 0))
    ispec.px = -2; // no tiling pattern with translucency

  p->setClipRegion(bgndRect, Qt::IntersectClip);
  int ro = tspec_.reduce_window_opacity;
  if (ro < 0)
  {
    if (isInactive) ro = -ro;
    else ro = 0;
  }
  if (ro > 0)
  {
    p->save();
    p->setOpacity(1.0 - static_cast<qreal>(ro)/100.0);
  }
  int dh = sspec.incrementH ? sspec.minH : qMax(sspec.minH - bgndRect.height(), 0);
  int dw = sspec.incrementW ? sspec.minW : qMax(sspec.minW - bgndRect.width(), 0);

#if (QT_VERSION >= QT_VERSION_CHECK(6,8,0))
  /* NOTE: This is a workaround for artifacts under Wayland. */
  if (!tspec_.isX11)
  {
    auto origMode = p->compositionMode();
    p->setCompositionMode(QPainter::CompositionMode_Clear);
    p->fillRect(bgndRect, Qt::transparent);
    p->setCompositionMode(origMode);
  }
#endif

  if (!renderInterior(p,bgndRect.adjusted(0,0,dw,dh),fspec,ispec,ispec.element+suffix))
  { // no window interior element but with reduced translucency
    p->fillRect(bgndRect, standardPalette().color(isInactive
                                                    ? QPalette::Inactive
                                                    : QPalette::Active,
                                                  QPalette::Window));
  }
  if (ro > 0)
    p->restore();
}

static inline bool isAnimatedScrollArea(QWidget *w)
{
  auto sa = qobject_cast<QAbstractScrollArea*>(w);
  return (sa != nullptr
          // -> polish(QWidget *widget)
          && (sa->frameStyle() & QFrame::StyledPanel)
          && (sa->backgroundRole() == QPalette::Window
              || sa->backgroundRole() == QPalette::Button)
          && sa->viewport() != nullptr
          && sa->viewport()->autoFillBackground()
          && sa->viewport()->backgroundRole() != QPalette::Window
          && sa->viewport()->backgroundRole() != QPalette::Button
          // exclude combo popups
          && !w->inherits("QComboBoxListView"));
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
      else if (qobject_cast<QProgressBar*>(o))
        isKisSlider_ = false;
      else if (w->isWindow()
               && w->testAttribute(Qt::WA_StyledBackground)
               && w->testAttribute(Qt::WA_TranslucentBackground)
               && !isPlasma_ && !isOpaque_ && !subApp_ /*&& !isLibreoffice_
               && tspec_.translucent_windows*/ // this could have weird effects with style or settings change
              )
      {
        int t = (w->windowFlags() & Qt::WindowType_Mask);
        if (t == Qt::Window || t == Qt::Dialog
            || t == Qt::Popup || t == Qt::ToolTip || t == Qt::Sheet
            /* the window type may have changed after polishing
               but we accept Qt::Tool (as with a dialog in Krita) */
            || (t == Qt::Tool && forcedTranslucency_.contains(w)))
        {
          if (qobject_cast<QMenu*>(w)) break; // QMenu has a filter event too
          QPainter p(w);
          p.setClipRegion(static_cast<QPaintEvent*>(e)->region());
          drawBg(&p,w);
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
      else if (QCommandLinkButton *cbtn = qobject_cast<QCommandLinkButton*>(o))
      {
        /* as in Qt -> qcommandlinkbutton.cpp -> QCommandLinkButton::paintEvent()
           but with modifications for taking into account icon states and text colors */

        QStylePainter p(cbtn);
        p.save();

        QStyleOptionButton option;
        option.initFrom(cbtn);
        option.features |= QStyleOptionButton::CommandLinkButton;
        option.text = QString();
        option.icon = QIcon(); // we draw the icon ourself
        if (cbtn->isChecked())
          option.state |= State_On;
        else if (cbtn->isDown())
          option.state |= State_Sunken;
        else if (cbtn->underMouse())
          option.state |= State_MouseOver;

        /* panel */
        p.drawControl(QStyle::CE_PushButton, option);

        const int leftMargin = 7;
        const int topMargin = 10;
        const int rightMargin = 4;
        const int bottomMargin = 10;

        int vOffset = 0, hOffset = 0;
        if (cbtn->isDown() && !cbtn->isChecked())
        {
          vOffset = pixelMetric(QStyle::PM_ButtonShiftVertical);
          hOffset = pixelMetric(QStyle::PM_ButtonShiftHorizontal);
        }

        bool isInactive(isWidgetInactive(cbtn));
        const label_spec lspec = getLabelSpec("PanelButtonCommand");

        /* find the state and set the text color accordingly */
        int state;
        QPalette pPalette = option.palette;
        QColor col, disabledCol;
        if (!cbtn->isEnabled())
        {
          state = 0;
          if (cbtn->isChecked())
          {
            if (isInactive)
              disabledCol = getFromRGBA(lspec.toggleInactiveColor);
            if (!disabledCol.isValid())
              disabledCol = getFromRGBA(lspec.toggleColor);
            if (disabledCol.isValid())
            {
              disabledCol.setAlpha(102); // 0.4 * disabledCol.alpha()
              pPalette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledCol);
            }
          }
        }
        else if (cbtn->isChecked())
        {
          state = 4;
          if (isInactive)
            col = getFromRGBA(lspec.toggleInactiveColor);
          if (!col.isValid())
            col = getFromRGBA(lspec.toggleColor);
        }
        else if (cbtn->isDown())
        {
          state = 3;
          if (isInactive)
            col = getFromRGBA(lspec.pressInactiveColor);
          if (!col.isValid())
            col = getFromRGBA(lspec.pressColor);
        }
        else if (cbtn->underMouse())
        {
          state = 2;
          if (isInactive)
            col = getFromRGBA(lspec.focusInactiveColor);
          if (!col.isValid())
            col = getFromRGBA(lspec.focusColor);
        }
        else
        {
          state = 1;
          if (isInactive)
            col = getFromRGBA(lspec.normalInactiveColor);
          if (!col.isValid())
            col = getFromRGBA(lspec.normalColor);
        }
        if (col.isValid())
          pPalette.setColor(QPalette::ButtonText, col);

        /* icon */
        if (!cbtn->icon().isNull())
          p.drawPixmap(leftMargin + hOffset, topMargin + vOffset,
                       getPixmapFromIcon(cbtn->icon(),
                                         getIconMode(disabledCol.isValid()
                                                     && !enoughContrast(disabledCol,
                                                                        standardPalette().color(QPalette::Window))
                                                       ? -1 : state,
                                                     isInactive, lspec),
                                         cbtn->isChecked() ? QIcon::On : QIcon::Off,
                                         cbtn->iconSize()));

        int textflags = Qt::TextShowMnemonic;
        if (!styleHint(QStyle::SH_UnderlineShortcut, &option, cbtn))
          textflags |= Qt::TextHideMnemonic;
        textflags |= Qt::AlignTop; // (see Kvantum.cpp -> Style::drawItemText)
        if (cbtn->layoutDirection() == Qt::RightToLeft)
          textflags |= Qt::AlignRight;
        else
          textflags |= Qt::AlignLeft;

        /* This is what QCommandLinkButtonPrivate::titleFont() does.
           It's bad because the font shouldn't be fixed. */
        QFont titleFont = cbtn->font();
        titleFont.setBold(true);
        titleFont.setPointSizeF(9.0);

        /* title */
        p.setFont(titleFont);
        int textOffset = cbtn->icon().actualSize(cbtn->iconSize()).width() + leftMargin + 6;
        QRect titleRect = cbtn->rect().adjusted(textOffset, topMargin, -rightMargin, 0);
        if (cbtn->description().isEmpty())
        {
          QFontMetrics fm(titleFont);
          titleRect.setTop(titleRect.top()
                           + qMax(0, (cbtn->icon().actualSize(cbtn->iconSize()).height()
                                      - fm.height()) / 2));
        }
        p.drawItemText(titleRect.translated(hOffset, vOffset),
                       textflags, pPalette, cbtn->isEnabled(), cbtn->text(), QPalette::ButtonText);

        /* description */
        textflags |= Qt::TextWordWrap | Qt::ElideRight;
        QFont descriptionFont = cbtn->font();
        descriptionFont.setPointSizeF(9.0); // -> QCommandLinkButtonPrivate::descriptionFont()
        p.setFont(descriptionFont);
        QFontMetrics fm(titleFont);
        int descriptionOffset = topMargin + fm.height();
        QRect descriptionRect = cbtn->rect().adjusted(textOffset, descriptionOffset,
                                                      -rightMargin, -bottomMargin);
        p.drawItemText(descriptionRect.translated(hOffset, vOffset), textflags,
                       pPalette, cbtn->isEnabled(), cbtn->description(), QPalette::ButtonText);
        p.restore();
        return true; // don't let QCommandLinkButton::paintEvent() be called
      }
    }
    break;

  case QEvent::HoverMove:
    if (QTabBar *tabbar = qobject_cast<QTabBar*>(o))
    { // see QEvent::HoverEnter below
      QHoverEvent *he = static_cast<QHoverEvent*>(e);
      int indx = tabbar->tabAt(he->position().toPoint());
      if (indx > -1)
      {
        int diff = qAbs(indx - tabbar->currentIndex());
        if (tabHoverRect_.isNull()
            && diff == 1)
        {
          /* the cursor has moved to a tab adjacent to the active tab */
          QRect r = tabbar->tabRect(indx);
          const frame_spec fspec = getFrameSpec(KSL("Tab"));
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
      int indx = tabbar->tabAt(he->position().toPoint());
      if (indx > -1 && qAbs(indx - tabbar->currentIndex()) == 1)
      {
        QRect r = tabbar->tabRect(indx);
        const frame_spec fspec = getFrameSpec(KSL("Tab"));
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
             /* WARNING: Toolbars and translucent windows have hover-enter event! */
             && !w->isWindow() && !qobject_cast<QToolBar*>(o)
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
        /* the popup may have been closed (with Qt>=5) */
        else if (!(animatedWidget_ == w && animationStartState_.startsWith(KL1("c-toggled"))))
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
        if (!(animatedWidget_ == w
              && (animationStartState_.startsWith(KL1("c-toggled"))
                  || animationStartState_.startsWith(KL1("normal")))))
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
        if ((isAnimatedScrollArea(w)
             // no animation without the top focused generic frame
             && elementExists(getFrameSpec(KSL("GenericFrame")).element+"-focused-top"))
            || (qobject_cast<QLineEdit*>(o)
                // this is needed for Qt>=5 -- Qt4 combo lineedits did not have FocusIn event
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
            if (auto sa = qobject_cast<QAbstractScrollArea*>(w))
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
      if ((isAnimatedScrollArea(w)
           // no animation without the top focused generic frame
           && elementExists(getFrameSpec(KSL("GenericFrame")).element+"-focused-top"))
          || qobject_cast<QComboBox*>(o)
          || qobject_cast<QLineEdit*>(o)
          || qobject_cast<QAbstractSpinBox*>(o))
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
    break;

  case QEvent::WindowActivate:
    if (!tspec_.no_inactiveness && qobject_cast<QAbstractItemView*>(o))
    {
      QPalette palette = w->palette();
      if (palette.color(QPalette::Active, QPalette::Text)
          != standardPalette().color(QPalette::Active, QPalette::Text))
      { // Custom text color; don't set palettes! The app is responsible for all colors.
        break;
      }
      bool _forcePalette(false);
      const label_spec lspec = getLabelSpec(KSL("ItemView"));
      /* set the normal inactive text color to the normal active one
         (needed when the app sets it inactive) */
      QColor col = getFromRGBA(lspec.normalColor);
      if (!col.isValid())
        col = standardPalette().color(QPalette::Active,QPalette::Text);
      if (palette.color(QPalette::Inactive, QPalette::Text) != col)
      {
        _forcePalette = true;
        palette.setColor(QPalette::Inactive, QPalette::Text, col);
      }
      if (!hasInactiveSelItemCol_)
      {
        if (_forcePalette)
          forcePalette(w, palette);
        break;
      }
      /* set the toggled inactive text color to the toggled active one
         (the main purpose of installing an event filter on the view) */
      col = getFromRGBA(lspec.toggleColor);
      if (palette.color(QPalette::Inactive, QPalette::HighlightedText) != col)
      {
        _forcePalette = true;
        palette.setColor(QPalette::Inactive, QPalette::HighlightedText, col);
      }
      /* use the active highlight color for the toggled (unfocused) item if there's
         no contrast with the pressed state because some apps (like Qt Designer)
         may not call PE_PanelItemViewItem but highlight the item instead */
      if (!toggledItemHasContrast_)
      {
        col = standardPalette().color(QPalette::Active,QPalette::Highlight);
        if (palette.color(QPalette::Inactive, QPalette::Highlight) != col)
        {
          _forcePalette = true;
          palette.setColor(QPalette::Inactive, QPalette::Highlight, col);
        }
      }
      if (_forcePalette)
        forcePalette(w, palette);
    }
    break;

  case QEvent::WindowDeactivate:
    if (!tspec_.no_inactiveness && qobject_cast<QAbstractItemView*>(o))
    {
      QPalette palette = w->palette();
      if (palette.color(QPalette::Active, QPalette::Text)
          != standardPalette().color(QPalette::Active, QPalette::Text))
      {
        break;
      }
      bool _forcePalette(false);
      const label_spec lspec = getLabelSpec(KSL("ItemView"));
      /* restore the normal inactive text color (which was changed at QEvent::WindowActivate) */
      QColor col = getFromRGBA(lspec.normalInactiveColor);
      if (!col.isValid())
        col = standardPalette().color(QPalette::Inactive,QPalette::Text);
      if (palette.color(QPalette::Inactive, QPalette::Text) != col)
      {
        _forcePalette = true;
        palette.setColor(QPalette::Inactive, QPalette::Text, col);
      }
      if (!hasInactiveSelItemCol_)
      { // custom text color
        if (_forcePalette)
          forcePalette(w, palette);
        break;
      }
      /* restore the toggled inactive text color (which was changed at QEvent::WindowActivate) */
      col = getFromRGBA(lspec.toggleInactiveColor);
      if (palette.color(QPalette::Inactive,QPalette::HighlightedText) != col)
      {
        _forcePalette = true;
        palette.setColor(QPalette::Inactive,QPalette::HighlightedText, col);
      }
      /* restore the inactive highlight color (which was changed at QEvent::WindowActivate) */
      if (!toggledItemHasContrast_)
      {
        col = standardPalette().color(QPalette::Inactive,QPalette::Highlight);
        if (palette.color(QPalette::Inactive, QPalette::Highlight) != col)
        {
          _forcePalette = true;
          palette.setColor(QPalette::Inactive, QPalette::Highlight, col);
        }
      }
      if (_forcePalette)
        forcePalette(w, palette);
    }
    break;

  case QEvent::Show:
    if (w)
    {
      if (animatedWidget_ && (w->windowType() == Qt::Popup
                              || w->windowType() == Qt::ToolTip))
      {
        popupOrigins_.insert(w, animatedWidget_);
        connect(w, &QObject::destroyed, this, &Style::forgetPopupOrigin);
      }

      if (QMenu *menu = qobject_cast<QMenu*>(o))
      {
        //if (isLibreoffice_) break;
        if (w->testAttribute(Qt::WA_X11NetWmWindowTypeMenu)) break; // detached menu
        if (w->testAttribute(Qt::WA_StyleSheetTarget)) break; // not drawn by Kvantum (see PM_MenuHMargin)

        /*
          This is a workaround for a fixed Qt5 bug (QTBUG-47043) that has reappeared
          in Qt6 in another form but with almost the same result: _NET_WM_WINDOW_TYPE
          is set to _NET_WM_WINDOW_TYPE_NORMAL for ordinary menus and combo popup menus.
          "QXcbWindowFunctions::setWmWindowType()" doesn't exist in Qt6 but the window
          type can be set by removing and resetting menu/combo attributes.
        */
        /*if (tspec_.isX11 && !e->spontaneous() && w->windowHandle() != nullptr)
        {
          if (w->testAttribute(Qt::WA_X11NetWmWindowTypeDropDownMenu))
          {
            w->setAttribute(Qt::WA_X11NetWmWindowTypeDropDownMenu, false);
            w->setAttribute(Qt::WA_X11NetWmWindowTypeDropDownMenu, true);
          }
          else if (w->testAttribute(Qt::WA_X11NetWmWindowTypePopupMenu))
          {
            w->setAttribute(Qt::WA_X11NetWmWindowTypePopupMenu, false);
            w->setAttribute(Qt::WA_X11NetWmWindowTypePopupMenu, true);
          }
        }*/

        if (movedMenus_.contains(w)) break; // already moved
        /* "magical" condition for a submenu */
        QPoint parentMenuCorner;
        QMenu *parentMenu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        /* this is a workaround for KDE global menu */
        if (parentMenu && parentMenu->activeAction() == nullptr) parentMenu = nullptr;
        if (parentMenu == nullptr)
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
        if (parentMenu == nullptr)
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
        /* get the available geometry (Qt menus don't
           spread across the available virtual geometry) */
        QRect ag;
        //QRect sg;
        QScreen *sc = nullptr;
        if (QWindow *win = w->windowHandle())
        {
          sc = win->screen();
          if (sc)
          {
            //sg = sc->geometry();
            ag = sc->availableGeometry();
          }
        }
        qreal dX = 0, dY = 0;
        /* this gives the real position AFTER pending movements
           because it's QWidgetData::crect (Qt -> qwidget.h) */
        const QRect g(w->geometry());

        /* WARNING: If compositing is stopped here, we aren't responsible.
                    A check for the state of compositing at this very moment
                    may be CPU-intensive. */
        if (!noComposite_
            && menuShadow_.count() == 4)
        {
          /* compensate for the offset created by the shadow */

          dY -= menuShadow_.at(1); // top shadow

          QPushButton *pBtn = nullptr;
          QToolButton *tBtn = nullptr;
          if (w->layoutDirection() == Qt::RightToLeft)
          { // see explanations for ltr below
            dX += menuShadow_.at(2);
            if (parentMenu)
            {
              if (parentMenuCorner.x() < g.left())
                dX -= menuShadow_.at(2) + menuShadow_.at(0);
              else
              {
                dX += menuShadow_.at(0)
                      - static_cast<qreal>(getMenuMargin(true)); // workaround for an old Qt bug
              }
            }
            else if (parentMenubar)
            {
              QString group = tspec_.merge_menubar_with_toolbar ? "Toolbar" : "MenuBar";
              if (parentMenubar->mapToGlobal(QPoint(0,0)).y() > g.bottom())
                dY +=  menuShadow_.at(1) + menuShadow_.at(3) + static_cast<qreal>(getFrameSpec(group).top);
              else
                dY -= static_cast<qreal>(getFrameSpec(group).bottom);

              QRect activeG = parentMenubar->actionGeometry(parentMenubar->activeAction());
              QPoint activeTopLeft = parentMenubar->mapToGlobal(activeG.topLeft());
              if (g.right() + 1 > activeTopLeft.x() + activeG.width())
              { // Qt positions the menu wrongly in this case but we don't add a workaround
                dX -= menuShadow_.at(2);
                qreal delta = menuShadow_.at(2)
                              - static_cast<qreal>(g.right() + 1 - (activeTopLeft.x() + activeG.width()));
                if (delta > 0)
                  dX += delta;
                else
                  dX -= qMin(menuShadow_.at(0), -delta);
              }
            }
            else if (!sunkenButton_.isNull()
                     && (!(pBtn = qobject_cast<QPushButton*>(sunkenButton_.data()))
                         || pBtn->menu() == w)
                     && (!(tBtn = qobject_cast<QToolButton*>(sunkenButton_.data()))
                         || tBtn->menu() == w
                         || (tBtn->defaultAction() && tBtn->defaultAction()->menu() == w)))
            {
              QPoint wTopLeft = sunkenButton_.data()->mapToGlobal(QPoint(0,0));
              if (wTopLeft.y() >= g.bottom())
                dY +=  menuShadow_.at(1) + menuShadow_.at(3);
              if (g.right() + 1 > wTopLeft.x() + sunkenButton_.data()->width())
              {
                dX -= menuShadow_.at(2);
                qreal delta = menuShadow_.at(2)
                              - static_cast<qreal>(g.right() + 1 - (wTopLeft.x() + sunkenButton_.data()->width()));
                if (delta > 0)
                  dX += delta;
                else
                  dX -= qMin(menuShadow_.at(0), -delta);
              }
            }
            else if (!ag.isEmpty())
            {
              if (tspec_.isX11)
              {
                if (g.top() != ag.top()
                    && (g.bottom() == ag.bottom()
                        || QCursor::pos(sc).y() > g.bottom()))
                {
                  dY += menuShadow_.at(1) + menuShadow_.at(3);
                }
                if (g.right() != ag.right()
                    && (g.left() == ag.left()
                        || QCursor::pos(sc).x() <= g.left()))
                {
                  dX -= menuShadow_.at(2) + menuShadow_.at(0);
                }
              }
              else
              {
                QRect pg;
                if (QWidget *p = w->parentWidget())
                  pg = p->window()->geometry();
                if ((!pg.isNull() && g.bottom() + 1 == pg.top())
                    || QCursor::pos(sc).y() > g.bottom())
                {
                  dY += menuShadow_.at(1) + menuShadow_.at(3);
                }
                if ((!pg.isNull() && g.left() == pg.right() + 1)
                    || QCursor::pos(sc).x() <= g.left())
                {
                  dX -= menuShadow_.at(2) + menuShadow_.at(0);
                }
              }
            }
          }
          else // ltr
          {
            dX -= menuShadow_.at(0); // left shadow
            if (parentMenu)
            {
              if (parentMenuCorner.x() > g.left())
              { // there wasn't enough space to the right of the parent
                dX += menuShadow_.at(0) + menuShadow_.at(2);
              }
              else
                dX -= menuShadow_.at(2); // right shadow of the left menu
            }
            else if (parentMenubar)
            {
              QString group = tspec_.merge_menubar_with_toolbar ? "Toolbar" : "MenuBar";
              if (parentMenubar->mapToGlobal(QPoint(0,0)).y() > g.bottom()) // menu is above menubar
                dY +=  menuShadow_.at(1) + menuShadow_.at(3) + static_cast<qreal>(getFrameSpec(group).top);
              else
                dY -= static_cast<qreal>(getFrameSpec(group).bottom);

              QPoint activeTopLeft = parentMenubar->mapToGlobal(parentMenubar->actionGeometry(
                                                                 parentMenubar->activeAction())
                                                               .topLeft());
              if (activeTopLeft.x() > g.left()) // because of the right screen border
              {
                dX += menuShadow_.at(0);
                qreal delta = menuShadow_.at(0) - static_cast<qreal>(activeTopLeft.x() - g.left());
                if (delta > 0)
                  dX -= delta;
                else
                  dX += qMin(menuShadow_.at(2), -delta);
              }
            }
            else if (!sunkenButton_.isNull()
                     && (!(pBtn = qobject_cast<QPushButton*>(sunkenButton_.data()))
                         || pBtn->menu() == w)
                     && (!(tBtn = qobject_cast<QToolButton*>(sunkenButton_.data()))
                         || tBtn->menu() == w
                         || (tBtn->defaultAction() && tBtn->defaultAction()->menu() == w)))
            { // the menu is triggered by a push or tool button
              QPoint wTopLeft = sunkenButton_.data()->mapToGlobal(QPoint(0,0));
              if (wTopLeft.y() >= g.bottom()) // above the button (strange! Qt doesn't add 1px)
                dY +=  menuShadow_.at(1) + menuShadow_.at(3);
              if (wTopLeft.x() > g.left()) // because of the right screen border
              {
                dX += menuShadow_.at(0);
                qreal delta = menuShadow_.at(0) - static_cast<qreal>(wTopLeft.x() - g.left());
                if (delta > 0)
                  dX -= delta;
                else
                  dX += qMin(menuShadow_.at(2), -delta);
              }
            }
            else if (!ag.isEmpty()) // probably a panel menu
            {
              if (tspec_.isX11)
              {
                /* snap to the screen bottom/right if possible and,
                   as the last resort, consider the cursor position */
                if (g.top() != ag.top()
                    && (g.bottom() == ag.bottom()
                        || QCursor::pos(sc).y() > g.bottom()))
                {
                  dY += menuShadow_.at(1) + menuShadow_.at(3);
                }
                if (g.left() != ag.left()
                    && (g.right() == ag.right()
                        || QCursor::pos(sc).x() > g.right()))
                {
                  dX += menuShadow_.at(0) + menuShadow_.at(2);
                }
              }
              else // the global position is unknown under Wayland
              {
                /* snap to the parent window's top/left side if needed and,
                   as the last resort, consider the cursor position */
                QRect pg;
                if (QWidget *p = w->parentWidget())
                  pg = p->window()->geometry();
                if ((!pg.isNull() && g.bottom() + 1 == pg.top())
                    || QCursor::pos(sc).y() > g.bottom())
                {
                  dY += menuShadow_.at(1) + menuShadow_.at(3);
                }
                if ((!pg.isNull() && g.right() + 1 == pg.left())
                    || QCursor::pos(sc).x() > g.right())
                {
                  dX += menuShadow_.at(0) + menuShadow_.at(2);
                }
              }
            }
          }
        }
        else if (!parentMenu && parentMenubar)
        { // triggered by a menubar, without compositing or shadow
          QString group = tspec_.merge_menubar_with_toolbar ? "Toolbar" : "MenuBar";
          if (parentMenubar->mapToGlobal(QPoint(0,0)).y() > g.bottom()) // menu is above menubar
            dY += static_cast<qreal>(getFrameSpec(group).top);
          else
            dY -= static_cast<qreal>(getFrameSpec(group).bottom);
        }

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
                dY += static_cast<qreal>(topActionRect.top());
              }
            }
          }
        }

        int DX = qRound(dX);
        int DY = qRound(dY);
        if (DX == 0 && DY == 0) break;

        /* This workaround isn't needed anymore. It didn't work with Wayland either. */
        // prevent the menu from switching to another screen
        /*if (!sg.isEmpty() && QApplication::screens().size() > 1)
        {
          if (g.top() + DY < sg.top() && g.top() >= sg.top())
            DY = sg.top() - g.top();
          if (w->layoutDirection() == Qt::RightToLeft)
          {
            if (g.right() + DX > sg.right() && g.right() <= sg.right())
              DX = sg.right() - g.right();
          }
          else if (g.left() + DX < sg.left() && g.left() >= sg.left())
            DX = sg.left() - g.left();
        }*/

        w->move(g.left() + DX, g.top() + DY);
        /* WARNING: Because of a bug in Qt 6.6, translucent menus -- especially context menus
                    -- may be drawn with their minimum sizes and without contents after being
                    moved on a non-primary screen. As a workaround, the menu is resized here. */
        w->resize(g.size());
        movedMenus_.insert(w);
        connect(w, &QObject::destroyed, this, &Style::forgetMovedMenu);
      }
      else if (tspec_.group_toolbar_buttons && qobject_cast<QToolButton*>(o))
      {
        if (QToolBar *toolBar = qobject_cast<QToolBar*>(w->parentWidget()))
        {
          if (toolBar->orientation() != Qt::Vertical)
            toolBar->update();
        }
      }
      else if (qobject_cast<QAbstractItemView*>(o))
      {
        if (tspec_.no_inactiveness) break;
        /* view palettes should also be set when the view is shown
           and not only when its window is activated/deactivated
           (-> QEvent::WindowActivate and QEvent::WindowDeactivate) */
        QPalette palette = w->palette();
        if (palette.color(QPalette::Active, QPalette::Text)
            != standardPalette().color(QPalette::Active, QPalette::Text))
        {
          break;
        }
        bool _forcePalette(false);
        const label_spec lspec = getLabelSpec(KSL("ItemView"));
        if (isWidgetInactive(w)) // FIXME: probably not needed with inactive window
        {
          QColor col = getFromRGBA(lspec.normalInactiveColor);
          if (!col.isValid())
            col = standardPalette().color(QPalette::Inactive,QPalette::Text);
          if (palette.color(QPalette::Inactive, QPalette::Text) != col)
          {
            _forcePalette = true;
            palette.setColor(QPalette::Inactive, QPalette::Text, col);
          }
          if (!hasInactiveSelItemCol_)
          {
            if (_forcePalette)
              forcePalette(w, palette);
            break;
          }
          col = getFromRGBA(lspec.toggleInactiveColor);
          if (palette.color(QPalette::Inactive,QPalette::HighlightedText) != col)
          {
            _forcePalette = true;
            palette.setColor(QPalette::Inactive,QPalette::HighlightedText, col);
          }
          if (!toggledItemHasContrast_)
          {
            col = standardPalette().color(QPalette::Inactive,QPalette::Highlight);
            if (palette.color(QPalette::Inactive, QPalette::Highlight) != col)
            {
              _forcePalette = true;
              palette.setColor(QPalette::Inactive, QPalette::Highlight, col);
            }
          }
        }
        else
        {
          QColor col = getFromRGBA(lspec.normalColor);
          if (!col.isValid())
            col = standardPalette().color(QPalette::Active,QPalette::Text);
          if (palette.color(QPalette::Inactive, QPalette::Text) != col)
          {
            _forcePalette = true;
            palette.setColor(QPalette::Inactive, QPalette::Text, col);
          }
          if (!hasInactiveSelItemCol_)
          {
            if (_forcePalette)
              forcePalette(w, palette);
            break;
          }
          col = getFromRGBA(lspec.toggleColor);
          if (palette.color(QPalette::Inactive, QPalette::HighlightedText) != col)
          {
            _forcePalette = true;
            palette.setColor(QPalette::Inactive, QPalette::HighlightedText, col);
          }
          if (!toggledItemHasContrast_)
          {
            col = standardPalette().color(QPalette::Active,QPalette::Highlight);
            if (palette.color(QPalette::Inactive, QPalette::Highlight) != col)
            {
              _forcePalette = true;
              palette.setColor(QPalette::Inactive, QPalette::Highlight, col);
            }
          }
        }
        if (_forcePalette)
          forcePalette(w, palette);
      }
      /* see the case of QMenu above */
      /*else if (w->inherits("QComboBoxPrivateContainer"))
      {
        if (tspec_.combo_menu && tspec_.isX11
            && !e->spontaneous() && w->windowHandle() != nullptr
            && w->testAttribute(Qt::WA_X11NetWmWindowTypeCombo))
        {
          w->setAttribute(Qt::WA_X11NetWmWindowTypeCombo, false);
          w->setAttribute(Qt::WA_X11NetWmWindowTypeCombo, true);
        }
      }*/
    }
    break;

  /* WARNING: For some reason (e.g., the exitence of an app stylesheet),
     the size hint of the spinbox may be wrong. Here we force a minimum
     size by using CT_SpinBox when the maximum size isn't set by the app
     or isn't smaller than our size. */
  case QEvent::ShowToParent:
    if (w
        /* not if it's just a QAbstractSpinBox, hoping that
           no one sets the minimum size in normal cases */
        && (qobject_cast<QSpinBox*>(o)
            || qobject_cast<QDoubleSpinBox*>(o)
            || qobject_cast<QDateTimeEdit*>(o)))
    {
      QSize size = sizeFromContents(CT_SpinBox,nullptr,QSize(),w).expandedTo(w->minimumSize());
      if (w->maximumWidth() > size.width())
        w->setMinimumWidth(size.width());
      /*if (w->maximumHeight() > size.height())
        w->setMinimumHeight(size.height());*/
    }
    /* correct line-edit palettes on stylable toolbars if needed */
    else if (qobject_cast<QLineEdit*>(o)
             && (!getFrameSpec(KSL("ToolbarLineEdit")).element.isEmpty()
                 || !getInteriorSpec(KSL("ToolbarLineEdit")).element.isEmpty())
             && getStylableToolbarContainer(w, true))
    {
      const label_spec tlspec = getLabelSpec("Toolbar");
      QColor col = getFromRGBA(tlspec.normalColor);
      if (enoughContrast(col, standardPalette().color(QPalette::Active,QPalette::Text)))
      {
        QColor col1 = col;
        QPalette palette = w->palette();
        palette.setColor(QPalette::Active, QPalette::Text, col1);
        if (!tspec_.no_inactiveness)
        {
          col1 = getFromRGBA(tlspec.normalInactiveColor);
          if (!col1.isValid()) col1 = col;
        }
        palette.setColor(QPalette::Inactive, QPalette::Text, col1);
        col1 = col; // placeholder
        col1.setAlpha(128);
        palette.setColor(QPalette::PlaceholderText, col1);
        col.setAlpha(102); // 0.4 * col.alpha()
        palette.setColor(QPalette::Disabled, QPalette::Text,col);
        forcePalette(w, palette);
        /* also correct the color of the symbolic clear icon (-> CE_ToolBar) */
        if (QAction *clearAction = w->findChild<QAction*>(KL1("_q_qlineeditclearaction")))
          clearAction->setIcon(standardIcon(QStyle::SP_LineEditClearButton, nullptr, w));
      }
    }
    break;

  /*case QEvent::PaletteChange :
    if (!isPcmanfm_ && qobject_cast<QLineEdit*>(o) && (tspec_.combo_as_lineedit || tspec_.square_combo_button))
    {
      if (QComboBox *cb = qobject_cast<QComboBox*>(w->parentWidget()))
      {
        if (cb->isVisible())
          cb->update(); // a workaround for bad codes that change line-edit base color
      }
    }
    break;*/

  case QEvent::Hide:
    if (w)
    {
      if (qobject_cast<QToolButton*>(o))
      {
        if (tspec_.group_toolbar_buttons)
        {
          if (QToolBar *toolBar = qobject_cast<QToolBar*>(w->parentWidget()))
          {
            if (toolBar->orientation() != Qt::Vertical)
              toolBar->update();
          }
        }
        //break; // toolbuttons may be animated (see below)
      }
      else if (w->isEnabled() && tspec_.animate_states)
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
          forgetPopupOrigin(o);
        }
        /* let the state animation continue (not necessary but useful
           for better flash prevention -- see FocusIn and FocusOut) */
        else if ((animatedWidget_ == w && opacityTimer_->isActive())
                 || (animatedWidgetOut_ == w && opacityTimerOut_->isActive()))
        {
          break;
        }
      }

      /* remove the widget from some lists when it becomes hidden */
      if (qobject_cast<QMenu*>(w))
        forgetMovedMenu(o);
      if (tspec_.animate_states
          && (w->windowType() == Qt::Popup
              || w->windowType() == Qt::ToolTip))
      {
        forgetPopupOrigin(o);
        break; // popups aren't animated (see below)
      }
    }
    /* Falls through. */

  case QEvent::Destroy: // not necessary
    if (w && tspec_.animate_states)
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
    break;

  default:
    return false;
  }

  return false;
}

}
