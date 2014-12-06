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

#include <QApplication>
#include <QPainter>
#include <QSvgRenderer>
#include <QSettings>
#include <QFile>
#include <QTimer>
#include <QToolButton>
#include <QToolBar>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QProgressBar>
#include <QMenu>
#include <QGroupBox>
#include <QAbstractScrollArea>
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QDockWidget>
#include <QDial>
#include <QScrollBar>
#include <QMdiArea>
#include <QToolBox>
#include <QDir>
#include <QTextStream>
#include <QLabel>
#include <QPixmapCache> 
//#include <QBitmap>
#include <QPaintEvent>
#include <QtCore/qmath.h>
//#include <QDialogButtonBox> // for dialog buttons layout

#if QT_VERSION >= 0x050000
#include <QSurfaceFormat>
#include <QWindow>
#endif

#include "themeconfig/ThemeConfig.h"

#define M_PI 3.14159265358979323846
#define DISABLED_OPACITY 0.7
#define SPIN_BUTTON_WIDTH 16
#define SLIDER_TICK_SIZE 5
#define COMBO_ARROW_LENGTH 20
#define MIN_CONTRAST 65

Kvantum::Kvantum() : QCommonStyle()
{
  progresstimer = new QTimer(this);

  settings = defaultSettings = themeSettings = NULL;
  defaultRndr = themeRndr = NULL;
  singleClick = true;

  QString homeDir = QDir::homePath();

  char * _xdg_config_home = getenv("XDG_CONFIG_HOME");
  if (!_xdg_config_home)
    xdg_config_home = QString("%1/.config").arg(homeDir);
  else
    xdg_config_home = QString(_xdg_config_home);

  QString kdeGlobals = QString("%1/.kde/share/config/kdeglobals").arg(homeDir);
  if (!QFile::exists(kdeGlobals))
    kdeGlobals = QString("%1/.kde4/share/config/kdeglobals").arg(homeDir);
  if (QFile::exists(kdeGlobals))
  {
    QFile file(kdeGlobals);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
      QTextStream in(&file);
      while (!in.atEnd())
      {
        QString line = in.readLine();
        if (line.startsWith("SingleClick"))
        {
          if (line.endsWith("=false") || line.endsWith("=0"))
            singleClick = false;
          break;
        }
      }
      file.close();
    }
  }

  // load global config file
  QString theme;
  if (QFile::exists(QString("%1/Kvantum/kvantum.kvconfig").arg(xdg_config_home)))
  {
    QSettings globalSettings (QString("%1/Kvantum/kvantum.kvconfig").arg(xdg_config_home),
                              QSettings::NativeFormat);

    if (globalSettings.status() == QSettings::NoError && globalSettings.contains("theme"))
      theme = globalSettings.value("theme").toString();
  }

  setBuiltinDefaultTheme();
  setUserTheme(theme);

  isPlasma = false;
  isLibreoffice = false;
  isSystemSettings = false;
  isDolphin = false;
  isKonsole = false;
  subApp = false;
  isOpaque = false;

  connect(progresstimer, SIGNAL(timeout()),
          this, SLOT(advanceProgresses()));

  itsShortcutHandler = NULL;
  itsWindowManager = NULL;
  blurHelper = NULL;
  const theme_spec tspec = settings->getThemeSpec();

  if (tspec.alt_mnemonic)
    itsShortcutHandler = new ShortcutHandler(this);

#if defined Q_WS_X11 || defined Q_OS_LINUX
  if (tspec.x11drag)
  {
    itsWindowManager = new WindowManager(this);
    itsWindowManager->initialize();
  }

  if (tspec.blurring)
    blurHelper = new BlurHelper(this);
#endif
}

Kvantum::~Kvantum()
{
  delete defaultSettings;
  delete themeSettings;

  delete defaultRndr;
  delete themeRndr;
}

void Kvantum::setBuiltinDefaultTheme()
{
  if (defaultSettings)
  {
    delete defaultSettings;
    defaultSettings = NULL;
  }
  if (defaultRndr)
  {
    delete defaultRndr;
    defaultRndr = NULL;
  }

  defaultSettings = new ThemeConfig(":default.kvconfig");
  defaultRndr = new QSvgRenderer();
  defaultRndr->load(QString(":default.svg"));
}

void Kvantum::setUserTheme(const QString &themename)
{
  if (themeSettings)
  {
    delete themeSettings;
    themeSettings = NULL;
  }
  if (themeRndr)
  {
    delete themeRndr;
    themeRndr = NULL;
  }

  if (!themename.isNull() && !themename.isEmpty())
  {
    if (QFile::exists(QString("%1/Kvantum/%2/%2.kvconfig")
                             .arg(xdg_config_home)
                             .arg(themename)))
    {
      themeSettings = new ThemeConfig(QString("%1/Kvantum/%2/%2.kvconfig")
                                             .arg(xdg_config_home)
                                             .arg(themename));
    }

    if (QFile::exists(QString("%1/Kvantum/%2/%2.svg")
                             .arg(xdg_config_home)
                             .arg(themename)))
    {
      themeRndr = new QSvgRenderer();
      themeRndr->load(QString("%1/Kvantum/%2/%2.svg")
                             .arg(xdg_config_home)
                             .arg(themename));
    }
  }

  setupThemeDeps();
}

void Kvantum::setupThemeDeps()
{
  if (themeSettings)
  {
    // always use the default config as fallback
    themeSettings->setParent(defaultSettings);
    settings = themeSettings;
  }
  else
    settings = defaultSettings;
}

void Kvantum::advanceProgresses()
{
  QMap<QWidget *,int>::iterator it;
  for (it = progressbars.begin(); it != progressbars.end(); ++it)
  {
    QWidget *widget = it.key();
    if (widget->isVisible())
    {
      it.value() += 2;
      widget->update();
    }
  }
}

// also checks for NULL widgets
static inline QWidget *getParent (const QWidget *widget, int level)
{
  if (!widget || level <= 0) return NULL;
  QWidget *w = widget->parentWidget();
  for (int i = 1; i < level && w; ++i)
    w = w->parentWidget();
  return w;
}

void Kvantum::undoTranslucency(QObject *o)
{
  QWidget *widget = static_cast<QWidget*>(o);
  translucentTopWidgets.remove(widget);
}

void Kvantum::polish(QWidget *widget)
{
  if (widget)
  {
    // for moving the window containing this widget
    if (itsWindowManager)
      itsWindowManager->registerWidget(widget);

    widget->setAttribute(Qt::WA_Hover, true);
    //widget->setAttribute(Qt::WA_MouseTracking, true);

    const hacks_spec hspec = settings->getHacksSpec();
    if (hspec.respect_darkness)
    {
      bool changePalette = false;
      if (qobject_cast<QAbstractItemView*>(widget)
          || qobject_cast<QAbstractScrollArea*>(widget)
          || qobject_cast<QTabWidget*>(widget))
        changePalette = true;
      else if (QLabel *label = qobject_cast<QLabel*>(widget))
      {
        if (!label->text().isEmpty())
          changePalette = true;
      }
      if (changePalette)
      {
        QPalette palette = widget->palette();
        QColor txtCol = palette.color(QPalette::Text);
        if (qAbs(palette.color(QPalette::Base).value() - txtCol.value()) < MIN_CONTRAST
            || qAbs(palette.color(QPalette::Window).value() - palette.color(QPalette::WindowText).value()) < MIN_CONTRAST
            || (qobject_cast<QAbstractItemView*>(widget)
                && qAbs(palette.color(QPalette::AlternateBase).value() - txtCol.value()) < MIN_CONTRAST))
        {
          polish(palette);
          widget->setPalette(palette);
        }
      }
    }

    switch (widget->windowFlags() & Qt::WindowType_Mask) {
      case Qt::Window:
      case Qt::Dialog: {
        widget->setAttribute(Qt::WA_StyledBackground);
        const theme_spec tspec = settings->getThemeSpec();
        /* take all precautions */
        if (((tspec.translucent_windows
              && !widget->testAttribute(Qt::WA_TranslucentBackground)
              && !widget->testAttribute(Qt::WA_NoSystemBackground))
             /* enable blurring for Konsole's main window if it's transparent */
             || (isKonsole && hspec.blur_konsole
                 && widget->testAttribute(Qt::WA_TranslucentBackground)))
            && !isPlasma && !isOpaque && !subApp && !isLibreoffice
            && widget->isWindow()
            && widget->windowType() != Qt::Desktop
            && !widget->testAttribute(Qt::WA_PaintOnScreen)
            && !widget->testAttribute(Qt::WA_X11NetWmWindowTypeDesktop)
            && !widget->inherits("KScreenSaver")
            && !widget->inherits("QTipLabel")
            && !widget->inherits("QSplashScreen")
            && !widget->windowFlags().testFlag(Qt::FramelessWindowHint)
            /* Without this, apps using QtWebKit might crash when quitting.
               Fortunately internalWinId() exists although it isn't documeneted.*/
            && widget->internalWinId()
            && !translucentTopWidgets.contains(widget))
        {
#if QT_VERSION < 0x050000
          /* workaround for a Qt4 bug, which makes translucent windows
             always appear at the top left corner (taken from QtCurve) */
          bool was_visible = widget->isVisible();
          bool moved = widget->testAttribute(Qt::WA_Moved);
          if (was_visible) widget->hide();
#endif

          widget->setAttribute(Qt::WA_TranslucentBackground);

#if QT_VERSION < 0x050000
          if (!moved) widget->setAttribute(Qt::WA_Moved, false);
          if (was_visible) widget->show();
#endif

          /* enable blurring... */
          if (blurHelper
              /* ... but not for Konsole's dialogs if
                 blurring isn't enabled for translucent windows */
              && tspec.blurring)
          {
            blurHelper->registerWidget(widget);
          }
          /* enable blurring for Konsole... */
          else if (isKonsole// && hspec.blur_konsole
                   /* ... but only for its main window */
                   //&& !widget->testAttribute(Qt::WA_NoSystemBackground)
                   && (widget->windowFlags() & Qt::WindowType_Mask) == Qt::Window)
          {
#if defined Q_WS_X11 || defined Q_OS_LINUX
            if (!blurHelper)
              blurHelper = new BlurHelper(this);
#endif
            if (blurHelper)
              blurHelper->registerWidget(widget);
          }

          widget->removeEventFilter(this);
          widget->installEventFilter(this);
          translucentTopWidgets.insert(widget);
          connect(widget, SIGNAL(destroyed(QObject*)),
                  SLOT(undoTranslucency(QObject*)));
        }
        break;
      }
      default: break;
    }

    if (isDolphin
        && qobject_cast<QAbstractScrollArea*>(getParent(widget,2))
        && !qobject_cast<QAbstractScrollArea*>(getParent(widget,3)))
    {
      /* Dolphin sets the background of its KItemListContainer's viewport
         to KColorScheme::View (-> kde-baseapps -> dolphinview.cpp).
         We force our base color here. */
      const color_spec cspec = settings->getColorSpec();
      QColor col = cspec.baseColor;
      if (col.isValid())
      {
        QPalette palette = widget->palette();
        palette.setColor(widget->backgroundRole(), col);
        widget->setPalette(palette);
      }
      /* hack Dolphin's view */
      if (hspec.transparent_dolphin_view && widget->autoFillBackground())
        widget->setAutoFillBackground(false);
    }

    // -> ktitlewidget.cpp
    if (widget->inherits("KTitleWidget"))
    {
      if (hspec.transparent_ktitle_label)
      {
        /*QPalette palette = widget->palette();
        palette.setColor(QPalette::Base,QColor(Qt::transparent));
        widget->setPalette(palette);*/
        if (QFrame *titleFrame = widget->findChild<QFrame *>())
          titleFrame->setAutoFillBackground(false);
      }
    }

    /*if (widget->autoFillBackground()
        && widget->parentWidget()
        && widget->parentWidget()->objectName() == "qt_scrollarea_viewport"
        && qobject_cast<QAbstractScrollArea*>(getParent(widget,2)))
    {
      widget->parentWidget()->setAutoFillBackground(false);
      widget->setAutoFillBackground(false);
    }*/

    if (qobject_cast<QMdiArea*>(widget))
      widget->setAutoFillBackground(true);
    else if (qobject_cast<QProgressBar*>(widget))
      widget->installEventFilter(this);
    /* without this, transparent backgrounds
       couldn't be used for scrollbar grooves */
    else if (qobject_cast<QScrollBar*>(widget))
      widget->setAttribute(Qt::WA_OpaquePaintEvent, false);
    /* remove ugly flat backgrounds when the window backround is styled */
    else if (QAbstractScrollArea *sa = qobject_cast<QAbstractScrollArea*>(widget))
    {
      if (/*sa->frameShape() == QFrame::NoFrame &&*/ // Krita and digiKam aren't happy with this
          sa->backgroundRole() == QPalette::Window
          || sa->backgroundRole() == QPalette::Button) // inside toolbox
      {
        QWidget *vp = sa->viewport();
        if (vp && (vp->backgroundRole() == QPalette::Window
                   || vp->backgroundRole() == QPalette::Button))
        {
          vp->setAutoFillBackground(false);
          foreach (QWidget *child, vp->findChildren<QWidget*>())
          {
            if (child->parent() == vp && (child->backgroundRole() == QPalette::Window
                                          || child->backgroundRole() == QPalette::Button))
              child->setAutoFillBackground(false);
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
      widget->parentWidget()->setAutoFillBackground(false);
    }

    if (!isLibreoffice // not required
        && !subApp
        && (qobject_cast<QMenu*>(widget)
            // systemsettings blurs tooltips in a wrong way
            || (widget->inherits("QTipLabel") && !isSystemSettings)))
    {
      const theme_spec tspec = settings->getThemeSpec();
      if (tspec.composite && !widget->testAttribute(Qt::WA_X11NetWmWindowTypeMenu))
        widget->setAttribute(Qt::WA_TranslucentBackground);
      /*else // round off the corners
      {
        widget->setAutoFillBackground(false);
        QPixmap pixmap = QPixmap::grabWidget(widget);
        QBitmap bm = pixmap.createHeuristicMask();
        pixmap.setMask(bm);
        QImage img = pixmap.toImage();
        if (qAlpha(img.pixel(0,0)) == 0)
          widget->setMask(bm);
      }*/
    }
  }
}

#if QT_VERSION < 0x040806
static QString getAppName(const QString &file)
{
  QString appName(file);
  int slashPos(appName.lastIndexOf('/'));
  if(slashPos != -1)
    appName.remove(0, slashPos+1);
  return appName;
}
#endif

void Kvantum::polish(QApplication *app)
{
#if QT_VERSION < 0x040806
  /* use this old-fashioned method to get the app name
     because, apparently, QApplication::applicationName()
     doesn't work correctly with all versions of Qt4 */
  QString appName = getAppName(app->argv()[0]);
#else
  const QString appName = app->applicationName();
#endif
  if (appName == "Qt-subapplication")
    subApp = true;
  else if (appName == "dolphin")
    isDolphin = true;
  else if (appName == "konsole")
    isKonsole = true;
  else if (appName == "soffice.bin")
    isLibreoffice = true;
  else if (appName == "plasma" || appName.startsWith("plasma-")
           || appName == "kded4") // this is for the infamous appmenu
    isPlasma = true;
  else if (appName == "systemsettings")
    isSystemSettings = true;

  const theme_spec tspec = settings->getThemeSpec();
  if (tspec.opaque.contains (appName))
    isOpaque = true;

  /* general colors
     FIXME Is this needed? Can't polish(QPalette&) alone do the job?
     The documentation for QApplication::setPalette() is ambiguous
     but, at least outside KDE and with Qt4, it's sometimes needed. */
  QPalette palette = app->palette();
  polish(palette);
  app->setPalette(palette);

  QCommonStyle::polish(app);
  if (itsShortcutHandler)
  {
    app->removeEventFilter(itsShortcutHandler);
    app->installEventFilter(itsShortcutHandler);
  }
}

void Kvantum::polish(QPalette &palette)
{
    const color_spec cspec = settings->getColorSpec();

    /* background colors */
    QColor col = cspec.windowColor;
    if (col.isValid())
      palette.setColor(QPalette::Window,col);
    col = cspec.baseColor;
    if (col.isValid())
      palette.setColor(QPalette::Base,col);
    col = cspec.altBaseColor;
    if (col.isValid())
      palette.setColor(QPalette::AlternateBase,col);
    col = cspec.buttonColor;
    if (col.isValid())
      palette.setColor(QPalette::Button,col);
    col = cspec.lightColor;

    if (col.isValid())
      palette.setColor(QPalette::Light,col);
    col = cspec.midLightColor;
    if (col.isValid())
      palette.setColor(QPalette::Midlight,col);
    col = cspec.darkColor;
    if (col.isValid())
      palette.setColor(QPalette::Dark,col);
    col = cspec.midColor;
    if (col.isValid())
      palette.setColor(QPalette::Mid,col);
    col = cspec.shadowColor;
    if (col.isValid())
      palette.setColor(QPalette::Shadow,col);

    col = cspec.highlightColor;
    if (col.isValid())
      palette.setColor(QPalette::Active,QPalette::Highlight,col);
    col = cspec.inactiveHighlightColor;
    if (col.isValid())
      palette.setColor(QPalette::Inactive,QPalette::Highlight,col);

    /* text colors */
    col = cspec.textColor;
    if (col.isValid())
    {
      palette.setColor(QPalette::Active,QPalette::Text,col);
      palette.setColor(QPalette::Inactive,QPalette::Text,col);
    }
    col = cspec.windowTextColor;
    if (col.isValid())
    {
      palette.setColor(QPalette::Active,QPalette::WindowText,col);
      palette.setColor(QPalette::Inactive,QPalette::WindowText,col);
    }
    col = cspec.buttonTextColor;
    if (col.isValid())
    {
      palette.setColor(QPalette::Active,QPalette::ButtonText,col);
      palette.setColor(QPalette::Inactive,QPalette::ButtonText,col);
    }
    col = cspec.tooltipTextColor;
    if (col.isValid())
      palette.setColor(QPalette::ToolTipText,col);
    col = cspec.highlightTextColor;
    if (col.isValid())
      palette.setColor(QPalette::HighlightedText,col);
    col = cspec.linkColor;
    if (col.isValid())
      palette.setColor(QPalette::Link,col);
    col = cspec.linkVisitedColor;
    if (col.isValid())
      palette.setColor(QPalette::LinkVisited,col);

    /* disabled text */
    col = cspec.disabledTextColor;
    if (col.isValid())
    {
      palette.setColor(QPalette::Disabled,QPalette::Text,col);
      palette.setColor(QPalette::Disabled,QPalette::WindowText,col);
      palette.setColor(QPalette::Disabled,QPalette::ButtonText,col);
    }
}

void Kvantum::unpolish(QWidget *widget)
{
  if (widget)
  {
    if (itsWindowManager)
      itsWindowManager->unregisterWidget(widget);

    /*widget->setAttribute(Qt::WA_Hover, false);*/

    switch (widget->windowFlags() & Qt::WindowType_Mask) {
      case Qt::Window:
      case Qt::Dialog: {
        if (blurHelper)
          blurHelper->unregisterWidget(widget);
        if (translucentTopWidgets.contains(widget))
        {
          widget->removeEventFilter(this);
          widget->setAttribute(Qt::WA_NoSystemBackground, false);
          widget->setAttribute(Qt::WA_TranslucentBackground, false);
        }
        widget->setAttribute(Qt::WA_StyledBackground, false); // FIXME is this needed?
        break;
      }
      default: break;
    }

    if (qobject_cast<QProgressBar*>(widget))
      progressbars.remove(widget);
    else if (qobject_cast<QToolBox*>(widget))
      widget->setBackgroundRole(QPalette::Button);

    if (!isLibreoffice // not required
        && !subApp
        && (qobject_cast<QMenu*>(widget)
            || (widget->inherits("QTipLabel") && !isSystemSettings)))
    {
      const theme_spec tspec = settings->getThemeSpec();
      if (tspec.composite && !widget->testAttribute(Qt::WA_X11NetWmWindowTypeMenu))
        widget->setAttribute(Qt::WA_TranslucentBackground, false);
      /*else
        widget->clearMask();*/
    }
  }
}

void Kvantum::unpolish(QApplication *app)
{
  if (itsShortcutHandler)
    app->removeEventFilter(itsShortcutHandler);
  QCommonStyle::unpolish(app);
}

void Kvantum::drawBg(QPainter *p, const QWidget *widget) const
{
  if (widget->palette().color(widget->backgroundRole()) == Qt::transparent)
    return; // Plasma FIXME needed?
  QRect bgndRect(widget->rect());
  interior_spec ispec = getInteriorSpec("Window");
  frame_spec fspec;
  default_frame_spec(fspec);

  QString suffix = "-normal";
  if (!widget->isActiveWindow())
    suffix = "-normal-inactive";

  p->setClipRegion(bgndRect, Qt::IntersectClip);
  renderInterior(p,bgndRect,fspec,ispec,ispec.element+suffix);
}

bool Kvantum::eventFilter(QObject *o, QEvent *e)
{
  QWidget *w = qobject_cast<QWidget*>(o);
  //const theme_spec tspec = settings->getThemeSpec();

  switch (e->type()) {
  case QEvent::Paint:
    if (w && w->isWindow()
        && w->testAttribute(Qt::WA_StyledBackground)
        && w->testAttribute(Qt::WA_TranslucentBackground)
        && !isPlasma && !isOpaque && !subApp && !isLibreoffice
        /*&& tspec.translucent_windows*/ // this could have weird effects with style or settings change
       )
    {
      switch (w->windowFlags() & Qt::WindowType_Mask) {
        case Qt::Window:
        case Qt::Dialog: {
          QPainter p(w);
          p.setClipRegion(static_cast<QPaintEvent*>(e)->region());
          drawBg(&p,w);
          break;
        }
        default: break;
      }
    }
    break;

  case QEvent::Show:
    if (w)
    {
      if (qobject_cast<QProgressBar *>(o))
      {
        progressbars.insert(w, 0);
        if (!progresstimer->isActive())
          progresstimer->start(50);
      }
    }
    break;

  case QEvent::Hide:
  case QEvent::Destroy:
    if (w)
    {
      progressbars.remove(w);
      if (progressbars.size() == 0)
        progresstimer->stop();
    }
    break;

  default:
    return false;
  }

  return false;
}

enum toolbarButtonKind
{
  tbLeft = -1,
  tbMiddle,
  tbRight,
  tbAlone
};

static bool hasArrow (const QToolButton *tb, const QStyleOptionToolButton *opt)
{
  if (!tb) return false;
  if (tb->popupMode() == QToolButton::MenuButtonPopup
      || ((tb->popupMode() == QToolButton::InstantPopup
           || tb->popupMode() == QToolButton::DelayedPopup)
          && opt && (opt->features & QStyleOptionToolButton::HasMenu)))
  {
    return true;
  }
  return false;
}

static int whichToolbarButton (const QToolButton *tb, const QStyleOptionToolButton *opt, const QToolBar *toolBar)
{
  int res = tbAlone;

  if (!tb || !toolBar)
    return res;

  QRect g = tb->geometry();

  if (toolBar->orientation() == Qt::Horizontal)
  {
    const QToolButton *left = qobject_cast<const QToolButton *>(toolBar->childAt (g.x()-1, g.y()));
    const QToolButton *right =  qobject_cast<const QToolButton *>(toolBar->childAt (g.x()+g.width()+1, g.y()));

    if (left)
    {
      if (right)
        res = tbMiddle;
      else
        res = tbRight;
    }
    else
    {
      if (right)
        res = tbLeft;
    }
  }
  else
  {
    if (hasArrow (tb, opt))
      return res;

    const QToolButton *top = qobject_cast<const QToolButton *>(toolBar->childAt (g.x(), g.y()-1));
    const QToolButton *bottom =  qobject_cast<const QToolButton *>(toolBar->childAt (g.x(), g.y()+g.height()+1));

    if (top && !hasArrow (top, opt))
    {
      if (bottom && !hasArrow (bottom, opt))
        res = tbMiddle;
      else
        res = tbRight;
    }
    else
    {
      if (bottom && !hasArrow (bottom, opt))
        res = tbLeft;
    }
  }

  return res;
}

void Kvantum::drawPrimitive(PrimitiveElement element,
                            const QStyleOption *option,
                            QPainter *painter,
                            const QWidget *widget) const
{
  int x,y,h,w;
  option->rect.getRect(&x,&y,&w,&h);
  QString status =
        (option->state & State_Enabled) ?
          (option->state & State_On) ? "toggled" :
          (option->state & State_Sunken) ? "pressed" :
          (option->state & State_Selected) ? "toggled" :
          (option->state & State_MouseOver) ? "focused" : "normal"
        : "disabled";

  bool isInactive = false;
  if (widget && !widget->isActiveWindow())
  {
    isInactive = true;
    status.append(QString("-inactive"));
  }

  switch(element) {
    case PE_Widget : {
      // only for windows and dialogs
      if (!widget || !widget->isWindow())
        break;

      interior_spec ispec = getInteriorSpec("Window");
      frame_spec fspec;
      default_frame_spec(fspec);

      QString suffix = "-normal";
      if (isInactive)
        suffix = "-normal-inactive";
      renderInterior(painter,option->rect,fspec,ispec,ispec.element+suffix);

      break;
    }

    case PE_FrameTabBarBase :
    case PE_FrameDockWidget :
    case PE_FrameStatusBar : {
      QString group = "TabBarFrame";
      if (element == PE_FrameDockWidget)
        group = "Dock";
      else if (element == PE_FrameStatusBar)
        group = "StatusBar";

      const frame_spec fspec = getFrameSpec(group);
      const interior_spec ispec = getInteriorSpec(group);

      if (status.startsWith("disabled"))
      {
        status.replace(QString("disabled"),QString("normal"));
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+status);
      renderFrame(painter,option->rect,fspec,fspec.element+"-"+status);
      if (!(option->state & State_Enabled))
        painter->restore();

      break;
    }

    case PE_PanelButtonCommand : {
      const QString group = "PanelButtonCommand";

      frame_spec fspec = getFrameSpec(group);
      const interior_spec ispec = getInteriorSpec(group);

      if (status.startsWith("disabled"))
      {
        status = "normal";
        if (option->state & State_On)
          status = "toggled";
        if (isInactive)
          status.append(QString("-inactive"));
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+status);
      renderFrame(painter,option->rect,fspec,fspec.element+"-"+status);
      if (!(option->state & State_Enabled))
        painter->restore();

      break;
    }

    case PE_PanelButtonTool : {
      const QString group = "PanelButtonTool";

      frame_spec fspec = getFrameSpec(group);
      const interior_spec ispec = getInteriorSpec(group);
      const indicator_spec dspec = getIndicatorSpec(group);
      label_spec lspec = getLabelSpec(group);

      // -> CE_ToolButtonLabel
      if (qobject_cast<const QAbstractItemView*>(getParent(widget,2)))
      {
        fspec.left = qMin(fspec.left,3);
        fspec.right = qMin(fspec.right,3);
        fspec.top = qMin(fspec.top,3);
        fspec.bottom = qMin(fspec.bottom,3);

        lspec.left = qMin(lspec.left,2);
        lspec.right = qMin(lspec.right,2);
        lspec.top = qMin(lspec.top,2);
        lspec.bottom = qMin(lspec.bottom,2);
      }

      const QToolButton *tb = qobject_cast<const QToolButton *>(widget);
      const QStyleOptionToolButton *opt = qstyleoption_cast<const QStyleOptionToolButton *>(option);

      // -> CE_ToolButtonLabel
      if (opt && opt->toolButtonStyle == Qt::ToolButtonTextOnly)
      {
        fspec.left = fspec.right = qMin(fspec.left,fspec.right);
        fspec.top = fspec.bottom = qMin(fspec.top,fspec.bottom);
        lspec.left = lspec.right = qMin(lspec.left,lspec.right);
        lspec.top = lspec.bottom = qMin(lspec.top,lspec.bottom);
      }

      QRect r = option->rect;

      /* this is just for tabbar scroll buttons */
      if (qobject_cast<const QTabBar*>(getParent(widget,1)))
        painter->fillRect(option->rect, option->palette.brush(QPalette::Window));

      bool drawRaised = false;
      if (status.startsWith("disabled"))
      {
        status = "normal";
        if (option->state & State_On)
          status = "toggled";
        if (isInactive)
          status.append(QString("-inactive"));
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      if (tb)
      {
        bool withArrow = hasArrow (tb, opt);
        bool isHorizontal = true;
        const theme_spec tspec = settings->getThemeSpec();
        if (tspec.group_toolbar_buttons)
        {
          if (const QToolBar *toolBar = qobject_cast<const QToolBar *>(tb->parentWidget()))
          {
            drawRaised = true;

            /* the disabled state is ugly
               for grouped tool buttons */
            if (!(option->state & State_Enabled))
              painter->restore();

            if (toolBar->orientation() == Qt::Vertical)
              isHorizontal = false;

            if (!isHorizontal && !withArrow)
            {
              r.setRect(0, 0, h, w);
              painter->save();
              QTransform m;
              m.translate(0, w);
              m.scale(1,-1);
              m.translate(0, w);
              m.rotate(-90);
              painter->setTransform(m, true);
            }

            int kind = whichToolbarButton (tb, opt, toolBar);
            if (kind != 2)
            {
              fspec.hasCapsule = true;
              fspec.capsuleV = 2;
              fspec.capsuleH = kind;
            }
          }
        }

        QString pbStatus = status;
        bool rtl(option->direction == Qt::RightToLeft);
        if (tb->popupMode() == QToolButton::MenuButtonPopup)
        {
          // merge with drop down button
          if (!fspec.hasCapsule)
          {
            fspec.capsuleV = 2;
            fspec.hasCapsule = true;
            fspec.capsuleH = rtl ? 1 : -1;
          }
          else if (fspec.capsuleH == 1)
            fspec.capsuleH = 0;
          else if (fspec.capsuleH == 2)
            fspec.capsuleH = rtl ? 1 : -1;
          // don't press the button if only its arrow is pressed
          pbStatus = (option->state & State_Enabled) ?
                       (option->state & State_Sunken) && tb->isDown() ? "pressed" :
                         (option->state & State_Selected) && tb->isDown() ? "toggled" :
                           (option->state & State_MouseOver) ? "focused" : "normal"
                     : "disabled";
          // don't focus the button if only its arrow is focused
          if (pbStatus == "focused"
              && opt && opt->activeSubControls == QStyle::SC_ToolButtonMenu)
          {
            pbStatus = "normal";
          }
          if (pbStatus == "disabled")
          {
            pbStatus = "normal";
            if (option->state & State_On)
              pbStatus = "toggled";
          }
          if (isInactive)
            pbStatus.append(QString("-inactive"));
        }
        else if ((tb->popupMode() == QToolButton::InstantPopup
                  || tb->popupMode() == QToolButton::DelayedPopup)
                 && opt && (opt->features & QStyleOptionToolButton::HasMenu))
        {
          // enlarge to put drop down arrow (-> SC_ToolButton)
          r.adjust(rtl ? -lspec.tispace-dspec.size-fspec.left-pixelMetric(PM_HeaderMargin) : 0,
                   0,
                   rtl ? 0 : lspec.tispace+dspec.size+fspec.right+pixelMetric(PM_HeaderMargin),
                   0);
        }

        if (!tb->autoRaise() || (!status.startsWith("normal") && !status.startsWith("disabled")) || drawRaised)
          renderInterior(painter,r,fspec,ispec,ispec.element+"-"+pbStatus);

        if (!isHorizontal && !withArrow)
          painter->restore();
      }
      else if (!(option->state & State_AutoRaise)
               || (!status.startsWith("normal") && !status.startsWith("disabled")))
      {
        renderInterior(painter,r,fspec,ispec,ispec.element+"-"+status);
      }

      if (!(option->state & State_Enabled) && !drawRaised)
        painter->restore();

      break;
    }

    case PE_FrameButtonTool : {
      const QString group = "PanelButtonTool";
      frame_spec fspec = getFrameSpec(group);
      const indicator_spec dspec = getIndicatorSpec(group);
      label_spec lspec = getLabelSpec(group);

      // -> CE_ToolButtonLabel
      if (qobject_cast<const QAbstractItemView*>(getParent(widget,2)))
      {
        fspec.left = qMin(fspec.left,3);
        fspec.right = qMin(fspec.right,3);
        fspec.top = qMin(fspec.top,3);
        fspec.bottom = qMin(fspec.bottom,3);

        lspec.left = qMin(lspec.left,2);
        lspec.right = qMin(lspec.right,2);
        lspec.top = qMin(lspec.top,2);
        lspec.bottom = qMin(lspec.bottom,2);
      }

      const QToolButton *tb = qobject_cast<const QToolButton *>(widget);
      const QStyleOptionToolButton *opt = qstyleoption_cast<const QStyleOptionToolButton *>(option);

      // -> CE_ToolButtonLabel
      if (opt && opt->toolButtonStyle == Qt::ToolButtonTextOnly)
      {
        fspec.left = fspec.right = qMin(fspec.left,fspec.right);
        fspec.top = fspec.bottom = qMin(fspec.top,fspec.bottom);
        lspec.left = lspec.right = qMin(lspec.left,lspec.right);
        lspec.top = lspec.bottom = qMin(lspec.top,lspec.bottom);
      }

      QRect r = option->rect;

      bool drawRaised = false;
      if (status.startsWith("disabled"))
      {
        status = "normal";
        if (option->state & State_On)
          status = "toggled";
        if (isInactive)
          status.append(QString("-inactive"));
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      if (tb)
      {
        bool withArrow = hasArrow (tb, opt);
        bool isHorizontal = true;
        const theme_spec tspec = settings->getThemeSpec();
        if (tspec.group_toolbar_buttons)
        {
          if (const QToolBar *toolBar = qobject_cast<const QToolBar *>(tb->parentWidget()))
          {
            drawRaised = true;

            /* the disabled state is ugly
               for grouped tool buttons */
            if (!(option->state & State_Enabled))
              painter->restore();

            if (toolBar->orientation() == Qt::Vertical)
              isHorizontal = false;

            if (!isHorizontal && !withArrow)
            {
              r.setRect(0, 0, h, w);
              painter->save();
              QTransform m;
              m.translate(0, w);
              m.scale(1,-1);
              m.translate(0, w);
              m.rotate(-90);
              painter->setTransform(m, true);
            }

            int kind = whichToolbarButton (tb, opt, toolBar);
            if (kind != 2)
            {
              fspec.hasCapsule = true;
              fspec.capsuleV = 2;
              fspec.capsuleH = kind;
            }
          }
        }

        QString fbStatus = status;
        bool rtl(option->direction == Qt::RightToLeft);
        if (tb->popupMode() == QToolButton::MenuButtonPopup)
        {
          // merge with drop down button
          if (!fspec.hasCapsule)
          {
            fspec.capsuleV = 2;
            fspec.hasCapsule = true;
            fspec.capsuleH = rtl ? 1 : -1;
          }
          else if (fspec.capsuleH == 1)
            fspec.capsuleH = 0;
          else if (fspec.capsuleH == 2)
            fspec.capsuleH = rtl ? 1 : -1;
          // don't press the button if only its arrow is pressed
          fbStatus = (option->state & State_Enabled) ?
                       (option->state & State_Sunken) && tb->isDown() ? "pressed" :
                         (option->state & State_Selected) && tb->isDown() ? "toggled" :
                           (option->state & State_MouseOver) ? "focused" : "normal"
                     : "disabled";
          // don't focus the button if only its arrow is focused
          if (fbStatus == "focused"
              && opt && opt->activeSubControls == QStyle::SC_ToolButtonMenu)
          {
            fbStatus = "normal";
          }
          if (fbStatus == "disabled")
          {
            fbStatus = "normal";
            if (option->state & State_On)
              fbStatus = "toggled";
          }
          if (isInactive)
            fbStatus.append(QString("-inactive"));
        }
        else if ((tb->popupMode() == QToolButton::InstantPopup
                  || tb->popupMode() == QToolButton::DelayedPopup)
                 && (opt && (opt->features & QStyleOptionToolButton::HasMenu)))
        {
          // enlarge to put drop down arrow (-> SC_ToolButton)
          r.adjust(rtl ? -lspec.tispace-dspec.size-fspec.left-pixelMetric(PM_HeaderMargin) : 0,
                   0,
                   rtl ? 0 : lspec.tispace+dspec.size+fspec.right+pixelMetric(PM_HeaderMargin),
                   0);
        }

        if (!tb->autoRaise() || (!status.startsWith("normal") && !status.startsWith("disabled")) || drawRaised)
          renderFrame(painter,r,fspec,fspec.element+"-"+fbStatus);

        if (!isHorizontal && !withArrow)
          painter->restore();
      }
      else if (!(option->state & State_AutoRaise)
               || (!status.startsWith("normal") && !status.startsWith("disabled")))
      {
        renderFrame(painter,r,fspec,fspec.element+"-"+status);
      }

      if (!(option->state & State_Enabled) && !drawRaised)
        painter->restore();

      break;
    }

    case PE_IndicatorRadioButton : {
      /* make exception for menuitems */
      /*if (qstyleoption_cast<const QStyleOptionMenuItem *>(option))
      {
        frame_spec fspec;
        default_frame_spec(fspec);
        interior_spec ispec;
        default_interior_spec(ispec);
        const indicator_spec dspec = getIndicatorSpec("MenuItem");
        
        if (option->state & State_Enabled)
        {
          if (option->state & State_MouseOver)
          {
            if (option->state & State_On)
              renderInterior(painter,option->rect,fspec,ispec,
                             dspec.element+"-radio-checked-focused");
            else
              renderInterior(painter,option->rect,fspec,ispec,
                             dspec.element+"-radio-focused");
          }
          else
          {
            if (option->state & State_On)
              renderInterior(painter,option->rect,fspec,ispec,
                             dspec.element+"-radio-checked-normal");
            else
              renderInterior(painter,option->rect,fspec,ispec,
                             dspec.element+"-radio-normal");
          }
        }
        else
        {
          if (status == "disabled")
          {
            painter->save();
            painter->setOpacity(DISABLED_OPACITY);
          }
          if (option->state & State_On)
            renderInterior(painter,option->rect,fspec,ispec,
                           dspec.element+"-radio-checked-normal");
          else
            renderInterior(painter,option->rect,fspec,ispec,
                           dspec.element+"-radio-normal");
          if (!(option->state & State_Enabled))
            painter->restore();
        }

        break;
      }*/

      const QString group = "RadioButton";
      const frame_spec fspec = getFrameSpec(group);
      const interior_spec ispec = getInteriorSpec(group);

      if (option->state & State_Enabled)
      {
        QString suffix;
        if (option->state & State_MouseOver)
        {
          if (option->state & State_On)
            suffix = "-checked-focused";
          else
            suffix = "-focused";
        }
        else
        {
          if (option->state & State_On)
            suffix = "-checked-normal";
          else
            suffix = "-normal";
        }
        if (isInactive)
          suffix.append(QString("-inactive"));
        renderFrame(painter,option->rect,fspec,fspec.element+suffix);
        renderInterior(painter,option->rect,fspec,ispec,ispec.element+suffix);
      }
      else
      {
        if (status.startsWith("disabled"))
        {
          painter->save();
          painter->setOpacity(DISABLED_OPACITY);
        }
        QString suffix;
        if (option->state & State_On)
          suffix = "-checked-normal";
        else
          suffix = "-normal";
        if (isInactive)
          suffix.append(QString("-inactive"));
        renderFrame(painter,option->rect,fspec,fspec.element+suffix);
        renderInterior(painter,option->rect,fspec,ispec,ispec.element+suffix);
        if (!(option->state & State_Enabled))
          painter->restore();
      }

      break;
    }

    case PE_IndicatorCheckBox : {
      /* make exception for menuitems */
      /*if (qstyleoption_cast<const QStyleOptionMenuItem *>(option))
      {
        frame_spec fspec;
        default_frame_spec(fspec);
        interior_spec ispec;
        default_interior_spec(ispec);
        const indicator_spec dspec = getIndicatorSpec("MenuItem");
        
        if (option->state & State_Enabled)
        {
          if (option->state & State_MouseOver)
          {
            if (option->state & State_On)
              renderInterior(painter,option->rect,fspec,ispec,
                             dspec.element+"-checkbox-checked-focused");
            else if (option->state & State_NoChange)
              renderInterior(painter,option->rect,fspec,ispec,
                             dspec.element+"-checkbox-tristate-focused");
            else
              renderInterior(painter,option->rect,fspec,ispec,
                             dspec.element+"-checkbox-focused");
          }
          else
          {
            if (option->state & State_On)
              renderInterior(painter,option->rect,fspec,ispec,
                             dspec.element+"-checkbox-checked-normal");
            else if (option->state & State_NoChange)
              renderInterior(painter,option->rect,fspec,ispec,
                             dspec.element+"-checkbox-tristate-normal");
            else
              renderInterior(painter,option->rect,fspec,ispec,
                             dspec.element+"-checkbox-normal");
          }
        }
        else
        {
          if (status == "disabled")
          {
            painter->save();
            painter->setOpacity(DISABLED_OPACITY);
          }
          if (option->state & State_On)
            renderInterior(painter,option->rect,fspec,ispec,
                           dspec.element+"-checkbox-checked-normal");
          else if (option->state & State_NoChange)
              renderInterior(painter,option->rect,fspec,ispec,
                             dspec.element+"-checkbox-tristate-normal");
          else
            renderInterior(painter,option->rect,fspec,ispec,
                           dspec.element+"-checkbox-normal");
          if (!(option->state & State_Enabled))
            painter->restore();
        }

        break;
      }*/

      const QString group = "CheckBox";
      const frame_spec fspec = getFrameSpec(group);
      const interior_spec ispec = getInteriorSpec(group);

      if (option->state & State_Enabled)
      {
        QString suffix;
        if (option->state & State_MouseOver)
        {
          if (option->state & State_On)
            suffix = "-checked-focused";
          else if (option->state & State_NoChange)
            suffix = "-tristate-focused";
          else
            suffix = "-focused";
        }
        else
        {
          if (option->state & State_On)
            suffix = "-checked-normal";
          else if (option->state & State_NoChange)
            suffix = "-tristate-normal";
          else
            suffix = "-normal";
        }
        if (isInactive)
          suffix.append(QString("-inactive"));
        renderFrame(painter,option->rect,fspec,fspec.element+suffix);
        renderInterior(painter,option->rect,fspec,ispec,ispec.element+suffix);
      }
      else
      {
        if (status.startsWith("disabled"))
        {
          painter->save();
          painter->setOpacity(DISABLED_OPACITY);
        }
        QString suffix;
        if (option->state & State_On)
          suffix = "-checked-normal";
        else if (option->state & State_NoChange)
          suffix = "-tristate-normal";
        else
          suffix = "-normal";
        if (isInactive)
          suffix.append(QString("-inactive"));
        renderFrame(painter,option->rect,fspec,fspec.element+suffix);
        renderInterior(painter,option->rect,fspec,ispec,ispec.element+suffix);
        if (!(option->state & State_Enabled))
          painter->restore();
      }

      break;
    }

    case PE_FrameFocusRect : {
      const QString group = "Focus";

      const frame_spec fspec = getFrameSpec(group);
      const interior_spec ispec = getInteriorSpec(group);

      renderFrame(painter,option->rect,fspec,fspec.element);
      renderInterior(painter,option->rect,fspec,ispec,ispec.element);

      break;
    }

    case PE_IndicatorBranch : {
      QString group = "TreeExpander";

      frame_spec fspec = getFrameSpec(group);
      interior_spec ispec = getInteriorSpec(group);
      indicator_spec dspec = getIndicatorSpec(group);

      if (status.startsWith("disabled"))
      {
        status.replace(QString("disabled"),QString("normal"));
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      renderFrame(painter,option->rect,fspec,fspec.element+"-"+status);
      renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+status);
      if (!(option->state & State_Enabled))
      {
        painter->restore();
        status = "disabled";
      }

      if (option->state & State_Children)
      {
        QString eStatus = "normal";
        if (status.startsWith("disabled"))
          eStatus = "disabled";
        else if (option->state & State_MouseOver)
          eStatus = "focused";
        else if (status.startsWith("toggled") || status.startsWith("pressed"))
          eStatus = "pressed";
        if (isInactive)
          eStatus.append(QString("-inactive"));
        if (option->state & State_Open)
          renderIndicator(painter,option->rect,fspec,dspec,dspec.element+"-minus-"+eStatus);
        else
          renderIndicator(painter,option->rect,fspec,dspec,dspec.element+"-plus-"+eStatus);
      }

      break;
    }

    /*
       We have two options here:

         (1) Using both PE_PanelMenu and PE_FrameMenu
             and setting PM_MenuPanelWidth properly; or
         (2) Using only PE_PanelMenu and setting
             PM_MenuHMargin and PM_MenuVMargin properly.

       The first method sometimes results in frameless menus,
       especially with context menus of subclassed lineedits,
       and can also make submenus overlap too much with their
       parent menus.
    */
    case PE_FrameMenu : {return;}
    case PE_PanelMenu : {
      /* At least toolbars may also use this, so continue
         only if it's really a menu. LibreOffice's menuitems 
         would have no background without this either. */
      if (!qobject_cast<const QMenu*>(widget)
          || isLibreoffice) // really not needed
        break;

      const QString group = "Menu";
      frame_spec fspec = getFrameSpec(group);
      const interior_spec ispec = getInteriorSpec(group);

      /* no reason to have different sizes for
         different parts of the menu frame */
      fspec.left = fspec.right = fspec.top = fspec.bottom = pixelMetric(PM_MenuHMargin,option,widget);

      const theme_spec tspec = settings->getThemeSpec();
      if (tspec.menu_shadow_depth > 0 && !subApp
          && widget && !widget->testAttribute(Qt::WA_X11NetWmWindowTypeMenu)) // not torn off
        renderFrame(painter,option->rect,fspec,fspec.element+"-shadow");
      else
        renderFrame(painter,option->rect,fspec,fspec.element+"-normal");

      renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-normal");

      break;
    }

    // SH_TitleBar_NoBorder is set to be true
    /*case PE_FrameWindow : {
      const QString group = "WindowFrame";

      const frame_spec fspec = getFrameSpec(group);

      renderFrame(painter,option->rect,fspec,fspec.element+"-"+status);

      break;
    }*/

    //case PE_FrameButtonBevel :
    case PE_Frame : {
      if (widget && qobject_cast<const QAbstractScrollArea*>(widget))
      {
        if (isDolphin)
        {
          if (QWidget *pw = widget->parentWidget())
          {
            const hacks_spec hspec = settings->getHacksSpec();
            if (hspec.transparent_dolphin_view
                // not renaming area
                && !qobject_cast<QAbstractScrollArea*>(pw)
                // only Dolphin's view
                && QString(pw->metaObject()->className()).startsWith("Dolphin"))
            {
              break;
            }
          }
        }

        const QString group = "GenericFrame";
        const frame_spec fspec = getFrameSpec(group);
        const interior_spec ispec = getInteriorSpec(group);

        if (status.startsWith("disabled"))
        {
          painter->save();
          painter->setOpacity(DISABLED_OPACITY);
        }
        QString suffix = "-normal";
        if (isInactive)
          suffix = "-normal-inactive";
        renderFrame(painter,option->rect,fspec,fspec.element+suffix);
        renderInterior(painter,option->rect,fspec,ispec,ispec.element+suffix);
        if (!(option->state & State_Enabled))
          painter->restore();
      }

      break;
    }

    case PE_FrameGroupBox : {
      const QString group = "GroupBox";
      const frame_spec fspec = getFrameSpec(group);
      const interior_spec ispec = getInteriorSpec(group);

      if (status.startsWith("disabled"))
      {
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      QString suffix = "-normal";
      if (isInactive)
        suffix = "-normal-inactive";
      renderFrame(painter,option->rect,fspec,fspec.element+suffix);
      renderInterior(painter,option->rect,fspec,ispec,ispec.element+suffix);
      if (!(option->state & State_Enabled))
        painter->restore();

      break;
    }

    case PE_FrameTabWidget : {
      const QString group = "TabFrame";
      const frame_spec fspec = getFrameSpec(group);
      const interior_spec ispec = getInteriorSpec(group);

      frame_spec fspec1 = fspec;
      int d = 0;
      int l = 0;
      int tp = 0;

      const theme_spec tspec = settings->getThemeSpec();
      if (tspec.attach_active_tab)
      {
        const QStyleOptionTabWidgetFrameV2 *twf =
            qstyleoption_cast<const QStyleOptionTabWidgetFrameV2*>(option);
        const QTabWidget *tw((const QTabWidget*)widget);
        if (tw && twf)
        {
          tp = tw->tabPosition();
          QRect tr = twf->selectedTabRect;
          switch (tw->tabPosition()) {
            case QTabWidget::North: {
              fspec1.top = 0;
              d = tr.x();
              l = tr.width();
            }
            case QTabWidget::South: {
              fspec1.bottom = 0;
              d = tr.x();
              l = tr.width();
              break;
            }
            case QTabWidget::West: {
              fspec1.left = 0;
              d = tr.y();
              l = tr.height();
            }
            case QTabWidget::East: {
              fspec1.right = 0;
              d = tr.y();
              l = tr.height();
              break;
            }
            default : {
              d = 0;
              l = 0;
            }
          }
        }
      }

      if (status.startsWith("disabled"))
      {
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      QString suffix = "-normal";
      if (isInactive)
        suffix = "-normal-inactive";
      renderInterior(painter,option->rect,fspec1,ispec,ispec.element+suffix);
      renderFrame(painter,
                  option->rect,
                  fspec,fspec.element+suffix,
                  d, l, tp);
      if (!(option->state & State_Enabled))
        painter->restore();

      break;
    }

    case PE_FrameLineEdit : {
      frame_spec fspec = getFrameSpec("LineEdit");
      if (qobject_cast<const QAbstractSpinBox*>(getParent(widget,1)))
      {
        fspec.hasCapsule = true;
        fspec.capsuleH = -1;
        fspec.capsuleV = 2;
      }
      else if (const QComboBox *cb = qobject_cast<const QComboBox*>(getParent(widget,1)))
      {
        fspec.hasCapsule = true;
        /* see if there is any icon on the left of
           the combo box (or right for RTL layout) */
        if (option->direction == Qt::RightToLeft)
        {
          const frame_spec fspec1 = getFrameSpec("DropDownButton");
          if (widget->width() < cb->width() - COMBO_ARROW_LENGTH - fspec1.left)
            fspec.capsuleH = 0;
          else
            fspec.capsuleH = 1;
        }
        else
        {
          if (widget->x() > 0)
            fspec.capsuleH = 0;
          else
            fspec.capsuleH = -1;
        }
        fspec.capsuleV = 2;
      }

      QString leStatus = (option->state & State_HasFocus) ? "focused" : "normal";
      if (isInactive)
        leStatus.append(QString("-inactive"));
      if (status.startsWith("disabled"))
      {
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      renderFrame(painter,
                  isLibreoffice && !qstyleoption_cast<const QStyleOptionSpinBox *>(option) ?
                    option->rect.adjusted(fspec.left,fspec.top,-fspec.right,-fspec.bottom) :
                    option->rect,
                  fspec,
                  fspec.element+"-"+leStatus);
      if (!(option->state & State_Enabled))
        painter->restore();

      break;
    }

    case PE_PanelLineEdit : {
      /* don't draw the interior or frame of a Plasma spinbox */
      if (isPlasma && widget
          && (!widget->parentWidget()
              || widget->parentWidget()->testAttribute(Qt::WA_NoSystemBackground)))
      {
        break;
      }

      /* force frame */
      drawPrimitive(PE_FrameLineEdit,option,painter,widget);

      const QString group = "LineEdit";

      const interior_spec ispec = getInteriorSpec(group);
      frame_spec fspec = getFrameSpec(group);
      /* no frame when editing itemview texts */
      if (qobject_cast<const QAbstractItemView*>(getParent(widget,2)))
      {
        fspec.left = fspec.right = fspec.top = fspec.bottom = 0;
      }
      if (qobject_cast<const QAbstractSpinBox*>(getParent(widget,1)))
      {
        fspec.hasCapsule = true;
        fspec.capsuleH = -1;
        fspec.capsuleV = 2;
      }
      else if (const QComboBox *cb = qobject_cast<const QComboBox*>(getParent(widget,1)))
      {
        fspec.hasCapsule = true;
        if (option->direction == Qt::RightToLeft)
        {
          const frame_spec fspec1 = getFrameSpec("DropDownButton");
          if (widget->width() < cb->width() - COMBO_ARROW_LENGTH - fspec1.left)
            fspec.capsuleH = 0;
          else
            fspec.capsuleH = 1;
        }
        else
        {
          if (widget->x() > 0)
            fspec.capsuleH = 0;
          else
            fspec.capsuleH = -1;
        }
        fspec.capsuleV = 2;
      }

      QString leStatus = (option->state & State_HasFocus) ? "focused" : "normal";
      if (isInactive)
        leStatus .append(QString("-inactive"));
      if (status.startsWith("disabled"))
      {
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+leStatus);
      if (!(option->state & State_Enabled))
        painter->restore();

      break;
    }

    /* toolbar is drawn at CE_ToolBar */
    /*case PE_PanelToolBar : {
      return;
    }*/

    case PE_IndicatorToolBarHandle :
    case PE_IndicatorToolBarSeparator : {
      const QString group = "Toolbar";

      frame_spec fspec;
      default_frame_spec(fspec);
      interior_spec ispec;
      default_interior_spec(ispec);
      const indicator_spec dspec = getIndicatorSpec(group);

      bool isVertical = false;
      if (!(option->state & State_Horizontal))
        isVertical = true;
      QRect r = option->rect;
      if (isVertical)
      {
        /*
           Since we rotate the whole toolbar in CE_ToolBar,
           the handle doesn't need any transformation and
           just its rectangle should be corrected.
        */
        r.setRect(y, x, h, w);
        if (element == PE_IndicatorToolBarSeparator)
        {
          painter->save();
          QTransform m;
          m.translate(w + 2*x, 0); // w+2*x is the width of vertical toolbar
          m.rotate(90);
          painter->setTransform(m, true);
        }
      }
      renderInterior(painter,r,fspec,ispec,
                     dspec.element
                       +(element == PE_IndicatorToolBarHandle ? "-handle" : "-separator"));

      if (isVertical && element == PE_IndicatorToolBarSeparator)
        painter->restore();

      break;
    }

    case PE_IndicatorSpinPlus :
    case PE_IndicatorSpinMinus :
    case PE_IndicatorSpinUp :
    case PE_IndicatorSpinDown : {
      bool up = true;
      if (element == PE_IndicatorSpinMinus || element == PE_IndicatorSpinDown)
        up = false;

      const QString group = "IndicatorSpinBox";

      frame_spec fspec = getFrameSpec(group);
      fspec.hasCapsule = true;
      if (up)
      {
        fspec.capsuleH = 1;
        fspec.capsuleV = 2;
      }
      else
      {
        fspec.capsuleH = 0;
        fspec.capsuleV = 2;
      }
      const interior_spec ispec = getInteriorSpec(group);
      indicator_spec dspec = getIndicatorSpec(group);

      QString iStatus = status; // indicator state
      QString bStatus = status; // button state
      if (!status.startsWith("disabled"))
      {
        const QStyleOptionSpinBox *opt = qstyleoption_cast<const QStyleOptionSpinBox*>(option);
        if (opt)
        {
          /* first disable the indicator if an
             upper or lower limit is reached */
          if (
              !(
                (up && opt->stepEnabled & QAbstractSpinBox::StepUpEnabled)
                || (!up && opt->stepEnabled & QAbstractSpinBox::StepDownEnabled)
               )
             )
          {
            iStatus = "disabled";
          }

          /* now handle the button state */

          // the subcontrol
#if QT_VERSION < 0x050000
          int sc = QStyle::SC_SpinBoxUp;
#else
          quint32 sc = QStyle::SC_SpinBoxUp;
#endif
          if (!up)
            sc = QStyle::SC_SpinBoxDown;

          // press or focus only the active button
          if (opt->activeSubControls != sc)
            bStatus = "normal";
        }
        // disable only the indicator, not the button
        if (iStatus == "disabled")
          bStatus = "normal";
        // don't focus the indicator when the cursor isn't on the button
        else if (bStatus.startsWith("normal"))
          iStatus = "normal";

        if (isInactive)
        {
          if (!iStatus.endsWith("-inactive"))
            iStatus.append(QString("-inactive"));
          if (!bStatus.endsWith("-inactive"))
            bStatus.append(QString("-inactive"));
        }
      }

      QString iString; // indicator string
      if (element == PE_IndicatorSpinPlus) iString = "-plus-";
      else if (element == PE_IndicatorSpinMinus) iString = "-minus-";
      else if (element == PE_IndicatorSpinUp) iString = "-up-";
      else  iString = "-down-";

      QRect r = option->rect;

      if (bStatus.startsWith("disabled"))
      {
        bStatus.replace(QString("disabled"),QString("normal"));
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      renderFrame(painter,r,fspec,fspec.element+"-"+bStatus);
      renderInterior(painter,r,fspec,ispec,ispec.element+"-"+bStatus);
      if (!(option->state & State_Enabled))
        painter->restore();

      /* a workaround for LibreOffice;
         also see subControlRect() -> CC_SpinBox */
      if (isLibreoffice)
      {
        bStatus = iStatus = "normal";
        if (up) iString = "-plus-";
        else iString = "-minus-";
      }

      // horizontally center both indicators
      fspec.left = 0;
      if (!up)
        fspec.right = 0;
      renderIndicator(painter,
                      r,
                      fspec,dspec,
                      dspec.element+iString+iStatus);

      break;
    }

    case PE_IndicatorHeaderArrow : {
      const QString group = "HeaderSection";
      frame_spec fspec = getFrameSpec(group);
      const indicator_spec dspec = getIndicatorSpec(group);
      const label_spec lspec = getLabelSpec(group);

      /* this is compensated in CE_HeaderLabel;
         also see SE_HeaderArrow */
      if (option->direction == Qt::RightToLeft)
      {
        fspec.right = 0;
        fspec.left += lspec.left;
      }
      else
      {
        fspec.left = 0;
        fspec.right += lspec.right;
      }

      const QStyleOptionHeader *opt =
        qstyleoption_cast<const QStyleOptionHeader *>(option);
      if (opt)
      {
        QString aStatus = "normal";
        if (status.startsWith("disabled"))
          aStatus = "disabled";
        else if (status.startsWith("toggled") || status.startsWith("pressed"))
          aStatus = "pressed";
        else if (option->state & State_MouseOver)
          aStatus = "focused";
        if (isInactive)
          aStatus.append(QString("-inactive"));
        if (opt->sortIndicator == QStyleOptionHeader::SortDown)
          renderIndicator(painter,option->rect,fspec,dspec,dspec.element+"-down-"+aStatus);
        else if (opt->sortIndicator == QStyleOptionHeader::SortUp)
          renderIndicator(painter,option->rect,fspec,dspec,dspec.element+"-up-"+aStatus);
      }

      break;
    }

    case PE_IndicatorButtonDropDown : {
      const QString group = "DropDownButton";

      frame_spec fspec = getFrameSpec(group);

      const QComboBox *combo = qobject_cast<const QComboBox *>(widget);
      if (combo /*&& !combo->duplicatesEnabled()*/)
      {
        fspec.hasCapsule = true;
        if (option->direction == Qt::RightToLeft)
        {
          fspec.capsuleH = -1;
          fspec.right = 0;
        }
        else
        {
          fspec.capsuleH = 1;
          fspec.left = 0; // no left frame in this case
        }
        fspec.capsuleV = 2;

        if (option->state & State_Selected)
          status.replace(QString("toggled"),QString("normal"));
      }
      const interior_spec ispec = getInteriorSpec(group);
      const indicator_spec dspec = getIndicatorSpec(group);

      const QToolButton *tb = qobject_cast<const QToolButton *>(widget);
      if (tb)
      {
        bool drawRaised = false;
        const theme_spec tspec = settings->getThemeSpec();
        if (tspec.group_toolbar_buttons)
        {
          if (const QToolBar *toolBar = qobject_cast<const QToolBar *>(tb->parentWidget()))
          {
            drawRaised = true;

            const QStyleOptionToolButton *opt = qstyleoption_cast<const QStyleOptionToolButton *>(option);
            int kind = whichToolbarButton (tb, opt, toolBar);
            if (kind != 2)
            {
              fspec.hasCapsule = true;
              fspec.capsuleV = 2;
              fspec.capsuleH = kind;
            }
          }
        }

        bool rtl(option->direction == Qt::RightToLeft);
        if (!fspec.hasCapsule)
        {
          fspec.hasCapsule = true;
          fspec.capsuleH = rtl ? -1 : 1;
          fspec.capsuleV = 2;
        }
        else if (fspec.capsuleH == -1)
          fspec.capsuleH = 0;
        else if (fspec.capsuleH == 2)
          fspec.capsuleH = rtl ? -1 : 1;
        if (rtl)
          fspec.right = 0;
        else
          fspec.left = 0; // no left frame in this case

        if (!tb->autoRaise() || (!status.startsWith("normal") && !status.startsWith("disabled")) || drawRaised)
        {
          if (status.startsWith("disabled"))
          {
            status.replace(QString("disabled"),QString("normal"));
            if (!drawRaised)
            {
              painter->save();
              painter->setOpacity(DISABLED_OPACITY);
            }
          }
          renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+status);
          renderFrame(painter,option->rect,fspec,fspec.element+"-"+status);
          if (!(option->state & State_Enabled))
          {
            status = "disabled";
            if (!drawRaised)
              painter->restore();
          }
        }
      }
      else if (!(option->state & State_AutoRaise)
               || (!status.startsWith("normal") && !status.startsWith("disabled")))
      {
        if (status.startsWith("disabled"))
        {
          status.replace(QString("disabled"),QString("normal"));
          painter->save();
          painter->setOpacity(DISABLED_OPACITY);
        }
        renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+status);
        renderFrame(painter,option->rect,fspec,fspec.element+"-"+status);
        if (!(option->state & State_Enabled))
        {
          painter->restore();
          status = "disabled";
        }
      }

      QString aStatus = "normal";
      if (status.startsWith("disabled"))
        aStatus = "disabled";
      else if (status.startsWith("toggled") || status.startsWith("pressed"))
        aStatus = "pressed";
      else if (option->state & State_MouseOver)
        aStatus = "focused";
      if (isInactive)
        aStatus.append(QString("-inactive"));
      renderIndicator(painter,
                      option->rect,
                      fspec,dspec,dspec.element+"-"+aStatus);

      break;
    }

    case PE_PanelMenuBar : {
      break;
    }

    case PE_IndicatorTabTear : {
      indicator_spec dspec = getIndicatorSpec("Tab");
      renderElement(painter,dspec.element+"-tear",option->rect,0,0);

      break;
    }

    case PE_IndicatorTabClose : {
      frame_spec fspec;
      default_frame_spec(fspec);
      const indicator_spec dspec = getIndicatorSpec("Tab");

      status = !(option->state & State_Enabled) ? "disabled" :
                 option->state & State_Sunken ? "pressed" :
                   option->state & State_MouseOver ? "focused" : "normal";
      if (isInactive)
        status.append(QString("-inactive"));
      renderIndicator(painter,option->rect,fspec,dspec,dspec.element+"-close-"+status);

      break;
    }

    case PE_IndicatorArrowUp :
    case PE_IndicatorArrowDown :
    case PE_IndicatorArrowLeft :
    case PE_IndicatorArrowRight : {
      frame_spec fspec;
      default_frame_spec(fspec);
      const indicator_spec dspec = getIndicatorSpec("IndicatorArrow");
      
      QString dir;
      if (element == PE_IndicatorArrowUp)
        dir = "-up-";
      else if (element == PE_IndicatorArrowDown)
        dir = "-down-";
      else if (element == PE_IndicatorArrowLeft)
        dir = "-left-";
      else
        dir = "-right-";

      QString aStatus = "normal";
      if (status.startsWith("disabled"))
        aStatus = "disabled";
      else if (status.startsWith("toggled") || status.startsWith("pressed"))
        aStatus = "pressed";
      else if (option->state & State_MouseOver)
        aStatus = "focused";
      if (isInactive)
        aStatus.append(QString("-inactive"));
      renderIndicator(painter,option->rect,fspec,dspec,dspec.element+dir+aStatus);

      break;
    }

    // this doesn't seem to be used at all
    /*case PE_IndicatorProgressChunk : {
      QCommonStyle::drawPrimitive(element,option,painter,widget);
      break;
    }*/

    case PE_IndicatorDockWidgetResizeHandle : {
      drawControl(CE_Splitter,option,painter,widget);

      break;
    }

    case PE_IndicatorMenuCheckMark : {
      // nothing, uses radio and checkbox at CE_MenuItem
      break;
    }

    //case PE_PanelItemViewRow :
    case PE_PanelItemViewItem : {
      /*
         Here frame has no real meaning but we force one by adjusting
         PM_FocusFrameHMargin and PM_FocusFrameVMargin for viewitems.
      */

      const frame_spec fspec = getFrameSpec("ItemView");
      interior_spec ispec = getInteriorSpec("ItemView");

      /* we want to know if the widget has focus */
      QString ivStatus = (option->state & State_Enabled) ?
                         // for Okular's navigation panel
                         ((option->state & State_Selected)
                          && (option->state & State_HasFocus)
                          && (option->state & State_Active)) ? "pressed" :
                         // for most widgets
                         (widget && widget->hasFocus() && (option->state & State_Selected)) ? "pressed" :
                         (option->state & State_Selected) ? "toggled" :
                         (option->state & State_MouseOver) ? "focused" : "normal"
                         : "disabled";
      if (isInactive)
        ivStatus.append(QString("-inactive"));

      renderFrame(painter,option->rect,fspec,fspec.element+"-"+ivStatus);
      renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+ivStatus);

      break;
    }

    case PE_PanelTipLabel : {
      const QString group = "ToolTip";

      frame_spec fspec = getFrameSpec(group);
      const interior_spec ispec = getInteriorSpec(group);
      fspec.left = fspec.right = fspec.top = fspec.bottom = pixelMetric(PM_ToolTipLabelFrameWidth);

      const theme_spec tspec = settings->getThemeSpec();
      if (tspec.tooltip_shadow_depth > 0 && !isLibreoffice && !subApp && !isSystemSettings)
        renderFrame(painter,option->rect,fspec,fspec.element+"-shadow");
      else
        renderFrame(painter,option->rect,fspec,fspec.element+"-normal");
      renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-normal");

      break;
    }

    default : {
      QCommonStyle::drawPrimitive(element,option,painter,widget);
      break;
    }
  }
}

void Kvantum::drawControl(ControlElement element,
                          const QStyleOption *option,
                          QPainter *painter,
                          const QWidget *widget) const
{
  int x,y,h,w;
  option->rect.getRect(&x,&y,&w,&h);
  QString status =
      (option->state & State_Enabled) ?
        (option->state & State_On) ? "toggled" :
        (option->state & State_Sunken) ? "pressed" :
        (option->state & State_Selected) ? "toggled" :
        (option->state & State_MouseOver) ? "focused" : "normal"
      : "disabled";

  bool isInactive = false;
  if (widget && !widget->isActiveWindow())
  {
    isInactive = true;
    status.append(QString("-inactive"));
  }

  const QIcon::Mode iconmode =
        (option->state & State_Enabled) ?
        (option->state & State_Sunken) ? QIcon::Active :
        (option->state & State_MouseOver) ? QIcon::Active : QIcon::Normal
      : QIcon::Disabled;

  const QIcon::State iconstate =
      (option->state & State_On) ? QIcon::On : QIcon::Off;

  switch (element) {
    case CE_MenuTearoff : {
      status = (option->state & State_Selected) ? "focused" : "normal";
      // see PM_MenuTearoffHeight and also PE_PanelMenu
      int margin = pixelMetric(PM_MenuHMargin);
      QRect r(option->rect.x() + margin,
              option->rect.y() + margin,
              option->rect.width() - 2*margin,
              7);
      const indicator_spec dspec = getIndicatorSpec("MenuItem");
      renderElement(painter,
                    dspec.element+"-tearoff-"+status,
                    r,
                    20,
                    0);

      break;
    }

    case CE_MenuItem : {
      const QStyleOptionMenuItem *opt =
          qstyleoption_cast<const QStyleOptionMenuItem *>(option);

      if (opt) {
        const QString group = "MenuItem";

        frame_spec fspec = getFrameSpec(group);
        const interior_spec ispec = getInteriorSpec(group);
        const indicator_spec dspec = getIndicatorSpec(group);
        const label_spec lspec = getLabelSpec(group);

        if (opt->menuItemType == QStyleOptionMenuItem::Separator)
          renderElement(painter,dspec.element+"-separator",option->rect,20,0);
        //else if (opt->menuItemType == QStyleOptionMenuItem::TearOff)
          //renderElement(painter,dspec.element+"-tearoff",option->rect,20,0);
        else
        {
          renderFrame(painter,option->rect,fspec,fspec.element+"-"+status);
          renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+status);

          const QStringList l = opt->text.split('\t');

          int smallIconSize = pixelMetric(PM_SmallIconSize);
          int talign = Qt::AlignVCenter | Qt::TextSingleLine;
          if (!styleHint(SH_UnderlineShortcut, opt, widget))
            talign |= Qt::TextHideMnemonic;
          else
            talign |= Qt::TextShowMnemonic;

          int state = 1;
          if (status.startsWith("disabled"))
            state = 0;
          else if (status.startsWith("toggled") || status.startsWith("pressed"))
            state = 2;

          bool rtl(option->direction == Qt::RightToLeft);

          if (l.size() > 0) // menu label
          {
            if (opt->icon.isNull())
              renderLabel(painter,option->palette,
                          option->rect.adjusted(rtl ?
                                                  0
                                                  : qMin(opt->maxIconWidth,smallIconSize)+lspec.tispace,
                                                0,
                                                rtl ?
                                                  -qMin(opt->maxIconWidth,smallIconSize)-lspec.tispace
                                                  : 0,
                                                0),
                          fspec,lspec,
                          Qt::AlignLeft | talign,
                          l[0],QPalette::Text,
                          state);
            else
              renderLabel(painter,option->palette,
                          option->rect,
                          fspec,lspec,
                          Qt::AlignLeft | talign,
                          l[0],QPalette::Text,
                          state,
                          opt->icon.pixmap(smallIconSize,iconmode,iconstate));
          }
          int iw = pixelMetric(PM_IndicatorWidth,option,widget);
          int ih = pixelMetric(PM_IndicatorHeight,option,widget);
          if (l.size() > 1) // shortcut
          {
            int space = lspec.right + fspec.right;
            if (opt->menuItemType == QStyleOptionMenuItem::SubMenu)
              space += dspec.size + lspec.tispace; // see CT_MenuItem
            if (opt->checkType != QStyleOptionMenuItem::NotCheckable)
              space += iw + lspec.tispace; // see CT_MenuItem
            renderLabel(painter,option->palette,
                        option->rect.adjusted(rtl ? space : 0,
                                              0,
                                              rtl ? 0 : -space,
                                              0),
                        fspec,lspec,
                        Qt::AlignRight | talign,
                        l[1],QPalette::Text,
                        state);
          }

          QStyleOptionMenuItem o(*opt);
          o.rect = alignedRect(QApplication::layoutDirection(),
                               ((isLibreoffice && opt->menuItemType != QStyleOptionMenuItem::SubMenu) ?
                                 Qt::AlignLeft :
                                 Qt::AlignRight)
                                | Qt::AlignVCenter,
                               QSize(iw,ih),
                               isLibreoffice ? o.rect.adjusted(6,-2,0,0) // FIXME why?
                                               /* we add a 2px right margin at CT_MenuItem */
                                             : interiorRect(o.rect,fspec).adjusted(rtl ? 2 : 0,
                                                                                   0,
                                                                                   rtl ? 0 : -2,
                                                                                   0));

          /* change the selected and pressed states to mouse-over */
          if (o.state & QStyle::State_Selected)
            o.state = (o.state & ~QStyle::State_Selected) | QStyle::State_MouseOver;
          if (o.state & QStyle::State_Sunken)
            o.state = (o.state & ~QStyle::State_Sunken) | QStyle::State_MouseOver;

          if (opt->menuItemType == QStyleOptionMenuItem::SubMenu)
            drawPrimitive(rtl ? PE_IndicatorArrowLeft : PE_IndicatorArrowRight,
                          &o,painter);

          if (opt->checkType == QStyleOptionMenuItem::Exclusive)
          {
            if (opt->checked)
              o.state |= State_On;
            drawPrimitive(PE_IndicatorRadioButton,&o,painter,widget);
          }

          if (opt->checkType == QStyleOptionMenuItem::NonExclusive)
          {
            if (opt->checked)
              o.state |= State_On;
            drawPrimitive(PE_IndicatorCheckBox,&o,painter,widget);
          }
        }
      }

      break;
    }

    case CE_ItemViewItem: {
      /*
          Here we rely on QCommonStyle::drawControl() for text
          eliding and other calculations and just use our custom
          colors instead of the default ones whenever possible.
      */
      if (const QStyleOptionViewItemV4 *opt = qstyleoption_cast<const QStyleOptionViewItemV4 *>(option))
      {
        if (!opt->text.isEmpty())
        {
          int state = 1;
          if (status.startsWith("disabled"))
            state = 0;
          else if (status.startsWith("toggled") || status.startsWith("pressed"))
            state = 3;
          else if (option->state & State_MouseOver)
            state = 2;
          if (state != 0)
          {
            const label_spec lspec = getLabelSpec("ItemView");
            QColor normalColor(lspec.normalColor);
            QColor focusColor(lspec.focusColor);
            QColor pressColor(lspec.pressColor);
            int baseValue = opt->palette.color(QPalette::Base).value();
            if (state == 1 && normalColor.isValid()
                // a minimum amount of contrast is needed
                && qAbs(baseValue - normalColor.value()) >= MIN_CONTRAST)
            {
              QStyleOptionViewItemV4 o(*opt);
              QPalette palette(opt->palette);
              /* if another color has been set intentionally,
                 like in Akregator's unread feeds, use it */
              if (widget && palette == widget->palette())
              {
                palette.setColor(QPalette::Text, normalColor);
                o.palette = palette;
                QCommonStyle::drawControl(element,&o,painter,widget);
                return;
              }
            }
            else if (state == 2 && focusColor.isValid()
                     && qAbs(baseValue - focusColor.value()) >= MIN_CONTRAST)
            {
              QStyleOptionViewItemV4 o(*opt);
              QPalette palette(opt->palette);
              palette.setColor(QPalette::Text, focusColor);
              palette.setColor(QPalette::HighlightedText, focusColor);
              o.palette = palette;
              QCommonStyle::drawControl(element,&o,painter,widget);
              return;
            }
            else if (state == 3 && pressColor.isValid())
            {
              QStyleOptionViewItemV4 o(*opt);
              QPalette palette(opt->palette);
              palette.setColor(QPalette::HighlightedText, pressColor);
              o.palette = palette;
              QCommonStyle::drawControl(element,&o,painter,widget);
              return;
            }
          }
        }
        QCommonStyle::drawControl(element,option,painter,widget);
      }

      break;
    }

    case CE_MenuHMargin:
    case CE_MenuVMargin:
    case CE_MenuEmptyArea : {
      break;
    }

    case CE_MenuBarItem : {
      const QStyleOptionMenuItem *opt =
          qstyleoption_cast<const QStyleOptionMenuItem *>(option);

      if (opt) {
        QString group = "MenuBarItem";
        frame_spec fspec = getFrameSpec(group);
        fspec.hasCapsule = true;
        fspec.capsuleH = 0;
        fspec.capsuleV = 2;
        const interior_spec ispec = getInteriorSpec(group);
        label_spec lspec = getLabelSpec(group);
        if (isPlasma)
        {
          lspec.left = lspec.right = lspec.top = lspec.bottom = 0;
        }

        if (status.startsWith("disabled"))
        {
          status.replace(QString("disabled"),QString("normal"));
          painter->save();
          painter->setOpacity(DISABLED_OPACITY);
        }
        renderFrame(painter,option->rect,fspec,fspec.element+"-"+status);
        renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+status);
        if (!(option->state & State_Enabled))
        {
          painter->restore();
          status = "disabled";
        }

        int talign = Qt::AlignLeft | Qt::AlignVCenter |Qt::TextSingleLine;
        if (!styleHint(SH_UnderlineShortcut, opt, widget))
          talign |= Qt::TextHideMnemonic;
        else
          talign |= Qt::TextShowMnemonic;
        int state = 1;
        if (status.startsWith("disabled"))
          state = 0;
        else if (status.startsWith("toggled") || status.startsWith("pressed"))
          state = 2;
        renderLabel(painter,option->palette,
                    option->rect,
                    fspec,lspec,
                    talign,opt->text,QPalette::WindowText,
                    state);
      }

      break;
    }

    case CE_MenuBarEmptyArea : {
        const QString group = "MenuBar";

        const frame_spec fspec = getFrameSpec(group);
        const interior_spec ispec = getInteriorSpec(group);

        // NOTE this does not use the status (otherwise always disabled)
        renderFrame(painter,option->rect,fspec,fspec.element+"-normal");
        renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-normal");

      break;
    }

    case CE_MenuScroller : {
      drawPrimitive(PE_PanelButtonTool,option,painter,widget);
      if (option->state & State_DownArrow)
        drawPrimitive(PE_IndicatorArrowDown,option,painter,widget);
      else
        drawPrimitive(PE_IndicatorArrowUp,option,painter,widget);

      break;
    }

    case CE_RadioButtonLabel : {
      const QStyleOptionButton *opt =
          qstyleoption_cast<const QStyleOptionButton *>(option);

      if (opt) {
        const QString group = "RadioButton";
        const frame_spec fspec = getFrameSpec(group);
        const label_spec lspec = getLabelSpec(group);

        int talign = Qt::AlignLeft | Qt::AlignVCenter;
        if (!styleHint(SH_UnderlineShortcut, opt, widget))
          talign |= Qt::TextHideMnemonic;
        else
          talign |= Qt::TextShowMnemonic;
        renderLabel(painter,option->palette,
                    option->rect,
                    fspec,lspec,
                    talign,opt->text,QPalette::WindowText,
                    option->state & State_Enabled ? option->state & State_MouseOver ? 2 : 1 : 0,
                    opt->icon.pixmap(opt->iconSize,iconmode,iconstate));
      }

      break;
    }

    case CE_CheckBoxLabel : {
      const QStyleOptionButton *opt =
          qstyleoption_cast<const QStyleOptionButton *>(option);

      if (opt) {
        const QString group = "CheckBox";
        const frame_spec fspec = getFrameSpec(group);
        const label_spec lspec = getLabelSpec(group);

        int talign = Qt::AlignLeft | Qt::AlignVCenter;
        if (!styleHint(SH_UnderlineShortcut, opt, widget))
          talign |= Qt::TextHideMnemonic;
        else
          talign |= Qt::TextShowMnemonic;
        renderLabel(painter,option->palette,
                    option->rect,
                    fspec,lspec,
                    talign,opt->text,QPalette::WindowText,
                    option->state & State_Enabled ? option->state & State_MouseOver ? 2 : 1 : 0,
                    opt->icon.pixmap(opt->iconSize,iconmode,iconstate));
      }

      break;
    }

    case CE_ComboBoxLabel : { // not editable
      const QStyleOptionComboBox *opt =
          qstyleoption_cast<const QStyleOptionComboBox *>(option);

      if (opt && !opt->editable) {
        if (option->state & State_Selected)
          status.replace(QString("toggled"),QString("normal"));

        const QString group = "ComboBox";
        const frame_spec fspec = getFrameSpec(group);
        const label_spec lspec = getLabelSpec(group);

        const QRect r = subControlRect(CC_ComboBox,opt,SC_ComboBoxEditField,widget);
        int talign = Qt::AlignLeft | Qt::AlignVCenter;
        if (!styleHint(SH_UnderlineShortcut, opt, widget))
          talign |= Qt::TextHideMnemonic;
        else
          talign |= Qt::TextShowMnemonic;

        int state = 1;
        if (status.startsWith("disabled"))
          state = 0;
        else if (status.startsWith("pressed"))
          state = 3;
        else if (status.startsWith("toggled"))
          state = 4;
        else if (option->state & State_MouseOver)
          state = 2;

        renderLabel(painter,option->palette,
                    /* since the label is vertically centered, this doesn't do
                       any harm and is good for Qt4 Designer and similar cases */
                    r.adjusted(0,-fspec.top-lspec.top,0,fspec.bottom+lspec.bottom),
                    fspec,lspec,
                    talign,opt->currentText,QPalette::ButtonText,
                    state,
                    opt->currentIcon.pixmap(opt->iconSize,iconmode,iconstate));
      }

      break;
    }

    case CE_TabBarTabShape : {
      const QStyleOptionTab *opt =
        qstyleoption_cast<const QStyleOptionTab *>(option);

      if (opt)
      {
        /* Let's forget about the pressed state. It's
           useless here and makes trouble in KDevelop. */
        status =
            (option->state & State_Enabled) ?
              (option->state & State_On) ? "toggled" :
              (option->state & State_Selected) ? "toggled" :
              (option->state & State_MouseOver) ? "focused" : "normal"
            : "disabled";
        if (isInactive)
          status.append(QString("-inactive"));

        frame_spec fspec = getFrameSpec("Tab");
        const interior_spec ispec = getInteriorSpec("Tab");

        QRect r = option->rect;
        bool verticalTabs = false;
        bool bottomTabs = false;

        if (opt->shape == QTabBar::RoundedEast
            || opt->shape == QTabBar::RoundedWest
            || opt->shape == QTabBar::TriangularEast
            || opt->shape == QTabBar::TriangularWest)
        {
            verticalTabs = true;
        }
        if (opt->shape == QTabBar::RoundedSouth || opt->shape == QTabBar::TriangularSouth)
          bottomTabs = true;

        if (status.startsWith("normal") || status.startsWith("focused"))
        {
          const theme_spec tspec = settings->getThemeSpec();
          if (tspec.joined_tabs
              && opt->position != QStyleOptionTab::OnlyOneTab)
          {
            int capsule = 2;
            if (opt->position == QStyleOptionTab::Beginning)
            {
              if (opt->selectedPosition != QStyleOptionTab::NextIsSelected)
              {
                fspec.hasCapsule = true;
                capsule = -1;
              }
            }
            else if (opt->position == QStyleOptionTab::Middle)
            {
              fspec.hasCapsule = true;
              if (opt->selectedPosition == QStyleOptionTab::NextIsSelected)
                capsule = 1;
              else if (opt->selectedPosition == QStyleOptionTab::PreviousIsSelected)
                capsule = -1;
              else
                capsule = 0;
            }
            else if (opt->position == QStyleOptionTab::End)
            {
              if (opt->selectedPosition != QStyleOptionTab::PreviousIsSelected)
              {
                fspec.hasCapsule = true;
                capsule = 1;
              }
            }
            /* will be flipped both vertically and horizontally */
            if (bottomTabs)
              capsule = -1*capsule;
            /* I've seen this only in KDevelop */
            if (opt->direction == Qt::RightToLeft)
              capsule = -1*capsule;
            fspec.capsuleH = capsule;
            fspec.capsuleV = 2;
          }
        }

        if (verticalTabs)
        {
          /* painter saving/restoring is needed not only to
             render texts of left and bottom tabs correctly
             but also because there are usually mutiple tabs */
          painter->save();
          int X, Y, rot;
          int xTr = 0; int xScale = 1;
          if (opt->shape == QTabBar::RoundedEast || opt->shape == QTabBar::TriangularEast)
          {
            X = w;
            Y = y;
            rot = 90;
          }
          else
          {
            X = 0;
            Y = y + h;
            rot = -90;
            xTr = h; xScale = -1;
          }
          r.setRect(0, 0, h, w);
          QTransform m;
          m.translate(X, Y);
          m.rotate(rot);
          /* flip left tabs vertically */
          m.translate(xTr, 0); m.scale(xScale,1);
          painter->setTransform(m, true);
        }
        else if (bottomTabs)
        {
          painter->save();
          QTransform m;
          /* flip bottom tabs both vertically and horizontally */
          r.setRect(0, 0, w, h);
          m.translate(x + w, h); m.scale(-1,-1);
          painter->setTransform(m, true);
        }


        if (status.startsWith("disabled"))
        {
          status.replace(QString("disabled"),QString("normal"));
          painter->save();
          painter->setOpacity(DISABLED_OPACITY);
        }
        renderInterior(painter,r,fspec,ispec,ispec.element+"-"+status);
        renderFrame(painter,r,fspec,fspec.element+"-"+status);
        if (!(option->state & State_Enabled))
          painter->restore();

        if (verticalTabs || bottomTabs)
          painter->restore();
      }

      break;
    }

    case CE_TabBarTabLabel : {
      const QStyleOptionTab *opt =
        qstyleoption_cast<const QStyleOptionTab *>(option);

      if (opt) {
        status =
            (option->state & State_Enabled) ?
              (option->state & State_On) ? "toggled" :
              (option->state & State_Selected) ? "toggled" :
              (option->state & State_MouseOver) ? "focused" : "normal"
            : "disabled";
        if (isInactive)
          status.append(QString("-inactive"));

        const QString group = "Tab";
        const frame_spec fspec = getFrameSpec(group);
        const label_spec lspec = getLabelSpec(group);

        int talign = Qt::AlignLeft | Qt::AlignVCenter;
        if (!styleHint(SH_UnderlineShortcut, opt, widget))
          talign |= Qt::TextHideMnemonic;
        else
          talign |= Qt::TextShowMnemonic;

        QRect r = option->rect;
        bool verticalTabs = false;
        if (opt->shape == QTabBar::RoundedEast
            || opt->shape == QTabBar::RoundedWest
            || opt->shape == QTabBar::TriangularEast
            || opt->shape == QTabBar::TriangularWest)
        {
          verticalTabs = true;
        }
        
        if (verticalTabs)
        {
          /* this wouldn't be needed if there
             were always only a single tab */
          painter->save();
          int X, Y, rot;
          if (opt->shape == QTabBar::RoundedEast || opt->shape == QTabBar::TriangularEast)
          {
            X = w;
            Y = y;
            rot = 90;
          }
          else
          {
            X = 0;
            Y = y + h;
            rot = -90;
          }
          r.setRect(0, 0, h, w);
          QTransform m;
          m.translate(X, Y);
          m.rotate(rot);
          painter->setTransform(m, true);
        }

        /* tabButtons (as in Rekonq);
           apparently the label rect includes them */
        int ltb = 0;
        int rtb = 0;
        if (widget)
        {
          if (verticalTabs)
          {
            ltb = subElementRect(QStyle::SE_TabBarTabLeftButton,option,widget).height();
            rtb = subElementRect(QStyle::SE_TabBarTabRightButton,option,widget).height();
          }
          else
          {
            ltb = subElementRect(QStyle::SE_TabBarTabLeftButton,option,widget).width();
            rtb = subElementRect(QStyle::SE_TabBarTabRightButton,option,widget).width();
          }
        }
        r.adjust(ltb, 0, -rtb, 0);

        QStyleOptionTabV2 tabV2(*opt);
        QSize iconSize;
        if (!tabV2.icon.isNull())
          iconSize = tabV2.iconSize;

        int state = 1;
        if (status.startsWith("disabled"))
          state = 0;
        else if (status.startsWith("pressed"))
          state = 3;
        else if (status.startsWith("toggled"))
          state = 4;
        else if (option->state & State_MouseOver)
          state = 2;

        /* since we draw text and icon together as label,
           for RTL we need to move the label to right */
        if (opt->direction == Qt::RightToLeft && !verticalTabs)
        {
          if (const QTabBar *tb = qobject_cast<const QTabBar*>(widget))
          {
            if (tb->tabsClosable())
              r = alignedRect(Qt::RightToLeft, Qt::AlignLeft,
                              QSize(w-pixelMetric(PM_TabCloseIndicatorWidth,option,widget), h),
                              option->rect);
          }
        }

        renderLabel(painter,option->palette,
                    r,
                    fspec,lspec,
                    talign,opt->text,QPalette::WindowText,
                    state,
                    opt->icon.pixmap((iconSize.isValid() ? 
                                       qMax(iconSize.width(), iconSize.height()) :
                                       pixelMetric(PM_TabBarIconSize)),
                                     iconmode,iconstate));

        if (verticalTabs)
          painter->restore();
      }

      break;
    }

    /*
       Toolboxes are good as they are. A separate style for them
       would have this disadvantage that their heights wouldn't
       be adjusted to values of frame widths and other spacings.
    */
    /*case CE_ToolBoxTabShape : {
      const QString group = "ToolboxTab";

      const frame_spec fspec = getFrameSpec(group);
      const interior_spec ispec = getInteriorSpec(group);

      renderFrame(painter,option->rect,fspec,fspec.element+"-"+status);
      renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+status);

      break;
    }*/

    case CE_ToolBoxTabLabel : {
      /*
          Here we rely on QCommonStyle::drawControl() and
          just use our custom colors, knowing that QCommonStyle
          uses QPalette::ButtonText for drawing the text.
      */
      if (const QStyleOptionToolBox *opt = qstyleoption_cast<const QStyleOptionToolBox *>(option))
      {
        if (!opt->text.isEmpty())
        {
          int state = 1;
          if (status.startsWith("disabled"))
            state = 0;
          else if (status.startsWith("toggled") || status.startsWith("pressed"))
            state = 3;
          else if (option->state & State_MouseOver)
            state = 2;
          if (state != 0)
          {
            const label_spec lspec = getLabelSpec("ToolboxTab");
            QColor normalColor(lspec.normalColor);
            QColor focusColor(lspec.focusColor);
            QColor pressColor(lspec.pressColor);
            if (state == 1 && normalColor.isValid())
            {
              QStyleOptionToolBox o(*opt);
              QPalette palette(opt->palette);
              palette.setColor(QPalette::ButtonText, normalColor);
              o.palette = palette;
              QCommonStyle::drawControl(element,&o,painter,widget);
              return;
            }
            else if (state == 2 && focusColor.isValid())
            {
              QStyleOptionToolBox o(*opt);
              QPalette palette(opt->palette);
              palette.setColor(QPalette::ButtonText, focusColor);
              o.palette = palette;
              QCommonStyle::drawControl(element,&o,painter,widget);
              return;
            }
            else if (state == 3 && pressColor.isValid())
            {
              QStyleOptionToolBox o(*opt);
              QPalette palette(opt->palette);
              palette.setColor(QPalette::ButtonText, pressColor);
              o.palette = palette;
              QCommonStyle::drawControl(element,&o,painter,widget);
              return;
            }
          }
        }
        QCommonStyle::drawControl(element,option,painter,widget);
      }

      break;
    }

    case CE_ProgressBarGroove : {
      const QString group = "Progressbar";

      const frame_spec fspec = getFrameSpec(group);
      const interior_spec ispec = getInteriorSpec(group);

      QRect r = option->rect;

      /* checking State_Horizontal wouldn't work with
         Krita's progress-spin boxes (KisSliderSpinBox) */
      const QProgressBar *pb = qobject_cast<const QProgressBar *>(widget);
      if (pb && pb->orientation() == Qt::Vertical)
      {
        /* we don't save and restore the painter to draw
           the contents and the label correctly below */
        r.setRect(0, 0, h, w);
        QTransform m;
        m.scale(-1,1);
        m.rotate(90);
        painter->setTransform(m, true);
      }

      if (status.startsWith("disabled"))
      {
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      QString suffix = "-normal";
      if (isInactive)
        suffix = "-normal-inactive";
      renderFrame(painter,r,fspec,fspec.element+suffix);
      renderInterior(painter,r,fspec,ispec,ispec.element+suffix);
      if (!(option->state & State_Enabled))
        painter->restore();

      break;
    }

    case CE_ProgressBarContents : {
      const QStyleOptionProgressBar *opt =
          qstyleoption_cast<const QStyleOptionProgressBar *>(option);

      if (opt)
      {
        status = (option->state & State_Enabled) ?
                   (option->state & State_MouseOver) ? "focused" : "normal"
                 : "disabled";
        if (isInactive)
          status.append(QString("-inactive"));

        const QString group = "ProgressbarContents";
        frame_spec fspec = getFrameSpec(group);
        const interior_spec ispec = getInteriorSpec(group);

        bool isVertical = false;
        bool inverted = false;
        const QProgressBar *pb = qobject_cast<const QProgressBar *>(widget);
        if (pb)
        {
          if (pb->orientation() == Qt::Vertical)
            isVertical = true;
          if (pb->invertedAppearance())
            inverted = true;
        }

        QRect r = option->rect;
        // after this, we could visualize horizontally...
        if (isVertical)
          r.setRect(y, x, h, w);

        if (opt->progress >= 0)
        {
          int empty = sliderPositionFromValue(opt->minimum,
                                              opt->maximum,
                                              opt->maximum - opt->progress + opt->minimum,
                                              isVertical ? h : w,
                                              false);
          if (isVertical ? inverted : !inverted)
            r.adjust(0,0,-empty,0);
          else
            r.adjust(empty,0,0,0);

          // don't draw frames if there isn't enough space
          if (r.width() <= fspec.left+fspec.right)
            fspec.left = fspec.right = 0;
          renderFrame(painter,r,fspec,fspec.element+"-"+status);
          renderInterior(painter,r,fspec,ispec,ispec.element+"-"+status);
        }
        else
        { // busy progressbar
          QWidget *wi = (QWidget *)widget;
          int animcount = progressbars[wi];
          int pm = qMin(pixelMetric(PM_ProgressBarChunkWidth),r.width()/2-2);         
          QRect R = r.adjusted(animcount,0,0,0);
          if (isVertical ? inverted : !inverted)
            R.setX(r.x()+(animcount%r.width()));
          else
            R.setX(r.x()+r.width()-(animcount%r.width()));
          R.setWidth(pm);

          if (R.x()+R.width() > r.x()+r.width())
          {
            const theme_spec tspec = settings->getThemeSpec();
            // wrap busy indicator
            if (!tspec.spread_progressbar)
            {
              fspec.hasCapsule = true;
              fspec.capsuleH = -1;
              fspec.capsuleV = 2;
            }
            R.setWidth(r.x() + r.width() - R.x());
            renderFrame(painter,R,fspec,fspec.element+"-"+status);
            renderInterior(painter,R,fspec,ispec,ispec.element+"-"+status, Qt::Horizontal);

            if (!tspec.spread_progressbar)
              fspec.capsuleH = 1;
            R = QRect(r.x(), r.y(), pm-R.width(), r.height());
            renderFrame(painter,R,fspec,fspec.element+"-"+status);
            renderInterior(painter,R,fspec,ispec,ispec.element+"-"+status, Qt::Horizontal);
          }
          else
          {
            renderFrame(painter,R,fspec,fspec.element+"-"+status);
            renderInterior(painter,R,fspec,ispec,ispec.element+"-"+status);
          }
        }
      }

      break;
    }

    case CE_ProgressBarLabel : {
      const QStyleOptionProgressBar *opt =
          qstyleoption_cast<const QStyleOptionProgressBar *>(option);

      if (opt && opt->textVisible)
      {
        const QString group = "Progressbar";

        frame_spec fspec = getFrameSpec(group);
        label_spec lspec = getLabelSpec(group);

        QRect r = option->rect;
        if (widget)
        {
          const QProgressBar *pb = qobject_cast<const QProgressBar *>(widget);
          if (pb)
          {
            QFont f(pb->font());
            f.setBold(true);

            int wdth = pb->height();

            if (pb->orientation() == Qt::Vertical)
            {
              wdth = pb->width();

              r.setRect(0, 0, h, w);
              QTransform m;
              if (pb->textDirection() == QProgressBar::TopToBottom)
              {
                m.translate(0, w); m.scale(1,-1);
                // be fully consistent
                int top = fspec.top;
                fspec.top = fspec.bottom;
                fspec.bottom = top;

                top = lspec.top;
                lspec.top = lspec.bottom;
                lspec.bottom = top;
              }
              else
              {
                m.translate(h, 0); m.scale(-1,1);
              }
              painter->setTransform(m, true);
            }

            wdth = wdth - fspec.top-fspec.bottom - lspec.top-lspec.bottom;
            if (f.pixelSize() > wdth)
              f.setPixelSize(wdth);
            else if (f.pointSize() > wdth)
              f.setPointSize(wdth);
            painter->setFont(f);
          }
        }

        renderLabel(painter,option->palette,
                    r,
                    fspec,lspec,
                    Qt::AlignCenter,opt->text,QPalette::WindowText,
                    option->state & State_Enabled ? option->state & State_MouseOver ? 2 : 1 : 0);
      }

      break;
    }

    case CE_Splitter : {
      const QString group = "Splitter";
      const frame_spec fspec = getFrameSpec(group);
      const interior_spec ispec = getInteriorSpec(group);
      const indicator_spec dspec = getIndicatorSpec(group);
      status =
          (option->state & State_Enabled) ?
            (option->state & State_Sunken) ? "pressed" :
            (option->state & State_MouseOver) ? "focused" : "normal"
          : "disabled";
      if (isInactive)
        status.append(QString("-inactive"));

      QRect r = option->rect;
      /* we don't check State_Horizontal because it may
         lead to wrong results (like in Qt4 Designer) */
      if (h < w)
      {
        painter->save();
        /* we enter x and y into our calculations because
           there may be more than one splitter handle */
        r.setRect(y, x, h, w);
        QTransform m;
        m.translate(0, 2*y+h);
        m.rotate(-90);
        m.translate(2*y+h, 0); m.scale(-1,1);
        painter->setTransform(m, true);
      }

      if (status.startsWith("disabled"))
      {
        status.replace(QString("disabled"),QString("normal"));
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      renderFrame(painter,r,fspec,fspec.element+"-"+status);
      renderInterior(painter,r,fspec,ispec,ispec.element+"-"+status);
      /* some UIs set the handle width */
      int iW = pixelMetric(PM_SplitterWidth)-fspec.left-fspec.right;
      if (iW > qMin(w,h)) iW = qMin(w,h);
      renderElement(painter,
                    dspec.element+"-"+status,
                    alignedRect(QApplication::layoutDirection(),
                                Qt::AlignCenter,
                                QSize(iW,dspec.size),
                                r),
                    0,0);
      if (!(option->state & State_Enabled))
        painter->restore();
      if (h < w)
        painter->restore();

      break;
    }

    case CE_ScrollBarAddLine :
    case CE_ScrollBarSubLine : {
      bool add = true;
      if (element == CE_ScrollBarSubLine)
        add = false;

      const QString group = "Scrollbar";

      const frame_spec fspec = getFrameSpec(group);
      const interior_spec ispec = getInteriorSpec(group);
      const indicator_spec dspec = getIndicatorSpec(group);

      QString iStatus = status; // indicator state
      if (!status.startsWith("disabled"))
      {
        const QStyleOptionSlider *opt = qstyleoption_cast<const QStyleOptionSlider *>(option);
        if (opt)
        {
#if QT_VERSION < 0x050000
          int sc = QStyle::SC_ScrollBarAddLine;
#else
          quint32 sc = QStyle::SC_ScrollBarAddLine;
#endif
          int limit = opt->maximum;
          if (!add)
          {
            sc = QStyle::SC_ScrollBarSubLine;
            limit = opt->minimum;
          }

          if (opt->sliderValue == limit)
            iStatus = "disabled";
          else if (opt->activeSubControls != sc)
            // don't focus the indicator when the cursor isn't on it
            iStatus = "normal";

          if (isInactive && !iStatus.endsWith("-inactive"))
            iStatus.append(QString("-inactive"));
        }
      }

      QRect r = option->rect;
      bool hrtl = false;
      if (option->state & State_Horizontal)
      {
        if (option->direction == Qt::RightToLeft)
          hrtl = true;
        painter->save();
        r.setRect(0, 0, h, w);
        QTransform m;
        m.translate(x, h);
        m.rotate(-90);
        m.translate(h, 0); m.scale(-1,1);
        painter->setTransform(m, true);
      }

      renderFrame(painter,r,fspec,fspec.element+"-normal");
      renderInterior(painter,r,fspec,ispec,ispec.element+"-normal");
      renderIndicator(painter,r,
                      fspec,dspec,
                      dspec.element+(add ?
                                       hrtl ? "-up-" : "-down-"
                                       : hrtl ? "-down-" : "-up-")
                                   +iStatus);

      if (option->state & State_Horizontal)
        painter->restore();

      break;
    }

    case CE_ScrollBarSlider : {
      QString sStatus = status; // slider state
      if (!status.startsWith("disabled"))
      {
        const QStyleOptionSlider *opt = qstyleoption_cast<const QStyleOptionSlider *>(option);
        if (opt && opt->activeSubControls != QStyle::SC_ScrollBarSlider)
        {
          sStatus = "normal";
          if (isInactive)
            sStatus = "normal-inactive";
        }
      }

      const QString group = "ScrollbarSlider";

      const frame_spec fspec = getFrameSpec(group);
      const interior_spec ispec = getInteriorSpec(group);
      const indicator_spec dspec = getIndicatorSpec(group);

      QRect r = option->rect;
      if (option->state & State_Horizontal)
      {
        /* the painter was saved at CC_ScrollBar,
           so no transformation here */
        r.setRect(y, x, h, w);
      }

      if (status.startsWith("disabled"))
      {
        sStatus.replace(QString("disabled"),QString("normal"));
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      renderFrame(painter,r,fspec,fspec.element+"-"+sStatus);
      renderInterior(painter,r,fspec,ispec,ispec.element+"-"+sStatus);
      renderElement(painter,
                    dspec.element+"-"+status, // let the grip change on mouse-over for the whole scrollbar
                    alignedRect(QApplication::layoutDirection(),
                                Qt::AlignCenter,
                                QSize(pixelMetric(PM_ScrollBarExtent)-fspec.left-fspec.right,dspec.size),
                                r),
                    0,0);
      if (!(option->state & State_Enabled))
        painter->restore();

      break;
    }

    case CE_HeaderSection : {
      const QString group = "HeaderSection";

      const frame_spec fspec = getFrameSpec(group);
      const interior_spec ispec = getInteriorSpec(group);

      if (status.startsWith("disabled"))
      {
        status.replace(QString("disabled"),QString("normal"));
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      renderFrame(painter,option->rect,fspec,fspec.element+"-"+status);
      renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+status);
      if (!(option->state & State_Enabled))
        painter->restore();

      break;
    }

    case CE_HeaderLabel : {
      const QStyleOptionHeader *opt =
        qstyleoption_cast<const QStyleOptionHeader *>(option);

      if (opt) {
        const QString group = "HeaderSection";

        frame_spec fspec = getFrameSpec(group);
        label_spec lspec = getLabelSpec(group);

        bool rtl(opt->direction == Qt::RightToLeft);

        if (opt->sortIndicator != QStyleOptionHeader::None)
        {
          if (rtl)
          {
            fspec.left = 0;
            lspec.left = 0;
          }
          else
          {
            fspec.right = 0;
            lspec.right = 0;
          }
        }

        /* for thin headers, like in Dolphin's details view */
        if (opt->icon.isNull())
        {
          fspec.top = fspec.bottom = lspec.top = lspec.bottom = 0;
        }

        int state = 1;
        if (status.startsWith("disabled"))
          state = 0;
        else if (status.startsWith("pressed"))
          state = 3;
        else if (status.startsWith("toggled"))
          state = 4;
        else if (option->state & State_MouseOver)
          state = 2;

        renderLabel(painter,option->palette,
                    option->rect.adjusted(rtl ?
                                            opt->sortIndicator != QStyleOptionHeader::None ?
                                             subElementRect(QStyle::SE_HeaderArrow,option,widget).width()
                                             +pixelMetric(PM_HeaderMargin) : 0
                                            : 0,
                                          0,
                                          rtl ?
                                            0
                                            : opt->sortIndicator != QStyleOptionHeader::None ?
                                               -subElementRect(QStyle::SE_HeaderArrow,option,widget).width()
                                               -pixelMetric(PM_HeaderMargin) : 0,
                                          0),
                    fspec,lspec,
                    opt->icon.isNull() ? opt->textAlignment | Qt::AlignVCenter : opt->textAlignment,
                    opt->text,QPalette::ButtonText,
                    state,
                    opt->icon.pixmap(pixelMetric(PM_SmallIconSize),iconmode,iconstate));
      }

      break;
    }

    case CE_ToolBar : {
      const QString group = "Toolbar";
      const frame_spec fspec = getFrameSpec(group);
      const interior_spec ispec = getInteriorSpec(group);
      const indicator_spec dspec = getIndicatorSpec(group);

      QRect r = option->rect;
      if (!(option->state & State_Horizontal))
      {
        r.setRect(0, 0, h, w);
        QTransform m;
        m.translate(w, 0);
        m.rotate(90);
        m.translate(0, w); m.scale(1,-1);
        painter->setTransform(m, true);
      }

      if (status.startsWith("disabled"))
      {
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      QString suffix = "-normal";
      if (isInactive)
        suffix = "-normal-inactive";
      renderFrame(painter,r,fspec,fspec.element+suffix);
      renderInterior(painter,r,fspec,ispec,ispec.element+suffix);
      if (!(option->state & State_Enabled))
        painter->restore();

      break;
    }

    case CE_SizeGrip : {
      const QString group = "SizeGrip";

      const frame_spec fspec = getFrameSpec(group);
      const interior_spec ispec = getInteriorSpec(group);
      const indicator_spec dspec = getIndicatorSpec(group);

      if (status.startsWith("disabled"))
      {
        status.replace(QString("disabled"),QString("normal"));
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      renderFrame(painter,option->rect,fspec,fspec.element+"-"+status);
      renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+status);
      if (!(option->state & State_Enabled))
      {
        painter->restore();
        status = "disabled";
        if (isInactive)
          status = "disabled-inactive";
      }
      renderIndicator(painter,option->rect,fspec,dspec,dspec.element+"-"+status);

      break;
    }

    case CE_PushButton : {
      const QStyleOptionButton *opt =
          qstyleoption_cast<const QStyleOptionButton *>(option);
      if (opt) {
        drawControl(QStyle::CE_PushButtonBevel,opt,painter,widget);
        QStyleOptionButton subopt(*opt);
        subopt.rect = subElementRect(SE_PushButtonContents,opt,widget);
        drawControl(QStyle::CE_PushButtonLabel,&subopt,painter,widget);
      }

      break;
    }

    case CE_PushButtonLabel : {
      const QStyleOptionButton *opt =
          qstyleoption_cast<const QStyleOptionButton *>(option);

      if (opt) {
        const QString group = "PanelButtonCommand";
        const frame_spec fspec = getFrameSpec(group);
        const indicator_spec dspec = getIndicatorSpec(group);
        label_spec lspec = getLabelSpec(group);
        if (isPlasma)
        {
          lspec.left = lspec.right = lspec.top = lspec.bottom = 0;
        }

        const QPushButton *pb = qobject_cast<const QPushButton *>(widget);
        if (!status.startsWith("disabled") && pb && pb->isDefault()) {
          QFont f(pb->font());
          f.setBold(true);
          painter->setFont(f);
        }

        /* opt->rect provided here is just for the label
           and not for the entire button. So, enlarge it!
           Also take into account the possibility of the presence of an indicator! */
        int ind = opt->features & QStyleOptionButton::HasMenu ? dspec.size+pixelMetric(PM_HeaderMargin) : 0;
        QRect r = option->rect.adjusted(-fspec.left + (opt->direction == Qt::RightToLeft ? ind : 0),
                                        -fspec.top,
                                        fspec.right - (opt->direction == Qt::RightToLeft ? 0 : ind),
                                        fspec.bottom);
        if (status.startsWith("toggled") || status.startsWith("pressed"))
        {
          int hShift = pixelMetric(PM_ButtonShiftHorizontal);
          int vShift = pixelMetric(PM_ButtonShiftVertical);
          r = r.adjusted(hShift,vShift,hShift,vShift);
        }
        int talign = Qt::AlignCenter | Qt::AlignVCenter;
        if (!styleHint(SH_UnderlineShortcut, opt, widget))
          talign |= Qt::TextHideMnemonic;
        else
          talign |= Qt::TextShowMnemonic;

        int state = 1;
        if (status.startsWith("disabled"))
          state = 0;
        else if (status.startsWith("pressed"))
          state = 3;
        else if (status.startsWith("toggled"))
          state = 4;
        else if (option->state & State_MouseOver)
          state = 2;

        renderLabel(painter,option->palette,
                    r,
                    fspec,lspec,
                    talign,opt->text,QPalette::ButtonText,
                    state,
                    opt->icon.pixmap(opt->iconSize,iconmode,iconstate));
      }

      break;
    }

    case CE_PushButtonBevel : { // bevel and indicator
      const QStyleOptionButton *opt =
          qstyleoption_cast<const QStyleOptionButton *>(option);

      if (opt) {
        const QString group = "PanelButtonCommand";
        const frame_spec fspec = getFrameSpec(group);
        const interior_spec ispec = getInteriorSpec(group);
        const indicator_spec dspec = getIndicatorSpec(group);
        const label_spec lspec = getLabelSpec(group);

        if (!(opt->features & QStyleOptionButton::Flat))
        {
          if (status.startsWith("disabled"))
          {
            status = "normal";
            if (option->state & State_On)
              status = "toggled";
            if (isInactive)
              status.append(QString("-inactive"));
            painter->save();
            painter->setOpacity(DISABLED_OPACITY);
          }
          renderFrame(painter,option->rect,fspec,fspec.element+"-"+status);
          renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+status);
          if (!(option->state & State_Enabled))
          {
            painter->restore();
            status = "disabled";
          }
        }

        if (opt->features & QStyleOptionButton::HasMenu)
        {
          QString aStatus = "normal";
          if (status.startsWith("disabled"))
            aStatus = "disabled";
          else if (status.startsWith("toggled") || status.startsWith("pressed"))
            aStatus = "pressed";
          else if (option->state & State_MouseOver)
            aStatus = "focused";
          if (isInactive)
            aStatus.append(QString("-inactive"));
          renderIndicator(painter,
                          option->rect.adjusted(opt->direction == Qt::RightToLeft ? lspec.left : 0,
                                                0,
                                                opt->direction == Qt::RightToLeft ? 0 : -lspec.right,
                                                0),
                          fspec,dspec,dspec.element+"-down-"+aStatus,
                          Qt::AlignRight | Qt::AlignVCenter);
        }

        const QPushButton *pb = qobject_cast<const QPushButton *>(widget);
        if (!status.startsWith("disabled") && pb && pb->isDefault())
        {
          renderFrame(painter,option->rect,fspec,fspec.element+"-default");
          renderIndicator(painter,
                          option->rect,
                          fspec,dspec,"button-default-indicator",
                          Qt::AlignBottom | (opt->direction == Qt::RightToLeft ?
                                             Qt::AlignLeft : Qt::AlignRight));
        }
      }

      break;
    }

    case CE_ToolButtonLabel : {
      const QStyleOptionToolButton *opt =
          qstyleoption_cast<const QStyleOptionToolButton *>(option);

      if (opt) {
        const QString group = "PanelButtonTool";
        frame_spec fspec = getFrameSpec(group);
        const indicator_spec dspec = getIndicatorSpec(group);
        label_spec lspec = getLabelSpec(group);
        if (isPlasma)
        {
          lspec.left = lspec.right = lspec.top = lspec.bottom = 0;
        }

        /* where there may not be enough space,
           especially in KDE new-stuff dialogs */
        if (qobject_cast<const QAbstractItemView*>(getParent(widget,2)))
        {
          fspec.left = qMin(fspec.left,3);
          fspec.right = qMin(fspec.right,3);
          fspec.top = qMin(fspec.top,3);
          fspec.bottom = qMin(fspec.bottom,3);

          lspec.left = qMin(lspec.left,2);
          lspec.right = qMin(lspec.right,2);
          lspec.top = qMin(lspec.top,2);
          lspec.bottom = qMin(lspec.bottom,2);
        }

        /* the right arrow is attached */
        if (const QToolButton *tb = qobject_cast<const QToolButton *>(widget))
        {
          if (tb->popupMode() == QToolButton::MenuButtonPopup
              || ((tb->popupMode() == QToolButton::InstantPopup
                   || tb->popupMode() == QToolButton::DelayedPopup)
                  && (opt->features & QStyleOptionToolButton::HasMenu)))
          {
            if (opt->direction == Qt::RightToLeft)
              fspec.left = 0;
            else
              fspec.right = 0;
          }

          /* respect the toolbar text color */
          const theme_spec tspec = settings->getThemeSpec();
          if (!tspec.group_toolbar_buttons
              && tb->autoRaise()
              && qobject_cast<const QToolBar *>(tb->parentWidget()))
          {
            const label_spec lspec1 = getLabelSpec("Toolbar");
            lspec.normalColor = lspec1.normalColor;
          }
        }

        const Qt::ToolButtonStyle tialign = opt->toolButtonStyle;
        /* Unlike in CE_PushButtonLabel, option->rect includes the whole
           button and not just its label here (-> CT_ToolButton)... */
        QRect r = option->rect;
        /* ... but this doesn't do any harm (and is good for
           centering text in framless buttons like in QtCreator's
           replace widget) because the text is centered. */
        if (tialign == Qt::ToolButtonTextOnly)
        {
          fspec.left = fspec.right = qMin(fspec.left,fspec.right);
          fspec.top = fspec.bottom = qMin(fspec.top,fspec.bottom);
          lspec.left = lspec.right = qMin(lspec.left,lspec.right);
          lspec.top = lspec.bottom = qMin(lspec.top,lspec.bottom);
          r.adjust(-fspec.left-lspec.left,
                   -fspec.top-lspec.top,
                   fspec.right+lspec.right,
                   fspec.bottom+lspec.bottom);
        }

        int talign = Qt::AlignCenter;
        if (!styleHint(SH_UnderlineShortcut, opt, widget))
          talign |= Qt::TextHideMnemonic;
        else
          talign |= Qt::TextShowMnemonic;

        /*
           Do NOT draw any label when all these conditions are satisfied
           because the button may have only an arrow inside it (which is
           treated as an icon, like in QtCreator's find widget):

             (1) The button style is icon-only;
             (2) There's no icon; but
             (3) There's an arrow.
        */
        Qt::Alignment iAlignment = Qt::AlignVCenter;
        if (tialign == Qt::ToolButtonIconOnly
            && opt->icon.isNull()
            && (opt->features & QStyleOptionToolButton::Arrow)
            && opt->arrowType != Qt::NoArrow)
        {
          iAlignment |= Qt::AlignHCenter;
        }
        else
        {
          int state = 1;
          if (status.startsWith("disabled"))
            state = 0;
          else if (status.startsWith("pressed"))
            state = 3;
          else if (status.startsWith("toggled"))
            state = 4;
          else if (option->state & State_MouseOver)
            state = 2;
          renderLabel(painter,option->palette,
                      !(opt->features & QStyleOptionToolButton::Arrow)
                          || opt->arrowType == Qt::NoArrow
                          || tialign == Qt::ToolButtonTextOnly ?
                        r : // may still have arrow because of MenuButtonPopup
                        // also add a margin between indicator and text (-> CT_ToolButton)
                        r.adjusted(opt->direction == Qt::RightToLeft ?
                                     0
                                     : dspec.size+lspec.tispace+pixelMetric(PM_HeaderMargin),
                                   0,
                                   opt->direction == Qt::RightToLeft ?
                                     -dspec.size-lspec.tispace-pixelMetric(PM_HeaderMargin)
                                     : 0,
                                   0),
                      fspec,lspec,
                      talign,opt->text,QPalette::ButtonText,
                      state,
                      opt->icon.pixmap(opt->iconSize,iconmode,iconstate),tialign);
          iAlignment |= Qt::AlignLeft;
        }

        /* we treat arrows as icons */
        if (!(opt->features & QStyleOptionToolButton::Arrow) || tialign == Qt::ToolButtonTextOnly)
          break;

        QString aStatus = "normal";
        if (status.startsWith("disabled"))
          aStatus = "disabled";
        else if (status.startsWith("toggled") || status.startsWith("pressed"))
          aStatus = "pressed";
        else if (option->state & State_MouseOver)
          aStatus = "focused";
        if (isInactive)
          aStatus.append(QString("-inactive"));
        if (!opt->text.isEmpty()) // it's empty for QStackedWidget
          r.adjust(lspec.left,lspec.top,-lspec.right,-lspec.bottom);
        switch (opt->arrowType) {
          case Qt::NoArrow :
            break;
          case Qt::UpArrow :
            renderIndicator(painter,r,
                            fspec,dspec,dspec.element+"-up-"+aStatus,
                            iAlignment);
            break;
          case Qt::DownArrow :
            renderIndicator(painter,r,
                            fspec,dspec,dspec.element+"-down-"+aStatus,
                            iAlignment);
            break;
          case Qt::LeftArrow :
            renderIndicator(painter,r,
                            fspec,dspec,dspec.element+"-left-"+aStatus,
                            iAlignment);
            break;
          case Qt::RightArrow :
            renderIndicator(painter,r,
                            fspec,dspec,dspec.element+"-right-"+aStatus,
                            iAlignment);
            break;
        }
      }

      break;
    }

    case CE_DockWidgetTitle : {
      const QStyleOptionDockWidget *opt =
          qstyleoption_cast<const QStyleOptionDockWidget *>(option);
      const QDockWidget *dw = qobject_cast<const QDockWidget *>(widget);

      if (opt) {
        const QString group = "DockTitle";

        frame_spec fspec = getFrameSpec(group);
        interior_spec ispec = getInteriorSpec(group);
        label_spec lspec = getLabelSpec(group);

        QRect r = option->rect;
        QRect tRect =subElementRect(SE_DockWidgetTitleBarText, option, widget);
        bool hasVertTitle = false;
        if (dw && (dw->features() & QDockWidget::DockWidgetVerticalTitleBar))
          hasVertTitle = true;

        if (hasVertTitle)
        {
          painter->save();
          r.setRect(0, 0, h, w);
          tRect.setRect(tRect.y(), tRect.x(),
                        tRect.height(), tRect.width());
          QTransform m;
          m.translate(w, 0);
          m.rotate(90);
          m.translate(0, w); m.scale(1,-1);
          painter->setTransform(m, true);
        }

        if (widget)
        {
          const QDockWidget *dw = qobject_cast<const QDockWidget *>(widget);
          if (dw)
          {
            QFont f(dw->font());
            f.setBold(true);
            painter->setFont(f);
          }
        }

        if (status.startsWith("disabled"))
        {
          status.replace(QString("disabled"),QString("normal"));
          painter->save();
          painter->setOpacity(DISABLED_OPACITY);
        }
        renderFrame(painter,r,fspec,fspec.element+"-"+status);
        renderInterior(painter,r,fspec,ispec,ispec.element+"-"+status);
        if (!(option->state & State_Enabled))
          painter->restore();

        if (hasVertTitle)
        {
          painter->save();
          QTransform m1;
          m1.translate(h, 0); m1.scale(-1,1);
          painter->setTransform(m1, true);
          /* because of the above transformations, the center
             of the text rectangle will be mirrored vertically
             if it isn't mirrored horizontally here */
          tRect.setRect(h-tRect.width()-tRect.x(), tRect.y(), tRect.width(), tRect.height());
        }
        
        /* text margins are already taken into
           account with PM_DockWidgetTitleMargin */
        fspec.left=fspec.right=fspec.top=fspec.bottom=0;
        lspec.left=lspec.right=lspec.top=lspec.bottom=0;
        
        QFontMetrics fm(painter->fontMetrics());
        QString title = fm.elidedText(opt->title, Qt::ElideRight, tRect.width());
        int talign = Qt::AlignHCenter | Qt::AlignVCenter;
        if (!styleHint(SH_UnderlineShortcut, opt, widget))
          talign |= Qt::TextHideMnemonic;
        else
          talign |= Qt::TextShowMnemonic;
        renderLabel(painter,option->palette,
                    tRect,
                    fspec,lspec,
                    talign,title,QPalette::WindowText,
                    option->state & State_Enabled ? option->state & State_MouseOver ? 2 : 1 : 0);

        if (hasVertTitle)
        {
          painter->restore();
          painter->restore();
        }
      }

      break;
    }

    case CE_RubberBand : {
      if(w > 0 && h > 0)
      {
        painter->save();
        QColor color = option->palette.color(QPalette::Active,QPalette::Highlight);
        painter->setClipRegion(option->rect);
        painter->setPen(color);
        color.setAlpha(50);
        painter->setBrush(color);
        painter->drawRect(option->rect.adjusted(0,0,-1,-1));
        painter->restore();
      }

      break;
    }

    case CE_ShapedFrame : {
      if (const QStyleOptionFrameV3 *f = qstyleoption_cast<const QStyleOptionFrameV3 *>(option))
      {
        /* skip ugly frames */
        if (f->frameShape != QFrame::HLine
            && f->frameShape != QFrame::VLine
            && (f->state & QStyle::State_Sunken || f->state & QStyle::State_Raised))
        {
          QCommonStyle::drawControl(element,option,painter,widget);
        }
      }

      break;
    }

    default : QCommonStyle::drawControl(element,option,painter,widget);
  }
}

void Kvantum::drawComplexControl(ComplexControl control,
                                 const QStyleOptionComplex *option,
                                 QPainter *painter,
                                 const QWidget *widget) const
{
  int x,y,h,w;
  option->rect.getRect(&x,&y,&w,&h);
  QString status =
        (option->state & State_Enabled) ?
          (option->state & State_On) ? "toggled" :
          (option->state & State_Sunken) ? "pressed" :
          (option->state & State_Selected) ? "toggled" :
          (option->state & State_MouseOver) ? "focused" : "normal"
        : "disabled";
  bool isInactive = false;
  if (widget && !widget->isActiveWindow())
  {
    isInactive = true;
    status.append(QString("-inactive"));
  }

  switch (control) {
    case CC_ToolButton : {
      const QStyleOptionToolButton *opt =
        qstyleoption_cast<const QStyleOptionToolButton *>(option);

      if (opt)
      {
        QStyleOptionToolButton o(*opt);

        o.rect = subControlRect(CC_ToolButton,opt,SC_ToolButton,widget);
        drawPrimitive(PE_PanelButtonTool,&o,painter,widget);
        drawPrimitive(PE_FrameButtonTool,&o,painter,widget);
        drawControl(CE_ToolButtonLabel,&o,painter,widget);

        if (widget)
        {
          const QToolButton *tb = qobject_cast<const QToolButton *>(widget);
          if (tb)
          {
            o.rect = subControlRect(CC_ToolButton,opt,SC_ToolButtonMenu,widget);
            if (tb->popupMode() == QToolButton::MenuButtonPopup)
              drawPrimitive(PE_IndicatorButtonDropDown,&o,painter,widget);
            else if ((tb->popupMode() == QToolButton::InstantPopup
                      || tb->popupMode() == QToolButton::DelayedPopup)
                     && (opt->features & QStyleOptionToolButton::HasMenu))
            {
              frame_spec fspec = getFrameSpec("PanelButtonTool");
              const indicator_spec dspec = getIndicatorSpec("PanelButtonTool");
              fspec.right = fspec.left = 0;
              // -> CE_ToolButtonLabel
              if (qobject_cast<const QAbstractItemView*>(getParent(widget,2)))
              {
                fspec.top = qMin(fspec.top,3);
                fspec.bottom = qMin(fspec.bottom,3);
              }
              QString aStatus = "normal";
              if (status.startsWith("disabled"))
                aStatus = "disabled";
              else if (status.startsWith("toggled") || status.startsWith("pressed"))
                aStatus = "pressed";
              else if (option->state & State_MouseOver)
                aStatus = "focused";
              if (isInactive)
                aStatus.append(QString("-inactive"));
              renderIndicator(painter,
                              o.rect,
                              fspec,dspec,
                              dspec.element+"-down-"+aStatus,
                              Qt::AlignLeft | Qt::AlignVCenter);
            }
          }
        }
      }

      break;
    }

    case CC_SpinBox : {
      const QStyleOptionSpinBox *opt =
        qstyleoption_cast<const QStyleOptionSpinBox *>(option);

      if (opt) {
        QStyleOptionSpinBox o(*opt);

        /* The field is automatically drawn as lineedit in PE_FrameLineEdit
           and PE_PanelLineEdit. Therefore, we shouldn't duplicate it here. */
        /*o.rect = subControlRect(CC_SpinBox,opt,SC_SpinBoxEditField,widget);
        drawPrimitive(PE_FrameLineEdit,&o,painter,widget);
        drawPrimitive(PE_PanelLineEdit,&o,painter,widget);*/

        if (opt->buttonSymbols == QAbstractSpinBox::UpDownArrows) {
          o.rect = subControlRect(CC_SpinBox,opt,SC_SpinBoxUp,widget);
          drawPrimitive(PE_IndicatorSpinUp,&o,painter,widget);
          o.rect = subControlRect(CC_SpinBox,opt,SC_SpinBoxDown,widget);
          drawPrimitive(PE_IndicatorSpinDown,&o,painter,widget);
        }
        else if (opt->buttonSymbols == QAbstractSpinBox::PlusMinus) {
          o.rect = subControlRect(CC_SpinBox,opt,SC_SpinBoxUp,widget);
          drawPrimitive(PE_IndicatorSpinPlus,&o,painter,widget);
          o.rect = subControlRect(CC_SpinBox,opt,SC_SpinBoxDown,widget);
          drawPrimitive(PE_IndicatorSpinMinus,&o,painter,widget);
        }
      }

      break;
    }

    case CC_ComboBox : {
      const QStyleOptionComboBox *opt =
          qstyleoption_cast<const QStyleOptionComboBox *>(option);

      if (opt) {
        if (option->state & State_Selected)
          status.replace(QString("toggled"),QString("normal"));

        bool rtl(opt->direction == Qt::RightToLeft);
        QStyleOptionComboBox o(*opt);

        const QString group = "ComboBox";

        frame_spec fspec = getFrameSpec(group);
        fspec.hasCapsule = true;
        fspec.capsuleH = rtl ? 1 : -1;
        fspec.capsuleV = 2;
        const interior_spec ispec = getInteriorSpec(group);

        int margin = 0; // see CC_ComboBox at subControlRect
        if (opt->editable && !opt->currentIcon.isNull())
          margin = fspec.left+fspec.right;
        else if (isLibreoffice)
          margin = fspec.left;
        // SC_ComboBoxEditField includes the icon too
        o.rect = subControlRect(CC_ComboBox,opt,SC_ComboBoxEditField,widget)
                 .adjusted(rtl ? 0 : -margin,
                           0,
                           rtl ? margin : 0,
                           0);

        if (status.startsWith("disabled"))
        {
          status.replace(QString("disabled"),QString("normal"));
          painter->save();
          painter->setOpacity(DISABLED_OPACITY);
        }
        if (isLibreoffice && opt->editable)
        {
          painter->fillRect(o.rect, option->palette.brush(QPalette::Base));
          const frame_spec fspec1 = getFrameSpec("LineEdit");
          renderFrame(painter,o.rect,fspec,fspec1.element+"-normal");
        }
        else
        {
          /* don't cover the lineedit area */
          int editWidth = 0;
          if (const QComboBox *cb = qobject_cast<const QComboBox*>(widget))
          {
            if (cb->lineEdit())
              editWidth = cb->lineEdit()->width();
          }
          QRect r = o.rect.adjusted(rtl ? editWidth : 0, 0, rtl ? 0 : -editWidth, 0);
          renderFrame(painter,r,fspec,fspec.element+"-"+status);
          renderInterior(painter,r,fspec,ispec,ispec.element+"-"+status);
        }
        if (!(option->state & State_Enabled))
        {
          painter->restore();
          status = "disabled";
        }

        /* when the combo box is editable, the edit field is drawn
           but the icon is missing (edit fields do not have icons)
           so we draw the icon here and center it */
        if (opt->editable && !opt->currentIcon.isNull())
        {
          const QIcon::Mode iconmode =
            (option->state & State_Enabled) ?
            (option->state & State_Sunken) ? QIcon::Active :
            (option->state & State_MouseOver) ? QIcon::Active : QIcon::Normal
            : QIcon::Disabled;

          const QIcon::State iconstate =
            (option->state & State_On) ? QIcon::On : QIcon::Off;

          int talign = Qt::AlignHCenter | Qt::AlignVCenter;
          if (!styleHint(SH_UnderlineShortcut, opt, widget))
            talign |= Qt::TextHideMnemonic;
          else
            talign |= Qt::TextShowMnemonic;
          label_spec lspec;
          default_label_spec(lspec);
          if (rtl)
            fspec.left = 0;
          else
            fspec.right = 0;
          int labelWidth = 0;
          if (const QComboBox *cb = qobject_cast<const QComboBox*>(widget))
          {
            if (cb->lineEdit())
              labelWidth = rtl ? o.rect.width()-cb->lineEdit()->width() : cb->lineEdit()->x();
          }

          int state = 1;
          if (status.startsWith("disabled"))
            state = 0;
          else if (status.startsWith("pressed"))
            state = 3;
          else if (status.startsWith("toggled"))
            state = 4;
          else if (option->state & State_MouseOver)
            state = 2;

          renderLabel(painter,option->palette,
                      QRect(rtl ?
                              o.rect.x()+o.rect.width()-labelWidth
                              : o.rect.x(),
                            o.rect.y(), labelWidth, o.rect.height()),
                      fspec,lspec,
                      talign,"",QPalette::ButtonText,
                      state,
                      opt->currentIcon.pixmap(opt->iconSize,iconmode,iconstate));
        }

        o.rect = subControlRect(CC_ComboBox,opt,SC_ComboBoxArrow,widget);
        drawPrimitive(PE_IndicatorButtonDropDown,&o,painter,widget);

      }

      break;
    }

    case CC_ScrollBar : {
      const QStyleOptionSlider *opt =
        qstyleoption_cast<const QStyleOptionSlider *>(option);

      if (opt) {
        QStyleOptionSlider o(*opt);

        const QString group = "ScrollbarGroove";

        const frame_spec fspec = getFrameSpec(group);
        const interior_spec ispec = getInteriorSpec(group);

        o.rect = subControlRect(CC_ScrollBar,opt,SC_ScrollBarGroove,widget);
        QRect r = o.rect;
        if (option->state & State_Horizontal)
        {
          painter->save();
          int H = r.height();
          r.setRect(r.y(), r.x(), H, r.width());
          QTransform m;
          m.translate(0, H);
          m.rotate(-90);
          m.translate(H, 0); m.scale(-1,1);
          painter->setTransform(m, true);
        }

        if (status.startsWith("disabled"))
        {
          painter->save();
          painter->setOpacity(DISABLED_OPACITY);
        }
        QString suffix = "-normal";
        if (isInactive)
          suffix = "-normal-inactive";
        renderFrame(painter,r,fspec,fspec.element+suffix);
        renderInterior(painter,r,fspec,ispec,ispec.element+suffix);
        if (!(option->state & State_Enabled))
          painter->restore();

        /* to not need any transformation for the
           horizontal state later, we draw the slider
           here, beforing restoring the painter */
        o.rect = subControlRect(CC_ScrollBar,opt,SC_ScrollBarSlider,widget);

        /* but we draw the glow first because the
           slider may be rounded at top or bottom */
        if (!status.startsWith("disabled"))
        {
          const frame_spec sFspec = getFrameSpec("ScrollbarSlider");
          int glowH = 2*pixelMetric(PM_ScrollBarExtent);
          int topGlowY, bottomGlowY, topGlowH, bottomGlowH;
          if (option->state & State_Horizontal)
          {
            topGlowY = qMax(o.rect.x()-glowH, r.y()+fspec.top);
            bottomGlowY = o.rect.x()+o.rect.width()-sFspec.bottom;
            topGlowH = o.rect.x()+sFspec.top-topGlowY;
          }
          else
          {
            topGlowY = qMax(o.rect.y()-glowH, r.y()+fspec.top);
            bottomGlowY = o.rect.y()+o.rect.height()-sFspec.bottom;
            topGlowH = o.rect.y()+sFspec.top-topGlowY;
          }
          bottomGlowH = glowH+sFspec.bottom - qMax(bottomGlowY+glowH+sFspec.bottom - (r.y()+r.height()-fspec.bottom), 0);
          QRect topGlow(r.x()+fspec.left,
                        topGlowY,
                        r.width()-fspec.left-fspec.right,
                        topGlowH);
          QRect bottomGlow(r.x()+fspec.left,
                           bottomGlowY,
                           r.width()-fspec.left-fspec.right,
                           bottomGlowH);
          renderElement(painter,ispec.element+"-topglow"+suffix,topGlow);
          renderElement(painter,ispec.element+"-bottomglow"+suffix,bottomGlow);
        }

        drawControl(CE_ScrollBarSlider,&o,painter,widget);

        if (option->state & State_Horizontal)
          painter->restore();

        o.rect = subControlRect(CC_ScrollBar,opt,SC_ScrollBarAddLine,widget);
        drawControl(CE_ScrollBarAddLine,&o,painter,widget);

        o.rect = subControlRect(CC_ScrollBar,opt,SC_ScrollBarSubLine,widget);
        drawControl(CE_ScrollBarSubLine,&o,painter,widget);
      }

      break;
    }

    case CC_Slider : {
      const QStyleOptionSlider *opt =
        qstyleoption_cast<const QStyleOptionSlider *>(option);

      if (opt)
      {
        QString group = "Slider";

        frame_spec fspec = getFrameSpec(group);
        interior_spec ispec = getInteriorSpec(group);

        QRect grooveRect = subControlRect(CC_Slider,opt,SC_SliderGroove,widget);
        QRect empty = grooveRect;
        QRect full = grooveRect;
        QRect slider = subControlRect(CC_Slider,opt,SC_SliderHandle,widget);
        QPoint sliderCenter = slider.center();

        // take into account the inversion
        if (option->state & State_Horizontal)
        {
          if (!opt->upsideDown) {
            full.setWidth(sliderCenter.x());
            empty.adjust(sliderCenter.x(),0,0,0);
          } else {
            empty.setWidth(sliderCenter.x());
            full.adjust(sliderCenter.x(),0,0,0);
          }
        }
        else
        {
          if (!opt->upsideDown) {
            full.setHeight(sliderCenter.y());
            empty.adjust(0,sliderCenter.y(),0,0);
          } else {
            empty.setHeight(sliderCenter.y());
            full.adjust(0,sliderCenter.y(),0,0);
          }
        }

        fspec.hasCapsule = true;
        fspec.capsuleH = 2;

        /* with a bit of visualization, we can get the
           horizontal bars from the vertical ones */
        if (option->state & State_Horizontal)
        {
          int H = empty.height();
          painter->save();
          grooveRect.setRect(grooveRect.y(), grooveRect.x(),
                             grooveRect.height(), grooveRect.width());
          if (!opt->upsideDown)
          {
            empty.setRect(empty.y(), sliderCenter.x(), H, empty.width());
            full.setRect(full.y(), full.x(), H, sliderCenter.x());
          }
          else
          {
            empty.setRect(empty.y(), empty.x(), H, sliderCenter.x());
            full.setRect(full.y(), sliderCenter.x(), H, full.width());
          }
          QTransform m;
          m.translate(0, H);
          m.rotate(-90);
          m.translate(H, 0); m.scale(-1,1);
          painter->setTransform(m, true);
        }

        QString suffix = "-normal";
        if (isInactive)
          suffix = "-normal-inactive";
        if (option->state & State_Enabled)
        {
          if (!opt->upsideDown)
            fspec.capsuleV = 1;
          else
            fspec.capsuleV = -1;
          renderFrame(painter,empty,fspec,fspec.element+suffix);
          renderInterior(painter,empty,fspec,ispec,ispec.element+suffix);
          if (!opt->upsideDown)
            fspec.capsuleV = -1;
          else
            fspec.capsuleV = 1;
          suffix.replace(QString("normal"),QString("toggled"));
          renderFrame(painter,full,fspec,fspec.element+suffix);
          renderInterior(painter,full,fspec,ispec,ispec.element+suffix);
        }
        else
        {
          painter->save();
          painter->setOpacity(DISABLED_OPACITY);

          fspec.hasCapsule = false;
          renderFrame(painter,grooveRect,fspec,fspec.element+suffix);
          renderInterior(painter,grooveRect,fspec,ispec,ispec.element+suffix);

          painter->restore();
        }

        if (option->state & State_Horizontal)
          painter->restore();

        const int len = pixelMetric(PM_SliderLength,option,widget);
        const int thick = pixelMetric(PM_SliderControlThickness,option,widget);

        /* slider ticks */
        QRect r = option->rect;
        if (option->state & State_Horizontal)
        {
          painter->save();
          r.setRect(y, x, h, w);
          QTransform m;
          m.translate(0, 2*y+h);
          m.rotate(-90);
          m.translate(2*y+h, 0); m.scale(-1,1);
          painter->setTransform(m, true);
        }
        if (status.startsWith("disabled"))
        {
          painter->save();
          painter->setOpacity(0.4);
        }
        suffix = "-normal";
        if (isInactive)
          suffix = "-normal-inactive";
        /* since we set the default size for CT_Slider, we use this
           to have no space between the slider's ticks and its handle */
        int extra = (r.width() - pixelMetric(PM_SliderThickness,option,widget))/2;
        int interval = opt->tickInterval;
        if (interval <= 0)
          interval = opt->pageStep;
        int available = r.height() - len;
        int min = opt->minimum;
        int max = opt->maximum;
        if (max == 99) max = 100; // to get the end tick
        if (opt->tickPosition & QSlider::TicksAbove)
        {
          QRect tickRect(r.x() + extra,
                         r.y(),
                         SLIDER_TICK_SIZE,
                         r.height());
          renderSliderTick(painter,ispec.element+"-tick"+suffix,
                           tickRect,
                           interval,available,min,max,
                           true,
                           opt->upsideDown);
        }
        if (opt->tickPosition & QSlider::TicksBelow)
        {
          QRect tickRect(r.x()+r.width()-SLIDER_TICK_SIZE - extra,
                         r.y(),
                         SLIDER_TICK_SIZE,
                         r.height());
          renderSliderTick(painter,ispec.element+"-tick"+suffix,
                           tickRect,
                           interval,available,min,max,
                           false,
                           opt->upsideDown);
        }
        if (!(option->state & State_Enabled))
          painter->restore();
        if (option->state & State_Horizontal)
          painter->restore();

        /* slider handle */
        group = "SliderCursor";
        fspec = getFrameSpec(group);
        ispec = getInteriorSpec(group);

        r = subControlRect(CC_Slider,opt,SC_SliderHandle,widget);
        /* derive other handles from the
           main one only when necessary */
        bool derive = false;
        if (len != thick)
        {
          if (option->state & State_Horizontal)
          {
            derive = true;
            painter->save();
            int sY = r.y();
            int sH = r.height();
            r.setRect(sY, r.x(), sH, r.width());
            QTransform m;
            if (opt->tickPosition == QSlider::TicksAbove)
            {
              m.translate(0, 2*sY+sH);
              m.scale(1,-1);
            }
            m.translate(0, 2*sY+sH);
            m.rotate(-90);
            m.translate(2*sY+sH, 0); m.scale(-1,1);
            painter->setTransform(m, true);
          }
          else if (opt->tickPosition == QSlider::TicksAbove)
          {
            derive = true;
            painter->save();
            QTransform m;
            m.translate(2*r.x()+r.width(), 0);
            m.scale(-1,1);
            painter->setTransform(m, true);
          }
        }

        renderFrame(painter,r,fspec,fspec.element+"-"+status);
        renderInterior(painter,r,fspec,ispec,ispec.element+"-"+status);

        // a decorative indicator if its element exists
        const indicator_spec dspec = getIndicatorSpec(group);
        renderIndicator(painter,r,fspec,dspec,dspec.element+"-"+status);

        if (derive)
          painter->restore();
      }

      break;
    }

    case CC_Dial : {
      const QStyleOptionSlider *opt =
          qstyleoption_cast<const QStyleOptionSlider *>(option);

      if (opt)
      {
        QRect dial(subControlRect(CC_Dial,opt,SC_DialGroove,widget));
        QRect handle(subControlRect(CC_Dial,opt,SC_DialHandle,widget));

        QString suffix;
        if (isInactive)
          suffix = "-inactive";

        renderElement(painter,"dial"+suffix,dial);
        renderElement(painter,"dial-handle"+suffix,handle);
        
        if (widget)
        {
          const QDial *d = qobject_cast<const QDial *>(widget);
          if (d && d->notchesVisible())
            renderElement(painter,"dial-notches"+suffix,dial);
        }
      }

      break;
    }

    case CC_TitleBar : {
      const QStyleOptionTitleBar *opt =
        qstyleoption_cast<const QStyleOptionTitleBar *>(option);

      if (opt) {
        int ts = opt->titleBarState;
        const QString tbStatus =
              (ts & Qt::WindowActive) ? "focused" : "normal";

        QStyleOptionTitleBar o(*opt);

        const QString group = "TitleBar";
        const frame_spec fspec = getFrameSpec(group);
        const interior_spec ispec = getInteriorSpec(group);
        const label_spec lspec = getLabelSpec(group);

        // SH_TitleBar_NoBorder is set to be true
        //renderFrame(painter,o.rect,fspec,fspec.element+"-"+status);
        renderInterior(painter,o.rect,fspec,ispec,ispec.element+"-"+tbStatus);

        o.rect = subControlRect(CC_TitleBar,opt,SC_TitleBarLabel,widget);
        QFontMetrics fm(painter->fontMetrics());
        QString title = fm.elidedText(o.text, Qt::ElideRight,
                                      o.rect.width()-(pixelMetric(PM_TitleBarHeight)-4+lspec.tispace)
                                                    // titlebars have no frame
                                                    -lspec.right-lspec.left);
        renderLabel(painter,option->palette,
                    o.rect,
                    fspec,lspec,
                    Qt::AlignLeft | Qt::AlignVCenter,title,QPalette::WindowText,
                    tbStatus == "normal" ? 1 : 2,
                    o.icon.pixmap(pixelMetric(PM_TitleBarHeight) - 4)); // 2-px margins for the icon

        indicator_spec dspec = getIndicatorSpec(group);
        Qt::WindowFlags tf = opt->titleBarFlags;

        renderIndicator(painter,
                        subControlRect(CC_TitleBar,opt,SC_TitleBarCloseButton,widget),
                        fspec,dspec,
                        dspec.element+"-close-"
                          + ((opt->activeSubControls & QStyle::SC_TitleBarCloseButton) ?
                              (option->state & State_Sunken) ? "pressed" : "focused"
                                : tbStatus == "focused" ? "normal" : "disabled"));
        if (tf & Qt::WindowMaximizeButtonHint)
          renderIndicator(painter,
                          subControlRect(CC_TitleBar,opt,SC_TitleBarMaxButton,widget),
                          fspec,dspec,
                          dspec.element+"-maximize-"
                            + ((opt->activeSubControls & QStyle::SC_TitleBarMaxButton) ?
                                (option->state & State_Sunken) ? "pressed" : "focused"
                                  : tbStatus == "focused" ? "normal" : "disabled"));
        if (!(ts & Qt::WindowMinimized))
        {
          if (tf & Qt::WindowMinimizeButtonHint)
            renderIndicator(painter,
                            subControlRect(CC_TitleBar,opt,SC_TitleBarMinButton,widget),
                            fspec,dspec,
                            dspec.element+"-minimize-"
                              + ((opt->activeSubControls & QStyle::SC_TitleBarMinButton) ?
                                  (option->state & State_Sunken) ? "pressed" : "focused"
                                    : tbStatus == "focused" ? "normal" : "disabled"));
          if (tf & Qt::WindowShadeButtonHint)
            renderIndicator(painter,
                            subControlRect(CC_TitleBar,opt,SC_TitleBarShadeButton,widget),
                            fspec,dspec,
                            dspec.element+"-shade-"
                              + ((opt->activeSubControls & QStyle::SC_TitleBarShadeButton) ?
                                  (option->state & State_Sunken) ? "pressed" : "focused"
                                    : tbStatus == "focused" ? "normal" : "disabled"));
        }
        if (!(tf & Qt::WindowShadeButtonHint))
          renderIndicator(painter,
                          subControlRect(CC_TitleBar,opt,SC_TitleBarNormalButton,widget),
                          fspec,dspec,
                          dspec.element+"-restore-"
                            + ((opt->activeSubControls & QStyle::SC_TitleBarNormalButton) ?
                                (option->state & State_Sunken) ? "pressed" : "focused"
                                  : tbStatus == "focused" ? "normal" : "disabled"));
        else
          renderIndicator(painter,
                          subControlRect(CC_TitleBar,opt,SC_TitleBarUnshadeButton,widget),
                          fspec,dspec,
                          dspec.element+"-restore-"
                            + ((opt->activeSubControls & QStyle::SC_TitleBarUnshadeButton) ?
                                (option->state & State_Sunken) ? "pressed" : "focused"
                                  : tbStatus == "focused" ? "normal" : "disabled"));
      }

      break;
    }

    default : QCommonStyle::drawComplexControl(control,option,painter,widget);
  }
}

int Kvantum::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
  switch (metric) {
    case PM_ButtonMargin : return 0;
    case PM_ButtonShiftHorizontal :
    case PM_ButtonShiftVertical : return 1;

    case PM_DefaultFrameWidth : {
      QString group;
      if (qstyleoption_cast<const QStyleOptionButton *>(option))
        group = "PanelButtonCommand";
      else
        group = "GenericFrame";

      const frame_spec fspec = getFrameSpec(group);
      return qMax(qMax(fspec.top,fspec.bottom),qMax(fspec.left,fspec.right));
    }

    case PM_SpinBoxFrameWidth :
    case PM_ComboBoxFrameWidth : return 0;

    case PM_MdiSubWindowFrameWidth : return 4;
    case PM_MdiSubWindowMinimizedWidth : return 200;

    case PM_LayoutLeftMargin :
    case PM_LayoutRightMargin :
    case PM_LayoutTopMargin :
    case PM_LayoutBottomMargin : return 4;

    case PM_LayoutHorizontalSpacing :
    case PM_LayoutVerticalSpacing : return 2;

    case PM_MenuBarPanelWidth :
    case PM_MenuBarVMargin :
    case PM_MenuBarHMargin :  return 0;

    case PM_MenuBarItemSpacing : return 2;

    case PM_MenuPanelWidth : return 0;

    case PM_MenuHMargin : 
    case PM_MenuVMargin:
    case PM_MenuTearoffHeight : {
      const theme_spec tspec = settings->getThemeSpec();
      const frame_spec fspec = getFrameSpec("Menu");

      int v = qMax(fspec.top,fspec.bottom);
      v += tspec.menu_shadow_depth;
      int h = qMax(fspec.left,fspec.right);
      h += tspec.menu_shadow_depth;
      /* a margin > 2px could create ugly
         corners without compositing */
      if (!tspec.composite || isLibreoffice || subApp
          || (widget && widget->testAttribute(Qt::WA_X11NetWmWindowTypeMenu))) // torn off
      {
        v = qMin(2,v);
        h = qMin(2,h);
      }
      if (metric == PM_MenuTearoffHeight)
        /* we set the height of tearoff indicator to be 7px */
        return qMax(v,h) + 7;
      else
        return qMax(v,h);
    }

    case PM_ToolBarFrameWidth :
    case PM_ToolBarItemSpacing : return 0;
    case PM_ToolBarHandleExtent : return 8;
    case PM_ToolBarSeparatorExtent : {
      const indicator_spec dspec = getIndicatorSpec("Toolbar");
      return dspec.size ? dspec.size : 8;
    }

    case PM_TabBarTabHSpace :
    case PM_TabBarTabVSpace :
    case PM_TabBarBaseHeight :
    case PM_TabBarBaseOverlap :
    case PM_TabBarTabShiftHorizontal :
    case PM_TabBarTabShiftVertical :
    case PM_ScrollView_ScrollBarSpacing : return 0;
    case PM_TabBar_ScrollButtonOverlap : return 1;

    case PM_TabBarScrollButtonWidth : {
      const frame_spec fspec = getFrameSpec("Tab");
      int extra = fspec.left + fspec.right - 4;
      return (extra > 0 ? 24 + extra : 24);
    }

    case PM_ToolBarIconSize :
    case PM_ToolBarExtensionExtent : return 16;
    case PM_ToolBarItemMargin : {
      const frame_spec fspec = getFrameSpec("Toolbar");
      int v = qMax(fspec.top,fspec.bottom);
      int h = qMax(fspec.left,fspec.right);
      return qMax(v,h);
    }

    case PM_ButtonIconSize :
    case PM_TabBarIconSize :
    case PM_SmallIconSize : return 16;
    case PM_LargeIconSize : return 32;

    case PM_FocusFrameVMargin :
    case PM_FocusFrameHMargin :  {
      int margin = 0;
      /* This is for putting the viewitem's text and icon inside
         its (forced) frame. It also sets the text-icon spacing
         (-> Qt ->qcommonstyle.cpp). It seems that apart from
         viewitems, it's only used for CT_ComboBox, whose default
         size I don't use. */
      const QString group = "ItemView";
      const frame_spec fspec = getFrameSpec(group);
      const label_spec lspec = getLabelSpec(group);
      if (metric == PM_FocusFrameHMargin)
        margin += qMax(fspec.left+lspec.left, fspec.right+lspec.right);
      else
        margin += qMax(fspec.top+lspec.top, fspec.bottom+lspec.bottom);

      if (margin == 0) return 2;
      else return margin;
    }

    case PM_CheckBoxLabelSpacing :
    case PM_RadioButtonLabelSpacing : return 5;

    case PM_SplitterWidth : {
      const theme_spec tspec = settings->getThemeSpec();
      return tspec.splitter_width;
    }

    case PM_ScrollBarExtent : {
      const theme_spec tspec = settings->getThemeSpec();
      return tspec.scroll_width;
    }
    case PM_ScrollBarSliderMin : return 36;

    case PM_ProgressBarChunkWidth : return 20;

    /* total slider */
    case PM_SliderThickness : {
      int thickness = pixelMetric(PM_SliderControlThickness,option,widget);
      if (const QStyleOptionSlider *opt = qstyleoption_cast<const QStyleOptionSlider*>(option))
      {
        if (opt->tickPosition & QSlider::TicksAbove)
          thickness += SLIDER_TICK_SIZE;
        if (opt->tickPosition & QSlider::TicksBelow)
          thickness += SLIDER_TICK_SIZE;
      }
      return thickness;
    }

    /* slider handle */
    case PM_SliderLength : {
      const theme_spec tspec = settings->getThemeSpec();
      return tspec.slider_handle_length;
    }
    case PM_SliderControlThickness : {
      const theme_spec tspec = settings->getThemeSpec();
      return tspec.slider_handle_width;
    }

    /* the default is good, although we don't use it */
    /*case PM_SliderSpaceAvailable: {
      return QCommonStyle::pixelMetric(metric,option,widget);
    }*/

    /* this would be exactly SLIDER_TICK_SIZE if we didn't leave CT_Slider
       to have its default size but it has no effect in our calculations */
    /*case PM_SliderTickmarkOffset: {
      return SLIDER_TICK_SIZE;
    }*/

    case PM_DockWidgetFrameWidth : {
      /*QString group = "Dock";
      const frame_spec fspec = getFrameSpec(group);
      const label_spec lspec = getLabelSpec(group);

      int v = qMax(fspec.top+lspec.top,fspec.bottom+lspec.bottom);
      int h = qMax(fspec.left+lspec.left,fspec.right+lspec.right);
      return qMax(v,h);*/
      return 0;
    }

    case PM_DockWidgetTitleMargin : {
      const QString group = "DockTitle";
      const label_spec lspec = getLabelSpec(group);
      const frame_spec fspec = getFrameSpec(group);
      int v = qMax(lspec.top+fspec.top, lspec.bottom+fspec.bottom);
      int h = qMax(lspec.left+fspec.left, lspec.right+fspec.right);
      return qMax(v,h);
    }

    case PM_TitleBarHeight : {
      // respect the text margins
      QString group = "TitleBar";
      const label_spec lspec = getLabelSpec("TitleBar");
      int v = lspec.top + lspec.bottom;
      return qMax(widget ? widget->fontMetrics().lineSpacing()+v
                           : option ? option->fontMetrics.lineSpacing()+v : 0,
                  24);
    }

    case PM_TextCursorWidth : return 1;

    case PM_HeaderMargin : return 2;

    case PM_ToolTipLabelFrameWidth : {
      const theme_spec tspec = settings->getThemeSpec();
      const frame_spec fspec = getFrameSpec("ToolTip");

      int v = qMax(fspec.top,fspec.bottom);
      v += tspec.tooltip_shadow_depth;
      int h = qMax(fspec.left,fspec.right);
      h += tspec.tooltip_shadow_depth;
      /* a margin > 2px could create ugly
         corners without compositing */
      if (!tspec.composite || isLibreoffice || subApp || isSystemSettings)
      {
        v = qMin(2,v);
        h = qMin(2,h);
      }
      return qMax(v,h);
    }

    case PM_IndicatorWidth :
    case PM_IndicatorHeight :
    case PM_ExclusiveIndicatorWidth :
    case PM_ExclusiveIndicatorHeight : {
      const theme_spec tspec = settings->getThemeSpec();
      /* make exception for menuitems and viewitems */
      if (isLibreoffice
          || qstyleoption_cast<const QStyleOptionMenuItem *>(option)
          || qobject_cast<const QAbstractItemView*>(getParent(widget,2)))
      {
        return qMin(QCommonStyle::pixelMetric(PM_IndicatorWidth,option,widget),
                    tspec.check_size);
      }
      return tspec.check_size;
    }

    default : return QCommonStyle::pixelMetric(metric,option,widget);
  }
}

/*
  To make Qt5 windows translucent, we should first set the surface format
  of their native handles. However, Qt5 windows may NOT have native handles
  associated with them before having a valid winId().

  We could use setAttribute(Qt::WA_NativeWindow) to make widgets native but
  we don't want enforceNativeChildren(), which is used when WA_NativeWindow
  is set, because it would interfere with setTransientParent(). We only want
  to use reateTLExtra() and createTLSysExtra(). There are to ways for that:

  (1) Using of private headers, which isn't a good idea for obvious reasons;

  (2) Setting Qt::AA_DontCreateNativeWidgetSiblings, so that the method
      enforceNativeChildren() isn't used in setAttribute() (-> qwidget.cpp).
*/
void Kvantum::setSurfaceFormat(QWidget *widget) const
{
#if QT_VERSION < 0x050000
  Q_UNUSED(widget);
  return;
#else
  if (!widget || !widget->isWindow()
      || !settings->getThemeSpec().translucent_windows
      || isPlasma || isOpaque || subApp || isLibreoffice
      || widget->testAttribute(Qt::WA_WState_Created))
  {
    return;
  }

  QWindow *window = widget->windowHandle();
  if (!window)
  {
    bool noNativeSiblings = true;
    if (!qApp->testAttribute(Qt::AA_DontCreateNativeWidgetSiblings))
    {
      noNativeSiblings = false;
      qApp->setAttribute(Qt::AA_DontCreateNativeWidgetSiblings, true);
    }
    widget->setAttribute(Qt::WA_NativeWindow, true);
    window = widget->windowHandle();
    /* reverse the changes */
    widget->setAttribute(Qt::WA_NativeWindow, false);
    if (!noNativeSiblings)
      qApp->setAttribute(Qt::AA_DontCreateNativeWidgetSiblings, false);
  }
  if (window)
  {
    QSurfaceFormat format = window->format();
    format.setAlphaBufferSize(8);
    window->setFormat(format);
  }
#endif
}

int Kvantum::styleHint(StyleHint hint,
                       const QStyleOption *option,
                       const QWidget *widget,
                       QStyleHintReturn *returnData) const
{
  setSurfaceFormat(widget); /* FIXME Why here and nowhere else?
                                     Perhaps because of its use in qapplication.cpp. */

  switch (hint) {
    case SH_EtchDisabledText :
    case SH_DitherDisabledText :
    case SH_Menu_AllowActiveAndDisabled :
    case SH_MenuBar_AltKeyNavigation :
    case SH_ItemView_ShowDecorationSelected :
    case SH_ItemView_ArrowKeysNavigateIntoChildren : return false;

    case SH_ItemView_ActivateItemOnSingleClick : return singleClick;

    case SH_ToolButton_PopupDelay :
    case SH_Menu_SubMenuPopupDelay : return 250;
    case SH_Menu_Scrollable :
    case SH_Menu_SloppySubMenus : return true;
    /* when set to true, only the last submenu is
       hidden on clicking anywhere outside the menu */
    case SH_Menu_FadeOutOnHide : return false;
    
    case SH_ComboBox_ListMouseTracking :
    case SH_Menu_MouseTracking :
    case SH_MenuBar_MouseTracking : return true;

    case SH_TabBar_Alignment : {
      const theme_spec tspec = settings->getThemeSpec();
      if (tspec.left_tabs)
        return Qt::AlignLeft;
      else
        return Qt::AlignCenter;
    }

    //case SH_ScrollBar_BackgroundMode : return Qt::OpaqueMode;

    case SH_ScrollBar_ContextMenu :
    case SH_ScrollBar_LeftClickAbsolutePosition : return true;

    case SH_Slider_StopMouseOverSlider : return true;

    case SH_ScrollView_FrameOnlyAroundContents : return true;

    case SH_UnderlineShortcut:
      return (widget && itsShortcutHandler) ? itsShortcutHandler->showShortcut(widget) : true;

    case SH_TitleBar_NoBorder: return true;
    case SH_TitleBar_AutoRaise: return true;

    case SH_GroupBox_TextLabelVerticalAlignment : return Qt::AlignVCenter;

    case SH_GroupBox_TextLabelColor: {
    const QString status =
        (option->state & State_Enabled) ?
          (option->state & State_MouseOver) ? "focused" :
          (option->state & State_On) ? "pressed" :
          (option->state & State_Sunken) ? "pressed" : "normal"
        : "disabled";
      const label_spec lspec = getLabelSpec("GroupBox");
      QColor normalColor(lspec.normalColor);
      QColor focusColor(lspec.focusColor);
      QColor pressColor(lspec.pressColor);
      if (status == "normal")
      {
        if (normalColor.isValid())
          return QColor(normalColor).rgba();
      }
      else if (status == "focused")
      {
        if (focusColor.isValid())
          return QColor(focusColor).rgba();
      }
      else if (status == "pressed")
      {
        if (pressColor.isValid())
          return QColor(pressColor).rgba();
      }

      return QCommonStyle::styleHint(hint,option,widget,returnData);
    }

    // for the sake of consistency (-> Kvantum.h -> renderLabel())
    case SH_ToolButtonStyle : return Qt::ToolButtonTextBesideIcon;

    case SH_RubberBand_Mask : {
      const QStyleOptionRubberBand *opt = qstyleoption_cast<const QStyleOptionRubberBand*>(option);
      if (!opt) return true;
      if (QStyleHintReturnMask *mask = qstyleoption_cast<QStyleHintReturnMask*>(returnData))
      {
        mask->region = option->rect;
        mask->region -= option->rect.adjusted(1,1,-1,-1);
      }
      return true;
    }

    //case SH_DialogButtonLayout: return QDialogButtonBox::GnomeLayout;

    //case SH_SpinControls_DisableOnBounds: return true;

    default : return QCommonStyle::styleHint(hint,option,widget,returnData);
  }
}

QCommonStyle::SubControl Kvantum::hitTestComplexControl (ComplexControl control,
                                                         const QStyleOptionComplex *option,
                                                         const QPoint &position,
                                                         const QWidget *widget) const
{
  return QCommonStyle::hitTestComplexControl(control,option,position,widget);
}

QSize Kvantum::sizeFromContents (ContentsType type,
                                 const QStyleOption *option,
                                 const QSize &contentsSize,
                                 const QWidget *widget) const
{
  if (!option)
    return contentsSize;

  int x,y,w,h;
  option->rect.getRect(&x,&y,&w,&h);

  /*int fh = 14; // font height
  if (widget)
    fh = QFontMetrics(widget->font()).height();*/

  int cw = contentsSize.width();
  //int ch = contentsSize.height();

  QSize defaultSize = QCommonStyle::sizeFromContents(type,option,contentsSize,widget);
  QSize s = QSize(0,0);

  switch (type) {
    case CT_LineEdit : {
      QFont f = QApplication::font();
        if (widget)
          f = widget->font();

      const QString group = "LineEdit";

      const frame_spec fspec = getFrameSpec(group);
      /* the label spec isn't used anywhere */
      label_spec lspec;
      default_label_spec(lspec);
      const size_spec sspec = getSizeSpec(group);

      s = sizeCalculated(f,fspec,lspec,sspec,"W",QPixmap());
      s = QSize(s.width() < cw ? cw : s.width(),s.height());

      break;
    }

    case CT_SpinBox : {
      /*const QStyleOptionSpinBox *opt =
        qstyleoption_cast<const QStyleOptionSpinBox *>(option);

      if (opt) {
        QFont f = QApplication::font();
        if (widget)
          f = widget->font();

        const QString group = "LineEdit";

        const frame_spec fspec = getFrameSpec(group);
        const label_spec lspec = getLabelSpec(group);
        const size_spec sspec = getSizeSpec(group);

        if (widget)
        {
          const QSpinBox *sb = qobject_cast<const QSpinBox *>(widget);
          if (sb)
            s = sizeCalculated(f,fspec,lspec,sspec,sb->text()+QString("%1").arg(sb->maximum()),QPixmap())
                + QSize(2*SPIN_BUTTON_WIDTH,0);
          else
          {
            const QDoubleSpinBox *sb1 = qobject_cast<const QDoubleSpinBox *>(widget);
            if (sb1)
	          s = sizeCalculated(f,fspec,lspec,sspec,sb1->text()+QString("%1").arg(sb1->maximum()),QPixmap())
	              + QSize(2*SPIN_BUTTON_WIDTH,0);
	        else
	        {
              const QDateTimeEdit *sb2 = qobject_cast<const QDateTimeEdit *>(widget);
              if (sb2)
                s = sizeCalculated(f,fspec,lspec,sspec,sb2->displayFormat(),QPixmap())
                    + QSize(2*SPIN_BUTTON_WIDTH,0);
              else
                s = defaultSize;
	        }
          }
        }
        else
          s = defaultSize;
      }*/
      const frame_spec fspec = getFrameSpec("LineEdit");
      const frame_spec fspec1 = getFrameSpec("IndicatorSpinBox");
#if QT_VERSION < 0x050000
      s = defaultSize + QSize(fspec.left + fspec1.right,
                              (fspec1.top > fspec.top ? fspec1.top : 0)
                               + (fspec1.bottom > fspec.bottom ? fspec1.bottom : 0));

      /* This is a workaround for some apps (like Kdenlive with its
         TimecodeDisplay) that presuppose all spinboxes should have
         vertical buttons and set an insufficient minimum width for them. */
      if (widget)
      {
        const QAbstractSpinBox *sb = qobject_cast<const QAbstractSpinBox *>(widget);
        if (sb && sb->minimumWidth() > s.width() - 2*SPIN_BUTTON_WIDTH)
          s.rwidth() = sb->minimumWidth() + 2*SPIN_BUTTON_WIDTH;
      }
#else
      /* Qt4 added 35px to the width of contentsSize
         but Qt5 doesn't (-> qabstractspinbox.cpp) */
      s = defaultSize + QSize(fspec.left + fspec1.right + SPIN_BUTTON_WIDTH,
                              (fspec1.top > fspec.top ? fspec1.top : 0)
                               + (fspec1.bottom > fspec.bottom ? fspec1.bottom : 0));
      if (widget)
      {
        const QAbstractSpinBox *sb = qobject_cast<const QAbstractSpinBox *>(widget);
        if (sb && sb->minimumWidth() > s.width() - SPIN_BUTTON_WIDTH)
          s.rwidth() = sb->minimumWidth() + SPIN_BUTTON_WIDTH;
      }
#endif

      break;
    }

    case CT_ComboBox : {
      const QStyleOptionComboBox *opt =
          qstyleoption_cast<const QStyleOptionComboBox *>(option);

      if (opt) {
        const QString group = "ComboBox";
        const frame_spec fspec = getFrameSpec(group);
        const label_spec lspec = getLabelSpec(group);

        s = defaultSize + QSize(fspec.left+fspec.right + lspec.left+lspec.right,
                                fspec.top+fspec.bottom + lspec.top+lspec.bottom);
        const frame_spec fspec1 = getFrameSpec("DropDownButton");
        s += QSize(COMBO_ARROW_LENGTH + (opt->direction == Qt::RightToLeft ? fspec1.left : fspec1.right), 0);

        /* With an editable combo that has icon, we take into account
           the margins when positioning the icon, so we need an extra
           space here (see CC_ComboBox at subControlRect) but the text
           margins we added above are redundant now. */
        if (opt->editable && !opt->currentIcon.isNull())
          s += QSize(fspec.left+fspec.right - lspec.left-lspec.right, 0);

        /* consider the top and bottom frames
           of lineedits inside editable combos */
        if (opt->editable)
        {
          const frame_spec fspec2 = getFrameSpec("LineEdit");
          s.rheight() += (fspec2.top > fspec.top ? fspec2.top-fspec.top : 0)
                         + (fspec2.bottom > fspec.bottom ? fspec2.bottom-fspec.bottom : 0);
        }
      }

      break;
    }

    case CT_PushButton : {
      const QStyleOptionButton *opt =
        qstyleoption_cast<const QStyleOptionButton *>(option);

      if (opt) {
        const QString group = "PanelButtonCommand";

        const frame_spec fspec = getFrameSpec(group);
        const label_spec lspec = getLabelSpec(group);
        const indicator_spec dspec = getIndicatorSpec(group);

       /*
          Like with CT_ToolButton, don't use sizeCalculated()!
       */

        /* also take into account the possibility of the presence of an indicator */
        s = defaultSize
            + QSize(opt->features & QStyleOptionButton::HasMenu ? dspec.size+pixelMetric(PM_HeaderMargin) : 0, 0);

        /* Add the spacings! The frame widths are calculated
           as pixelMetric(PM_DefaultFrameWidth,option,widget). */
        const QString txt = opt->text;
        if (!txt.isEmpty() && !opt->icon.isNull())
          s = s + QSize(lspec.left+lspec.right + lspec.tispace,
                        lspec.top+lspec.bottom);
        else if (!txt.isEmpty())
          s = s + QSize(lspec.left+lspec.right,
                        lspec.top+lspec.bottom);
        else // a 2-px margin for icons
          s = s + QSize(4, 4);

        /* this was for KColorButton but apparently
           it isn't needed when sizeCalculated() isn't used */
        /*if (txt.size() == 0 && opt->icon.isNull())
        {
          int smallIconSize = pixelMetric(PM_SmallIconSize);
          s = QSize(s.width() < smallIconSize ? smallIconSize : s.width(),
                    s.height() < smallIconSize ? smallIconSize : s.height());
        }*/

        /* take in to account the boldness of default button text */
        if (!txt.isEmpty())
        {
          const QPushButton *pb = qobject_cast<const QPushButton *>(widget);
          if (pb/* && pb->isDefault()*/)
          {
            QFont f = pb->font();
            QSize s1 = textSize(f, txt);
            f.setBold(true);
            s = s + textSize(f, txt) - s1;
          }
        }
      }

      break;
    }

    case CT_RadioButton : {
      const QStyleOptionButton *opt =
        qstyleoption_cast<const QStyleOptionButton *>(option);

      if (opt) {
        QFont f = QApplication::font();
        if (widget)
          f = widget->font();

        const QString group = "RadioButton";

        const frame_spec fspec = getFrameSpec(group);
        const label_spec lspec = getLabelSpec(group);
        const size_spec sspec = getSizeSpec(group);

        /* add the height of radio button indicator to have a
           reasonable vertical distance between radio buttons */
	    bool hasLabel = true;
	    if (widget)
	    {
	      const QAbstractButton *ab = qobject_cast<const QAbstractButton *>(widget);
	      if (ab && ab->text().isEmpty())
	        hasLabel = false;
	    }
        s = sizeCalculated(f,fspec,lspec,sspec,opt->text,opt->icon.pixmap(opt->iconSize));
        int dw = defaultSize.width() - s.width();
        //int dh = defaultSize.height() - s.height();
        s = s + QSize((dw > 0 ? dw : 0) + (pixelMetric(PM_CheckBoxLabelSpacing) + pixelMetric(PM_ExclusiveIndicatorWidth)),
                      /*(dh > 0 ? dh : 0) +*/ (hasLabel ? 0 : pixelMetric(PM_ExclusiveIndicatorHeight)));
      }

      break;
    }

    case CT_CheckBox : {
      const QStyleOptionButton *opt =
        qstyleoption_cast<const QStyleOptionButton *>(option);

      if (opt) {
        QFont f = QApplication::font();
        if (widget)
          f = widget->font();

        const QString group = "CheckBox";

        const frame_spec fspec = getFrameSpec(group);
        const label_spec lspec = getLabelSpec(group);
        const size_spec sspec = getSizeSpec(group);

        /* add the height of checkbox indicator to have a
           reasonable vertical distance between checkboxes */
	    bool hasLabel = true;
	    if (widget)
	    {
	      const QAbstractButton *ab = qobject_cast<const QAbstractButton *>(widget);
	      if (ab && ab->text().isEmpty())
	        hasLabel = false;
	    }
        s = sizeCalculated(f,fspec,lspec,sspec,opt->text,opt->icon.pixmap(opt->iconSize));
        int dw = defaultSize.width() - s.width();
        //int dh = defaultSize.height() - s.height();
        s = s + QSize((dw > 0 ? dw : 0) + (pixelMetric(PM_CheckBoxLabelSpacing) + pixelMetric(PM_IndicatorWidth)),
                      /*(dh > 0 ? dh : 0) +*/ (hasLabel ? 0 : pixelMetric(PM_IndicatorHeight)));
      }

      break;
    }

    case CT_MenuItem : {
      const QStyleOptionMenuItem *opt =
        qstyleoption_cast<const QStyleOptionMenuItem *>(option);

      if (opt) {
        QFont f = QApplication::font();
        if (widget)
          f = widget->font();

        const QString group = "MenuItem";
        const frame_spec fspec = getFrameSpec(group);
        const label_spec lspec = getLabelSpec(group);
        const size_spec sspec = getSizeSpec(group);

        if (opt->menuItemType == QStyleOptionMenuItem::Separator)
          s = QSize(cw,10); /* FIXME there is no PM_MenuSeparatorHeight pixel metric */
        else
          s = sizeCalculated(f,fspec,lspec,sspec,opt->text,opt->icon.pixmap(opt->maxIconWidth));

        /* even when there's no icon, another menuitem may
           have icon and that's considered at CE_MenuItem,
           so take it into account here too because text-icon 
           spacing hasn't been added with sizeCalculated() */
        if(opt->icon.isNull())
          s.rwidth() += lspec.tispace;

        /* To have enough space between the text and the check
           button or arrow when there's no shortcut (like in
           Ark's settings menu), we add a small extra space.
           We also add 2px for the right margin. */
        int extra = textSize(f,"ww").width() + 2;

        if (opt->menuItemType == QStyleOptionMenuItem::SubMenu)
        {
          const indicator_spec dspec = getIndicatorSpec("IndicatorArrow");
          s.rwidth() += dspec.size + lspec.tispace
                        + extra;
          s.rheight() += (dspec.size > s.height() ? dspec.size : 0);
        }

        if (opt->checkType == QStyleOptionMenuItem::Exclusive
            || opt->checkType == QStyleOptionMenuItem::NonExclusive)
        {
          int cSize = pixelMetric(PM_IndicatorWidth,option,widget);
          s.rwidth() += cSize + lspec.tispace
                        + extra;
          s.rheight() += (cSize > s.height() ? cSize : 0);
        }
      }

      break;
    }

    case CT_MenuBarItem : {
      const QStyleOptionMenuItem *opt =
        qstyleoption_cast<const QStyleOptionMenuItem *>(option);

      if (opt) {
        QFont f = QApplication::font();
        if (widget)
          f = widget->font();

        QString group = "MenuBarItem";
        frame_spec fspec = getFrameSpec(group);
        const label_spec lspec = getLabelSpec(group);
        const size_spec sspec = getSizeSpec(group);

        s = sizeCalculated(f,fspec,lspec,sspec,opt->text,opt->icon.pixmap(opt->maxIconWidth));
      }

      break;
    }

    case CT_ToolButton : {
      const QStyleOptionToolButton *opt =
        qstyleoption_cast<const QStyleOptionToolButton *>(option);

      if (opt) {
        QFont f = QApplication::font();
        if (widget)
          f = widget->font();

        const QString group = "PanelButtonTool";
        frame_spec fspec = getFrameSpec(group);
        label_spec lspec = getLabelSpec(group);
        const indicator_spec dspec = getIndicatorSpec(group);

        // -> CE_ToolButtonLabel
        if (qobject_cast<const QAbstractItemView*>(getParent(widget,2)))
        {
          fspec.left = qMin(fspec.left,3);
          fspec.right = qMin(fspec.right,3);
          fspec.top = qMin(fspec.top,3);
          fspec.bottom = qMin(fspec.bottom,3);

          lspec.left = qMin(lspec.left,2);
          lspec.right = qMin(lspec.right,2);
          lspec.top = qMin(lspec.top,2);
          lspec.bottom = qMin(lspec.bottom,2);
        }

        const Qt::ToolButtonStyle tialign = opt->toolButtonStyle;

        // -> CE_ToolButtonLabel
        if (tialign == Qt::ToolButtonTextOnly)
        {
          fspec.left = fspec.right = qMin(fspec.left,fspec.right);
          fspec.top = fspec.bottom = qMin(fspec.top,fspec.bottom);
          lspec.left = lspec.right = qMin(lspec.left,lspec.right);
          lspec.top = lspec.bottom = qMin(lspec.top,lspec.bottom);
        }

        /*
           Don't use sizeCalculated() for calculating the size
           because the button may be vertical, like in digiKam.
        */

        s = defaultSize
            /* Unlike the case of CT_PushButton, the frame widths aren't taken
               into account yet. Qt seems to consider toolbuttons frameless,
               althought it may add 4px to their widths or heights
               (-> qtoolbutton.cpp -> QSize QToolButton::sizeHint() const). */
            + QSize(fspec.left+fspec.right, fspec.top+fspec.bottom)
            + QSize(!(opt->features & QStyleOptionToolButton::Arrow)
                        || opt->arrowType == Qt::NoArrow
                        || tialign == Qt::ToolButtonTextOnly ?
                      0 :
                      // also add a margin between indicator and text (-> CE_ToolButtonLabel)
                      dspec.size+lspec.tispace+pixelMetric(PM_HeaderMargin),
                    0);

        /* add the spacings if there's a text */
        if (!opt->text.isEmpty())
        {
          if (tialign == Qt::ToolButtonTextOnly /*|| tialign == Qt::ToolButtonIconOnly*/)
            s = s + QSize(lspec.left+lspec.right,
                          lspec.top+lspec.bottom);
          else if(tialign == Qt::ToolButtonTextBesideIcon)
            s = s + QSize(lspec.left+lspec.right + lspec.tispace,
                          lspec.top+lspec.bottom);
          else if(tialign == Qt::ToolButtonTextUnderIcon)
            s = s + QSize(lspec.left+lspec.right,
                          lspec.top+lspec.bottom + lspec.tispace);
        }

        if (widget)
        {
          const QToolButton *tb = qobject_cast<const QToolButton *>(widget);
          if (tb)
          {
            if (tb->popupMode() == QToolButton::MenuButtonPopup)
            {
              const QString group1 = "DropDownButton";
              const frame_spec fspec1 = getFrameSpec(group1);
              const indicator_spec dspec1 = getIndicatorSpec(group1);
              s.rwidth() += fspec1.left+fspec1.right+dspec1.size+2; /* Why doesn't this or any other value for s
                                                                       have any effect on Krita's KisToolButton? */
            }
            else if ((tb->popupMode() == QToolButton::InstantPopup
                      || tb->popupMode() == QToolButton::DelayedPopup)
                     && (opt->features & QStyleOptionToolButton::HasMenu))
            {
              s.rwidth() += lspec.tispace+dspec.size + pixelMetric(PM_HeaderMargin);
            }
          }
        }
      }

      break;
    }

    case CT_TabBarTab : {
      const QStyleOptionTab *opt =
        qstyleoption_cast<const QStyleOptionTab *>(option);

      if (opt) {
        QFont f = QApplication::font();
        if (widget)
          f = widget->font();

        const QString group = "Tab";
        const frame_spec fspec = getFrameSpec(group);
        const label_spec lspec = getLabelSpec(group);
        const size_spec sspec = getSizeSpec(group);

        s = sizeCalculated(f,fspec,lspec,sspec,opt->text,opt->icon.pixmap(pixelMetric(PM_ToolBarIconSize)));

        bool verticalTabs = false;
        if (opt->shape == QTabBar::RoundedEast
            || opt->shape == QTabBar::RoundedWest
            || opt->shape == QTabBar::TriangularEast
            || opt->shape == QTabBar::TriangularWest)
        {
          verticalTabs = true;
        }

        if (widget) {
          const QTabBar *tb = qobject_cast<const QTabBar*>(widget);
          if (tb && tb->tabsClosable())
            s.rwidth() += pixelMetric(verticalTabs ? PM_TabCloseIndicatorHeight : PM_TabCloseIndicatorWidth,
                                      option,widget)
                          + lspec.tispace;

          // tabButtons
          /*int tbh = 0;
          QRect tbRect = subElementRect(QStyle::SE_TabBarTabLeftButton,option,widget);
          s.rwidth() += tbRect.width();
          tbh = tbRect.height();

          tbRect = subElementRect(QStyle::SE_TabBarTabRightButton,option,widget)
          s.rwidth() += tbRect.width();
          int h = tbRect.height();
          if (h > tbh) tbh= h;

          if (tbh > s.height()) s.rheight() = tbh;*/
        }

        if (verticalTabs)
          s.transpose();

        // for Calligra Words
        int dw = defaultSize.width() - s.width();
        int dh = defaultSize.height() - s.height();
        if (!verticalTabs)
          s += QSize(dw > 0 ? dw + fspec.left+fspec.right+lspec.left+lspec.right : 0,
                     dh > 0 ? dh + fspec.top+fspec.bottom+lspec.top+lspec.bottom : 0);
        else
          s += QSize(dw > 0 ? dw + fspec.top+fspec.bottom+lspec.top+lspec.bottom : 0,
                     dh > 0 ? dh + fspec.left+fspec.right+lspec.left+lspec.right : 0);
      }

      break;
    }

    case CT_HeaderSection : {
      const QStyleOptionHeader *opt =
        qstyleoption_cast<const QStyleOptionHeader *>(option);

      if (opt) {
        QFont f = QApplication::font();
        if (widget)
          f = widget->font();

        const QString group = "HeaderSection";

        const frame_spec fspec = getFrameSpec(group);
        const label_spec lspec = getLabelSpec(group);
        const size_spec sspec = getSizeSpec(group);
        const indicator_spec dspec = getIndicatorSpec(group);

        s = sizeCalculated(f,fspec,lspec,sspec,opt->text,opt->icon.pixmap(pixelMetric(PM_SmallIconSize)));
        if (opt->sortIndicator != QStyleOptionHeader::None)
          s.rwidth() += dspec.size + pixelMetric(PM_HeaderMargin);
      }

      break;
    }

    /* digiKam doesn't like this calculation */
    /*case CT_Slider : {
      if (option->state & State_Horizontal)
        s = QSize(defaultSize.width(), pixelMetric(PM_SliderThickness,option,widget));
      else
        s = QSize(pixelMetric(PM_SliderThickness,option,widget), defaultSize.height());
      return s;
    }*/

    case CT_ItemViewItem : {
      /*
         This works alongside SE_ItemViewItemText.

         Margins are (partially) set with PM_FocusFrameHMargin and
         PM_FocusFrameVMargin by default (-> Qt -> qcommonstyle.cpp).
      */

      s = defaultSize;

      const QStyleOptionViewItem *opt =
          qstyleoption_cast<const QStyleOptionViewItem *>(option);
      if (opt)
      {
        const frame_spec fspec = getFrameSpec("ItemView");
        const label_spec lspec = getLabelSpec("ItemView");
        QStyleOptionViewItem::Position pos = opt->decorationPosition;

        s.rheight() += fspec.top + fspec.bottom;
        /* the width is already increased with PM_FocusFrameHMargin */
        //s.rwidth() += fspec.left + fspec.right;

        const QStyleOptionViewItemV2 *vopt =
            qstyleoption_cast<const QStyleOptionViewItemV2 *>(option);
        bool hasIcon = false;
        if (vopt && (vopt->features & QStyleOptionViewItemV2::HasDecoration))
          hasIcon = true;

        /* this isn't needed anymore because PM_FocusFrameHMargin and
           PM_FocusFrameVMargin are adjusted to put icon inside frame */
        /*if (hasIcon)
        {
          // put the icon inside the frame (->SE_ItemViewItemDecoration)
          s.rwidth() += fspec.left + fspec.right;
          s.rheight() += fspec.top + fspec.bottom;
          // forget about text-icon spacing because the text margin
          // is used for it automatically (-> Qt -> qcomonstyle.cpp)
          if (pos == QStyleOptionViewItem::Top || pos == QStyleOptionViewItem::Bottom)
            s.rheight() += lspec.tispace;
          else if (pos == QStyleOptionViewItem::Left || pos == QStyleOptionViewItem::Right)
            s.rwidth() += lspec.tispace;
        }*/

        Qt::Alignment align = opt->displayAlignment;

        if (align & Qt::AlignLeft)
        {
          if (!hasIcon || pos != QStyleOptionViewItem::Left)
            s.rwidth() += lspec.left;
        }
        else if (align & Qt::AlignRight)
        {
          if (!hasIcon || pos != QStyleOptionViewItem::Right)
            s.rwidth() += lspec.right;
        }
        else if (!hasIcon)
          s.rwidth() += lspec.left + lspec.right;

        if (align & Qt::AlignTop)
        {
          if (!hasIcon || pos != QStyleOptionViewItem::Top)
            s.rheight() += lspec.top;
        }
        else if (align & Qt::AlignBottom)
        {
          if (!hasIcon || pos != QStyleOptionViewItem::Bottom)
            s.rheight() += lspec.bottom;
        }
        else if (!hasIcon)
          s.rheight() += lspec.top + lspec.bottom;
      }

      // the item text may be inside a button like in Kate's font preferences (see SE_PushButtonContents)
      /*const QStyleOptionViewItem *opt =
          qstyleoption_cast<const QStyleOptionViewItem *>(option);
      if (opt)
      {
        const frame_spec fspec = getFrameSpec("ItemView");
        const frame_spec fspec1 = getFrameSpec("PanelButtonCommand");
        int h = opt->font.pointSize() + fspec.top + fspec.bottom + fspec1.top + fspec1.bottom;
        if (h > s.height())
          s.setHeight(h);
      }*/

      break;
    }

    case CT_TabWidget : {
      const frame_spec fspec = getFrameSpec("TabFrame");
      s = defaultSize + QSize(fspec.left+fspec.right,
                              fspec.top+fspec.bottom);

      break;
    }

    default : return defaultSize;
  }

  // I'm too cautious to not add this:
  return s.expandedTo(defaultSize);
}

QSize Kvantum::sizeCalculated(const QFont &font,
                              const frame_spec &fspec, // frame spec
                              const label_spec &lspec, // label spec
                              const size_spec &sspec, // size spec
                              const QString &text,
                              const QPixmap &icon,
                              // text-icon alignment
                              const Qt::ToolButtonStyle tialign) const
{
  QSize s;
  s.setWidth(fspec.left+fspec.right+lspec.left+lspec.right);
  s.setHeight(fspec.top+fspec.bottom+lspec.top+lspec.bottom);
  if (lspec.hasShadow) {
    s.rwidth() += lspec.xshift+lspec.depth;
    s.rheight() += lspec.yshift+lspec.depth;
  }

  QSize ts = textSize (font, text);
  int tw = ts.width();
  int th = ts.height();

  if (tialign == Qt::ToolButtonIconOnly) {
    s.rwidth() += icon.width();
    s.rheight() += icon.height();
  } else if (tialign == Qt::ToolButtonTextOnly) {
    s.rwidth() += tw;
    s.rheight() += th;
  } else if (tialign == Qt::ToolButtonTextBesideIcon) {
    s.rwidth() += (icon.isNull() ? 0 : icon.width()) + (icon.isNull() ? 0 : (text.isEmpty() ? 0 : lspec.tispace)) + tw;
    s.rheight() += qMax(icon.height(),th);
  } else if (tialign == Qt::ToolButtonTextUnderIcon) {
    s.rwidth() += qMax(icon.width(),tw);
    s.rheight() += icon.height() + (icon.isNull() ? 0 : lspec.tispace) + th;
  }

  if ( (sspec.minH > 0) && (s.height() < sspec.minH) )
    s.setHeight(sspec.minH);

  if ( (sspec.minW > 0) && (s.width() < sspec.minW) )
    s.setWidth(sspec.minW);

  return s;
}

QSize Kvantum::textSize (const QFont &font, const QString &text) const
{
  // compute width and height of text. QFontMetrics seems to ignore \n
  // and returns wrong values

  int tw = 0;
  int th = 0;

  if (!text.isEmpty()) {
    /* remove & mnemonic character and tabs (for menu items) */
    // FIXME don't remove & if it is not followed by a character
    QString t = QString(text).remove('\t');
    {
      int i=0;
      while ( i<t.size() ) {
        if ( t.at(i) == '&' ) {
          // see if next character is not a space
          if ( (i+1<t.size()) && (t.at(i+1) != ' ') ) {
            t.remove(i,1);
            i++;
          }
        }
        i++;
      }
    }

    /* deal with \n */
    QStringList l = t.split('\n');

    th = QFontMetrics(font).height()*(l.size());
    for (int i=0; i<l.size(); i++) {
      tw = qMax(tw,QFontMetrics(font).width(l[i]));
    }
  }

  return QSize(tw,th);
}

QRect Kvantum::subElementRect(SubElement element, const QStyleOption *option, const QWidget *widget) const
{
  switch (element) {
    case SE_CheckBoxFocusRect :
    case SE_RadioButtonFocusRect :
    case SE_ProgressBarGroove :
    case SE_HeaderLabel :
    case SE_ProgressBarLabel : return option->rect;

    case SE_HeaderArrow : {
      const QString group = "HeaderSection";
      const frame_spec fspec = getFrameSpec(group);
      const indicator_spec dspec = getIndicatorSpec(group);
      const label_spec lspec = getLabelSpec(group);

      return alignedRect(QApplication::layoutDirection(),
                         Qt::AlignRight,
                         QSize(option->direction == Qt::RightToLeft ?
                                 fspec.left+lspec.left+dspec.size
                                 : fspec.right+lspec.right+dspec.size,
                               option->rect.height()),
                         option->rect);
    }

    case SE_ProgressBarContents : {
      const theme_spec tspec = settings->getThemeSpec();
      if (tspec.spread_progressbar)
        return option->rect;

      frame_spec fspec = getFrameSpec("Progressbar");
      // the vertical progressbar will be made out of the horizontal one
      const QProgressBar *pb = qobject_cast<const QProgressBar *>(widget);
      if (pb && pb->orientation() == Qt::Vertical)
      {
        int top = fspec.top;
        fspec.top = fspec.right;
        int bottom = fspec.bottom;
        fspec.bottom = fspec.left;
        fspec.left = top;
        fspec.right = bottom;
      }

      return interiorRect(option->rect, fspec);
    }

    case SE_LineEditContents : {
      frame_spec fspec = getFrameSpec("LineEdit");
      /* no frame when editing itemview texts */
      if (qobject_cast<const QAbstractItemView*>(getParent(widget,2)))
      {
        fspec.left = fspec.right = fspec.top = fspec.bottom = 0;
      }
      QRect rect = interiorRect(option->rect, fspec);

      /* in these cases there are capsules */
      if (widget)
      {
        if (qobject_cast<const QComboBox*>(widget->parentWidget()))
          rect.adjust(option->direction == Qt::RightToLeft ? -fspec.left : 0,
                      0,
                      option->direction == Qt::RightToLeft ? 0 : fspec.right,
                      0);
        else if (qobject_cast<const QAbstractSpinBox*>(widget->parentWidget()))
          rect.adjust(0,0,fspec.right,0);
      }

      /* this is for editable view items */
      int h = QCommonStyle::subElementRect(element,option,widget).height();
      if (rect.height() < h)
        return rect.adjusted(0,-h/2,0,h/2);
      else
        return rect;
    }

    case SE_ItemViewItemText : {
      /*
         This works alongside CT_ItemViewItem.
      */

      QRect r = QCommonStyle::subElementRect(element,option,widget);
      const QStyleOptionViewItem *opt =
          qstyleoption_cast<const QStyleOptionViewItem *>(option);

      if (opt)
      {
        const QStyleOptionViewItemV2 *vopt =
            qstyleoption_cast<const QStyleOptionViewItemV2 *>(option);
        bool hasIcon = false;
        if (vopt && (vopt->features & QStyleOptionViewItemV2::HasDecoration))
          hasIcon = true;

        Qt::Alignment align = opt->displayAlignment;
        QStyleOptionViewItem::Position pos = opt->decorationPosition;
        const label_spec lspec = getLabelSpec("ItemView");

        /* The right and left text margins are added in
           PM_FocusFrameHMargin, so there's no need to this.
           They're always equal to each other because otherwise,
           eliding would be incorrect. They also set the
           horizontal text-icon spacing. */
        /*if (align & Qt::AlignLeft)
        {
          if (!hasIcon || pos != QStyleOptionViewItem::Left)
            r.adjust(lspec.left, 0, 0, 0);
        }
        else if (align & Qt::AlignRight)
        {
          if (!hasIcon || pos != QStyleOptionViewItem::Right)
            r.adjust(0, 0, -lspec.right, 0);
        }*/

        /* also add the top and bottom frame widths
           because they aren't added in qcommonstyle.cpp */
        const frame_spec fspec = getFrameSpec("ItemView");
        if (align & Qt::AlignTop)
        {
          if (!hasIcon || pos != QStyleOptionViewItem::Top)
            r.adjust(0, lspec.top+fspec.top, 0, 0);
        }
        else if (align & Qt::AlignBottom)
        {
          if (!hasIcon || pos != QStyleOptionViewItem::Bottom)
            r.adjust(0, 0, 0, -lspec.bottom-fspec.bottom);
        }

        if (hasIcon)
        {
          /* forget about text-icon spacing because the text
             margin is used for it (-> Qt -> qcomonstyle.cpp) */
          ;
          /*if (pos == QStyleOptionViewItem::Left)
            r.adjust(lspec.tispace, 0, lspec.tispace, 0);
          else if (pos == QStyleOptionViewItem::Right)
            r.adjust(-lspec.tispace, 0, -lspec.tispace, 0);
          else if (pos == QStyleOptionViewItem::Top)
            r.adjust(0, lspec.tispace, 0, lspec.tispace);
          else if (pos == QStyleOptionViewItem::Bottom)
            r.adjust(0, -lspec.tispace, 0, -lspec.tispace);*/
        }
        else
        {
          /* deal with the special case, where the text has no
             vertical alignment (a bug in the Qt file dialog?) */
          if (align == Qt::AlignRight
              || align == Qt::AlignLeft
              || align == Qt::AlignHCenter
              || align == Qt::AlignJustify)
          {
            const QStyleOptionViewItemV4 *vopt1 =
              qstyleoption_cast<const QStyleOptionViewItemV4 *>(option);
            if (vopt1)
            {
              QString txt = vopt1->text;
              if (!txt.isEmpty())
              {
                QStringList l = txt.split('\n');
                int txtHeight = QFontMetrics(opt->font).height()*(l.size());
                r = alignedRect(QApplication::layoutDirection(),
                    align | Qt::AlignVCenter,
                    QSize(r.width(), txtHeight),
                    r);
              }
            }
          }
        }
      }
      return r;
    }

    /* this isn't needed anymore because PM_FocusFrameHMargin and
       PM_FocusFrameVMargin are adjusted to put icons inside frame */
    /*case SE_ItemViewItemDecoration : {
      QRect r = QCommonStyle::subElementRect(element,option,widget);
      const QStyleOptionViewItem *opt =
          qstyleoption_cast<const QStyleOptionViewItem *>(option);
      if (opt)
      {
        // put the icon inside the frame
        const QStyleOptionViewItemV2 *vopt =
            qstyleoption_cast<const QStyleOptionViewItemV2 *>(option);
        if (vopt && (vopt->features & QStyleOptionViewItemV2::HasDecoration))
        {
          QStyleOptionViewItem::Position pos = opt->decorationPosition;
          const frame_spec fspec = getFrameSpec("ItemView");
          if (pos == QStyleOptionViewItem::Left)
            r.adjust(fspec.left, 0, fspec.left, 0);
          else if (pos == QStyleOptionViewItem::Right)
            r.adjust(-fspec.right, 0, -fspec.right, 0);
          else if (pos == QStyleOptionViewItem::Top)
            r.adjust(0, fspec.top, 0, fspec.top);
          else if (pos == QStyleOptionViewItem::Bottom)
            r.adjust(0, -fspec.bottom, 0, -fspec.bottom);
        }
      }
      return r;
    }*/

    case SE_PushButtonContents : {
      QRect r = QCommonStyle::subElementRect(element,option,widget);
      const QStyleOptionButton *opt =
          qstyleoption_cast<const QStyleOptionButton *>(option);
      // Kate's preferences for its default text style
      if (opt && !opt->text.isEmpty() && widget)
      {
        if (qobject_cast<const QAbstractItemView *>(widget))
        {
          const frame_spec fspec = getFrameSpec("PanelButtonCommand");
          const label_spec lspec = getLabelSpec("PanelButtonCommand");
          r.adjust(-fspec.left-lspec.left,
                   -fspec.top-lspec.top,
                   fspec.right+lspec.right,
                   fspec.bottom+lspec.bottom);
        }
      }
      return r;
    }

    case SE_TabWidgetTabContents : {
      const frame_spec fspec = getFrameSpec("TabFrame");
      return QCommonStyle::subElementRect(element,option,widget).adjusted(fspec.left,fspec.top,-fspec.right,-fspec.bottom);
    }

    default : return QCommonStyle::subElementRect(element,option,widget);
  }
}

QRect Kvantum::subControlRect(ComplexControl control,
                              const QStyleOptionComplex *option,
                              SubControl subControl,
                              const QWidget *widget) const
{
  int x,y,h,w;
  option->rect.getRect(&x,&y,&w,&h);

  switch (control) {
    case CC_TitleBar :
      switch (subControl) {
        case SC_TitleBarLabel : {
          // see qcommonstyle.cpp
          int delta = 0;
          if (const QStyleOptionTitleBar *tb = qstyleoption_cast<const QStyleOptionTitleBar *>(option))
            delta = tb->rect.height() - 2;
          return QCommonStyle::subControlRect(control,option,subControl,widget)
                               .adjusted(option->direction == Qt::RightToLeft ?
                                           0
                                           : -delta,
                                         0,
                                         option->direction == Qt::RightToLeft ?
                                           delta
                                           : 0,
                                         0);
        }
        case SC_TitleBarCloseButton :
        case SC_TitleBarMaxButton :
        case SC_TitleBarMinButton :
        case SC_TitleBarShadeButton :
        case SC_TitleBarNormalButton :
        case SC_TitleBarUnshadeButton :
        case SC_TitleBarSysMenu :
        case SC_TitleBarContextHelpButton : {
          // level the buttons with the title
          const label_spec lspec = getLabelSpec("TitleBar");
          int v = (lspec.top - lspec.bottom)/2;
          return QCommonStyle::subControlRect(control,option,subControl,widget).adjusted(0, v, 0, v);
        }

        default : return QCommonStyle::subControlRect(control,option,subControl,widget);
      }
      break;

    case CC_SpinBox : {
      int sw = SPIN_BUTTON_WIDTH; // spin button width
      // a workaround for LibreOffice
      if (isLibreoffice)
        sw = 12;

      // take into account the right frame width
      const frame_spec fspec = getFrameSpec("IndicatorSpinBox");
      switch (subControl) {
        case SC_SpinBoxFrame :
          return QRect();
        case SC_SpinBoxEditField :
          return QRect(x,
                       y,
                       w - (sw + fspec.right) - sw,
                       h);
        case SC_SpinBoxUp :
          return QRect(x + w - (sw + fspec.right),
                       y,
                       sw + fspec.right,
                       h);
        case SC_SpinBoxDown :
          return QRect(x + w - (sw + fspec.right) - sw,
                       y,
                       sw,
                       h);

        default : return QCommonStyle::subControlRect(control,option,subControl,widget);
      }
      break;
    }

    case CC_ComboBox :
      switch (subControl) {
        case SC_ComboBoxFrame : {
          /* for an editable combo that has an icon, take
             into account the right and left margins when
             positioning the icon (also -> CT_ComboBox) */
          const QStyleOptionComboBox *opt =
              qstyleoption_cast<const QStyleOptionComboBox *>(option);
          if (opt && opt->editable && !opt->currentIcon.isNull())
          {
            const frame_spec fspec = getFrameSpec("ComboBox");
            return QRect(opt->direction == Qt::RightToLeft ?
                           x+fspec.left+fspec.right
                           : x-fspec.left-fspec.right,
                         y, w, h);
          }
          else
            return QRect();
        }
        case SC_ComboBoxEditField : {
          int margin = 0;
          if (isLibreoffice)
          {
            const frame_spec fspec = getFrameSpec("LineEdit");
            margin = fspec.left;
          }
          else
          {
            /* as in SC_ComboBoxFrame above */
            const QStyleOptionComboBox *opt =
                qstyleoption_cast<const QStyleOptionComboBox *>(option);
            if (opt && opt->editable && !opt->currentIcon.isNull())
            {
              const frame_spec fspec = getFrameSpec("ComboBox");
              margin = fspec.left+fspec.right;
            }
          }
          const frame_spec fspec1 = getFrameSpec("DropDownButton");
          return QRect(option->direction == Qt::RightToLeft ?
                         x+COMBO_ARROW_LENGTH+fspec1.left
                         : x+margin,
                       y,
                       option->direction == Qt::RightToLeft ?
                         w-(COMBO_ARROW_LENGTH+fspec1.left)-margin
                         : w-(COMBO_ARROW_LENGTH+fspec1.right)-margin,
                       h);
        }
        case SC_ComboBoxArrow : {
          const frame_spec fspec = getFrameSpec("DropDownButton");
          return QRect(option->direction == Qt::RightToLeft ?
                         x
                         : x+w-(COMBO_ARROW_LENGTH+fspec.right),
                       y,
                       option->direction == Qt::RightToLeft ?
                         COMBO_ARROW_LENGTH+fspec.left
                         : COMBO_ARROW_LENGTH+fspec.right,
                       h);
        }
        case SC_ComboBoxListBoxPopup : {
          /* level the popup list with the bottom or top edge of the combobox */
          int popupMargin = QCommonStyle::pixelMetric(PM_FocusFrameVMargin);
          return option->rect.adjusted(0, -popupMargin, 0, popupMargin);
        }

        default : return QCommonStyle::subControlRect(control,option,subControl,widget);
      }
      break;

     /*case CC_MdiControls :
       switch (subControl) {
         case SC_MdiCloseButton : return QRect(0,0,30,30);

         default : return QCommonStyle::subControlRect(control,option,subControl,widget);
       }
       break;*/

    case CC_ScrollBar : {
      const int extent = pixelMetric(PM_ScrollBarExtent,option,widget);
      const bool horiz = (option->state & State_Horizontal);
      switch (subControl) {
        case SC_ScrollBarGroove :
          if (horiz)
            return QRect(x+extent,y,w-2*extent,h);
          else
            return QRect(x,y+extent,w,h-2*extent);
        case SC_ScrollBarSubLine :
          if (horiz)
            return QRect(option->direction == Qt::RightToLeft ? x+w-extent : x,
                         y,extent,extent);
          else
            return QRect(x,y,extent,extent);
        case SC_ScrollBarAddLine :
          if (horiz)
            return QRect(option->direction == Qt::RightToLeft ? x : x+w-extent,
                         y,extent,extent);
          else
            return QRect(x,y+h-extent,extent,extent);
        case SC_ScrollBarSlider : {
          const QStyleOptionSlider *opt =
              qstyleoption_cast<const QStyleOptionSlider *>(option);

          if (opt)
          {
            QRect r = subControlRect(CC_ScrollBar,option,SC_ScrollBarGroove,widget);
            r.getRect(&x,&y,&w,&h);

            const int minLength = pixelMetric(PM_ScrollBarSliderMin,option,widget);
            int maxLength; // max slider length
            if (horiz)
              maxLength = w;
            else
              maxLength = h;
            const int valueRange = opt->maximum - opt->minimum;
            int length = maxLength;
            if (opt->minimum != opt->maximum)
            {
              length = (opt->pageStep*maxLength) / (valueRange+opt->pageStep);

              if ((length < minLength) || (valueRange > INT_MAX/2))
                length = minLength;
              if (length > maxLength)
                length = maxLength;
            }

            const int start = sliderPositionFromValue(opt->minimum,
                                                      opt->maximum,
                                                      opt->sliderPosition,
                                                      maxLength - length,
                                                      opt->upsideDown);
            if (horiz)
              return QRect(opt->direction == Qt::RightToLeft ? x+w-start-length : x+start,
                           y,length,h);
            else
              return QRect(x,y+start,w,length);
          }
        }

        default : return QCommonStyle::subControlRect(control,option,subControl,widget);
      }

      break;
    }

    case CC_Slider : {
      const QStyleOptionSlider *opt =
        qstyleoption_cast<const QStyleOptionSlider *>(option);
      int halfTick = SLIDER_TICK_SIZE/2;
      const bool horiz = (option->state & State_Horizontal);
      switch (subControl) {
        case SC_SliderGroove : {
          if (opt)
          {
            const theme_spec tspec = settings->getThemeSpec();
            const int grooveThickness = tspec.slider_width;
             int ticks = opt->tickPosition;
            if (horiz)
            {
              QRect r = QRect(x,y+(h-grooveThickness)/2,w,grooveThickness);
              if (ticks == QSlider::TicksAbove)
                r.adjust(0,halfTick,0,halfTick);
              else if (ticks == QSlider::TicksBelow)
                r.adjust(0,-halfTick,0,-halfTick);
              return r;
            }
            else
            {
              QRect r = QRect(x+(w-grooveThickness)/2,y,grooveThickness,h);
              if (ticks == QSlider::TicksAbove) // left
                r.adjust(halfTick,0,halfTick,0);
              else if (ticks == QSlider::TicksBelow) // right
                r.adjust(-halfTick,0,-halfTick,0);
              return r;
            }
          }
        }

        case SC_SliderHandle : {
          if (opt)
          {
            subControlRect(CC_Slider,option,SC_SliderGroove,widget).getRect(&x,&y,&w,&h);

            const int len = pixelMetric(PM_SliderLength, option, widget);
            const int handleThickness = pixelMetric(PM_SliderControlThickness, option, widget);
            const int sliderPos = sliderPositionFromValue (opt->minimum,
                                                           opt->maximum,
                                                           opt->sliderPosition,
                                                           (horiz ? w : h) - len,
                                                           opt->upsideDown);

            if (horiz)
              return QRect(x+sliderPos,
                           y+(h-handleThickness)/2,
                           len,
                           handleThickness);
            else
              return QRect(x+(w-handleThickness)/2,
                           y+sliderPos,
                           handleThickness,
                           len);
          }

        }

        default : return QCommonStyle::subControlRect(control,option,subControl,widget);
      }

      break;
    }

    case CC_Dial : {
      switch (subControl) {
        case SC_DialGroove : return alignedRect(QApplication::layoutDirection(),
                                                Qt::AlignHCenter | Qt::AlignVCenter,
                                                QSize(qMin(option->rect.width(),option->rect.height()),
                                                      qMin(option->rect.width(),option->rect.height())),
                                                option->rect);
        case SC_DialHandle : {
          const QStyleOptionSlider *opt =
            qstyleoption_cast<const QStyleOptionSlider *>(option);

          if (opt) // taken from Qtcurve
          {
            qreal angle(0);
            if (opt->maximum == opt->minimum)
              angle = M_PI/2;
            else
            {
              const qreal fraction(qreal(opt->sliderValue - opt->minimum)/
                                   qreal(opt->maximum - opt->minimum));
              if(opt->dialWrapping)
                angle = 1.5*M_PI - fraction*2*M_PI;
              else
                angle = (M_PI*8 - fraction*10*M_PI)/6;
            }
            QRect r(option->rect);
            // Outer circle...
            if (r.width() > r.height())
            {
              r.setLeft(r.x() + (r.width()-r.height())/2);
              r.setWidth(r.height());
            }
            else
            {
              r.setTop(r.y() + (r.height()-r.width())/2);
              r.setHeight(r.width());
            }
            QPoint center = r.center();
            int handleSize= r.width()/5;
            //const qreal radius=0.5*(r.width() - handleSize);
            const qreal radius=0.5*(r.width() - 2*handleSize);
            center += QPoint(radius*qCos(angle), -radius*qSin(angle));
            r = QRect(r.x(), r.y(), handleSize, handleSize);
            r.moveCenter(center);
            return r;
          }

        }

        default : return QCommonStyle::subControlRect(control,option,subControl,widget);
      }

      break;
    }

    case CC_ToolButton : {
      switch (subControl) {
        case SC_ToolButton : {
          const QStyleOptionToolButton *opt =
            qstyleoption_cast<const QStyleOptionToolButton *>(option);

          if (opt)
          {
            if (widget)
            {
              const QToolButton *tb = qobject_cast<const QToolButton *>(widget);
              if (tb)
              {
                if (tb->popupMode() == QToolButton::MenuButtonPopup)
                {
                  const QString group = "DropDownButton";
                  const frame_spec fspec = getFrameSpec(group);
                  const indicator_spec dspec = getIndicatorSpec(group);
                  return option->rect.adjusted(opt->direction == Qt::RightToLeft ?
                                                 fspec.left+fspec.right+dspec.size+2
                                                 : 0,
                                               0,
                                               opt->direction == Qt::RightToLeft ?
                                                 0
                                                 : -fspec.left-fspec.right-dspec.size-2,
                                               0);
                }
                else if ((tb->popupMode() == QToolButton::InstantPopup
                          || tb->popupMode() == QToolButton::DelayedPopup)
                         && (opt->features & QStyleOptionToolButton::HasMenu))
                {
                  const QString group = "PanelButtonTool";
                  frame_spec fspec = getFrameSpec(group);
                  const indicator_spec dspec = getIndicatorSpec(group);
                  const label_spec lspec = getLabelSpec(group);
                  // -> CE_ToolButtonLabel
                  if (qobject_cast<const QAbstractItemView*>(getParent(widget,2)))
                  {
                    fspec.left = qMin(fspec.left,3);
                    fspec.right = qMin(fspec.right,3);
                  }
                  return option->rect.adjusted(opt->direction == Qt::RightToLeft ?
                                                 lspec.tispace+dspec.size
                                                   // -> CE_ToolButtonLabel
                                                   + (opt->toolButtonStyle == Qt::ToolButtonTextOnly ?
                                                      qMin(fspec.left,fspec.right) : fspec.left)
                                                   + pixelMetric(PM_HeaderMargin)
                                                 : 0,
                                               0,
                                               opt->direction == Qt::RightToLeft ?
                                                 0
                                                 : - lspec.tispace-dspec.size
                                                     - (opt->toolButtonStyle == Qt::ToolButtonTextOnly ?
                                                        qMin(fspec.left,fspec.right) : fspec.right)
                                                     - pixelMetric(PM_HeaderMargin),
                                               0);
                }
              }
            }
          }

          return option->rect;
        }

        case SC_ToolButtonMenu : {
          const QStyleOptionToolButton *opt =
          qstyleoption_cast<const QStyleOptionToolButton *>(option);

          if (opt)
          {
            if (widget)
            {
              const QToolButton *tb = qobject_cast<const QToolButton *>(widget);
              if (tb)
              {
                bool rtl(opt->direction == Qt::RightToLeft);
                if (tb->popupMode() == QToolButton::MenuButtonPopup)
                {
                  const QString group = "DropDownButton";
                  const frame_spec fspec = getFrameSpec(group);
                  const indicator_spec dspec = getIndicatorSpec(group);
                  int l = fspec.left+fspec.right+dspec.size+2;
                  return QRect(rtl ? x : x+w-l,
                               y,l,h);
                }
                else if ((tb->popupMode() == QToolButton::InstantPopup
                          || tb->popupMode() == QToolButton::DelayedPopup)
                         && (opt->features & QStyleOptionToolButton::HasMenu))
                {
                  const QString group = "PanelButtonTool";
                  frame_spec fspec = getFrameSpec(group);
                  const indicator_spec dspec = getIndicatorSpec(group);
                  // -> CE_ToolButtonLabel
                  if (qobject_cast<const QAbstractItemView*>(getParent(widget,2)))
                  {
                    fspec.left = qMin(fspec.left,3);
                    fspec.right = qMin(fspec.right,3);
                  }
                  int l = dspec.size
                          // -> CE_ToolButtonLabel
                          + (opt->toolButtonStyle == Qt::ToolButtonTextOnly ?
                             qMin(fspec.left,fspec.right) : rtl ? fspec.left : fspec.right)
                          + pixelMetric(PM_HeaderMargin);
                  return QRect(rtl ? x : x+w-l,
                               y,l,h);
                }
              }
            }
          }

          return option->rect;
        }

        default : return QCommonStyle::subControlRect(control,option,subControl,widget);
      }

      break;
    }

    case CC_GroupBox : {
      const QStyleOptionGroupBox *opt =
        qstyleoption_cast<const QStyleOptionGroupBox *>(option);
      if (opt)
      {
        frame_spec fspec;
        default_frame_spec(fspec);
        label_spec lspec = getLabelSpec("GroupBox");
        size_spec sspec;
        default_size_spec(sspec);

        bool checkable = false;
        if (const QGroupBox *gb = qobject_cast<const QGroupBox *>(widget))
        {
          // if checkable, don't use lspec.left, use PM_CheckBoxLabelSpacing for spacing
          if (gb->isCheckable())
          {
            checkable = true;
            lspec.left = 0;
          }
        }
        QSize s = sizeCalculated(widget->font(),fspec,lspec,sspec,opt->text,QPixmap());
        int checkSize = (checkable ? pixelMetric(PM_IndicatorWidth)+pixelMetric(PM_CheckBoxLabelSpacing) : 0);

        switch (subControl) {
          case SC_GroupBoxCheckBox : {
            return QRect(x + 10,
                         option->rect.y(),
                         pixelMetric(PM_IndicatorWidth),
                         pixelMetric(PM_IndicatorHeight));
          }
          case SC_GroupBoxLabel : {
            return QRect(x + 10 + checkSize,
                         y,
                         s.width(),
                         s.height());
          }
          case SC_GroupBoxContents : {
            fspec = getFrameSpec("GroupBox");
            int top = (checkable ? checkSize : s.height());
            return QRect(x + fspec.left,
                         y + top,
                         w - fspec.left - fspec.right,
                         h - top - fspec.bottom);
          }

          default : return QCommonStyle::subControlRect(control,option,subControl,widget);
        }
      }
    }

    default : return QCommonStyle::subControlRect(control,option,subControl,widget);
  }

  return QCommonStyle::subControlRect(control,option,subControl,widget);
}

#if QT_VERSION < 0x050000
QIcon Kvantum::standardIconImplementation (QStyle::StandardPixmap standardIcon,
                                           const QStyleOption *option,
                                           const QWidget *widget) const
#else
QIcon Kvantum::standardIcon (QStyle::StandardPixmap standardIcon,
                             const QStyleOption *option,
                             const QWidget *widget ) const
#endif
{
  switch (standardIcon) {
    case SP_ToolBarHorizontalExtensionButton : {
      int s = pixelMetric(PM_ToolBarExtensionExtent);
      QPixmap pm(QSize(s,s));
      pm.fill(Qt::transparent);

      QPainter painter(&pm);

      QStyleOption opt;
      opt.rect = QRect(0,0,s,s);
      opt.state |= State_Enabled;

      drawPrimitive(PE_IndicatorArrowRight,&opt,&painter,0);

      return QIcon(pm);
    }
    case SP_ToolBarVerticalExtensionButton : {
      int s = pixelMetric(PM_ToolBarExtensionExtent);
      QPixmap pm(QSize(s,s));
      pm.fill(Qt::transparent);

      QPainter painter(&pm);

      QStyleOption opt;
      opt.rect = QRect(0,0,s,s);
      opt.state |= State_Enabled;

      drawPrimitive(PE_IndicatorArrowDown,&opt,&painter,0);

      return QIcon(pm);
    }
    case SP_TitleBarMinButton : {
      int s = 12;
      QPixmap pm(QSize(s,s));
      pm.fill(Qt::transparent);

      QPainter painter(&pm);

      if (renderElement(&painter,"mdi-minimize-normal",QRect(0,0,s,s)))
        return QIcon(pm);
      else break;
    }
    case SP_TitleBarMaxButton : {
      int s = 12;
      QPixmap pm(QSize(s,s));
      pm.fill(Qt::transparent);

      QPainter painter(&pm);

      if (renderElement(&painter,"mdi-maximize-normal",QRect(0,0,s,s)))
        return QIcon(pm);
      else break;
    }
    case SP_TitleBarCloseButton : {
      int s = 12;
      QPixmap pm(QSize(s,s));
      pm.fill(Qt::transparent);

      QPainter painter(&pm);

      if (renderElement(&painter,"mdi-close-normal",QRect(0,0,s,s)))
        return QIcon(pm);
      else break;
    }
    case SP_TitleBarMenuButton : {
      int s = 12;
      QPixmap pm(QSize(s,s));
      pm.fill(Qt::transparent);

      QPainter painter(&pm);

      if (renderElement(&painter,"mdi-menu-normal",QRect(0,0,s,s)))
        return QIcon(pm);
      else break;
    }
    case SP_TitleBarNormalButton : {
      int s = 12;
      QPixmap pm(QSize(s,s));
      pm.fill(Qt::transparent);

      QPainter painter(&pm);

      if (renderElement(&painter,"mdi-restore-normal",QRect(0,0,s,s)))
        return QIcon(pm);
      else break;
    }
    case SP_DialogCancelButton :
    case SP_DialogNoButton :
    /*case SP_DialogDiscardButton :*/ {
      int s = pixelMetric(PM_SmallIconSize);
      QPixmap pm(QSize(s,s));
      pm.fill(Qt::transparent);

      QPainter painter(&pm);

      if (renderElement(&painter,"cancel-button",QRect(0,0,s,s)))
        return QIcon(pm);
      else break;
    }
    case SP_DialogOkButton :
    case SP_DialogYesButton :
    case SP_DialogApplyButton : {
      int s = pixelMetric(PM_SmallIconSize);
      QPixmap pm(QSize(s,s));
      pm.fill(Qt::transparent);

      QPainter painter(&pm);

      if (renderElement(&painter,"ok-button",QRect(0,0,s,s)))
        return QIcon(pm);
      else break;
    }
    case SP_DialogOpenButton : {
      int s = pixelMetric(PM_SmallIconSize);
      QPixmap pm(QSize(s,s));
      pm.fill(Qt::transparent);

      QPainter painter(&pm);

      if (renderElement(&painter,"open-button",QRect(0,0,s,s)))
        return QIcon(pm);
      else break;
    }
    case SP_DialogSaveButton : {
      int s = pixelMetric(PM_SmallIconSize);
      QPixmap pm(QSize(s,s));
      pm.fill(Qt::transparent);

      QPainter painter(&pm);

      if (renderElement(&painter,"save-button",QRect(0,0,s,s)))
        return QIcon(pm);
      else break;
    }
    case SP_ArrowLeft :
    case SP_ArrowBack : {
      int s = pixelMetric(PM_SmallIconSize);
      QPixmap pm(QSize(s,s));
      pm.fill(Qt::transparent);

      QPainter painter(&pm);

      // a 2-px margin
      if (renderElement(&painter,"arrow-left-focused",QRect(2,2,s-4,s-4)))
        return QIcon(pm);
      else break;
    }
    case SP_ArrowRight :
    case SP_ArrowForward : {
      int s = pixelMetric(PM_SmallIconSize);
      QPixmap pm(QSize(s,s));
      pm.fill(Qt::transparent);

      QPainter painter(&pm);

      if (renderElement(&painter,"arrow-right-focused",QRect(2,2,s-4,s-4)))
        return QIcon(pm);
      else break;
    }
    case SP_FileDialogToParent : {
      int s = pixelMetric(PM_SmallIconSize);
      QPixmap pm(QSize(s,s));
      pm.fill(Qt::transparent);

      QPainter painter(&pm);

      if (renderElement(&painter,"arrow-up-focused",QRect(2,2,s-4,s-4)))
        return QIcon(pm);
      else break;
    }
    case SP_FileDialogNewFolder : {
      int s = pixelMetric(PM_SmallIconSize);
      QPixmap pm(QSize(s,s));
      pm.fill(Qt::transparent);

      QPainter painter(&pm);

      if (renderElement(&painter,"arrow-plus-focused",QRect(2,2,s-4,s-4)))
        return QIcon(pm);
      else break;
    }

#if QT_VERSION < 0x050000
    default : return QCommonStyle::standardIconImplementation(standardIcon,option,widget);
  }

  return QCommonStyle::standardIconImplementation(standardIcon,option,widget);
#else
    default : return QCommonStyle::standardIcon(standardIcon,option,widget);
  }

  return QCommonStyle::standardIcon(standardIcon,option,widget);
#endif
}

QRect Kvantum::squaredRect(const QRect &r) const {
  int e = (r.width() > r.height()) ? r.height() : r.width();
  return QRect(r.x(),r.y(),e,e);
}

bool Kvantum::renderElement(QPainter *painter,
                            const QString &element,
                            const QRect &bounds, int hsize, int vsize,
                            Qt::Orientation orientation) const
{
  Q_UNUSED(orientation);

  if (!bounds.isValid())
    return false;

  QSvgRenderer *renderer = 0;
  QString element_ (element);

  if (themeRndr && themeRndr->isValid()
      && (themeRndr->elementExists(element_)
          || (element_.contains("-inactive")
              && themeRndr->elementExists(element_.remove(QString("-inactive"))))))
  {
    renderer = themeRndr;
  }
  /* always use the default SVG image (which doesn't contain
     any object for the inactive state) as fallback */
  else if (defaultRndr && defaultRndr->isValid()
           && defaultRndr->elementExists(element_.remove(QString("-inactive"))))
  {
    renderer = defaultRndr;
  }
  else
    return false;

  if (renderer)
  {
    if (hsize > 0 || vsize > 0)
    {
      int width = hsize > 0 ? hsize : bounds.width();
      int height = vsize > 0 ? vsize : bounds.height();
      QString str = QString("%1-%2-%3").arg(element_)
                                       .arg(QString().setNum(width))
                                       .arg(QString().setNum(height));
      QPixmap pixmap;
      if (!QPixmapCache::find(str, &pixmap))
      {
        pixmap = QPixmap (width, height);
        pixmap.fill(QColor(Qt::transparent));
        QPainter p;
        p.begin(&pixmap);
        renderer->render(&p,element_);
        p.end();
        QPixmapCache::insert(str, pixmap);
      }
      painter->drawTiledPixmap(bounds,pixmap);
    }
    else
      renderer->render(painter,element_,bounds);
  }

  return true;
}

void Kvantum::renderSliderTick(QPainter *painter,
                               const QString &element,
                               const QRect &ticksRect,
                               const int interval,
                               const int available,
                               const int min,
                               const int max,
                               bool above,
                               bool inverted) const
{
  if (!ticksRect.isValid())
    return;

  QSvgRenderer *renderer = 0;
  QString element_ (element);

  if (themeRndr && themeRndr->isValid()
      && (themeRndr->elementExists(element_)
          || (element_.contains("-inactive")
              && themeRndr->elementExists(element_.remove(QString("-inactive"))))))
  {
    renderer = themeRndr;
  }
  else if (defaultRndr && defaultRndr->isValid()
           && defaultRndr->elementExists(element_.remove(QString("-inactive"))))
  {
    renderer = defaultRndr;
  }
  else
    return;

  if (renderer)
  {
    if (interval < 1) return;

    int thickness = 1;
    int len = pixelMetric(PM_SliderLength);
    int x = ticksRect.x();
    int y = ticksRect.y();
    if (!above)
    {
      painter->save();
      QTransform m;
      m.translate(2*x+ticksRect.width(), 0);
      m.scale(-1,1);
      painter->setTransform(m, true);
    }
    int current = min;
    while (current <= max)
    {
      const int position = sliderPositionFromValue(min,max,current,available,inverted) + len/2;
      renderer->render(painter,element_,QRect(x,
                                              y+position,
                                              SLIDER_TICK_SIZE,
                                              thickness));

      current += interval;
    }
    if (!above)
      painter->restore();
  }
}

void Kvantum::renderFrame(QPainter *painter,
                          const QRect &bounds, // frame bounds
                          const frame_spec &fspec, // frame spec
                          const QString &element, // frame SVG element
                          int d, // distance of the attached tab from the edge
                          int l, // length of the attached tab
                          int tp) const // tab position
{
  if (!bounds.isValid() || !fspec.hasFrame)
    return;

  int x0,y0,x1,y1,w,h;
  bounds.getRect(&x0,&y0,&w,&h);
  /* for "historical" reasons, we have to add 1
     (-> QRect documentation) */
  x1 = bounds.bottomRight().x() + 1;
  y1 = bounds.bottomRight().y() + 1;

  if (!fspec.hasCapsule || (fspec.capsuleH == 2 && fspec.capsuleV == 2))
  {
    /*********
     ** Top **
     *********/
    if (l != 0 && tp == QTabWidget::North)
    {
      renderElement(painter,element+"-top",
                    QRect(x0+fspec.left,
                          y0,
                          d-x0-fspec.left,
                          fspec.top),
                    0,0,Qt::Horizontal);
      renderElement(painter,element+"-top",
                    QRect(d+l,
                          y0,
                          x0+w-fspec.left-d-l,
                          fspec.top),
                    0,0,Qt::Horizontal);
    }
    else
      renderElement(painter,element+"-top",
                    QRect(x0+fspec.left,y0,w-fspec.left-fspec.right,fspec.top),
                    0,0,Qt::Horizontal);

    /************
     ** Bottom **
     ************/
    if (l != 0 && tp == QTabWidget::South)
    {
      renderElement(painter,element+"-bottom",
                    QRect(x0+fspec.left,
                          y1-fspec.bottom,
                          d-x0-fspec.left,
                          fspec.bottom),
                    0,0,Qt::Horizontal);
      renderElement(painter,element+"-bottom",
                    QRect(d+l,
                          y1-fspec.bottom,
                          x0+w-fspec.left-d-l,
                          fspec.bottom),
                    0,0,Qt::Horizontal);
    }
    else
      renderElement(painter,element+"-bottom",
                    QRect(x0+fspec.left,y1-fspec.bottom,w-fspec.left-fspec.right,fspec.bottom),
                    0,0,Qt::Horizontal);

    /**********
     ** Left **
     **********/
    if (l != 0 && tp == QTabWidget::West)
    {
      renderElement(painter,element+"-left",
                    QRect(x0,
                          y0+fspec.top,
                          fspec.left,
                          d-y0-fspec.top),
                    0,0,Qt::Horizontal);
      renderElement(painter,element+"-left",
                    QRect(x0,
                          d+l,
                          fspec.left,
                          y0+h-fspec.bottom-d-l),
                    0,0,Qt::Horizontal);
    }
    else
      renderElement(painter,element+"-left",
                    QRect(x0,y0+fspec.top,fspec.left,h-fspec.top-fspec.bottom),
                    0,0,Qt::Horizontal);

    /***********
     ** Right **
     ***********/
    if (l != 0 && tp == QTabWidget::East)
    {
      renderElement(painter,element+"-right",
                    QRect(x1-fspec.right,
                          y0+fspec.top,
                          fspec.right,
                          d-y0-fspec.top),
                    0,0,Qt::Horizontal);
      renderElement(painter,element+"-right",
                    QRect(x1-fspec.right,
                          d+l,
                          fspec.right,
                          y0+h-fspec.bottom-d-l),
                    0,0,Qt::Horizontal);
    }
    else
      renderElement(painter,element+"-right",
                    QRect(x1-fspec.right,y0+fspec.top,fspec.right,h-fspec.top-fspec.bottom),
                    0,0,Qt::Horizontal);

    /*************
     ** Topleft **
     *************/
    renderElement(painter,element+"-topleft",
                  QRect(x0,y0,fspec.left,fspec.top),
                  0,0,Qt::Horizontal);

    /**************
     ** Topright **
     **************/
    renderElement(painter,element+"-topright",
                  QRect(x1-fspec.right,y0,fspec.right,fspec.top),
                  0,0,Qt::Horizontal);

    /****************
     ** Bottomleft **
     ****************/
    renderElement(painter,element+"-bottomleft",
                  QRect(x0,y1-fspec.bottom,fspec.left,fspec.bottom),
                  0,0,Qt::Horizontal);

    /*****************
     ** Bottomright **
     *****************/
    renderElement(painter,element+"-bottomright",
                  QRect(x1-fspec.right,y1-fspec.bottom,fspec.right,fspec.bottom),
                  0,0,Qt::Horizontal);
  }
  else // with capsule
  {
    if (fspec.capsuleH == 0 && fspec.capsuleV == 0)
      return;

    /* to simplify calculations, we first get margins */
    int left = 0, right = 0, top = 0, bottom = 0;
    if (fspec.capsuleH == -1 || fspec.capsuleH == 2)
      left = fspec.left;
    if (fspec.capsuleH == 1 || fspec.capsuleH == 2)
      right = fspec.right;
    if (fspec.capsuleV == -1  || fspec.capsuleV == 2)
      top = fspec.top;
    if (fspec.capsuleV == 1 || fspec.capsuleV == 2)
      bottom = fspec.bottom;

    /*********
     ** Top **
     *********/
    if (top > 0)
    {
      renderElement(painter,element+"-top",
                    QRect(x0+left,y0,w-left-right,top),
                    0,0,Qt::Horizontal);

      // topleft corner
      if (left > 0)
        renderElement(painter,element+"-topleft",
                      QRect(x0,y0,left,top),
                      0,0,Qt::Horizontal);
      // topright corner
      if (right > 0)
        renderElement(painter,element+"-topright",
                      QRect(x1-right,y0,right,top),
                      0,0,Qt::Horizontal);
    }

    /************
     ** Bottom **
     ************/
    if (bottom > 0)
    {
      renderElement(painter,element+"-bottom",
                    QRect(x0+left,y1-bottom,w-left-right,bottom),
                    0,0,Qt::Horizontal);

      // bottomleft corner
      if (left > 0)
        renderElement(painter,element+"-bottomleft",
                      QRect(x0,y1-bottom,left,bottom),
                      0,0,Qt::Horizontal);
      // bottomright corner
      if (right > 0)
        renderElement(painter,element+"-bottomright",
                      QRect(x1-right,y1-bottom,right,bottom),
                      0,0,Qt::Horizontal);
    }

    /**********
     ** Left **
     **********/
    if (left > 0)
      renderElement(painter,element+"-left",
                    QRect(x0,y0+top,left,h-top-bottom),
                    0,0,Qt::Horizontal);

    /***********
     ** Right **
     ***********/
    if (right > 0)
      renderElement(painter,element+"-right",
                    QRect(x1-right,y0+top,right,h-top-bottom),
                    0,0,Qt::Horizontal);
  }
}

void Kvantum::renderInterior(QPainter *painter,
                             const QRect &bounds, // frame bounds
                             const frame_spec &fspec, // frame spec
                             const interior_spec &ispec, // interior spec
                             const QString &element, // interior SVG element
                             Qt::Orientation orientation) const
{
  if (!bounds.isValid() || !ispec.hasInterior)
    return;

  if (!fspec.hasCapsule || (fspec.capsuleH == 2 && fspec.capsuleV == 2))
    renderElement(painter,element,interiorRect(bounds,fspec),
                  ispec.px,ispec.py,orientation);
  else
  {
    // add these to compensate the absence of the frame
    int left = 0, right = 0, top = 0, bottom = 0;
    if (fspec.capsuleH == 0)
    {
      left = fspec.left;
      right = fspec.right;
    }
    else if (fspec.capsuleH == -1)
      right = fspec.right;
    else if (fspec.capsuleH == 1)
      left = fspec.left;

    if (fspec.capsuleV == 0)
    {
      top = fspec.top;
      bottom = fspec.bottom;
    }
    else if (fspec.capsuleV == -1)
      bottom = fspec.bottom;
    else if (fspec.capsuleV == 1)
      top = fspec.top;

    if (orientation == Qt::Vertical)
      renderElement(painter,element,
                    interiorRect(bounds,fspec).adjusted(-left,-top,right,bottom),
                    ispec.py,ispec.px,orientation);
    else
      renderElement(painter,element,
                    interiorRect(bounds,fspec).adjusted(-left,-top,right,bottom),
                    ispec.px,ispec.py,orientation);
  }
}

void Kvantum::renderIndicator(QPainter *painter,
                              const QRect &bounds, // frame bounds
                              const frame_spec &fspec, // frame spec
                              const indicator_spec &dspec, // indicator spec
                              const QString &element, // indocator SVG element
                              Qt::Alignment alignment) const
{
  const QRect interior = interiorRect(bounds,fspec);
  const QRect sq = squaredRect(interior);
  int s = 0;
  if (!sq.isValid())
    s = dspec.size;
  else // make the indicator smaller if there isn't enough space
    s = (sq.width() > dspec.size) ? dspec.size : sq.width();

  renderElement(painter,element,
                alignedRect(QApplication::layoutDirection(),alignment,QSize(s,s),interior),
                0,0,Qt::Horizontal);
}

void Kvantum::renderLabel(
                          QPainter *painter,
                          const QPalette &palette,
                          const QRect &bounds, // frame bounds
                          const frame_spec &fspec, // frame spec
                          const label_spec &lspec, // label spec
                          int talign, // text alignment
                          const QString &text,
                          QPalette::ColorRole textRole, // text color role
                          int state, // widget state (0->disabled, 1->normal, 2->focused, 3->pressed, 4->toggled)
                          const QPixmap &icon,
                          const Qt::ToolButtonStyle tialign // relative positions of text and icon
                         ) const
{
  // compute text and icon rect
  QRect r;
  if (/*!isPlasma &&*/ // we ignore Plasma text margins just for push and tool buttons and menubars
      tialign != Qt::ToolButtonIconOnly
      && !text.isEmpty())
    r = labelRect(bounds,fspec,lspec);
  else
    r = interiorRect(bounds,fspec);

  if (!r.isValid())
    return;

  QRect ricon = r;
  QRect rtext = r;

  if (tialign == Qt::ToolButtonTextBesideIcon)
  {
    ricon = alignedRect(QApplication::layoutDirection(),
                        Qt::AlignVCenter | Qt::AlignLeft,
                        QSize(icon.width(),icon.height()),
                        r);
    rtext = QRect(QApplication::layoutDirection() == Qt::RightToLeft ?
                    r.x()
                    : r.x()+icon.width() + (icon.isNull() ? 0 : lspec.tispace),
                  r.y(),
                  r.width()-ricon.width() - (icon.isNull() ? 0 : lspec.tispace),
                  r.height());
  }
  else if (tialign == Qt::ToolButtonTextUnderIcon)
  {
    ricon = alignedRect(QApplication::layoutDirection(),
                        Qt::AlignTop | Qt::AlignHCenter,
                        QSize(icon.width(),icon.height()),
                        r);
    rtext = QRect(r.x(),
                  r.y()+icon.height() + (icon.isNull() ? 0 : lspec.tispace),
                  r.width(),
                  r.height()-ricon.height() - (icon.isNull() ? 0 : lspec.tispace));
  }
  else if (tialign == Qt::ToolButtonIconOnly)
  {
    ricon = alignedRect(QApplication::layoutDirection(),
                        Qt::AlignCenter,
                        QSize(icon.width(),icon.height()),
                        r);
  }

  if (text.isEmpty())
  {
    ricon = alignedRect(QApplication::layoutDirection(),
                        Qt::AlignCenter,
                        QSize(icon.width(),icon.height()),
                        r);
  }

  if (tialign != Qt::ToolButtonTextOnly && !icon.isNull())
    painter->drawPixmap(ricon,icon);

  if (((isPlasma && icon.isNull()) // Why do some Plasma toolbuttons pretend to have only icons?
       || tialign != Qt::ToolButtonIconOnly)
      && !text.isEmpty())
  {
    if (state != 0)
    {
      QColor shadowColor(lspec.shadowColor);
      if (lspec.hasShadow && shadowColor.isValid())
      {
        painter->save();
        shadowColor.setAlpha(lspec.a);
        painter->setPen(QPen(shadowColor));
        for (int i=0; i<lspec.depth; i++)
          painter->drawText(rtext.adjusted(lspec.xshift+i,lspec.yshift+i,0,0),talign,text);
        painter->restore();
      }

      QColor normalColor(lspec.normalColor);
      QColor focusColor(lspec.focusColor);
      QColor pressColor(lspec.pressColor);
      QColor toggleColor(lspec.toggleColor);
      if (state == 1 && normalColor.isValid())
      {
        painter->save();
        painter->setPen(QPen(normalColor));
        painter->drawText(rtext,talign,text);
        painter->restore();
        return;
      }
      else if (state == 2 && focusColor.isValid())
      {
        painter->save();
        painter->setPen(QPen(focusColor));
        painter->drawText(rtext,talign,text);
        painter->restore();
        return;
      }
      else if (state == 3 && pressColor.isValid())
      {
        painter->save();
        painter->setPen(QPen(pressColor));
        painter->drawText(rtext,talign,text);
        painter->restore();
        return;
      }
      else if (state == 4 && toggleColor.isValid())
      {
        painter->save();
        painter->setPen(QPen(toggleColor));
        painter->drawText(rtext,talign,text);
        painter->restore();
        return;
      }
    }

    QCommonStyle::drawItemText(painter,
                               rtext,
                               talign,
                               palette,
                               state == 0 ? false: true,
                               text,
                               textRole);
  }
}

inline frame_spec Kvantum::getFrameSpec(const QString &widgetName) const
{
  return settings->getFrameSpec(widgetName);
}

inline interior_spec Kvantum::getInteriorSpec(const QString &widgetName) const
{
  return settings->getInteriorSpec(widgetName);
}

inline indicator_spec Kvantum::getIndicatorSpec(const QString &widgetName) const
{
  return settings->getIndicatorSpec(widgetName);
}

inline label_spec Kvantum::getLabelSpec(const QString &widgetName) const
{
  return settings->getLabelSpec(widgetName);
}

inline size_spec Kvantum::getSizeSpec(const QString &widgetName) const
{
  return settings->getSizeSpec(widgetName);
}
