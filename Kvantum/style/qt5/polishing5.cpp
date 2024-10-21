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

#include "Kvantum5.h"

#include <QProcess>
#include <QSvgRenderer>
#include <QApplication>
#include <QMenu>
#include <QPushButton>
#include <QCommandLinkButton>
#include <QCheckBox>
#include <QRadioButton>
#include <QToolBox>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QProgressBar>
#include <QGroupBox>
#include <QMdiSubWindow>
#include <QDockWidget>
#include <QStatusBar>
#include <QMainWindow>
#include <QToolBar>
#include <QScrollBar>
#include <QWindow>
#include <QDialog>
#include <QLayout> // only for forceSizeGrip
#include <QCompleter> // only for combo menu change in Kvantum Manager
#include <QScroller>

namespace Kvantum
{

/* WARNING: For easily distinguishing animated widgets (-> "eventFiltering.cpp")
            from others that also have event filter, event filters should be
            installed with care. */
void Style::polish(QWidget *widget)
{
  if (!widget) return;

  /* Reset the palette if the widget doesn't have a separate palette.
     This will be useful if the style changes to Kvantum in runtime. */
  if (!widget->testAttribute(Qt::WA_SetPalette))
    widget->setPalette(QPalette());

  QWidget *pw = widget->parentWidget();

  /*if (!pw || !pw->inherits("QWebEngineView")) // FIXME: a bug in QtWebEngine?
    widget->setAttribute(Qt::WA_Hover, true);*/
  /*
     It's better to add the hover effect selectively and only to
     the following widgets (some of which are dealt with later):

       QAbstractButton (all kinds of buttons)
       QAbstractSlider
       Direct children of QAbstractItemView (view items)
       QSplitterHandle
       QHeaderView
       QSizeGrip
       QDockWidget
       QComboBox
       QTabBar
       QAbstractSpinBox
       QScrollBar

     We don't add the hover efffect to QLineEdit, QAbstractScrollArea
     and non-checkable QGroupBox.
  */
  if (qobject_cast<QAbstractButton*>(widget)
      || qobject_cast<QAbstractSlider*>(widget)
      || qobject_cast<QAbstractItemView*>(pw)
      || widget->inherits("QSplitterHandle")
      || widget->inherits("QHeaderView")
      || widget->inherits("QSizeGrip"))
  {
    widget->setAttribute(Qt::WA_Hover, true);
  }

  //widget->setAttribute(Qt::WA_MouseTracking, true);

  bool respectDarkness(hspec_.respect_darkness);

  /* respect the toolbar text color if the widget is shown after
     its parent toolbar and without repainting it (unlike in CE_ToolBar) */
  const label_spec tLspec = getLabelSpec(QStringLiteral("Toolbar"));
  QColor tColor = getFromRGBA(tLspec.normalColor);
  if (enoughContrast(standardPalette().color(QPalette::Active,QPalette::Text), tColor)
      && !qobject_cast<QToolButton*>(widget) // flat toolbuttons are dealt with at CE_ToolButtonLabel
      && getStylableToolbarContainer(widget))
  {
    QColor inactiveCol;
    if (tspec_.no_inactiveness)
      inactiveCol = tColor;
    else
    {
      inactiveCol = getFromRGBA(tLspec.normalInactiveColor);
      if (!inactiveCol.isValid()) inactiveCol = tColor;
    }
    QColor disabledCol = tColor;
    disabledCol.setAlpha(102); // 0.4 * disabledCol.alpha()
    QColor ptColor = tColor; // placeholder
    ptColor.setAlpha(128);

    if (isStylableToolbar(pw)) // an immediate child of a stylable toolbar
    {
      /* as in Cantata (previously, also Krita and Amarok) */
      QPalette palette = widget->palette();
      palette.setColor(QPalette::Active, QPalette::ButtonText, tColor);
      palette.setColor(QPalette::Inactive, QPalette::ButtonText, inactiveCol);
      palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledCol);
      palette.setColor(QPalette::Active, QPalette::WindowText, tColor);
      palette.setColor(QPalette::Inactive, QPalette::WindowText, inactiveCol);
      palette.setColor(QPalette::Disabled, QPalette::WindowText, disabledCol);
      if (qobject_cast<QLabel*>(widget))
      {
        palette.setColor(QPalette::Active, QPalette::Text, tColor);
        palette.setColor(QPalette::Inactive, QPalette::Text, inactiveCol);
        palette.setColor(QPalette::Disabled, QPalette::Text, disabledCol);
        palette.setColor(QPalette::PlaceholderText, ptColor);
        respectDarkness = false; // we don't want to change its text color again
      }
      forcePalette(widget, palette);
    }

    if (qobject_cast<QLineEdit*>(widget))
    {
      if (!getFrameSpec(QStringLiteral("ToolbarLineEdit")).element.isEmpty()
          || !getInteriorSpec(QStringLiteral("ToolbarLineEdit")).element.isEmpty())
      {
        QPalette palette = widget->palette();
        if (palette.color(QPalette::Active, QPalette::Text) != tColor)
        {
          palette.setColor(QPalette::Active, QPalette::Text, tColor);
          palette.setColor(QPalette::Inactive, QPalette::Text, inactiveCol);
          palette.setColor(QPalette::Disabled, QPalette::Text, disabledCol);
          palette.setColor(QPalette::PlaceholderText, ptColor);
          forcePalette(widget, palette);
          /* also correct the color of the symbolic clear icon */
          if (QAction *clearAction = widget->findChild<QAction*>(QLatin1String("_q_qlineeditclearaction")))
            clearAction->setIcon(standardIcon(QStyle::SP_LineEditClearButton, nullptr, widget));
        }
      }
    }
    else if (qobject_cast<QComboBox*>(widget)
            && (!getFrameSpec(QStringLiteral("ToolbarComboBox")).element.isEmpty()
                || !getInteriorSpec(QStringLiteral("ToolbarComboBox")).element.isEmpty()))
    {
      tColor = getFromRGBA(getLabelSpec(QStringLiteral("ToolbarComboBox")).normalColor);
      if (tColor.isValid())
      {
        QColor disabledCol = tColor;
        disabledCol.setAlpha(102); // 0.4 * disabledCol.alpha()
        QPalette palette = widget->palette();
        if (tColor != palette.color(QPalette::ButtonText))
        {
          palette.setColor(QPalette::ButtonText, tColor);
          palette.setColor(QPalette::Disabled,QPalette::ButtonText, disabledCol);
          forcePalette(widget, palette);
        }
      }
    }
  }
  else if (qobject_cast<QLabel*>(widget))
  {
    if (qobject_cast<QToolBar*>(pw))
    { // on toolbars, labels get the button text color; that's corrected here
      QPalette palette = widget->palette();
      palette.setColor(QPalette::Active, QPalette::ButtonText,
                       standardPalette().color(QPalette::Active,QPalette::WindowText));
      palette.setColor(QPalette::Inactive, QPalette::ButtonText,
                       standardPalette().color(QPalette::Inactive,QPalette::WindowText));
      palette.setColor(QPalette::Disabled, QPalette::ButtonText,
                       standardPalette().color(QPalette::Disabled,QPalette::WindowText));
      forcePalette(widget, palette);
      respectDarkness = false;
    }
    else if (qobject_cast<QMenu*>(widget->window()))
    {
      /* The label may be shown after its parent menu is polished below, in
         "switch (widget->windowFlags()...)". Other widgets are ignored for now. */
      QColor menuTextColor = getFromRGBA(getLabelSpec(QStringLiteral("MenuItem")).normalColor);
      if (menuTextColor.isValid())
      {
        if (menuTextColor != standardPalette().color(QPalette::Active,QPalette::WindowText))
        {
          QPalette palette = widget->palette();
          palette.setColor(QPalette::Active,QPalette::WindowText,menuTextColor);
          palette.setColor(QPalette::Inactive,QPalette::WindowText,menuTextColor);
          forcePalette(widget, palette);
          respectDarkness = false;
        }
      }
    }
  }

  if (respectDarkness)
  {
    QColor winCol = standardPalette().color(QPalette::Active,QPalette::Window);
    if (qGray(winCol.rgb()) <= 100 // there should be darkness to be respected
        // it's usual to define custom colors in text edits
        && !widget->inherits("QTextEdit") && !widget->inherits("QPlainTextEdit"))
    {
      bool changePalette(false);
      if (qobject_cast<QAbstractScrollArea*>(widget))
      { // we don't want to give a solid backgeound to LXQt's desktop by accident
        QWidget *win = widget->window();
        if (!win->testAttribute(Qt::WA_X11NetWmWindowTypeDesktop)
            && win->windowType() != Qt::Desktop)
        {
          changePalette = true;
        }
      }
      else if (qobject_cast<QTabWidget*>(widget)
               || (qobject_cast<QLabel*>(widget) && !qobject_cast<QLabel*>(widget)->text().isEmpty()))
      {
        changePalette = true;
      }
      if (changePalette)
      {
        QPalette palette = widget->palette();
        QColor txtCol = palette.color(QPalette::Text);
        if (!enoughContrast(palette.color(QPalette::Base), txtCol)
            || !enoughContrast(palette.color(QPalette::Window), palette.color(QPalette::WindowText))
            || (qobject_cast<QAbstractItemView*>(widget)
                && !enoughContrast(overlayColor(palette.color(QPalette::Base),
                                                palette.color(QPalette::AlternateBase)),
                                   txtCol)))
        {
          polish(palette);
          forcePalette(widget, palette);
        }
      }
    }
  }

  switch (widget->windowFlags() & Qt::WindowType_Mask) {
    case Qt::Window:
    case Qt::Dialog:
    case Qt::Popup:
    case Qt::ToolTip: // a window, not a real tooltip
    case Qt::Sheet: { // WARNING: What the hell?! On Linux? Yes, a Qt5 bug!
      if (itsWindowManager_)
        itsWindowManager_->registerWidget(widget);
      QRegion wMask = widget->mask();
      if (!wMask.isEmpty() && wMask != QRegion(widget->rect()))
        break; // like Vokoscreen's QvkAnimateWindow
      /* popup, not menu (-> GoldenDict); also, although it may be
         hard to believe, a menu can have the Dialog flag (-> qlipper)
         and a window can have the ToolTip flag (-> LXQtGroupPopup) */
      if (qobject_cast<QMenu*>(widget))
      { // some apps (like QtAV Player) do weird things with menus
        QColor menuTextColor = getFromRGBA(getLabelSpec(QStringLiteral("MenuItem")).normalColor);
        if (menuTextColor.isValid())
        {
          QPalette palette = widget->palette();
          if (menuTextColor != palette.color(QPalette::WindowText))
          {
            palette.setColor(QPalette::Active,QPalette::WindowText,menuTextColor);
            palette.setColor(QPalette::Inactive,QPalette::WindowText,menuTextColor);
            QColor baseCol = palette.color(QPalette::Base);
            if (baseCol.alpha() < 255 && baseCol == standardPalette().color(QPalette::Base))
            {
              QColor winCol = standardPalette().color(QPalette::Window);
              winCol.setAlpha(255);
              baseCol = overlayColor(winCol,baseCol);
              palette.setColor(QPalette::Base,baseCol);
              QColor altBaseCol = standardPalette().color(QPalette::AlternateBase);
              if (altBaseCol.alpha() < 255)
              {
                altBaseCol = overlayColor(baseCol,altBaseCol);
                palette.setColor(QPalette::AlternateBase,altBaseCol);
              }
            }
            forcePalette(widget, palette);
          }
        }
        break;
      }
      else if (widget->inherits("QTipLabel")
               || qobject_cast<QLabel*>(widget) // a floating label, as in Filelight
               || widget->inherits("QComboBoxPrivateContainer") // at most, a menu
               /* like Vokoscreen's (old) QvkRegionChoise */
               || (widget->windowFlags().testFlag(Qt::WindowStaysOnTopHint)
                   && widget->testAttribute(Qt::WA_NoSystemBackground)
                   && ((widget->windowFlags() & Qt::WindowType_Mask) == Qt::ToolTip
                       || (widget->windowState() & Qt::WindowFullScreen))))
      {
        break;
      }
      widget->setAttribute(Qt::WA_StyledBackground);
      /* take all precautions */
      if (!isPlasma_ && !subApp_ /*&& !isLibreoffice_*/
          && widget->isWindow()
          && widget->windowType() != Qt::Desktop
          && !widget->testAttribute(Qt::WA_PaintOnScreen)
          && !widget->testAttribute(Qt::WA_X11NetWmWindowTypeDesktop)
          && !widget->inherits("KScreenSaver")
          && !widget->inherits("QSplashScreen"))
      {
        if (hspec_.forceSizeGrip
            && widget->minimumSize() != widget->maximumSize()
            && !widget->windowFlags().testFlag(Qt::Popup)
            && !widget->windowFlags().testFlag(Qt::CustomizeWindowHint))
        {
          /*if (QMainWindow* mw = qobject_cast<QMainWindow*>(widget))
          {
            QStatusBar *sb = mw->statusBar();
            sb->setSizeGripEnabled(true);
          }
          else*/ if (QDialog *d = qobject_cast<QDialog*>(widget))
          {
            /* QProgressDialog has a bug that gives a wrong position to
               its resize grip. It has no layout and there's no reason
               to force resize grip on any dialog without layout. */
            QLayout *lo = widget->layout();
            if (lo && lo->sizeConstraint() != QLayout::SetFixedSize
                && lo->sizeConstraint() != QLayout::SetNoConstraint)
            {
              d->setSizeGripEnabled(true);
            }
          }
        }
        /* translucency and blurring */
        if (!translucentWidgets_.contains(widget))
        {
          theme_spec tspec_now = settings_->getCompositeSpec();

          bool hasForcedTranslucency(false);
          bool makeTranslucent(false);
          if (!widget->windowFlags().testFlag(Qt::FramelessWindowHint)
              && !widget->windowFlags().testFlag(Qt::X11BypassWindowManagerHint))
          {
            if (forcedTranslucency_.contains(widget))
            {
              makeTranslucent = true;
              hasForcedTranslucency = true;
            }
            /* WARNING:
               Unlike most Qt5 windows, there are some opaque ones
               that are polished BEFORE being created (as in Octopi).
               Also the theme may change from Kvantum and to it again. */
            else if (!isOpaque_ && tspec_now.translucent_windows)
            {
              if (!widget->testAttribute(Qt::WA_TranslucentBackground)
                  || !widget->testAttribute(Qt::WA_NoSystemBackground))
              {
                makeTranslucent = true;
              }
              if (makeTranslucent)
              {
                /* a Qt5 window couldn't be made translucent if it's
                   already created without the alpha channel of its
                   surface format being set (as in Spectacle) */
                if (widget->testAttribute(Qt::WA_WState_Created))
                {
                  if (QWindow *window = widget->windowHandle())
                  {
                    if (window->format().alphaBufferSize() != 8)
                      makeTranslucent = false;
                  }
                }
              }
            }
          }
          /* the FramelessWindowHint or X11BypassWindowManagerHint flag
             may be set after the window is created but before it's polished
             (like in lxqt-leave) */
          else if (forcedTranslucency_.contains(widget))
          {
            forcedTranslucency_.remove(widget);
            widget->setAttribute(Qt::WA_NoSystemBackground, false);
            break;
          }
          if (makeTranslucent
              /* enable blurring for hard-coded translucency */
              || (tspec_now.composite
                  && (hspec_.blur_translucent
                      /* let unusual tooltips with hard-coded translucency
                         have shadow (like in LXQtGroupPopup or KDE system settings) */
                      || (widget->windowFlags() & Qt::WindowType_Mask) == Qt::ToolTip)
                  && widget->testAttribute(Qt::WA_TranslucentBackground)))
          {
            if (!widget->testAttribute(Qt::WA_TranslucentBackground)
                || (makeTranslucent && !widget->testAttribute(Qt::WA_NoSystemBackground)))
            {
              if (!widget->testAttribute(Qt::WA_TranslucentBackground))
                widget->setAttribute(Qt::WA_TranslucentBackground);
              else
                widget->setAttribute(Qt::WA_NoSystemBackground);
              forcedTranslucency_.insert(widget); // needed in unpolish()
            }

            /* enable blurring */
            if (!makeTranslucent || tspec_now.blurring)
            {
              if (blurHelper_ == nullptr)
              {
                if (tspec_now.menu_shadow_depth > 0)
                  getShadow(QStringLiteral("Menu"), getMenuMargin(true), getMenuMargin(false));
                if (tspec_now.tooltip_shadow_depth > 0)
                {
                  const frame_spec fspec = getFrameSpec(QStringLiteral("ToolTip"));
                  int thickness = qMax(qMax(fspec.top,fspec.bottom), qMax(fspec.left,fspec.right));
                  thickness += tspec_now.tooltip_shadow_depth;
                  getShadow(QStringLiteral("ToolTip"), thickness);
                }
                blurHelper_ = new BlurHelper(this, menuShadow_, tooltipShadow_,
                                             tspec_.menu_blur_radius, tspec_.tooltip_blur_radius,
                                             tspec_.contrast, tspec_.intensity, tspec_.saturation,
                                             hspec_.blur_only_active_window);
              }
              if (blurHelper_)
                blurHelper_->registerWidget(widget);
            }

            if (makeTranslucent)
              widget->installEventFilter(this);
            translucentWidgets_.insert(widget);
            if (!hasForcedTranslucency) // no duplicate connection (unimportant)
              connect(widget, &QObject::destroyed, this, &Style::noTranslucency);
          }
        }
      }

      if (gtkDesktop_) // under gtk DEs, set the titlebar according to dark_titlebar
        widget->installEventFilter(this);

      break;
    }
    default:
      break;
  }

  if (isDolphin_
      && qobject_cast<QAbstractScrollArea*>(getParent(widget,2))
      && !qobject_cast<QAbstractScrollArea*>(getParent(widget,3)))
  {
    /* Dolphin sets the background of its KItemListContainer's viewport
       to KColorScheme::View (-> kde-baseapps -> dolphinview.cpp).
       We force our base color here. */
    QPalette palette = widget->palette();
    palette.setColor(QPalette::Active, widget->backgroundRole(),
                     standardPalette().color(QPalette::Active,QPalette::Base));
    palette.setColor(QPalette::Inactive, widget->backgroundRole(),
                     standardPalette().color(QPalette::Inactive,QPalette::Base));
    forcePalette(widget, palette);
    /* hack Dolphin's view */
    if (hspec_.transparent_dolphin_view && widget->autoFillBackground())
      widget->setAutoFillBackground(false);
  }
  else if (isPcmanfm_ && (hspec_.transparent_pcmanfm_view || hspec_.transparent_pcmanfm_sidepane))
  {
    QWidget *gp = getParent(widget,2);
    if ((hspec_.transparent_pcmanfm_view
         && widget->autoFillBackground()
         && (gp && gp->inherits("Fm::FolderView") && !gp->inherits("PCManFM::DesktopWindow")
             && qobject_cast<QMainWindow*>(gp->window())))
        || (hspec_.transparent_pcmanfm_sidepane
            && ((pw && pw->inherits("Fm::DirTreeView"))
                || (gp && gp->inherits("Fm::SidePane")))))
    {
      widget->setAutoFillBackground(false);
    }
  }

  /* Text editors and some other widgets shouldn't have a translucent base color.
     (line-edits are dealt with separately and only when needed in "Kvantum.cpp".) */
  QPalette wp = widget->palette();
  QColor wBaseCol = wp.color(QPalette::Base);
  if (wBaseCol.alpha() < 255 && wBaseCol == standardPalette().color(QPalette::Base)
      && ((isOpaque_ && qobject_cast<QAbstractItemView*>(widget)) // like in VLC play list view
          /* Without combo menu, "QComboBoxPrivateContainer" should be opaque.
             With combo menu, it may be changed by the app and so, polished again (like in Lyx). */
          || widget->inherits("QComboBoxPrivateContainer")
          || (!tspec_.combo_menu && widget->inherits("QComboBoxListView")) // simple combo popup
          || widget->inherits("QTextEdit") || widget->inherits("QPlainTextEdit")
          || qobject_cast<QAbstractItemView*>(getParent(widget,2)) // inside view-items
          || widget->inherits("KSignalPlotter"))) // probably has a bug
  {
    QColor winCol = standardPalette().color(QPalette::Window);
    winCol.setAlpha(255);
    wBaseCol = overlayColor(winCol,wBaseCol);
    wp.setColor(QPalette::Base,wBaseCol);
    QColor altBaseCol = standardPalette().color(QPalette::AlternateBase);
    if (altBaseCol.alpha() < 255)
    {
      altBaseCol = overlayColor(wBaseCol,altBaseCol);
      wp.setColor(QPalette::AlternateBase,altBaseCol);
    }
    forcePalette(widget, wp);
  }

  if (widget->inherits("KTitleWidget") && hspec_.transparent_ktitle_label)
  { // -> ktitlewidget.cpp
    /*QPalette palette = widget->palette();
    palette.setColor(QPalette::Base,QColor(Qt::transparent));
    forcePalette(widget, palette);*/
    if (QFrame *titleFrame = widget->findChild<QFrame*>())
      titleFrame->setAutoFillBackground(false);
  }

  /*if (widget->autoFillBackground()
      && widget->parentWidget()
      && widget->parentWidget()->objectName() == "qt_scrollarea_viewport"
      && qobject_cast<QAbstractScrollArea*>(getParent(widget,2)))
  {
    widget->parentWidget()->setAutoFillBackground(false);
    widget->setAutoFillBackground(false);
  }*/

  if (widget->inherits("QComboBoxPrivateScroller"))
  { // needed for combo menu scrollers to have transparent backgrounds
    QPalette palette = widget->palette();
    palette.setColor(widget->backgroundRole(), QColor(Qt::transparent));
    forcePalette(widget, palette);
  }
  else if (qobject_cast<QMdiSubWindow*>(widget))
    /* to integrate the corner area, autoFillBackground isn't set
       for QMdiArea, so QMdiSubWindow should be drawn at PE_Widget */
    widget->setAttribute(Qt::WA_StyledBackground);
  else if (qobject_cast<QDockWidget*>(widget))
    widget->setAttribute(Qt::WA_Hover, true);
  else if (qobject_cast<QLineEdit*>(widget) || widget->inherits("KCalcDisplay"))
  {
    if (qobject_cast<QLineEdit*>(widget)
        && (!getFrameSpec(QStringLiteral("ToolbarLineEdit")).element.isEmpty()
            || !getInteriorSpec(QStringLiteral("ToolbarLineEdit")).element.isEmpty())
        && getStylableToolbarContainer(widget, true))
    {
      if (!tspec_.animate_states)
        widget->installEventFilter(this);
    }
    else // in rare cases like KNotes' font combos or Kcalc
    {
      QPalette palette = widget->palette();
      if (palette.color(QPalette::Active, QPalette::Base)
          == standardPalette().color(QPalette::Active, QPalette::Base))
      {
        QColor col = standardPalette().color(QPalette::Active,QPalette::Text);
        if (col != palette.color(QPalette::Active,QPalette::Text))
        {
          palette.setColor(QPalette::Active,QPalette::Text,col);
          palette.setColor(QPalette::Inactive,QPalette::Text,
                           standardPalette().color(QPalette::Inactive,QPalette::Text));
          forcePalette(widget, palette);
        }
      }
    }

    if (qobject_cast<QLineEdit*>(widget)
        && (tspec_.animate_states
            /* a workaround for bad codes that change line-edit base color */
            /*|| (!isPcmanfm_ && (tspec_.combo_as_lineedit || tspec_.square_combo_button)
                && qobject_cast<QComboBox*>(pw))*/))
    {
      widget->installEventFilter(this);
    }
  }
  else if (qobject_cast<QComboBox*>(widget)
           || qobject_cast<QSlider*>(widget))
  {
    widget->setAttribute(Qt::WA_Hover, true);
    if (tspec_.animate_states)
      widget->installEventFilter(this);
    /* set an appropriate vertical margin for combo popup items */
    if (QComboBox *combo = qobject_cast<QComboBox*>(widget))
    {
      if(!hasParent(widget, "QWebView"))
      {
        QAbstractItemView *itemView(combo->view());
        if(itemView && itemView->itemDelegate())
        {
          if (itemView->itemDelegate()->inherits("QComboMenuDelegate"))
          { // enforce translucency on the combo menu in every possible way
            QPalette vPalette = itemView->viewport()->palette();
            QColor menuTextColor;
            bool baseContrast(false);
            if (itemView->viewport()->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly).isEmpty())
            { // font menus use the palette text color, so we set it to the menu text color when needed
              menuTextColor = getFromRGBA(getLabelSpec(QStringLiteral("MenuItem")).normalColor);
              baseContrast = enoughContrast(vPalette.color(QPalette::Text), menuTextColor);
            }

            if (itemView->style() != this)
            { // no mercy to intruding styles (as in SMPLayer preferences)
              QString ss;
              if (baseContrast)
              {
                ss = QStringLiteral("QAbstractItemView{background-color: transparent; color: %1}")
                     .arg(getLabelSpec(QStringLiteral("MenuItem")).normalColor);
              }
              else
                ss = QStringLiteral("QAbstractItemView{background-color: transparent;}");
              itemView->setStyleSheet(ss);
            }

            QPalette palette = itemView->palette();
            palette.setColor(itemView->backgroundRole(), QColor(Qt::transparent));
            forcePalette(itemView, palette);

            itemView->viewport()->setAutoFillBackground(false);
            if (baseContrast)
            {
              vPalette.setColor(QPalette::Text, menuTextColor);
              forcePalette(itemView->viewport(), vPalette);
            }
          }
          else if (itemView->itemDelegate()->inherits("QComboBoxDelegate")
                   || qobject_cast<KvComboItemDelegate*>(combo->itemDelegate()))
          {
            /* the combo menu setting may have been toggled in Kvantum Manager */
            QPalette palette = itemView->palette();
            if (palette.color(itemView->backgroundRole()) == QColor(Qt::transparent))
            {
              if (itemView->styleSheet() == QStringLiteral("QAbstractItemView{background-color: transparent;}")
                  || itemView->styleSheet()
                       == QStringLiteral("QAbstractItemView{background-color: transparent; color: %1}")
                          .arg(getLabelSpec(QStringLiteral("MenuItem")).normalColor))
              {
                itemView->setStyleSheet(QString());
              }

              QColor winCol = standardPalette().color(QPalette::Window);
              winCol.setAlpha(255);
              QColor baseCol = standardPalette().color(QPalette::Base);
              if (baseCol.alpha() < 255)
                baseCol = overlayColor(winCol,baseCol);

              palette.setColor(QPalette::Base, baseCol);
              palette.setColor(QPalette::Window, winCol);
              forcePalette(itemView, palette);
              QColor vBgCol;
              if (itemView->viewport())
              {
                itemView->viewport()->setAutoFillBackground(true);
                vBgCol = palette.color(itemView->viewport()->backgroundRole());
              }
              else // impossible
                vBgCol = baseCol;
              QList<QScrollBar*> scrollbars = combo->findChildren<QScrollBar*>();
              for (int i = 0; i < scrollbars.size(); ++i)
              {
                  QScrollBar *sb = scrollbars.at(i);
                  sb->setAutoFillBackground(true);
                  palette = sb->palette();
                  palette.setColor(sb->backgroundRole(), vBgCol);
                  forcePalette(sb, palette);
              }
              if (combo->completer())
              {
                if (QAbstractItemView *cv = combo->completer()->popup())
                {
                    palette = cv->palette();
                    palette.setColor(QPalette::Text, standardPalette().color(QPalette::Text));
                    palette.setColor(QPalette::Base, baseCol);
                    palette.setColor(QPalette::Window, winCol);
                    forcePalette(cv, palette);
                }
              }
            }
            else if (itemView->viewport())
              itemView->viewport()->setAutoFillBackground(true);
            if (itemView->itemDelegate()->inherits("QComboBoxDelegate"))
            {
              /* PM_FocusFrameVMargin is used for viewitems */
              itemView->setItemDelegate(new KvComboItemDelegate(pixelMetric(PM_FocusFrameVMargin),
                                                                itemView));
              if (!tspec_.animate_states) // see eventFilter() -> QEvent::StyleChange
                widget->installEventFilter(this);
            }
          }
        }
      }
    }
  }
  else if (qobject_cast<QTabBar*>(widget))
  {
    widget->setAttribute(Qt::WA_Hover, true);
    if (tspec_.active_tab_overlap > 0) // see QEvent::HoverEnter
      widget->installEventFilter(this);
  }
  else if (qobject_cast<QProgressBar*>(widget))
  {
    widget->setAttribute(Qt::WA_Hover, true);
    widget->installEventFilter(this);
  }
  else if (QGroupBox *gb = qobject_cast<QGroupBox*>(widget))
  {
    if (gb->isCheckable())
    {
      widget->setAttribute(Qt::WA_Hover, true);
      if (tspec_.animate_states)
        widget->installEventFilter(this);
    }
  }
  else if (qobject_cast<QCommandLinkButton*>(widget))
    widget->installEventFilter(this); // to paint it
  else if ((tspec_.animate_states &&
            (qobject_cast<QPushButton*>(widget)
             || (qobject_cast<QToolButton*>(widget)
                 && !qobject_cast<QLineEdit*>(pw)) // exclude line-edit clear buttons
             || qobject_cast<QCheckBox*>(widget)
             || qobject_cast<QRadioButton*>(widget)
             || (qobject_cast<QAbstractButton*>(widget) && qobject_cast<QTabBar*>(pw)) // tab close button
             || widget->inherits("QComboBoxPrivateContainer")))
            /* unfortunately, KisSliderSpinBox uses a null widget in drawing
               its progressbar, so we can identify it only through eventFilter()
               (digiKam has its own version of it, called "DAbstractSliderSpinBox") */
           || widget->inherits("KisAbstractSliderSpinBox") || widget->inherits("Digikam::DAbstractSliderSpinBox")
           /* Although KMultiTabBarTab is a push button, it uses PE_PanelButtonTool
              for drawing its panel, but not if its state is normal. To force the
              normal text color on it, we need to make it use PE_PanelButtonTool
              with the normal state too and that can be done at its paint event. */
           || widget->inherits("KMultiTabBarTab"))
  {
      widget->installEventFilter(this);
  }
  else if (qobject_cast<QAbstractSpinBox*>(widget))
  {
    widget->setAttribute(Qt::WA_Hover, true);
    // see eventFilter() for the reason
    widget->installEventFilter(this);
  }
  else if (qobject_cast<QScrollBar*>(widget))
  {
    widget->setAttribute(Qt::WA_Hover, true);
    /* without this, transparent backgrounds
       couldn't be used for scrollbar grooves */
    widget->setAttribute(Qt::WA_OpaquePaintEvent, false);

    if (tspec_.animate_states)
      widget->installEventFilter(this);
  }
  else if (QAbstractScrollArea *sa = qobject_cast<QAbstractScrollArea*>(widget))
  {
    QWidget *vp = sa->viewport();
    if(hspec_.kinetic_scrolling
       && vp && !vp->testAttribute(Qt::WA_StyleSheetTarget) && !QScroller::hasScroller(vp)
       && !widget->autoFillBackground() && !widget->inherits("QComboBoxListView")
       && !widget->inherits("QTextEdit") && !widget->inherits("QPlainTextEdit")
       && !widget->inherits("KSignalPlotter"))
    {
      bool insideSubwin(false);
      QWidget *parent = pw;
      while (parent)
      {
        if (qobject_cast<QMdiSubWindow*>(parent))
        {
          insideSubwin = true; // as in Krita
          break;
        }
        if (parent->isWindow()) break;
        parent = parent->parentWidget();
      }
      if(!insideSubwin)
      {
        QScroller::grabGesture(vp, QScroller::LeftMouseButtonGesture);
        auto sp = QScroller::scroller(vp)->scrollerProperties();
        sp.setScrollMetric(QScrollerProperties::OvershootScrollTime, static_cast<qreal>(0.3));
        sp.setScrollMetric(QScrollerProperties::OvershootScrollDistanceFactor, static_cast<qreal>(0.1));
        sp.setScrollMetric(QScrollerProperties::OvershootDragDistanceFactor, static_cast<qreal>(0.1));
        QScroller::scroller(vp)->setScrollerProperties(sp);
      }
    }
    if (/*sa->frameShape() == QFrame::NoFrame &&*/ // Krita and digiKam aren't happy with this
        sa->backgroundRole() == QPalette::Window
        || sa->backgroundRole() == QPalette::Button) // inside toolbox
    {
      if (vp && (vp->backgroundRole() == QPalette::Window
                 || vp->backgroundRole() == QPalette::Button))
      { // remove ugly flat backgrounds (when the window backround is styled)
        vp->setAutoFillBackground(false);
        const QList<QWidget*> children = vp->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
        for (QWidget *child : children)
        {
          if (child->backgroundRole() == QPalette::Window
              || child->backgroundRole() == QPalette::Button)
          {
            child->setAutoFillBackground(false);
          }
        }
      }
      else
      {
        // support animation only if the background is flat
        if ((tspec_.animate_states
             && (sa->frameStyle() & QFrame::StyledPanel) // see Qt -> qabstractscrollarea.cpp
             && vp && vp->autoFillBackground()
             && themeRndr_ && themeRndr_->isValid()) // the default SVG file doesn't have a focus state for frames
           || (hasInactiveSelItemCol_
               && qobject_cast<QAbstractItemView*>(widget))) // enforce the text color of inactive selected items
        {
          widget->installEventFilter(this);
        }
        // set the background correctly when scrollbars are either inside the frame or inside a combo popup
        if ((tspec_.scrollbar_in_view || (widget->inherits("QComboBoxListView")
                                          && (!tspec_.combo_menu /*|| isLibreoffice_*/)))
            && vp && vp->autoFillBackground()
            && (!vp->testAttribute(Qt::WA_StyleSheetTarget)
                || vp->styleSheet().isEmpty() || !vp->styleSheet().contains(QLatin1String("background")))
            // but not when the combo popup is drawn as a menu
            && !(tspec_.combo_menu /*&& !isLibreoffice_*/ && widget->inherits("QComboBoxListView"))
            // also consider pcmanfm hacking keys
            && !(isPcmanfm_
                 && ((hspec_.transparent_pcmanfm_view && pw
                      && pw->inherits("Fm::FolderView") && !pw->inherits("PCManFM::DesktopWindow"))
                     || (hspec_.transparent_pcmanfm_sidepane
                         && (sa->inherits("Fm::DirTreeView") || (pw && pw->inherits("Fm::SidePane")))))))
        {
          QColor col = vp->palette().color(vp->backgroundRole());
          QColor col1;
          if (!tspec_.no_inactiveness)
            col1 = vp->palette().color(QPalette::Inactive, vp->backgroundRole());
          if (col.isValid())
          {
            QPalette palette;
            if (QScrollBar *sb = sa->horizontalScrollBar())
            {
              sb->setAutoFillBackground(true);
              sb->setBackgroundRole(vp->backgroundRole()); // sometimes needed
              palette = sb->palette();
              palette.setColor(sb->backgroundRole(), col);
              if (col1.isValid() && col1 != col)
                palette.setColor(QPalette::Inactive, sb->backgroundRole(), col1);
              forcePalette(sb, palette);
            }
            if (QScrollBar *sb = sa->verticalScrollBar())
            {
              sb->setAutoFillBackground(true);
              sb->setBackgroundRole(vp->backgroundRole());
              palette = sb->palette();
              palette.setColor(sb->backgroundRole(), col);
              if (col1.isValid() && col1 != col)
                palette.setColor(QPalette::Inactive, sb->backgroundRole(), col1);
              forcePalette(sb, palette);
            }
            // FIXME: is this needed?
            palette = widget->palette();
            if (palette.color(vp->backgroundRole()) != col)
            {
              palette.setColor(widget->backgroundRole(), col);
              forcePalette(widget, palette);
            }
          }
        }
      }
    }
  }
  else if (qobject_cast<QToolBox*>(widget))
  {
    widget->setBackgroundRole(QPalette::NoRole);
    widget->setAutoFillBackground(false);
  }
  // taken from Oxygen
  else if (qobject_cast<QToolBox*>(getParent(widget,3)))
  {
    widget->setBackgroundRole(QPalette::NoRole);
    widget->setAutoFillBackground(false);
    pw->setAutoFillBackground(false);
  }
  // remove the ugly shadow of QWhatsThis tooltips
  else if (widget->inherits("QWhatsThat"))
  {
    QPalette palette = widget->palette();
    QColor shadow = palette.shadow().color();
    shadow.setAlpha(0);
    palette.setColor(QPalette::Shadow, shadow);
    forcePalette(widget, palette);
  }
  else if (QStatusBar *sb = qobject_cast<QStatusBar*>(widget))
  {
    if (hspec_.forceSizeGrip)
    { // WARNING: adding size grip to non-window widgets may cause crash
      if (QMainWindow *mw = qobject_cast<QMainWindow*>(pw))
      {
        if (mw->minimumSize() != mw->maximumSize())
        {
          QLayout *lo = mw->layout();
          if (lo && lo->sizeConstraint() != QLayout::SetFixedSize)
            sb->setSizeGripEnabled(true);
        }
      }
    }
  }
  // update grouped toolbar buttons when one of them is shown/hidden
  else if (!tspec_.animate_states // otherwise it already has event filter installed on it
           && tspec_.group_toolbar_buttons && qobject_cast<QToolButton*>(widget))
  {
    if (QToolBar *toolBar = qobject_cast<QToolBar*>(pw))
    {
      if (toolBar->orientation() != Qt::Vertical)
        widget->installEventFilter(this);
    }
  }

  bool isMenuOrTooltip(!noComposite_
                       && !subApp_
                       && (qobject_cast<QMenu*>(widget)
                           /* no shadow for tooltips that are already translucent */
                           || (widget->inherits("QTipLabel") /*&& !isLibreoffice_*/
                               && (!widget->testAttribute(Qt::WA_TranslucentBackground)
                                   || !widget->testAttribute(Qt::WA_NoSystemBackground)
                                   || forcedTranslucency_.contains(widget)))));
  if ((isMenuOrTooltip
          /* because of combo menus or round corners */
       || (/*tspec_.isX11 && */widget->inherits("QComboBoxPrivateContainer")))
      && !translucentWidgets_.contains(widget))
  {
    theme_spec tspec_now = settings_->getCompositeSpec();
    if (tspec_now.composite)
    {
      if (tspec_now.menu_shadow_depth > 0)
        getShadow(QStringLiteral("Menu"), getMenuMargin(true), getMenuMargin(false));
      if (qobject_cast<QMenu*>(widget))
      {
        /* On the one hand, RTL submenus aren't positioned correctly by Qt and, since
           the RTL property isn't set yet, we should move them later. On the other hand,
           menus should be moved to compensate for the offset created by their shadows. */
        widget->installEventFilter(this);
      }

      if (!widget->testAttribute(Qt::WA_TranslucentBackground))
        widget->setAttribute(Qt::WA_TranslucentBackground); // Qt5 may need this too
      else if (!widget->testAttribute(Qt::WA_NoSystemBackground))
        widget->setAttribute(Qt::WA_NoSystemBackground);

      translucentWidgets_.insert(widget);
      connect(widget, &QObject::destroyed, this, &Style::noTranslucency);

      if (!widget->inherits("QComboBoxPrivateContainer") || tspec_.combo_menu)
      {
        if (blurHelper_ == nullptr && tspec_now.popup_blurring)
        {
          if (tspec_now.tooltip_shadow_depth > 0)
          {
            const frame_spec fspec = getFrameSpec(QStringLiteral("ToolTip"));
            int thickness = qMax(qMax(fspec.top,fspec.bottom), qMax(fspec.left,fspec.right));
            thickness += tspec_now.tooltip_shadow_depth;
            getShadow(QStringLiteral("ToolTip"), thickness);
          }
          blurHelper_ = new BlurHelper(this, menuShadow_, tooltipShadow_,
                                       tspec_.menu_blur_radius, tspec_.tooltip_blur_radius,
                                       tspec_.contrast, tspec_.intensity, tspec_.saturation,
                                       hspec_.blur_only_active_window);
        }
        /* blurHelper_ may exist because of blurring hard-coded translucency */
        if (blurHelper_ && tspec_now.popup_blurring)
          blurHelper_->registerWidget(widget);
      }
    }
    else if (qobject_cast<QMenu*>(widget)) // for menubars and submenus (eventFilter -> case QEvent::Show)
      widget->installEventFilter(this);
  }
}

void Style::polish(QApplication *app)
{
  const QString appName = app->applicationName();
  if (appName == "Qt-subapplication")
    subApp_ = true;
  else if (appName == "dolphin")
    isDolphin_ = true;
  else if (appName == "pcmanfm-qt")
    isPcmanfm_ = true;
  else if (appName == "soffice.bin")
    isLibreoffice_ = true;
  else if (appName == "krita")
    isKrita_ = true;
  else if (appName == "plasma" || appName.startsWith("plasma-")
           || appName == "plasmashell" // Plasma5
           || appName == "kded4") // this is for the infamous appmenu
    isPlasma_ = true;

  if (tspec_.opaque.contains(appName, Qt::CaseInsensitive))
    isOpaque_ = true;

  /* force our palette */
  QPalette palette = app->palette();
  polish(palette);
  app->setPalette(palette);

  QCommonStyle::polish(app);
  if (itsShortcutHandler_)
    app->installEventFilter(itsShortcutHandler_);
}

void Style::polish(QPalette &palette)
{
  palette = standardPalette();
}

void Style::unpolish(QWidget *widget)
{
  if (widget)
  {

    /*widget->setAttribute(Qt::WA_Hover, false);*/

    switch (widget->windowFlags() & Qt::WindowType_Mask) {
      case Qt::Window:
      case Qt::Dialog:
      case Qt::Popup:
      case Qt::ToolTip:
      case Qt::Sheet: {
        if (itsWindowManager_)
          itsWindowManager_->unregisterWidget(widget);
        if (qobject_cast<QMenu*>(widget)
            || widget->inherits("QTipLabel")
            || qobject_cast<QLabel*>(widget))
        {
          break;
        }
        if (blurHelper_)
          blurHelper_->unregisterWidget(widget);
        if ((forcedTranslucency_.contains(widget)
             && !widget->windowFlags().testFlag(Qt::FramelessWindowHint)
             && !widget->windowFlags().testFlag(Qt::X11BypassWindowManagerHint))
            // was made translucent because of combo menu or round corners
            || (widget->inherits("QComboBoxPrivateContainer")
                && translucentWidgets_.contains(widget)))
        {
          widget->removeEventFilter(this);
          widget->setAttribute(Qt::WA_NoSystemBackground, false);
        }
        if (gtkDesktop_)
          widget->removeEventFilter(this);
        widget->setAttribute(Qt::WA_StyledBackground, false); // FIXME is this needed?
        /* this is needed with tranlucent windows when
           the theme is changed from Kvantum and to it again */
        translucentWidgets_.remove(widget);
        forcedTranslucency_.remove(widget);
        break;
      }
      default:
        break;
    }

    if (widget->inherits("KisAbstractSliderSpinBox")
        || widget->inherits("Digikam::DAbstractSliderSpinBox")
        || widget->inherits("KMultiTabBarTab")
        || qobject_cast<QProgressBar*>(widget)
        || qobject_cast<QAbstractSpinBox*>(widget)
        || qobject_cast<QToolButton*>(widget)
        || qobject_cast<QCommandLinkButton*>(widget) // we paint it
        || qobject_cast<QComboBox*>(widget) // for both state anomation and delegate
        || (tspec_.active_tab_overlap > 0 && qobject_cast<QTabBar*>(widget))
        || (tspec_.animate_states &&
            (qobject_cast<QPushButton*>(widget)
             || qobject_cast<QCheckBox*>(widget)
             || qobject_cast<QRadioButton*>(widget)
             || (qobject_cast<QAbstractButton*>(widget) && qobject_cast<QTabBar*>(widget->parentWidget()))
             || qobject_cast<QScrollBar*>(widget)
             || qobject_cast<QSlider*>(widget)
             || qobject_cast<QLineEdit*>(widget)
             || qobject_cast<QAbstractScrollArea*>(widget)
             //|| widget->inherits("QComboBoxPrivateContainer") // done above
             || qobject_cast<QGroupBox*>(widget)))
         || (hasInactiveSelItemCol_ && qobject_cast<QAbstractItemView*>(widget)))
    {
      widget->removeEventFilter(this);
    }
    else if (qobject_cast<QToolBox*>(widget))
      widget->setBackgroundRole(QPalette::Button);

    if (hspec_.kinetic_scrolling)
    {
      if (QAbstractScrollArea *sa = qobject_cast<QAbstractScrollArea*>(widget))
      {
        QWidget *vp = sa->viewport();
        if (vp && !vp->testAttribute(Qt::WA_StyleSheetTarget)
            && !widget->autoFillBackground() && !widget->inherits("QComboBoxListView")
            && !widget->inherits("QTextEdit") && !widget->inherits("QPlainTextEdit")
            && !widget->inherits("KSignalPlotter"))
        {
          QScroller::ungrabGesture(vp);
        }
      }
    }

    if (qobject_cast<QMenu*>(widget) || widget->inherits("QTipLabel"))
    {
      if (blurHelper_)
        blurHelper_->unregisterWidget(widget);
      if (qobject_cast<QMenu*>(widget))
        widget->removeEventFilter(this);
      if (translucentWidgets_.contains(widget))
      {
        widget->setAttribute(Qt::WA_PaintOnScreen, false);
        widget->setAttribute(Qt::WA_NoSystemBackground, false);
        /* menus may be cached, so that if not removed from the list,
           they might lack translucency the next time they appear */
        translucentWidgets_.remove(widget);
        forcedTranslucency_.remove(widget);
      }
      //widget->clearMask();
    }
  }
}

void Style::unpolish(QApplication *app)
{
  /* WARNING: This method is also called when an app style sheet is set (as in QupZilla),
              in which case, main windows might not be drawn at all if translucency isn't
              removed here. However, due to a bug in Qt 5.13.1 -> QWidget::setAttribute(),
              setting WA_TranslucentBackground to false might cause a mess later.
              Since Qt's bug tracker is unresponsive most of the time, I only set
              WA_NoSystemBackground to false. */
  QSetIterator<QWidget*> i(forcedTranslucency_);
  while (i.hasNext())
  {
    if (QWidget *w = i.next())
      w->setAttribute(Qt::WA_NoSystemBackground, false);
  }
  forcedTranslucency_.clear();
  translucentWidgets_.clear();

  /* Reset all forced palettes.
     This will be useful if the style changes from Kvantum in runtime. */
  const auto widgets = app->allWidgets();
  for (const auto &widget : widgets)
  {
    if (widget->property("_kv_fPalette").toBool())
    {
      widget->setPalette(QPalette());
      widget->setProperty("_kv_fPalette", QVariant());
    }
  }

  if (app && itsShortcutHandler_)
    app->removeEventFilter(itsShortcutHandler_);
  QCommonStyle::unpolish(app);
}

/* Set the standard palette to the Kvantum theme palette
   (to be used in the code because QApplication::palette()
   may not be reliable with apps like Qt Designer). */
QPalette Style::standardPalette() const
{
  if (standardPalette_.isBrushSet(QPalette::Active,QPalette::Base))
    return standardPalette_;

  QColor col1;
  bool hasInactiveness(!tspec_.no_inactiveness);

  /* background colors */
  QColor col = getFromRGBA(cspec_.windowColor);
  if (col.isValid())
  {
    standardPalette_.setColor(QPalette::Active,QPalette::Window,col);
    standardPalette_.setColor(QPalette::Disabled,QPalette::Window,col); // used in generatedIconPixmap()
    col1 = getFromRGBA(cspec_.inactiveWindowColor);
    if (col1.isValid() && hasInactiveness)
      standardPalette_.setColor(QPalette::Inactive,QPalette::Window,col1);
    else
      standardPalette_.setColor(QPalette::Inactive,QPalette::Window,col);
  }

  col = getFromRGBA(cspec_.baseColor);
  if (col.isValid())
  {
    standardPalette_.setColor(QPalette::Active,QPalette::Base,col);
    standardPalette_.setColor(QPalette::Disabled,QPalette::Base,col); // some apps may use it
    col1 = getFromRGBA(cspec_.inactiveBaseColor);
    if (col1.isValid() && hasInactiveness)
      standardPalette_.setColor(QPalette::Inactive,QPalette::Base,col1);
    else
      standardPalette_.setColor(QPalette::Inactive,QPalette::Base,col);
  }
  else // just to know that all brushes are set
    standardPalette_.setColor(QPalette::Active,QPalette::Base,QColor(Qt::white));

  col = getFromRGBA(cspec_.altBaseColor);
  if (col.isValid())
  {
    standardPalette_.setColor(QPalette::Active,QPalette::AlternateBase,col);
    standardPalette_.setColor(QPalette::Disabled,QPalette::AlternateBase,col);
    col1 = getFromRGBA(cspec_.inactiveAltBaseColor);
    if (col1.isValid() && hasInactiveness)
      standardPalette_.setColor(QPalette::Inactive,QPalette::AlternateBase,col1);
    else
      standardPalette_.setColor(QPalette::Inactive,QPalette::AlternateBase,col);
  }
  else
  {
    col = standardPalette_.color(QPalette::Active,QPalette::Base);
    int l = col.lightness();
    if (l < 127) l += 10; else l -= 10;
    col.setHsl(col.hue(), col.saturation(), l, col.alpha());
    standardPalette_.setColor(QPalette::Active,QPalette::AlternateBase,col);
    standardPalette_.setColor(QPalette::Disabled,QPalette::AlternateBase,col);
    if (!hasInactiveness)
      standardPalette_.setColor(QPalette::Inactive,QPalette::AlternateBase,col);
    else
    {
      col = standardPalette_.color(QPalette::Inactive,QPalette::Base);
      l = col.lightness();
      if (l < 127) l += 10; else l -= 10;
      col.setHsl(col.hue(), col.saturation(), l, col.alpha());
      standardPalette_.setColor(QPalette::Inactive,QPalette::AlternateBase,col);
    }
  }

  col = getFromRGBA(cspec_.buttonColor);
  if (col.isValid())
    standardPalette_.setColor(QPalette::Button,col);

  col = getFromRGBA(cspec_.lightColor);
  if (col.isValid())
    standardPalette_.setColor(QPalette::Light,col);
  col = getFromRGBA(cspec_.midLightColor);
  if (col.isValid())
    standardPalette_.setColor(QPalette::Midlight,col);
  col = getFromRGBA(cspec_.darkColor);
  if (col.isValid())
    standardPalette_.setColor(QPalette::Dark,col);
  col = getFromRGBA(cspec_.midColor);
  if (col.isValid())
    standardPalette_.setColor(QPalette::Mid,col);
  col = getFromRGBA(cspec_.shadowColor);
  if (col.isValid())
    standardPalette_.setColor(QPalette::Shadow,col);

  col = getFromRGBA(cspec_.highlightColor);
  if (col.isValid())
  {
    standardPalette_.setColor(QPalette::Active,QPalette::Highlight,col);
    standardPalette_.setColor(QPalette::Disabled,QPalette::Highlight,col);
    col1 = getFromRGBA(cspec_.inactiveHighlightColor);
    if (col1.isValid() && col1 != col && hasInactiveness)
      standardPalette_.setColor(QPalette::Inactive,QPalette::Highlight,col1);
    else
    {
      /* NOTE: Qt has a nasty bug which, sometimes, prevents updating of inactive widgets
               when the active and inactive highlight colors are the same. As a workaround,
               we make them just a little different from each other. */
      int v = col.value();
      if (v == 0) v++; else v--;
      col.setHsv(col.hue(), col.saturation(), v, col.alpha());
      standardPalette_.setColor(QPalette::Inactive,QPalette::Highlight,col);
    }
  }

  col = getFromRGBA(cspec_.tooltipBaseColor);
  if (col.isValid())
    standardPalette_.setColor(QPalette::ToolTipBase,col);
  else
  { // for backward compatibility
    col = getFromRGBA(cspec_.tooltipTextColor);
    if (col.isValid())
    {
      col1 = QColor(Qt::white);
      if (qGray(col.rgb()) >= 127)
        col1 = QColor(Qt::black);
      standardPalette_.setColor(QPalette::ToolTipBase,col1);
    }
  }

  /* text colors */
  col = getFromRGBA(cspec_.textColor);
  if (col.isValid())
  {
    QColor placeholderTextColor = col;
    placeholderTextColor.setAlpha(128);
    standardPalette_.setColor(QPalette::Active,QPalette::Text,col);
    standardPalette_.setColor(QPalette::PlaceholderText,placeholderTextColor);
    col1 = getFromRGBA(cspec_.inactiveTextColor);
    if (col1.isValid() && hasInactiveness)
      standardPalette_.setColor(QPalette::Inactive,QPalette::Text,col1);
    else
      standardPalette_.setColor(QPalette::Inactive,QPalette::Text,col);
  }

  col = getFromRGBA(cspec_.windowTextColor);
  if (col.isValid())
  {
    standardPalette_.setColor(QPalette::Active,QPalette::WindowText,col);
    col1 = getFromRGBA(cspec_.inactiveWindowTextColor);
    if (col1.isValid() && hasInactiveness)
      standardPalette_.setColor(QPalette::Inactive,QPalette::WindowText,col1);
    else
      standardPalette_.setColor(QPalette::Inactive,QPalette::WindowText,col);
  }

  col = getFromRGBA(cspec_.buttonTextColor);
  if (col.isValid())
  {
    standardPalette_.setColor(QPalette::Active,QPalette::ButtonText,col);
    standardPalette_.setColor(QPalette::Inactive,QPalette::ButtonText,col);
  }

  col = getFromRGBA(cspec_.tooltipTextColor);
  if (col.isValid())
    standardPalette_.setColor(QPalette::ToolTipText,col);

  col = getFromRGBA(cspec_.highlightTextColor);
  if (col.isValid())
  {
    standardPalette_.setColor(QPalette::Active,QPalette::HighlightedText,col);
    col1 = getFromRGBA(cspec_.inactiveHighlightTextColor);
    if (col1.isValid() && hasInactiveness)
      standardPalette_.setColor(QPalette::Inactive,QPalette::HighlightedText,col1);
    else
      standardPalette_.setColor(QPalette::Inactive,QPalette::HighlightedText,col);
    /* also, derive the disabled highlighted text color */
    col.setAlpha(102); // 0.4 * col.alpha()
    standardPalette_.setColor(QPalette::Disabled,QPalette::HighlightedText,col);
  }

  col = getFromRGBA(cspec_.linkColor);
  if (col.isValid())
    standardPalette_.setColor(QPalette::Link,col);
  col = getFromRGBA(cspec_.linkVisitedColor);
  if (col.isValid())
    standardPalette_.setColor(QPalette::LinkVisited,col);

  /* disabled text */
  col = getFromRGBA(cspec_.disabledTextColor);
  if (col.isValid())
  {
    standardPalette_.setColor(QPalette::Disabled,QPalette::Text,col);
    standardPalette_.setColor(QPalette::Disabled,QPalette::WindowText,col);
    standardPalette_.setColor(QPalette::Disabled,QPalette::ButtonText,col);
  }

  return standardPalette_;
}

}
