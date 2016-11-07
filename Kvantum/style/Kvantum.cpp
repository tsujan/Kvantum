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

#include <QProcess>
#include <QDir>
#include <QPainter>
#include <QSettings>
#include <QTimer>
#include <QSvgRenderer>
#include <QApplication>
#include <QToolButton>
#include <QToolBar>
#include <QMainWindow>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QProgressBar>
#include <QMenu>
#include <QGroupBox>
#include <QAbstractScrollArea>
//#include <QAbstractButton>
#include <QAbstractItemView>
#include <QDockWidget>
#include <QDial>
#include <QScrollBar>
#include <QMdiArea>
#include <QToolBox>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QDateTimeEdit>
#include <QPixmapCache>
//#include <QBitmap>
#include <QPaintEvent>
#include <QtCore/qmath.h>
#include <QMenuBar>
#include <QGraphicsView>
#include <QDialog>
#include <QStatusBar>
#include <QCheckBox>
#include <QRadioButton>
#include <QLayout> // only for forceSizeGrip
//#include <QDebug>
//#include <QDialogButtonBox> // for dialog buttons layout

#if QT_VERSION >= 0x050000
#include <QSurfaceFormat>
#include <QWindow>
#if QT_VERSION >= 0x050500 && (defined Q_WS_X11 || defined Q_OS_LINUX)
#include <QtPlatformHeaders/QXcbWindowFunctions>
#endif
#endif

#define M_PI 3.14159265358979323846
#define DISABLED_OPACITY 0.7
#define SLIDER_TICK_SIZE 5 // 10 at most
#define COMBO_ARROW_LENGTH 20
#define TOOL_BUTTON_ARROW_MARGIN 2
#define TOOL_BUTTON_ARROW_SIZE 10 // when there isn't enough space (~ PM_MenuButtonIndicator)
#define TOOL_BUTTON_ARROW_OVERLAP 4 // when there isn't enough space
#define MIN_CONTRAST 78
#define ANIMATION_FRAME 40 // in ms
#define OPACITY_STEP 20 // percent

#if QT_VERSION < 0x050000
#define QStyleOptionTabBarBase_v2 QStyleOptionTabBarBaseV2
#define QStyleOptionTabWidgetFrame_v2 QStyleOptionTabWidgetFrameV2
#define QStyleOptionViewItem_v2 QStyleOptionViewItemV2
#define QStyleOptionViewItem_v4 QStyleOptionViewItemV4
#define QStyleOptionTab_v2 QStyleOptionTabV2
#define QStyleOptionProgressBar_v2 QStyleOptionProgressBarV2
#define QStyleOptionFrame_v3 QStyleOptionFrameV3
#else
#define QStyleOptionTabBarBase_v2 QStyleOptionTabBarBase
#define QStyleOptionTabWidgetFrame_v2 QStyleOptionTabWidgetFrame
#define QStyleOptionViewItem_v2 QStyleOptionViewItem
#define QStyleOptionViewItem_v4 QStyleOptionViewItem
#define QStyleOptionTab_v2 QStyleOptionTab
#define QStyleOptionProgressBar_v2 QStyleOptionProgressBar
#define QStyleOptionFrame_v3 QStyleOptionFrame
#endif

namespace Kvantum
{
static QString readDconfSetting(const QString &setting) // by Craig Drummond
{
  // For some reason, dconf does not seem to terminate correctly when run under some desktops (e.g. KDE)
  // Destroying the QProcess seems to block, causing the app to appear to hang before starting.
  // So, create QProcess on the heap - and only wait 1.5s for response. Connect finished to deleteLater
  // so that the object is destroyed.
  QString schemeToUse=QLatin1String("/org/gnome/desktop/interface/");
  QProcess *process=new QProcess();
  process->start(QLatin1String("dconf"), QStringList() << QLatin1String("read") << schemeToUse+setting);
  QObject::connect(process, SIGNAL(finished(int)), process, SLOT(deleteLater()));

  if (process->waitForFinished(1500)) {
    QString resp = process->readAllStandardOutput();
    resp = resp.trimmed();
    resp.remove('\'');

    if (resp.isEmpty()) {
      // Probably set to the default, and dconf does not store defaults! Therefore, need to read via gsettings...
      schemeToUse=schemeToUse.mid(1, schemeToUse.length()-2).replace("/", ".");
      QProcess *gsettingsProc=new QProcess();
      gsettingsProc->start(QLatin1String("gsettings"), QStringList() << QLatin1String("get") << schemeToUse << setting);
      QObject::connect(gsettingsProc, SIGNAL(finished(int)), process, SLOT(deleteLater()));
      if (gsettingsProc->waitForFinished(1500)) {
        resp = gsettingsProc->readAllStandardOutput();
        resp = resp.trimmed();
        resp.remove('\'');
      } else {
        gsettingsProc->kill();
      }
    }
    return resp;
  }
  process->kill();
  return QString();
}

static void setAppFont()
{
  QString fontName=readDconfSetting("font-name");
  if (!fontName.isEmpty())
  {
    QStringList parts=fontName.split(' ', QString::SkipEmptyParts);
    if (parts.length()>1)
    {
      uint size=parts.takeLast().toUInt();
      if (size>5 && size<20)
      {
        QFont f(parts.join(" "), size);
        QApplication::setFont(f);
      }
    }
  }
}

Style::Style() : QCommonStyle()
{
  progressTimer_ = new QTimer(this);
  opacityTimer_ = opacityTimerOut_ = NULL;
  animationOpacity_ = animationOpacityOut_ = 100;
  animationStartState_ = animationStartStateOut_ = "normal";
  animatedWidget_ = animatedWidgetOut_ = NULL;

  settings_ = defaultSettings_ = themeSettings_ = NULL;
  defaultRndr_ = themeRndr_ = NULL;

  gtkDesktop_ = false;
  noComposite_ = false;

  QString homeDir = QDir::homePath();

  char * _xdg_config_home = getenv("XDG_CONFIG_HOME");
  if (!_xdg_config_home)
    xdg_config_home = QString("%1/.config").arg(homeDir);
  else
    xdg_config_home = QString(_xdg_config_home);

  // load global config file
  QString theme;
  QString themeChooserFile = QString("%1/Kvantum/kvantum.kvconfig").arg(xdg_config_home);
  if (QFile::exists(themeChooserFile))
  {
    QSettings themeChooser (themeChooserFile,QSettings::NativeFormat);
    if (themeChooser.status() == QSettings::NoError && themeChooser.contains("theme"))
      theme = themeChooser.value("theme").toString();
  }

  setBuiltinDefaultTheme();
  setUserTheme(theme);

  tspec_ = settings_->getThemeSpec();
  hspec_ = settings_->getHacksSpec();
  cspec_ = settings_->getColorSpec();

  QByteArray desktop = qgetenv("XDG_CURRENT_DESKTOP").toLower();
  QSet<QByteArray> gtkDesktops = QSet<QByteArray>() << "gnome" << "unity" << "pantheon";
  gtkDesktop_ = gtkDesktops.contains(desktop);


  if (tspec_.respect_DE)
  {
    if (gtkDesktop_)
    {
      hspec_.iconless_pushbutton = true;
      hspec_.iconless_menu = true;
#if defined Q_WS_X11 || defined Q_OS_LINUX
      tspec_.x11drag = WindowManager::DRAG_MENUBAR_AND_PRIMARY_TOOLBAR;
#endif
      noComposite_ = true;
      // without compositing, these keys should be corrected
      tspec_.translucent_windows = false;
      tspec_.blurring = false;
    }
    else if (desktop == QByteArray("kde"))
    {
      QString kdeGlobals = QString("%1/kdeglobals").arg(xdg_config_home);
      if (!QFile::exists(kdeGlobals))
        kdeGlobals = QString("%1/.kde/share/config/kdeglobals").arg(homeDir);
      if (!QFile::exists(kdeGlobals))
        kdeGlobals = QString("%1/.kde4/share/config/kdeglobals").arg(homeDir);
      if (QFile::exists(kdeGlobals))
      {
        QSettings KDESettings(kdeGlobals, QSettings::NativeFormat);
        QVariant v;
        int iconSize;
        KDESettings.beginGroup("KDE");
        v = KDESettings.value ("SingleClick");
        KDESettings.endGroup();
        if (v.isValid())
          tspec_.double_click = !v.toBool();
        else
          tspec_.double_click = false;
        KDESettings.beginGroup("DialogIcons");
        v = KDESettings.value ("Size");
        KDESettings.endGroup();
        if (v.isValid())
        {
          iconSize = v.toInt();
          if (iconSize > 0 && iconSize <= 256)
            tspec_.large_icon_size = iconSize;
        }
        else
          tspec_.large_icon_size = 32;
        KDESettings.beginGroup("SmallIcons");
        v = KDESettings.value ("Size");
        KDESettings.endGroup();
        if (v.isValid())
        {
          iconSize = v.toInt();
          if (iconSize > 0 && iconSize <= 256)
            tspec_.small_icon_size = iconSize;
        }
        else
          tspec_.small_icon_size = 16;
      }
    }
  }

  isPlasma_ = false;
  isLibreoffice_ = false;
  isDolphin_ = false;
  isPcmanfm_ = false;
  subApp_ = false;
  isOpaque_ = false;
  isKisSlider_ = false;
  pixelRatio_ = 1;

#if QT_VERSION >= 0x050500
  int dpr = qApp->devicePixelRatio();
  if (dpr > 1)
    pixelRatio_ = dpr;
#endif

  connect(progressTimer_, SIGNAL(timeout()),
          this, SLOT(advanceProgressbar()));

  if (tspec_.animate_states)
  {
    opacityTimer_ = new QTimer(this);
    opacityTimerOut_ = new QTimer(this);
    connect(opacityTimer_, SIGNAL(timeout()),
            this, SLOT(setAnimationOpacity()));
    connect(opacityTimerOut_, SIGNAL(timeout()),
            this, SLOT(setAnimationOpacityOut()));
  }

  itsShortcutHandler_ = NULL;
  if (tspec_.alt_mnemonic)
    itsShortcutHandler_ = new ShortcutHandler(this);

  // decide about connecting active tabs to others and using floating tabs once for all
  joinedActiveTab_ = joinedActiveFloatingTab_ = hasFloatingTabs_ = false;
  if (themeRndr_ && themeRndr_->isValid())
  {
    if (themeRndr_->elementExists("floating-"+getInteriorSpec("Tab").element+"-normal"))
      hasFloatingTabs_ = true;
    if (tspec_.joined_inactive_tabs)
    {
      QString sepName = getFrameSpec("Tab").element + "-separator";
      if (themeRndr_->elementExists(sepName+"-normal")
          || themeRndr_->elementExists(sepName+"-toggled"))
      {
        joinedActiveTab_ = true;
      }
      if (hasFloatingTabs_)
      {
        sepName = "floating-"+sepName;
        if (themeRndr_->elementExists(sepName+"-normal")
            || themeRndr_->elementExists(sepName+"-toggled"))
        {
          joinedActiveFloatingTab_ = true;
        }
      }
    }
  }

  itsWindowManager_ = NULL;
  blurHelper_ = NULL;

#if defined Q_WS_X11 || defined Q_OS_LINUX
  if (tspec_.x11drag)
  {
    itsWindowManager_ = new WindowManager(this, tspec_.x11drag);
    itsWindowManager_->initialize();
  }

  if (tspec_.blurring)
    creatBlurHelper();
#endif
}

Style::~Style()
{
  delete defaultSettings_;
  delete themeSettings_;

  delete defaultRndr_;
  delete themeRndr_;
}

void Style::setBuiltinDefaultTheme()
{
  if (defaultSettings_)
  {
    delete defaultSettings_;
    defaultSettings_ = NULL;
  }
  if (defaultRndr_)
  {
    delete defaultRndr_;
    defaultRndr_ = NULL;
  }

  defaultSettings_ = new ThemeConfig(":/Kvantum/default.kvconfig");
  defaultRndr_ = new QSvgRenderer();
  defaultRndr_->load(QString(":/Kvantum/default.svg"));
}

void Style::setUserTheme(const QString &themename)
{
  if (themeSettings_)
  {
    delete themeSettings_;
    themeSettings_ = NULL;
  }
  if (themeRndr_)
  {
    delete themeRndr_;
    themeRndr_ = NULL;
  }

  if (!themename.isNull() && !themename.isEmpty()
      /* "Default" is reserved by Kvantum Manager for copied default theme */
      && themename != "Default"
      /* "Kvantum" is reserved for the alternative installation paths */
      && themename != "Kvantum"
      /* no space in theme name */
      && !(themename.simplified()).contains (" ")
      /* "#" is reserved by Kvantum Manager as an ending for copied root themes */
      && (!themename.contains("#")
          || (themename.count("#") == 1 && themename.endsWith("#"))))
  {
    QString userConfig, userSvg, temp;

    temp = QString("%1/Kvantum/%2/%2.kvconfig")
           .arg(xdg_config_home).arg(themename);
    if (QFile::exists(temp))
      userConfig = temp;
    temp = QString("%1/Kvantum/%2/%2.svg")
           .arg(xdg_config_home).arg(themename);
    if (QFile::exists(temp))
      userSvg = temp;

    /* search in the alternative theme installation paths
       only if there's no such theme in the config folder */
    if (!themename.contains("#") // copied themes don't come here
        && userConfig.isEmpty() && userSvg.isEmpty())
    {
      QString homeDir = QDir::homePath();
      temp = QString("%1/.themes/%2/Kvantum/%2.kvconfig")
             .arg(homeDir).arg(themename);
      if (QFile::exists(temp))
        userConfig = temp;
      temp = QString("%1/.themes/%2/Kvantum/%2.svg")
             .arg(homeDir).arg(themename);
      if (QFile::exists(temp))
        userSvg = temp;

      if (userConfig.isEmpty() && userSvg.isEmpty())
      {
        temp = QString("%1/.local/share/themes/%2/Kvantum/%2.kvconfig")
               .arg(homeDir).arg(themename);
        if (QFile::exists(temp))
          userConfig = temp;
        temp = QString("%1/.local/share/themes/%2/Kvantum/%2.svg")
               .arg(homeDir).arg(themename);
        if (QFile::exists(temp))
          userSvg = temp;
      }

      /* this can't be about a copied theme anymore */
      if (!userConfig.isEmpty())
        themeSettings_ = new ThemeConfig(userConfig);
      if (!userSvg.isEmpty())
      {
        themeRndr_ = new QSvgRenderer();
        themeRndr_->load(userSvg);
      }
      if (themeSettings_ || themeRndr_)
      {
        setupThemeDeps();
        return;
      }
    }

    /*******************
     ** kvconfig file **
     *******************/
    if (!userConfig.isEmpty())
    { // user theme
      themeSettings_ = new ThemeConfig(userConfig);
    }
    else if (userSvg.isEmpty() // otherwise it's a user theme without config file
             && !themename.contains("#")) // root theme names can't have the ending "#"
    { // root theme
      temp = QString(DATADIR)
             + QString("/Kvantum/%1/%1.kvconfig").arg(themename);
      if (QFile::exists(temp))
        themeSettings_ = new ThemeConfig(temp);
      else
      {
        temp = QString(DATADIR)
               + QString("/Kvantum/%1/%1.svg").arg(themename);
        if (!QFile::exists(temp)) // otherwise the checked root theme was just an SVG image
        {
          temp = QString(DATADIR)
                 + QString("/themes/%1/Kvantum/%1.kvconfig").arg(themename);
          if (QFile::exists(temp))
            themeSettings_ = new ThemeConfig(temp);
        }
      }
    }
    /***************
     ** SVG image **
     ***************/
    if (!userSvg.isEmpty())
    { // user theme
      themeRndr_ = new QSvgRenderer();
      themeRndr_->load(userSvg);
    }
    else
    {
      if (!themename.contains("#"))
      {
        if (userConfig.isEmpty()) // otherwise it's a user theme without SVG image
        { // root theme
          temp = QString(DATADIR)
                 + QString("/Kvantum/%1/%1.svg").arg(themename);
          if (QFile::exists(temp))
          {
            themeRndr_ = new QSvgRenderer();
            themeRndr_->load(temp);
          }
          else
          {
            temp = QString(DATADIR)
                   + QString("/Kvantum/%1/%1.kvconfig").arg(themename);
            if (!QFile::exists(temp)) // otherwise the checked root theme was just a config file
            {
              temp = QString(DATADIR)
                     + QString("/themes/%1/Kvantum/%1.svg").arg(themename);
              if (QFile::exists(temp))
              {
                themeRndr_ = new QSvgRenderer();
                themeRndr_->load(temp);
              }
            }
          }
        }
      }
      else if (!userConfig.isEmpty()) // otherwise, the folder has been emptied manually
      { // find the SVG image of the root theme, of which this is a copy
        QString _themename = themename.left(themename.length() - 1);
        if (!_themename.isEmpty() && !_themename.contains("#"))
        {
          temp = QString(DATADIR)
                 + QString("/Kvantum/%1/%1.svg").arg(_themename);
          if (QFile::exists(temp))
          {
            themeRndr_ = new QSvgRenderer();
            themeRndr_->load(temp);
          }
          else
          {
            temp = QString(DATADIR)
                   + QString("/Kvantum/%1/%1.kvconfig").arg(_themename);
            if (!QFile::exists(temp)) // otherwise the checked root theme was just a config file
            {
              temp = QString(DATADIR)
                     + QString("/themes/%1/Kvantum/%1.svg").arg(_themename);
              if (QFile::exists(temp))
              {
                themeRndr_ = new QSvgRenderer();
                themeRndr_->load(temp);
              }
            }
          }
        }
      }
    }
  }

  setupThemeDeps();
}

void Style::setupThemeDeps()
{
  if (themeSettings_)
  {
    // always use the default config as fallback
    themeSettings_->setParent(defaultSettings_);
    settings_ = themeSettings_;
  }
  else
    settings_ = defaultSettings_;
}

void Style::creatBlurHelper()
{
  QList<int> menuS = getShadow("Menu", getMenuMargin(true), getMenuMargin(false));
  QList<int> tooltipS = getShadow("ToolTip", pixelMetric(PM_ToolTipLabelFrameWidth));
  blurHelper_ = new BlurHelper(this,menuS,tooltipS);
}

void Style::advanceProgressbar()
{
  QMap<QWidget *,int>::iterator it;
  for (it = progressbars_.begin(); it != progressbars_.end(); ++it)
  {
    QWidget *widget = it.key();
    if (widget && widget->isVisible())
    {
      it.value() += 2;
      widget->update();
    }
  }
}

void Style::setAnimationOpacity()
{ //qDebug() << animationOpacity_;
  if (animationOpacity_ >= 100 || !animatedWidget_)
    opacityTimer_->stop();
  else
  {
    if (animationOpacity_ <= 100 - OPACITY_STEP)
      animationOpacity_ += OPACITY_STEP;
    else
      animationOpacity_ = 100;
    animatedWidget_->update();
  }
}

void Style::setAnimationOpacityOut()
{ //qDebug() << animatedWidgetOut_;
  if (animationOpacityOut_ >= 100 || !animatedWidgetOut_)
    opacityTimerOut_->stop();
  else
  {
    if (animationOpacityOut_ <= 100 - OPACITY_STEP)
      animationOpacityOut_ += OPACITY_STEP;
    else
      animationOpacityOut_ = 100;
    animatedWidgetOut_->update();
  }
}

int Style::getMenuMargin(bool horiz) const
{
  const frame_spec fspec = getFrameSpec("Menu");
  int margin = horiz ? qMax(fspec.left,fspec.right) : qMax(fspec.top,fspec.bottom);
  if (!noComposite_) // used without compositing at PM_SubMenuOverlap
    margin += settings_->getCompositeSpec().menu_shadow_depth;
  return margin;
}

// This is also used to adjust submenu position horizontally when menus have shadow.
void Style::getMenuHShadows()
{
  if (menuHShadows_.count() == 2)
    return;

  QSvgRenderer *renderer = 0;
  qreal divisor = 0;
  menuHShadows_ << 0 << 0;  // [left, right]
  QList<QString> direction;
  direction << "left" << "right";
  frame_spec fspec = getFrameSpec("Menu");
  QString element = fspec.element;

  for (int i = 0; i < 2; ++i)
  {
    if (themeRndr_ && themeRndr_->isValid() && themeRndr_->elementExists(element+"-shadow-"+direction[i]))
      renderer = themeRndr_;
    else renderer = defaultRndr_;
    if (renderer)
    {
      QRectF br = renderer->boundsOnElement(element+"-shadow-"+direction[i]);
      divisor = br.width();
      if (qRound(divisor))
      {
        if (themeRndr_ && themeRndr_->isValid() && themeRndr_->elementExists(element+"-shadow-hint-"+direction[i]))
          renderer = themeRndr_;
        else if (defaultRndr_->elementExists(element+"-shadow-hint-"+direction[i]))
          renderer = defaultRndr_;
        else renderer = 0;
        if (renderer)
        {
          br = renderer->boundsOnElement(element+"-shadow-hint-"+direction[i]);
          menuHShadows_[i] = getMenuMargin(true)*(br.width()/divisor);
        }
      }
    }
  }
}

QList<int> Style::getShadow (const QString &widgetName, int thicknessH, int thicknessV)
{
  QSvgRenderer *renderer = 0;
  qreal divisor = 0;
  QList<int> shadow;
  shadow << 0 << 0 << 0 << 0;
  QList<QString> direction;
  direction << "left" << "top" << "right" << "bottom";
  frame_spec fspec = getFrameSpec(widgetName);
  QString element = fspec.element;

  for (int i = 0; i < 4; ++i)
  {
    if (widgetName == "Menu" && i%2 == 0 // left and right
        && menuHShadows_.count() == 2)
    {
      shadow[i] = menuHShadows_[i/2];
      continue;
    }
    if (themeRndr_ && themeRndr_->isValid() && themeRndr_->elementExists(element+"-shadow-"+direction[i]))
      renderer = themeRndr_;
    else renderer = defaultRndr_;
    if (renderer)
    {
      QRectF br = renderer->boundsOnElement(element+"-shadow-"+direction[i]);
      divisor = (i%2 ? br.height() : br.width());
      if (divisor)
      {
        if (themeRndr_ && themeRndr_->isValid() && themeRndr_->elementExists(element+"-shadow-hint-"+direction[i]))
          renderer = themeRndr_;
        else if (defaultRndr_->elementExists(element+"-shadow-hint-"+direction[i]))
          renderer = defaultRndr_;
        else renderer = 0;
        if (renderer)
        {
          br = renderer->boundsOnElement(element+"-shadow-hint-"+direction[i]);
          shadow[i] = i%2 ? thicknessV*(br.height()/divisor) : thicknessH*(br.width()/divisor);
        }
      }
    }
  }

  if (widgetName == "Menu" && menuHShadows_.isEmpty())
  {
    menuHShadows_ << shadow[0] << shadow[2];
  }

  return shadow; // [left, top, right, bottom]
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

static inline bool enoughContrast (QColor col1, QColor col2)
{
  if (!col1.isValid() || !col2.isValid()) return false;
  if (qAbs(qGray(col1.rgb()) - qGray(col2.rgb())) < MIN_CONTRAST)
    return false;
  return true;
}

/* Qt >= 5.2 accepts #ARGB as the color name but most apps use #RGBA.
   Here we get the alpha from #RGBA if it exists (and include Qt < 5.2). */
static inline QColor getFromRGBA(const QString str)
{
  QColor col(str);
  if (str.isEmpty() || !(str.size() == 9 && str.startsWith("#")))
    return col;
  bool ok;
  int alpha = str.right(2).toInt(&ok, 16);
  if (ok)
  {
    QString tmp(str);
    tmp.remove(7, 2);
    col = QColor(tmp);
    col.setAlpha(alpha);
  }
  return col;
}

void Style::noTranslucency(QObject *o)
{
  QWidget *widget = static_cast<QWidget*>(o);
  translucentWidgets_.remove(widget);
  hardCodedTranslucency_.remove(widget);
}

int Style::mergedToolbarHeight(const QWidget *menubar) const
{
  if (!tspec_.merge_menubar_with_toolbar || isPlasma_) return 0;
  QWidget *p = getParent(menubar,1);
  if (!p) return 0;
  QList<QToolBar*> tList = p->findChildren<QToolBar*>();
  if (!tList.isEmpty())
  {
    for (int i = 0; i < tList.count(); ++i)
    {
      if (tList.at(i)->isVisible() && tList.at(i)->orientation() == Qt::Horizontal
          && menubar->y()+menubar->height() == tList.at(i)->y())
      {
        return tList.at(i)->height();
        break;
      }
    }
  }
  return 0;
}

bool Style::isStylableToolbar(const QWidget *w) const
{
  const QToolBar *tb = qobject_cast<const QToolBar*>(w);
  if (!tb) return false;
  if (!hspec_.single_top_toolbar) return true;
  if (tb->orientation() == Qt::Vertical) return false;
  if (QMainWindow *mw = qobject_cast<QMainWindow*>(getParent(w,1)))
  {
    if (QWidget *mb = mw->menuWidget()) // WARNING: an empty menubar may be created by menuBar()
    {
      if (mb->isVisible())
      {
        if (mb->y()+mb->height() == tb->y())
          return true;
      }
      else if (tb->y() == 0 && tb->isVisible()) // FIXME: Why can KtoolBar be invisible here?
        return true;
    }
    else if (tb->y() == 0) return true;
  }
  return false;
}

void Style::polish(QWidget *widget)
{
  if (!widget) return;

  // for moving the window containing this widget
  if (itsWindowManager_)
    itsWindowManager_->registerWidget(widget);

  QWidget *pw = widget->parentWidget();

  if (!pw || !pw->inherits("QWebEngineView")) // FIXME: a bug in QtWebEngine?
    widget->setAttribute(Qt::WA_Hover, true);
  //widget->setAttribute(Qt::WA_MouseTracking, true);

  /* So far I haven't found any use for this: */
  /*if (qobject_cast<QMenu*>(widget))
  {
    QColor menuTextColor = getFromRGBA(getLabelSpec("Menu").normalColor);
    QPalette palette = widget->palette();
    if (menuTextColor.isValid() && menuTextColor != palette.color(QPalette::Text))
    {
      palette.setColor(QPalette::Active,QPalette::Text,menuTextColor);
      widget->setPalette(palette);
    }
  }*/

  /* respect the toolbar text color */
  QColor toolbarTextColor = getFromRGBA(getLabelSpec("Toolbar").normalColor);
  QColor windowTextColor = getFromRGBA(cspec_.windowTextColor);
  if (toolbarTextColor.isValid() && toolbarTextColor != windowTextColor)
  {
    QWidget *gp = getParent(pw,1);
    if ((!qobject_cast<QToolButton*>(widget) // flat toolbuttons are dealt with at CE_ToolButtonLabel
         && !qobject_cast<QLineEdit*>(widget)
         && qobject_cast<QMainWindow*>(gp) && isStylableToolbar(pw) // Krita, Amarok
         && !pw->findChild<QTabBar*>())
        || (widget->inherits("AnimatedLabelStack") // Amarok
            && isStylableToolbar(gp)
            && qobject_cast<QMainWindow*>(getParent(gp,1))))
    {
      QPalette palette = widget->palette();
      palette.setColor(QPalette::Active,widget->foregroundRole(),toolbarTextColor);
      palette.setColor(QPalette::Inactive,widget->foregroundRole(),toolbarTextColor);
      palette.setColor(QPalette::Active,QPalette::WindowText,toolbarTextColor); // for KAction in locationbar as in K3b
      palette.setColor(QPalette::Inactive,QPalette::WindowText,toolbarTextColor);
      widget->setPalette(palette);
    }
  }

  if (hspec_.respect_darkness
      && !isPcmanfm_) // we don't want to give a solid backgeound to LXQT's desktop by accident
  {
    QColor winCol = getFromRGBA(cspec_.windowColor);
    if (winCol.isValid() && qGray(winCol.rgb()) <= 100 // there should be darkness to be respected
        // it's usual to define custom colors in text edits
        && !widget->inherits("QTextEdit") && !widget->inherits("QPlainTextEdit")
        && (qobject_cast<QAbstractItemView*>(widget)
            || qobject_cast<QAbstractScrollArea*>(widget)
            || qobject_cast<QTabWidget*>(widget)
            || (qobject_cast<QLabel*>(widget) && !qobject_cast<QLabel*>(widget)->text().isEmpty())))
    {
      QPalette palette = widget->palette();
      QColor txtCol = palette.color(QPalette::Text);
      if (!enoughContrast(palette.color(QPalette::Base), txtCol)
          || !enoughContrast(palette.color(QPalette::Window), palette.color(QPalette::WindowText))
          || (qobject_cast<QAbstractItemView*>(widget)
              && !enoughContrast(palette.color(QPalette::AlternateBase), txtCol)))
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
      /* take all precautions */
      if (!isPlasma_ && !subApp_ && !isLibreoffice_
          && widget->isWindow()
          && widget->windowType() != Qt::Desktop
          && !widget->testAttribute(Qt::WA_PaintOnScreen)
          && !widget->testAttribute(Qt::WA_X11NetWmWindowTypeDesktop)
          && !widget->inherits("KScreenSaver")
          && !widget->inherits("QTipLabel")
          && !widget->inherits("QSplashScreen"))
            
      {
        if (widget->minimumSize() != widget->maximumSize())
        {
          /*if (QMainWindow* mw = qobject_cast<QMainWindow*>(widget))
          {
            if (hspec_.forceSizeGrip)
            {
              QStatusBar *sb = mw->statusBar();
              sb->setSizeGripEnabled(true);
            }
          }
          else*/ if (QDialog *d = qobject_cast<QDialog*>(widget))
          {
            if (hspec_.forceSizeGrip)
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
        }
        bool opaqueWindow(tspec_.translucent_windows && !isOpaque_
                          && !widget->testAttribute(Qt::WA_TranslucentBackground)
                          && !widget->testAttribute(Qt::WA_NoSystemBackground)
                          && !widget->windowFlags().testFlag(Qt::FramelessWindowHint));
        if ((opaqueWindow
            /* enable blurring for hard-coded transluceny */
            || (hspec_.blur_translucent
                && widget->testAttribute(Qt::WA_TranslucentBackground)))
            /* FIXME: I included this because I thought, without it, QtWebKit
               apps would crash on quitting but that wasn't the case. However,
               only blurring needs it and it's taken care of by BlurHelper. */
            //&& widget->internalWinId()
            && !translucentWidgets_.contains(widget))
        {
#if QT_VERSION < 0x050000
          /* workaround for a Qt4 bug, which makes translucent windows
             always appear at the top left corner (taken from QtCurve) */
          bool was_visible = widget->isVisible();
          bool moved = widget->testAttribute(Qt::WA_Moved);
          if (was_visible) widget->hide();
#endif

          if (opaqueWindow)
            widget->setAttribute(Qt::WA_TranslucentBackground);

#if QT_VERSION < 0x050000
          if (!moved) widget->setAttribute(Qt::WA_Moved, false);
          if (was_visible) widget->show();
#endif

          /* enable blurring */
          if (tspec_.blurring
              && blurHelper_) // not needed
          {
            blurHelper_->registerWidget(widget);
          }
          /* enable blurring for hard-coded transluceny */
          else if (!opaqueWindow && hspec_.blur_translucent)
          {
#if defined Q_WS_X11 || defined Q_OS_LINUX
            if (!blurHelper_)
              creatBlurHelper();
#endif
            if (blurHelper_)
              blurHelper_->registerWidget(widget);
          }

          if (opaqueWindow)
          {
            widget->removeEventFilter(this);
            widget->installEventFilter(this);
          }
          else
            hardCodedTranslucency_.insert(widget);
          translucentWidgets_.insert(widget);
          connect(widget, SIGNAL(destroyed(QObject*)),
                  SLOT(noTranslucency(QObject*)));
        }
      }
      break;
    }
    default: break;
  }

  if (isDolphin_
      && qobject_cast<QAbstractScrollArea*>(getParent(widget,2))
      && !qobject_cast<QAbstractScrollArea*>(getParent(widget,3)))
  {
    /* Dolphin sets the background of its KItemListContainer's viewport
       to KColorScheme::View (-> kde-baseapps -> dolphinview.cpp).
       We force our base color here. */
    QColor col = getFromRGBA(cspec_.baseColor);
    if (col.isValid())
    {
      QPalette palette = widget->palette();
      palette.setColor(widget->backgroundRole(), col);
      widget->setPalette(palette);
    }
    /* hack Dolphin's view */
    if (hspec_.transparent_dolphin_view && widget->autoFillBackground())
      widget->setAutoFillBackground(false);
  }
  else if (isPcmanfm_ && (hspec_.transparent_pcmanfm_view || hspec_.transparent_pcmanfm_sidepane))
  {
    QWidget *gp = getParent(widget,2);
    if ((hspec_.transparent_pcmanfm_view
         && widget->autoFillBackground()
         && (gp && gp->inherits("Fm::FolderView") && !gp->inherits("PCManFM::DesktopWindow")))
        || (hspec_.transparent_pcmanfm_sidepane
            && ((pw && pw->inherits("Fm::DirTreeView"))
                || (gp && gp->inherits("Fm::SidePane")))))
    {
      widget->setAutoFillBackground(false);
    }
  }

  // -> ktitlewidget.cpp
  if (widget->inherits("KTitleWidget"))
  {
    if (hspec_.transparent_ktitle_label)
    {
      /*QPalette palette = widget->palette();
      palette.setColor(QPalette::Base,QColor(Qt::transparent));
      widget->setPalette(palette);*/
      if (QFrame *titleFrame = widget->findChild<QFrame*>())
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
  else if (qobject_cast<QProgressBar*>(widget)
           || (tspec_.animate_states &&
               (qobject_cast<QPushButton*>(widget)
                || (qobject_cast<QToolButton*>(widget)
                    && !qobject_cast<QLineEdit*>(pw)) // exclude line-edit clear buttons
                || qobject_cast<QCheckBox*>(widget)
                || qobject_cast<QRadioButton*>(widget)
                || qobject_cast<QSlider*>(widget)
                || qobject_cast<QLineEdit*>(widget)
                || widget->inherits("QComboBoxPrivateContainer")
                || (qobject_cast<QGroupBox*>(widget) && qobject_cast<QGroupBox*>(widget)->isCheckable())
                || qobject_cast<QComboBox*>(widget)))
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
      widget->removeEventFilter(this);
      widget->installEventFilter(this);
  }
  else if (qobject_cast<QLineEdit*>(widget) || widget->inherits("KCalcDisplay"))
  { // in rare cases like KNotes' font combos or Kcalc
    QColor col = getFromRGBA(cspec_.textColor);
    if (col.isValid())
    {
      QPalette palette = widget->palette();
      if (col != palette.color(QPalette::Active,QPalette::Text))
      {
        palette.setColor(QPalette::Active,QPalette::Text,col);
        palette.setColor(QPalette::Inactive,QPalette::Text,col);
        widget->setPalette(palette);
      }
    }
  }
  else if (qobject_cast<QAbstractSpinBox*>(widget))
  {// see eventFilter() for the reason
    widget->removeEventFilter(this);
    widget->installEventFilter(this);
  }
  else if (qobject_cast<QScrollBar*>(widget))
  {
    /* without this, transparent backgrounds
       couldn't be used for scrollbar grooves */
    widget->setAttribute(Qt::WA_OpaquePaintEvent, false);

    if (tspec_.animate_states)
    {
      widget->removeEventFilter(this);
      widget->installEventFilter(this);
    }
  }
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
      else
      {
        // support animation only if the background is flat
        if (tspec_.animate_states
            && themeRndr_ && themeRndr_->isValid()) // the default SVG file doesn't have a focus state for frames
        {
          widget->removeEventFilter(this);
          widget->installEventFilter(this);
        }
        // set the background correctly when scrollbars are either inside the frame or inside a combo popup
        if ((tspec_.scrollbar_in_view || (widget->inherits("QComboBoxListView") && !tspec_.combo_menu))
            && vp && vp->autoFillBackground()
            && (vp->styleSheet().isEmpty() || !vp->styleSheet().contains("background"))
            // but not when the combo popup is drawn as a menu
            && !(tspec_.combo_menu && widget->inherits("QComboBoxListView"))
            // also consider pcmanfm hacking keys
            && !(isPcmanfm_
                 && ((hspec_.transparent_pcmanfm_view && pw
                      && pw->inherits("Fm::FolderView") && !pw->inherits("PCManFM::DesktopWindow"))
                     || (hspec_.transparent_pcmanfm_sidepane
                         && (sa->inherits("Fm::DirTreeView") || (pw && pw->inherits("Fm::SidePane")))))))
        {
          QColor col = vp->palette().color(vp->backgroundRole());
          if (col.isValid())
          {
            QPalette palette;
            if (QScrollBar *sb = sa->horizontalScrollBar())
            {
              sb->setAutoFillBackground(true);
              palette = sb->palette();
              palette.setColor(sb->backgroundRole(), col);
              sb->setPalette(palette);
            }
            if (QScrollBar *sb = sa->verticalScrollBar())
            {
              sb->setAutoFillBackground(true);
              palette = sb->palette();
              palette.setColor(sb->backgroundRole(), col);
              sb->setPalette(palette);
            }
            // FIXME: is this needed?
            palette = widget->palette();
            if (palette.color(vp->backgroundRole()) != col)
            {
              palette.setColor(widget->backgroundRole(), col);
              widget->setPalette(palette);
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
    widget->setPalette(palette);
  }
  else if (QStatusBar *sb = qobject_cast<QStatusBar*>(widget))
  {
    if (hspec_.forceSizeGrip)
    { // WARNING: adding size grip to non-window widgets may cause crash
      if (QMainWindow *mw = qobject_cast<QMainWindow*>(pw))
      {
        if (mw->minimumSize() != mw->maximumSize())
          sb->setSizeGripEnabled(true);
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
      {
        widget->removeEventFilter(this);
        widget->installEventFilter(this);
      }
    }
  }

  if (!isLibreoffice_ // not required
      && !noComposite_
      && !subApp_
      && ((qobject_cast<QMenu*>(widget) && !widget->testAttribute(Qt::WA_X11NetWmWindowTypeMenu))
             /* no shadow for tooltips that are already translucent */
          || (widget->inherits("QTipLabel") && !widget->testAttribute(Qt::WA_TranslucentBackground)))
      && !translucentWidgets_.contains(widget))
  {
    theme_spec tspec_now = settings_->getCompositeSpec();
    if (tspec_now.composite)
    {
      if (qobject_cast<QMenu*>(widget))
      {
        getMenuHShadows();
        /* RTL submenus aren't positioned correctly. To fix that,
           we should move them but the RTL property isn't set yet. */
        if (qobject_cast<QMenu*>(pw))
        {
          widget->removeEventFilter(this);
          widget->installEventFilter(this);
        }
      }
#if QT_VERSION >= 0x050000
      /* FIXME: On rare occasions, the backgrounds of translucent tooltips are filled by
         the window background color. I don't know the root of this bug but what follows
         is a workaround, which works with setSurfaceFormat() below. */
      else// if (widget->inherits("QTipLabel"))
      {
        QPalette palette = widget->palette();
        QColor winCol = palette.window().color();
        winCol.setAlpha(0);
        palette.setColor(QPalette::Window, winCol);
        widget->setPalette(palette);
      }
#endif
      widget->setAttribute(Qt::WA_TranslucentBackground);
      translucentWidgets_.insert(widget);
      connect(widget, SIGNAL(destroyed(QObject*)),
              SLOT(noTranslucency(QObject*)));
#if defined Q_WS_X11 || defined Q_OS_LINUX
      if (!blurHelper_ && tspec_now.popup_blurring)
        creatBlurHelper();
#endif
      /* blurHelper_ may exist because of blurring hard-coded transluceny */
      if (blurHelper_ && tspec_now.popup_blurring)
        blurHelper_->registerWidget(widget);
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

void Style::polish(QApplication *app)
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
    subApp_ = true;
  else if (appName == "dolphin")
    isDolphin_ = true;
  else if (appName == "pcmanfm-qt")
    isPcmanfm_ = true;
  else if (appName == "soffice.bin")
    isLibreoffice_ = true;
  else if (appName == "plasma" || appName.startsWith("plasma-")
           || appName == "plasmashell" // Plasma5
           || appName == "kded4") // this is for the infamous appmenu
    isPlasma_ = true;

  if (tspec_.opaque.contains(appName, Qt::CaseInsensitive))
    isOpaque_ = true;

  /* general colors
     FIXME Is this needed? Can't polish(QPalette&) alone do the job?
     The documentation for QApplication::setPalette() is ambiguous
     but, at least outside KDE and with Qt4, it's sometimes needed. */
  QPalette palette = app->palette();
  polish(palette);
  app->setPalette(palette);

  QCommonStyle::polish(app);
  if (itsShortcutHandler_)
  {
    app->removeEventFilter(itsShortcutHandler_);
    app->installEventFilter(itsShortcutHandler_);
  }

  if (gtkDesktop_) // under gtk DEs, always use their font
    setAppFont();
}

void Style::polish(QPalette &palette)
{
    /* background colors */
    QColor col = getFromRGBA(cspec_.windowColor);
    if (col.isValid())
      palette.setColor(QPalette::Window,col);
    col = getFromRGBA(cspec_.baseColor);
    if (col.isValid())
      palette.setColor(QPalette::Base,col);
    col = getFromRGBA(cspec_.altBaseColor);
    if (col.isValid())
      palette.setColor(QPalette::AlternateBase,col);
    col = getFromRGBA(cspec_.buttonColor);
    if (col.isValid())
      palette.setColor(QPalette::Button,col);

    col = getFromRGBA(cspec_.lightColor);
    if (col.isValid())
      palette.setColor(QPalette::Light,col);
    col = getFromRGBA(cspec_.midLightColor);
    if (col.isValid())
      palette.setColor(QPalette::Midlight,col);
    col = getFromRGBA(cspec_.darkColor);
    if (col.isValid())
      palette.setColor(QPalette::Dark,col);
    col = getFromRGBA(cspec_.midColor);
    if (col.isValid())
      palette.setColor(QPalette::Mid,col);
    col = getFromRGBA(cspec_.shadowColor);
    if (col.isValid())
      palette.setColor(QPalette::Shadow,col);

    col = getFromRGBA(cspec_.highlightColor);
    if (col.isValid())
      palette.setColor(QPalette::Active,QPalette::Highlight,col);
    col = getFromRGBA(cspec_.inactiveHighlightColor);
    if (col.isValid())
      palette.setColor(QPalette::Inactive,QPalette::Highlight,col);

    col = getFromRGBA(cspec_.tooltipBasetColor);
    if (col.isValid())
      palette.setColor(QPalette::ToolTipBase,col);
    else
    { // for backward compatibility
      col = getFromRGBA(cspec_.tooltipTextColor);
      if (col.isValid())
      {
        QColor col1 = QColor(Qt::white);
        if (qGray(col.rgb()) >= 127)
          col1 = QColor(Qt::black);
        palette.setColor(QPalette::ToolTipBase,col1);
      }
    }

    /* text colors */
    col = getFromRGBA(cspec_.textColor);
    if (col.isValid())
    {
      palette.setColor(QPalette::Active,QPalette::Text,col);
      palette.setColor(QPalette::Inactive,QPalette::Text,col);
    }
    col = getFromRGBA(cspec_.windowTextColor);
    if (col.isValid())
    {
      palette.setColor(QPalette::Active,QPalette::WindowText,col);
      palette.setColor(QPalette::Inactive,QPalette::WindowText,col);
    }
    col = getFromRGBA(cspec_.buttonTextColor);
    if (col.isValid())
    {
      palette.setColor(QPalette::Active,QPalette::ButtonText,col);
      palette.setColor(QPalette::Inactive,QPalette::ButtonText,col);
    }
    col = getFromRGBA(cspec_.tooltipTextColor);
    if (col.isValid())
      palette.setColor(QPalette::ToolTipText,col);
    col = getFromRGBA(cspec_.highlightTextColor);
    if (col.isValid())
      palette.setColor(QPalette::HighlightedText,col);
    col = getFromRGBA(cspec_.linkColor);
    if (col.isValid())
      palette.setColor(QPalette::Link,col);
    col = getFromRGBA(cspec_.linkVisitedColor);
    if (col.isValid())
      palette.setColor(QPalette::LinkVisited,col);

    /* disabled text */
    col = getFromRGBA(cspec_.disabledTextColor);
    if (col.isValid())
    {
      palette.setColor(QPalette::Disabled,QPalette::Text,col);
      palette.setColor(QPalette::Disabled,QPalette::WindowText,col);
      palette.setColor(QPalette::Disabled,QPalette::ButtonText,col);
    }
}

void Style::unpolish(QWidget *widget)
{
  if (widget)
  {
    if (itsWindowManager_)
      itsWindowManager_->unregisterWidget(widget);

    /*widget->setAttribute(Qt::WA_Hover, false);*/

    switch (widget->windowFlags() & Qt::WindowType_Mask) {
      case Qt::Window:
      case Qt::Dialog: {
        if (blurHelper_)
          blurHelper_->unregisterWidget(widget);
        if (translucentWidgets_.contains(widget)
            && !hardCodedTranslucency_.contains(widget))
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

    if (widget->inherits("KisAbstractSliderSpinBox")
        || widget->inherits("Digikam::DAbstractSliderSpinBox")
        || widget->inherits("KMultiTabBarTab")
        || qobject_cast<QProgressBar*>(widget)
        || qobject_cast<QAbstractSpinBox*>(widget)
        || qobject_cast<QToolButton*>(widget)
        || (tspec_.animate_states &&
            (qobject_cast<QPushButton*>(widget)
             || qobject_cast<QCheckBox*>(widget)
             || qobject_cast<QRadioButton*>(widget)
             || qobject_cast<QScrollBar*>(widget)
             || qobject_cast<QSlider*>(widget)
             || qobject_cast<QLineEdit*>(widget)
             || qobject_cast<QAbstractScrollArea*>(widget)
             || widget->inherits("QComboBoxPrivateContainer")
             || qobject_cast<QGroupBox*>(widget)
             || qobject_cast<QComboBox*>(widget))))
    {
      widget->removeEventFilter(this);
    }
    else if (qobject_cast<QToolBox*>(widget))
      widget->setBackgroundRole(QPalette::Button);

    if ((qobject_cast<QMenu*>(widget) || widget->inherits("QTipLabel")))
    {
      if (blurHelper_)
        blurHelper_->unregisterWidget(widget);
      if (translucentWidgets_.contains(widget))
      {
        if (qobject_cast<QMenu*>(widget) && qobject_cast<QMenu*>(getParent(widget,1)))
          widget->removeEventFilter(this);
        widget->setAttribute(Qt::WA_PaintOnScreen, false);
        widget->setAttribute(Qt::WA_NoSystemBackground, false);
        widget->setAttribute(Qt::WA_TranslucentBackground, false);
      }
      //widget->clearMask();
    }
  }
}

void Style::unpolish(QApplication *app)
{
  if (itsShortcutHandler_)
    app->removeEventFilter(itsShortcutHandler_);
  QCommonStyle::unpolish(app);
}

void Style::drawBg(QPainter *p, const QWidget *widget) const
{
  if (widget->palette().color(widget->backgroundRole()) == Qt::transparent)
    return; // Plasma FIXME needed?
  QRect bgndRect(widget->rect());
  interior_spec ispec = getInteriorSpec("DialogTranslucent");
  if (ispec.element.isEmpty())
    ispec = getInteriorSpec("Dialog");
  if (!ispec.element.isEmpty()
      && !widget->windowFlags().testFlag(Qt::FramelessWindowHint)) // not a panel
  {
    if (QWidget *child = widget->childAt(0,0))
    { // even dialogs may have menubar or toolbar (as in Qt Designer)
      if (qobject_cast<QMenuBar*>(child) || qobject_cast<QToolBar*>(child))
      {
        ispec = getInteriorSpec("WindowTranslucent");
        if (ispec.element.isEmpty())
          ispec = getInteriorSpec("Window");
      }
    }
  }
  else
  {
    ispec = getInteriorSpec("WindowTranslucent");
    if (ispec.element.isEmpty())
      ispec = getInteriorSpec("Window");
  }
  frame_spec fspec;
  default_frame_spec(fspec);

  QString suffix = "-normal";
  if (!widget->isActiveWindow())
    suffix = "-normal-inactive";

  p->setClipRegion(bgndRect, Qt::IntersectClip);
  renderInterior(p,bgndRect,fspec,ispec,ispec.element+suffix);
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
               /*&& tspec_.translucent_windows*/ // this could have weird effects with style or settings_ change
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

  case QEvent::HoverEnter:
    if (w && w->isEnabled() && tspec_.animate_states
        && !w->isWindow() // WARNING: Translucent (Qt5) windows have enter event!
        && !qobject_cast<QAbstractSpinBox*>(o) && !qobject_cast<QProgressBar*>(o)
        && !qobject_cast<QLineEdit*>(o) && !qobject_cast<QAbstractScrollArea*>(o)
        && !(tspec_.combo_as_lineedit
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
        animatedWidget_ = NULL;
      }
      if (qobject_cast<QPushButton*>(o) || qobject_cast<QToolButton*>(o))
      {
        QAbstractButton *ab = qobject_cast<QAbstractButton*>(o);
        if (!ab->isDown() && !ab->isChecked())
        {
          animationStartState_ = "normal";
          if (!w->isActiveWindow())
            animationStartState_.append("-inactive");
          animatedWidget_ = w;
          animationOpacity_ = 0;
          opacityTimer_->start(ANIMATION_FRAME);
        }
        else if (ab->isChecked())
        {
          animationStartState_ = "toggled";
          if (!w->isActiveWindow())
            animationStartState_.append("-inactive");
        }
      }
      else if (qobject_cast<QCheckBox*>(o) || qobject_cast<QRadioButton*>(o))
      {
        QAbstractButton *ab = qobject_cast<QAbstractButton*>(o);
        if (!ab->isChecked())
          animationStartState_ = "-normal";
        else
        {
          if (QCheckBox *cb = qobject_cast<QCheckBox*>(o))
          {
            if (cb->checkState() == Qt::PartiallyChecked)
              animationStartState_ = "-tristate-normal";
            else
              animationStartState_ = "-checked-normal";
          }
          else
           animationStartState_ = "-checked-normal";
        }
        if (!w->isActiveWindow())
          animationStartState_.append("-inactive");
        animatedWidget_ = w;
        animationOpacity_ = 0;
        opacityTimer_->start(ANIMATION_FRAME);
      }
      else if (qobject_cast<QGroupBox*>(o))
      { // no animation; just for getting the next start state
        if (animatedWidget_ != w)
        {
          animatedWidget_ = w;
          animationOpacity_ = 100;
        }
      }
      else if (qobject_cast<QComboBox*>(o))
      {
        if (!w->hasFocus())
          animationStartState_ = "normal";
        else if (!animationStartState_.startsWith("toggled")) // the popup may have been closed (with Qt5)
          animationStartState_ = "pressed";
        if (!w->isActiveWindow())
          animationStartState_.append("-inactive");
        animatedWidget_ = w;
        animationOpacity_ = 0;
        opacityTimer_->start(ANIMATION_FRAME);
      }
      else if (qobject_cast<QScrollBar*>(o) || qobject_cast<QSlider*>(o))
      {
        animationStartState_ = "normal";
        if (!w->isActiveWindow())
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
          && !(tspec_.combo_as_lineedit && qobject_cast<QComboBox*>(o)->lineEdit()))
      { // QEvent::MouseButtonPress may follow this
        if (opacityTimer_->isActive()) // the cusror may have been on the popup scrollbar
        {
          opacityTimer_->stop();
          animationOpacity_ = 100;
          animatedWidget_->update();
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
            || (tspec_.combo_as_lineedit
                && qobject_cast<QComboBox*>(o) && qobject_cast<QComboBox*>(o)->lineEdit()))
        {
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
        if (opacityTimerOut_->isActive())
        {
          opacityTimerOut_->stop();
          animationOpacityOut_ = 100;
          animatedWidgetOut_->update();
        }
        if (qobject_cast<QComboBox*>(o)
            && !(tspec_.combo_as_lineedit && qobject_cast<QComboBox*>(o)->lineEdit()))
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

  // we use HoverLeave and not Leave because of popups of comboxes
  case QEvent::HoverLeave:
    if (w && w->isEnabled() && tspec_.animate_states && animatedWidget_ == w
        && !qobject_cast<QAbstractSpinBox*>(o)
        && !qobject_cast<QLineEdit*>(o) && !qobject_cast<QAbstractScrollArea*>(o)
        && !(tspec_.combo_as_lineedit
             && qobject_cast<QComboBox*>(o) && qobject_cast<QComboBox*>(o)->lineEdit()))
    {
      if (qobject_cast<QPushButton*>(o) || qobject_cast<QToolButton*>(o))
      {
        QAbstractButton *ab = qobject_cast<QAbstractButton*>(o);
        if (ab->isCheckable() && ab->isChecked())
          break;
      }
      if (!opacityTimer_->isActive() && !qobject_cast<QGroupBox*>(o))
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
      if (QAbstractButton *ab = qobject_cast<QAbstractButton*>(o))
      { // includes checkboxes and radio buttons too
        if (qobject_cast<QPushButton*>(o) || qobject_cast<QToolButton*>(o))
        {
          if ((!ab->isCheckable() && ab->isDown()) || ab->isChecked())
          {
              if (!ab->isCheckable() && ab->isDown())
            animationStartState_ = "pressed";
            else
              animationStartState_ = "toggled";
          }
          else
            animationStartState_ = "pressed";
          if (!w->isActiveWindow())
            animationStartState_.append("-inactive");
        }
        animatedWidget_ = w;
        animationOpacity_ = 0;
        opacityTimer_->start(ANIMATION_FRAME);
      } // FIXME: Also take "autoExclusive" into account?
      else if (qobject_cast<QGroupBox*>(o))
      {
        animatedWidget_ = w;
        animationOpacity_ = 0;
        opacityTimer_->start(ANIMATION_FRAME);
      }
      else if ((qobject_cast<QComboBox*>(o) // impossible because of popup
                && !(tspec_.combo_as_lineedit && qobject_cast<QComboBox*>(o)->lineEdit()))
               || qobject_cast<QScrollBar*>(o) || qobject_cast<QSlider*>(o))
      {
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
        && animatedWidget_ == w
        // these widget are dealt with in QEvent::FocusIn
        && !qobject_cast<QAbstractSpinBox*>(o)
        && !qobject_cast<QLineEdit*>(o) && !qobject_cast<QAbstractScrollArea*>(o))
    {
      if (QAbstractButton *ab = qobject_cast<QAbstractButton*>(o))
      {
        if (qobject_cast<QCheckBox*>(o) || qobject_cast<QRadioButton*>(o)
            || (ab->isCheckable() && ab->isChecked())) // from toggled to toggled
        {
          break;
        }
      }
      else if (qobject_cast<QGroupBox*>(o))
        break;
      animatedWidget_ = w;
      animationOpacity_ = 0;
      opacityTimer_->start(ANIMATION_FRAME);
    }
    break;
  }

  case QEvent::Show:
    if (w)
    {
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
      else if (!noComposite_ && w->layoutDirection() == Qt::RightToLeft
               && menuHShadows_.count() == 2
               && qobject_cast<QMenu*>(o) && qobject_cast<QMenu*>(getParent(w,1)))
      {
        // correct the submenu position
        w->move(w->x() + menuHShadows_.at(0)
                       - (getMenuMargin(true) - menuHShadows_.at(1)),
                w->y());
      }
      else if (qobject_cast<QToolButton*>(o))
      {
        if (QToolBar *toolBar = qobject_cast<QToolBar*>(w->parentWidget()))
          toolBar->update();
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
      QSize size = sizeFromContents(CT_SpinBox,NULL,QSize(),w);
      if (w->maximumWidth() > size.width())
        w->setMinimumWidth(size.width());
      if (w->maximumHeight() > size.height())
        w->setMinimumHeight(size.height());
    }
    break;

  case QEvent::Hide:
    if (tspec_.group_toolbar_buttons && qobject_cast<QToolButton*>(o))
    {
      if (QToolBar *toolBar = qobject_cast<QToolBar*>(w->parentWidget()))
        toolBar->update();
      //break; // toolbuttons may be animated (see below)
    }
    else if (w && w->isEnabled() && tspec_.animate_states
             && w->inherits("QComboBoxPrivateContainer")
             && qobject_cast<QComboBox*>(getParent(w, 1)))
    {
      /* start with an appropriate state on closing popup, considering
         that lineedits only have normal and focused states in Kvantum */
      if (tspec_.combo_as_lineedit && qobject_cast<QComboBox*>(getParent(w, 1))->lineEdit())
        animationStartState_ = "normal"; // -> QEvent::FocusIn
      else
        animationStartState_ = "toggled";
      /* ensure that the combobox will be animated on closing popup
         (especially needed if the cursor has been on the popup) */
      animatedWidget_ = getParent(w, 1);
      animationOpacity_ = 0;
      break;
    }
  case QEvent::Destroy: // FIXME: Isn't QEvent::Hide enough?
    if (w)
    {
      if (!progressbars_.isEmpty() && qobject_cast<QProgressBar*>(o))
      {
        progressbars_.remove(w);
        if (progressbars_.size() == 0)
          progressTimer_->stop();
      }
      else
      {
        if (animatedWidget_ == w)
        {
          opacityTimer_->stop();
          animatedWidget_ = NULL;
          animationOpacity_ = 100;
        }
        if (animatedWidgetOut_ == w)
        {
          opacityTimerOut_->stop();
          animatedWidgetOut_ = NULL;
          animationOpacityOut_ = 100;
        }
      }
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

/*static bool hasArrow (const QToolButton *tb, const QStyleOptionToolButton *opt)
{
  if (!tb || !opt) return false;
  if (tb->popupMode() == QToolButton::MenuButtonPopup
      || ((tb->popupMode() == QToolButton::InstantPopup
           || tb->popupMode() == QToolButton::DelayedPopup)
          && opt && (opt->features & QStyleOptionToolButton::HasMenu)))
  {
    return true;
  }
  return false;
}*/

static int whichToolbarButton (const QToolButton *tb, const QToolBar *toolBar)
{
  int res = tbAlone;

  if (!tb || !toolBar)
    return res;

  if (toolBar->orientation() == Qt::Horizontal)
  {
    QRect g = tb->geometry();
    const QToolButton *left = qobject_cast<const QToolButton*>(toolBar->childAt (g.x()-1, g.y()));
    const QToolButton *right =  qobject_cast<const QToolButton*>(toolBar->childAt (g.x()+g.width()+1, g.y()));

    if (left && g.height() == left->height())
    {
      if (right && g.height() == right->height())
        res = tbMiddle;
      else
        res = tbRight;
    }
    else if (right && g.height() == right->height())
      res = tbLeft;
  }
  // we don't group buttons on a vertical toolbar
  /*else
  {
    // opt was QStyleOptionToolButton*
    if (hasArrow (tb, opt))
      return res;

    QRect g = tb->geometry();
    const QToolButton *top = qobject_cast<const QToolButton*>(toolBar->childAt (g.x(), g.y()-1));
    const QToolButton *bottom =  qobject_cast<const QToolButton*>(toolBar->childAt (g.x(), g.y()+g.height()+1));

    if (top && !hasArrow (top, opt) && opt->icon.isNull() == top->icon().isNull())
    {
      if (bottom && !hasArrow (bottom, opt) && opt->icon.isNull() == bottom->icon().isNull())
        res = tbMiddle;
      else
        res = tbRight;
    }
    else if (bottom && !hasArrow (bottom, opt) && opt->icon.isNull() == bottom->icon().isNull())
      res = tbLeft;
  }*/

  return res;
}

/* get the widest day/month string if needed */
static QString maxDay;
static QString maxMonth;
static QString maxFullDay;
static QString maxFullMonth;
static void getMaxDay(bool full)
{
  QString day;
  int max = 0;
  QLocale::FormatType format = full ? QLocale::LongFormat : QLocale::ShortFormat;
  for (int i=1; i<=7 ; ++i)
  {
    QString theDay = QLocale::system().dayName(i,format);
    int size = QFontMetrics(QApplication::font()).width(theDay);
    if (max < size)
    {
      max = size;
      day = theDay;
    }
  }
  if (full) maxFullDay = day;
  else maxDay = day;
}
static void getMaxMonth(bool full)
{
  QString month;
  int max = 0;
  QLocale::FormatType format = full ? QLocale::LongFormat : QLocale::ShortFormat;
  for (int i=1; i<=12 ; ++i)
  {
    QString theMonth = QLocale::system().monthName(i,format);
    int size = QFontMetrics(QApplication::font()).width(theMonth);
    if (max < size)
    {
      max = size;
      month = theMonth;
    }
  }
  if (full) maxFullMonth = month;
  else maxMonth = month;
}

static inline QString spinMaxText (const QAbstractSpinBox *sp)
{
  QString maxTxt;
  if (const QSpinBox *sb = qobject_cast<const QSpinBox*>(sp))
  {
    int max = sb->maximum();
    int min = sb->minimum();
    max = (max + min < 0 ? -min : max);
    maxTxt = QString("%1%2%3").arg(sb->prefix()).arg(max).arg(sb->suffix());
    if (min < 0) maxTxt = "-" + maxTxt;
  }
  else if (const QDoubleSpinBox *sb = qobject_cast<const QDoubleSpinBox*>(sp))
  {
    double max = sb->maximum();
    double min = sb->minimum();
    int Max = (max + min < 0 ? -min : max);
    maxTxt = QString("%1%2%3").arg(sb->prefix()).arg(Max).arg(sb->suffix());
    if (sb->decimals() > 0)
    {
      maxTxt = maxTxt + ".";
      for (int i = 0; i < sb->decimals() ; ++i) maxTxt = maxTxt + "0";
    }
    if (min < 0) maxTxt = "-" + maxTxt;
  }
  else if (const QDateTimeEdit *sb = qobject_cast<const QDateTimeEdit*>(sp))
  {
    maxTxt = sb->displayFormat();
    /* take into account leading zeros */
    QRegExp exp = QRegExp("hh|HH|mm|ss");
    maxTxt.replace(exp,"00");
    exp = QRegExp("h|H|m|s");
    maxTxt.replace(exp,"00");
    maxTxt.replace("zzz","000");
    maxTxt.replace("z","000");
    /* year ('0' is a little wider that 'y') */
    maxTxt.replace("yy","00");
    maxTxt.replace("yyyy","0000");
    /* am/pm */
    maxTxt.replace("ap","pm",Qt::CaseInsensitive);
    maxTxt.replace("a","pm",Qt::CaseInsensitive);
    /* these will be replaced later */
    maxTxt.replace("dddd","eeee");
    maxTxt.replace("MMMM","ffff");
    maxTxt.replace("ddd","eee");
    maxTxt.replace("MMM","fff");
    /* leading zeros */
    exp = QRegExp("dd|MM");
    maxTxt.replace(exp,"00");
    exp = QRegExp("d|M");
    maxTxt.replace(exp,"00");
    /* time zone */
    maxTxt.replace("t",sb->dateTime().toString("t"));
    /* full day/month name */
    if (maxTxt.contains("eeee"))
    {
      if (maxFullDay.isNull()) getMaxDay(true);
      maxTxt.replace("eeee",maxFullDay);
    }
    if (maxTxt.contains("ffff"))
    {
      if (maxFullMonth.isNull()) getMaxMonth(true);
      maxTxt.replace("ffff",maxFullMonth);
    }
    /* short day/month name */
    if (maxTxt.contains("eee"))
    {
      if (maxDay.isNull()) getMaxDay(false);
      maxTxt.replace("eee",maxDay);
    }
    if (maxTxt.contains("fff"))
    {
      if (maxMonth.isNull()) getMaxMonth(false);
      maxTxt.replace("fff",maxMonth);
    }
  }
  if (!maxTxt.isEmpty())
  {
    QString svt = sp->specialValueText();
    if (!svt.isEmpty())
    {
      QFontMetrics fm(sp->font());
      if (fm.width(svt) > fm.width(maxTxt))
        maxTxt = svt;
    }
  }
  return maxTxt;
}

/* Does the (tool-)button have a panel drawn at PE_PanelButtonCommand?
   This is used for setting the text color of non-flat, panelless buttons that are
   already styled, like those in QtCreator's find bar or QupZilla's bookmark toolbar. */
static QSet<const QWidget*> paneledButtons;

/* Is this button drawn in a standard way? If so, we don't want
   to force any text color on it with forceButtonTextColor(). */
static QSet<const QWidget*> standardButton;

/* Although not usual, it's possible that a subclassed toolbutton sets its palette
   in its paintEvent(), in which case, using of forceButtonTextColor() below could
   result in an infinite loop if our criterion for setting a new palette was only
   the text color. We use the following QHash to prevent such loops. */
static QHash<QWidget*,QColor> txtColForced;

void Style::removeFromSet(QObject *o)
{
  QWidget *widget = static_cast<QWidget*>(o);
  paneledButtons.remove(widget);
  standardButton.remove(widget);
  txtColForced.remove(widget);
}

/* KCalc (KCalcButton), Dragon Player and, perhaps, some other apps set the text color
   of their pushbuttons, although those buttons have bevel like ordinary pushbuttons,
   and digiKam sets the text color of its vertical toolbuttons. This is a method to force
   the push or tool button text colors when the bevel is drawn at CE_PushButtonBevel or
   PE_PanelButtonTool, without forcing any color when the bevel is drawn differently, as
   in Amarok's BreadcrumbItemButton (ElidingButton). */
void Style::forceButtonTextColor(QWidget *widget, QColor col) const
{
  /* eliminate any possibility of getting caught in infinite loops */
  if (widget && txtColForced.contains(widget) && txtColForced.value(widget) == col)
    return;

  QAbstractButton *b = qobject_cast<QAbstractButton*>(widget);
  if (!b) return;
  if (!col.isValid())
    col = QApplication::palette().color(QPalette::ButtonText);
  //QPushButton *pb = qobject_cast<QPushButton*>(b);
  //QToolButton *tb = qobject_cast<QToolButton*>(b);
  if (col.isValid()
      //&& (!pb || !pb->isFlat())
      //&& (!tb || paneledButtons.contains(widget))
      && !b->text().isEmpty()) // make exception for the cursor-like KUrlNavigatorToggleButton
  {
    QPalette palette = b->palette();
    if (col != palette.color(QPalette::ButtonText))
    {
      palette.setColor(QPalette::Active,QPalette::ButtonText,col);
      palette.setColor(QPalette::Inactive,QPalette::ButtonText,col);
      b->setPalette(palette);
      txtColForced.insert(widget,col);
      connect(widget, SIGNAL(destroyed(QObject*)), SLOT(removeFromSet(QObject*)), Qt::UniqueConnection);
    }
  }
}

/* Compute the size of a text. */
static inline QSize textSize(const QFont &font, const QString &text)
{
  int tw, th;
  tw = th = 0;

  if (!text.isEmpty())
  {
    QString t(text);
    /* remove the '&' mnemonic character and tabs (for menu items) */
    t.remove('\t');
    int i = 0;
    while (i < t.size())
    {
      if (t.at(i) == '&')
        t.remove(i,1);
      i++;
    }

    /* deal with newlines */
    QStringList l = t.split('\n');

    if (l.size() == 1)
      th = QFontMetrics(font).height()*(l.size());
    else
    {
      /* For some fonts, e.g. Noto Sans, QFontMetrics(font)::height() returns
         a too big height for multiline texts but QFontMetrics::boundingRect()
         returns the correct height with character M. I don't know how they
         found the so-called "magic constant" 1.6 but it seems to be correct. */
      th = QFontMetrics(font).boundingRect(QLatin1Char('M')).height()*1.6;
      th *= l.size();
    }
    for (int i=0; i<l.size(); i++)
      tw = qMax(tw,QFontMetrics(font).width(l[i]));
  }

  return QSize(tw,th);
}

QString Style::getState(const QStyleOption *option, const QWidget *widget) const
{ // here only widget may be NULL
  QString status =
        (option->state & State_Enabled) ?
          (option->state & State_On) ? "toggled" :
          (option->state & State_Sunken) ? "pressed" :
          (option->state & State_Selected) ? "toggled" :
          (option->state & State_MouseOver) ? "focused" : "normal"
        : "disabled";
  if (widget && !widget->isActiveWindow())
    status.append("-inactive");
  return status;

  /*
     The following condition will be needed later:
       (option->state & State_Enabled) && ((status.startsWith("toggled") || status.startsWith("pressed"))
     Logically, it can be written as:
       (A || (!B && C)) || (!A && B)
     where,
       A = option->state & State_On
       B = option->state & State_Sunken
       C = option->state & State_Selected
     And the latter expression is equivalent to:
       A || B || C
  */
}

/* This method is used, instead of drawPrimitive(PE_PanelLineEdit,...), for drawing the lineedit
   of an editable combobox because animatedWidget_ is always NULL for KUrlComboBox -> KLineEdit.
   Although it's only needed in such special cases, it can always be used safely. */
void Style::drawComboLineEdit(const QStyleOption *option,
                              QPainter *painter,
                              const QWidget *lineedit,
                              const QWidget *combo) const
{
  if (!lineedit || !combo) return;
  if (isPlasma_ && lineedit->window()->testAttribute(Qt::WA_NoSystemBackground))
    return;

  const QString group = "LineEdit";
  interior_spec ispec = getInteriorSpec(group);
  frame_spec fspec = getFrameSpec(group);
  label_spec lspec = getLabelSpec(group);
  lspec.top = qMax(0,lspec.top-1);
  lspec.bottom = qMax(0,lspec.bottom-1);
  const size_spec sspec = getSizeSpec(group);

  if (isLibreoffice_) // impossible because lineedit != NULL
  {
    fspec.left = fspec.right = fspec.top = fspec.bottom = qMin(fspec.left,3);
  }
  else
  {
    if (!lineedit->styleSheet().isEmpty() && lineedit->styleSheet().contains("padding"))
    {
      fspec.left = fspec.right = fspec.top = fspec.bottom = qMin(fspec.left,3);
    }
    else
    {
      if (lineedit->minimumWidth() == lineedit->maximumWidth())
      {
        fspec.left = qMin(fspec.left,3);
        fspec.right = qMin(fspec.right,3);
      }
      if (lineedit->height() < sizeCalculated(lineedit->font(),fspec,lspec,sspec,"W",QSize()).height())
      {
        fspec.top = qMin(fspec.top,3);
        fspec.bottom = qMin(fspec.bottom,3);
      }
    }
  }

  if (qobject_cast<QAbstractItemView*>(getParent(combo,1)))
  {
    fspec.left = fspec.right = fspec.top = fspec.bottom = fspec.expansion = 0;
  }

  fspec.hasCapsule = true;
  if (option->direction == Qt::RightToLeft)
  {
    if (lineedit->width() < combo->width() - COMBO_ARROW_LENGTH
                            - (tspec_.combo_as_lineedit ? fspec.left : getFrameSpec("ComboBox").left))
      fspec.capsuleH = 0;
    else fspec.capsuleH = 1;
  }
  else
  {
    if (lineedit->x() > 0) fspec.capsuleH = 0;
    else fspec.capsuleH = -1;
  }
  fspec.capsuleV = 2;

  // lineedits only have normal and focused states in Kvantum
  QString leStatus = (option->state & State_HasFocus) ? "-focused" : "-normal";
  if (!lineedit->isActiveWindow())
    leStatus.append("-inactive");
  if (!(option->state & State_Enabled))
  {
    painter->save();
    painter->setOpacity(DISABLED_OPACITY);
  }
  renderFrame(painter,
              isLibreoffice_ ? // impossible
                option->rect.adjusted(fspec.left,fspec.top,-fspec.right,-fspec.bottom) :
                option->rect,
              fspec,
              fspec.element+leStatus);
  renderInterior(painter,option->rect,fspec,ispec,ispec.element+leStatus);
  if (!(option->state & State_Enabled))
    painter->restore();
}

void Style::drawPrimitive(PrimitiveElement element,
                          const QStyleOption *option,
                          QPainter *painter,
                          const QWidget *widget) const
{
  int x,y,h,w;
  option->rect.getRect(&x,&y,&w,&h);

  switch(element) {
    case PE_Widget : {
      // only for windows and dialogs
      if (widget // it's NULL with QML
          && !widget->isWindow())
        break;

      // we don't accept custom background colors for windows...
      if (!widget // QML
          || (option->palette.color(QPalette::Window) != QApplication::palette().color(QPalette::Window)
              && !widget->testAttribute(Qt::WA_TranslucentBackground)
              && !widget->testAttribute(Qt::WA_NoSystemBackground)))
      {
        if (option->palette.color(QPalette::Window) == option->palette.color(QPalette::Base))
          break; // ...but make an exception for apps like KNotes
        else
          painter->fillRect(option->rect, QApplication::palette().color(QPalette::Window));
      }

      interior_spec ispec = getInteriorSpec("Dialog");
      if (widget && !ispec.element.isEmpty()
          && !widget->windowFlags().testFlag(Qt::FramelessWindowHint)) // not a panel)
      {
        if (QWidget *child = widget->childAt(0,0))
        {
          if (qobject_cast<QMenuBar*>(child) || qobject_cast<QToolBar*>(child))
            ispec = getInteriorSpec("Window");
        }
      }
      else
        ispec = getInteriorSpec("Window");
      frame_spec fspec;
      default_frame_spec(fspec);

      QString suffix = "-normal";
      if (widget && !widget->isActiveWindow())
        suffix = "-normal-inactive";
      renderInterior(painter,option->rect,fspec,ispec,ispec.element+suffix);

      break;
    }

    case PE_FrameStatusBar : {return;} // simple is elegant

    case PE_FrameDockWidget : {
      frame_spec fspec = getFrameSpec("Dock");
      const interior_spec ispec = getInteriorSpec("Dock");
      fspec.expansion = 0;

      QString status = getState(option,widget);
      if (!(option->state & State_Enabled))
      {
        status.replace("disabled","normal");
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+status);
      renderFrame(painter,option->rect,fspec,fspec.element+"-"+status);
      if (!(option->state & State_Enabled))
        painter->restore();

      break;
    }

    case PE_FrameTabBarBase : {
      if (const QStyleOptionTabBarBase_v2 *opt
              = qstyleoption_cast<const QStyleOptionTabBarBase_v2*>(option))
      {
        QRect r = option->rect;
        // FIXME: Why does Qt draw redundant frames when there's a corner widget?
        if (!r.contains(opt->tabBarRect) || r == opt->tabBarRect)
          return;

        int l = 0; int d = 0;
        QRect tr = opt->selectedTabRect;
        if (tspec_.attach_active_tab)
        {
          if (tr.isNull()) return;
          d = tr.x();
          l = tr.width();
        }
        bool verticalTabs = false;
        bool bottomTabs = false;
        // as with CE_TabBarTabShape
        if (opt->shape == QTabBar::RoundedEast
            || opt->shape == QTabBar::RoundedWest
            || opt->shape == QTabBar::TriangularEast
            || opt->shape == QTabBar::TriangularWest)
        {
          if (tspec_.attach_active_tab)
          {
            l = tr.height();
            d = tr.y();
          }
          verticalTabs = true;
          painter->save();
          int X, Y, rot;
          int xTr = 0; int xScale = 1;
          if (tspec_.mirror_doc_tabs
              && (opt->shape == QTabBar::RoundedEast || opt->shape == QTabBar::TriangularEast))
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
          m.translate(xTr, 0); m.scale(xScale,1);
          painter->setTransform(m, true);
        }
        else if (tspec_.mirror_doc_tabs
                 && (opt->shape == QTabBar::RoundedSouth || opt->shape == QTabBar::TriangularSouth))
        {
          if (tspec_.attach_active_tab)
            d = w - l - d;
          bottomTabs = true;
          painter->save();
          QTransform m;
          r.setRect(0, 0, w, h);
          m.translate(x + w, h); m.scale(-1,-1);
          painter->setTransform(m, true);
        }

        frame_spec fspec = getFrameSpec("TabBarFrame");
        const interior_spec ispec = getInteriorSpec("TabBarFrame");
        fspec.expansion = 0;

        QString status = getState(option,widget);
        if (!(option->state & State_Enabled))
        {
          status.replace("disabled","normal");
          painter->save();
          painter->setOpacity(DISABLED_OPACITY);
        }
        // TabBarFrame seems to have a redundant focus state
        else if (!status.startsWith("normal"))
        {
          if (status.endsWith("-inactive")) status = "normal-inactive";
          else status = "normal";
        }
        renderInterior(painter,r,fspec,ispec,ispec.element+"-"+status);
        renderFrame(painter,r,fspec,fspec.element+"-"+status, d,l,0,0,1);
        if (!(option->state & State_Enabled))
          painter->restore();
        if (verticalTabs || bottomTabs)
          painter->restore();
      }

      break;
    }

    case PE_PanelButtonCommand : {
      const QString group = "PanelButtonCommand";

      frame_spec fspec = getFrameSpec(group);
      const interior_spec ispec = getInteriorSpec(group);

      QString status;
      if (option->state & State_Enabled)
        status = getState(option,widget);
      else
      {
        status = "normal";
        if (option->state & State_On)
          status = "toggled";
        if (widget && !widget->isActiveWindow())
          status.append("-inactive");
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
      QString group = "PanelButtonTool";
      QWidget *p = getParent(widget,1);
      QWidget *gp = getParent(p,1);
      if (p && (isStylableToolbar(p) || isStylableToolbar(gp))
          && (!getFrameSpec("ToolbarButton").element.isEmpty()
              || !getInteriorSpec("ToolbarButton").element.isEmpty()))
      {
        group = "ToolbarButton";
      }

      frame_spec fspec = getFrameSpec(group);

      /* prevent drawing pushbuttons as toolbuttons (as in QupZilla or KNotes) */
      if (const QPushButton *pb = qobject_cast<const QPushButton*>(widget))
      {
        fspec.expansion = 0;
        if (pb->text().isEmpty())
        {
          painter->fillRect(option->rect, option->palette.brush(QPalette::Button));
          break;
        }
      }

      /* Due to a Qt5 bug (which I call "the hover bug"), after their menus are closed,
         comboboxes and buttons will have the WA_UnderMouse attribute without the cursor
         being over them. Hence we use the following logic in several places. It has no
         effect on Qt4 apps and will be harmless if the bug is fixed. */
      QString status = getState(option,widget);
      if (status.startsWith("focused")
          && widget && !widget->rect().contains(widget->mapFromGlobal(QCursor::pos())))
      {
        status.replace("focused","normal");
      }

      bool hasPanel = false;

      interior_spec ispec = getInteriorSpec(group);
      indicator_spec dspec = getIndicatorSpec(group);
      label_spec lspec = getLabelSpec(group);
      lspec.left = qMax(0,lspec.left-1);
      lspec.top = qMax(0,lspec.top-1);
      lspec.right = qMax(0,lspec.right-1);
      lspec.bottom = qMax(0,lspec.bottom-1);

      const QToolButton *tb = qobject_cast<const QToolButton*>(widget);
      const QStyleOptionToolButton *opt = qstyleoption_cast<const QStyleOptionToolButton*>(option);
      //const QStyleOptionTitleBar *titleBar = qstyleoption_cast<const QStyleOptionTitleBar*>(option);

      /* this is just for tabbar scroll buttons */
      if (qobject_cast<QTabBar*>(p))
      {
        painter->fillRect(option->rect, option->palette.brush(QPalette::Window));
        //fspec.expansion = 0;
      }
      /*else if ((titleBar && (titleBar->titleBarFlags & Qt::WindowType_Mask) == Qt::Tool)
               || qobject_cast<QDockWidget*>(getParent(widget,1)))
      {
        return;
      }*/
      else if (opt && opt->text.size() == 0 && opt->icon.isNull()
               && (!widget || !widget->inherits("QDockWidgetTitleButton")))
        fspec.expansion = 0; // color button

      // -> CE_MenuScroller and PE_PanelMenu
      if (qstyleoption_cast<const QStyleOptionMenuItem*>(option))
      {
        fspec.left = fspec.right = fspec.top = fspec.bottom = 0;
      }

      // -> CE_ToolButtonLabel
      if (qobject_cast<QAbstractItemView*>(gp))
      {
        fspec.left = qMin(fspec.left,3);
        fspec.right = qMin(fspec.right,3);
        fspec.top = qMin(fspec.top,3);
        fspec.bottom = qMin(fspec.bottom,3);
        //fspec.expansion = 0;

        //lspec.left = qMin(lspec.left,2);
        //lspec.right = qMin(lspec.right,2);
        lspec.top = qMin(lspec.top,2);
        lspec.bottom = qMin(lspec.bottom,2);
        lspec.tispace = qMin(lspec.tispace,2);
      }

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
      if (!(option->state & State_Enabled))
      {
        status = "normal";
        if (option->state & State_On)
          status = "toggled";
        if (widget && !widget->isActiveWindow())
          status.append("-inactive");
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      if (tb)
      {
        /* always show menu titles in the toggled state */
        if (!hspec_.transparent_menutitle
            && tb->isDown() && tb->toolButtonStyle() == Qt::ToolButtonTextBesideIcon
            && qobject_cast<QMenu*>(p))
        {
          status.replace("pressed","toggled");
        }

        bool rtl(option->direction == Qt::RightToLeft);

        // lack of space  (-> CE_ToolButtonLabel)
        if (opt && opt->toolButtonStyle == Qt::ToolButtonIconOnly && !opt->icon.isNull())
        {
          if (tb->popupMode() != QToolButton::MenuButtonPopup)
          {
            if ((tb->popupMode() == QToolButton::InstantPopup
                 || tb->popupMode() == QToolButton::DelayedPopup)
                && (opt->features & QStyleOptionToolButton::HasMenu))
            {
              if (tb->width() < opt->iconSize.width()+fspec.left+fspec.right
                                +dspec.size+ pixelMetric(PM_HeaderMargin)+lspec.tispace)
              {
                if (rtl)
                  fspec.right = qMin(fspec.right,3);
                else
                  fspec.left = qMin(fspec.left,3);
                //fspec.expansion = 0;
                dspec.size = qMin(dspec.size,TOOL_BUTTON_ARROW_SIZE-TOOL_BUTTON_ARROW_OVERLAP);
                lspec.tispace=0;
              }
            }
            else
            {
              if (tb->width() < opt->iconSize.width()+fspec.left+fspec.right)
              {
                fspec.left = qMin(fspec.left,3);
                fspec.right = qMin(fspec.right,3);
              }
              if (tb->height() < opt->iconSize.height()+fspec.top+fspec.bottom)
              {
                fspec.top = qMin(fspec.top,3);
                fspec.bottom = qMin(fspec.bottom,3);
                //fspec.expansion = 0;
              }
            }
          }
          else
          {
            const frame_spec fspec1 = getFrameSpec("DropDownButton");
            if (tb->width() < opt->iconSize.width()+fspec.left
                              +(rtl ? fspec1.left : fspec1.right)
                              +TOOL_BUTTON_ARROW_SIZE+2*TOOL_BUTTON_ARROW_MARGIN)
            {
              fspec.left = qMin(fspec.left,3);
              fspec.right = qMin(fspec.right,3);
              //fspec.expansion = 0;
            }
          }
        }

        /*bool withArrow = hasArrow (tb, opt);
        bool isHorizontal = true;*/
        if (tspec_.group_toolbar_buttons)
        {
          if (const QToolBar *toolBar = qobject_cast<const QToolBar*>(tb->parentWidget()))
          {
            /*if (toolBar->orientation() == Qt::Vertical)
              isHorizontal = false;*/
            if (toolBar->orientation() != Qt::Vertical)
            {
              /* the disabled state is ugly for grouped tool buttons */
              if (!(option->state & State_Enabled))
                painter->restore();
              drawRaised = true;
              ispec.px = ispec.py = 0;
              int kind = whichToolbarButton (tb, toolBar);
              if (kind != 2)
              {
                fspec.hasCapsule = true;
                fspec.capsuleV = 2;
                fspec.capsuleH = kind;
              }
            }

            /*if (!isHorizontal && !withArrow)
            {
              r.setRect(0, 0, h, w);
              painter->save();
              QTransform m;
              m.scale(1,-1);
              m.rotate(-90);
              painter->setTransform(m, true);
            }*/
          }
        }

        QString pbStatus = status;
        if (tb->popupMode() == QToolButton::MenuButtonPopup)
        {
          if (fspec.expansion <= 0) // otherwise the drop-down part will be integrated
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
                && ((opt && opt->activeSubControls == QStyle::SC_ToolButtonMenu)
                    || !widget->rect().contains(widget->mapFromGlobal(QCursor::pos())))) // hover bug
            {
              pbStatus = "normal";
            }
            if (pbStatus == "disabled")
              pbStatus = "normal";
            if (option->state & State_On) // it may be checkable
              pbStatus = "toggled";
            if (!widget->isActiveWindow())
              pbStatus.append("-inactive");
          }
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

        bool animate(widget->isEnabled() && animatedWidget_ == widget);
        if (!tb->autoRaise() || !pbStatus.startsWith("normal") || drawRaised)
        {
          if (animate)
          {
            if (animationStartState_ == pbStatus)
              animationOpacity_ = 100;
            else if (animationOpacity_ < 100
                     && (!tb->autoRaise() || !animationStartState_.startsWith("normal") || drawRaised))
            {
              renderFrame(painter,r,fspec,fspec.element+"-"+animationStartState_,0,0,0,0,0,drawRaised);
              renderInterior(painter,r,fspec,ispec,ispec.element+"-"+animationStartState_,drawRaised);
            }
            painter->save();
            painter->setOpacity((qreal)animationOpacity_/100);
          }
          renderFrame(painter,r,fspec,fspec.element+"-"+pbStatus,0,0,0,0,0,drawRaised);
          renderInterior(painter,r,fspec,ispec,ispec.element+"-"+pbStatus,drawRaised);
          if (animate)
          {
            painter->restore();
            if (animationOpacity_ >= 100)
              animationStartState_ = pbStatus;
          }
          hasPanel = true;
        }
        // fade out animation
        else if (animate)
        {
          if (animationStartState_ == pbStatus)
            animationOpacity_ = 100;
          else if (animationOpacity_ < 100)
          {
            painter->save();
            painter->setOpacity(1.0 - (qreal)animationOpacity_/100);
            renderFrame(painter,r,fspec,fspec.element+"-"+animationStartState_);
            renderInterior(painter,r,fspec,ispec,ispec.element+"-"+animationStartState_);
            painter->restore();
          }
          if (animationOpacity_ >= 100)
            animationStartState_ = pbStatus;
        }

        /*if (!isHorizontal && !withArrow)
          painter->restore();*/
      }
      else if (!(option->state & State_AutoRaise) || !status.startsWith("normal"))
      {
        bool libreoffice = false;
        if (isLibreoffice_ && (option->state & State_Enabled) && !status.startsWith("toggled")
            && enoughContrast(getFromRGBA(lspec.normalColor), QApplication::palette().color(QPalette::ButtonText)))
        {
          libreoffice = true;
          painter->save();
          painter->setOpacity(0.5);
        }
        renderFrame(painter,r,fspec,fspec.element+"-"+status);
        renderInterior(painter,r,fspec,ispec,ispec.element+"-"+status);
        if (libreoffice) painter->restore();
        hasPanel = true;
      }

      if (!(option->state & State_Enabled) && !drawRaised)
        painter->restore();

      if (widget && hasPanel && !paneledButtons.contains(widget))
      {
        paneledButtons.insert(widget);
        connect(widget, SIGNAL(destroyed(QObject*)), SLOT(removeFromSet(QObject*)), Qt::UniqueConnection);
      }

      /* force text color if the button isn't drawn in a standard way */
      if (widget && !standardButton.contains(widget)
          && (option->state & State_Enabled))
      {
        QColor col;
        if (hasPanel)
        {
          col = getFromRGBA(lspec.normalColor);
          if (status.startsWith("pressed"))
            col = getFromRGBA(lspec.pressColor);
          else if (status.startsWith("toggled"))
            col = getFromRGBA(lspec.toggleColor);
          else if (option->state & State_MouseOver)
            col = getFromRGBA(lspec.focusColor);
        }
        else
          /* FIXME: in fact, the foreground color of the parent widget should be
             used here (-> CE_ToolButtonLabel) but I've encountered no problem yet */
          col = QApplication::palette().color(QPalette::WindowText);
        forceButtonTextColor(widget,col);
      }

      break;
    }

    /* the frame is always drawn at PE_PanelButtonTool */
    case PE_FrameButtonTool : {return;}

    case PE_IndicatorRadioButton : {
      const interior_spec ispec = getInteriorSpec("RadioButton");

      if (option->state & State_Enabled)
      {
        QString suffix, prefix;
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
        if (qstyleoption_cast<const QStyleOptionMenuItem*>(option)
            && themeRndr_ && themeRndr_->isValid()
            && themeRndr_->elementExists("menu-"+ispec.element+suffix))
          prefix = "menu-"; // make exception for menuitems
        if (widget && !widget->isActiveWindow())
          suffix.append("-inactive");
        if (isLibreoffice_ && suffix == "-checked-focused"
            && qstyleoption_cast<const QStyleOptionMenuItem*>(option))
          painter->fillRect(option->rect, option->palette.brush(QPalette::Window));
        bool animate(widget && animatedWidget_ == widget
                     && !qstyleoption_cast<const QStyleOptionMenuItem*>(option)
                     && !qobject_cast<const QAbstractScrollArea*>(widget));
        if (animate)
        {
          if (animationStartState_ == suffix)
            animationOpacity_ = 100;
          else if (animationOpacity_ < 100)
            renderElement(painter, ispec.element+animationStartState_, option->rect);
          painter->save();
          painter->setOpacity((qreal)animationOpacity_/100);
        }
        renderElement(painter, prefix+ispec.element+suffix, option->rect);
        if (animate)
        {
          painter->restore();
          if (animationOpacity_ >= 100)
            animationStartState_ = suffix;
        }
      }
      else
      {
        if (!(option->state & State_Enabled))
        {
          painter->save();
          painter->setOpacity(DISABLED_OPACITY);
        }
        QString suffix, prefix;
        if (option->state & State_On)
          suffix = "-checked-normal";
        else
          suffix = "-normal";
        if (qstyleoption_cast<const QStyleOptionMenuItem*>(option)
            && themeRndr_ && themeRndr_->isValid()
            && themeRndr_->elementExists("menu-"+ispec.element+suffix))
          prefix = "menu-";
        if (widget && !widget->isActiveWindow())
          suffix.append("-inactive");
        renderElement(painter, prefix+ispec.element+suffix, option->rect);
        if (!(option->state & State_Enabled))
          painter->restore();
      }

      break;
    }

    case PE_IndicatorCheckBox : {
      const interior_spec ispec = getInteriorSpec("CheckBox");

      if (option->state & State_Enabled)
      {
        QString suffix, prefix;
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
        if (qstyleoption_cast<const QStyleOptionMenuItem*>(option)
            && themeRndr_ && themeRndr_->isValid()
            && themeRndr_->elementExists("menu-"+ispec.element+suffix))
          prefix = "menu-"; // make exception for menuitems
        if (widget && !widget->isActiveWindow())
          suffix.append("-inactive");
        if (isLibreoffice_ && suffix == "-checked-focused"
            && qstyleoption_cast<const QStyleOptionMenuItem*>(option))
          painter->fillRect(option->rect, option->palette.brush(QPalette::Window));
        bool animate(widget && animatedWidget_ == widget
                     && !qstyleoption_cast<const QStyleOptionMenuItem*>(option)
                     && !qobject_cast<const QAbstractScrollArea*>(widget));
        if (animate)
        {
          if (animationStartState_ == suffix)
            animationOpacity_ = 100;
          else if (animationOpacity_ < 100)
            renderElement(painter, ispec.element+animationStartState_, option->rect);
          painter->save();
          painter->setOpacity((qreal)animationOpacity_/100);
        }
        renderElement(painter, prefix+ispec.element+suffix, option->rect);
        if (animate)
        {
          painter->restore();
          if (animationOpacity_ >= 100)
            animationStartState_ = suffix;
        }
      }
      else
      {
        if (!(option->state & State_Enabled))
        {
          painter->save();
          painter->setOpacity(DISABLED_OPACITY);
        }
        QString suffix, prefix;
        if (option->state & State_On)
          suffix = "-checked-normal";
        else if (option->state & State_NoChange)
          suffix = "-tristate-normal";
        else
          suffix = "-normal";
        if (qstyleoption_cast<const QStyleOptionMenuItem*>(option)
            && themeRndr_ && themeRndr_->isValid()
            && themeRndr_->elementExists("menu-"+ispec.element+suffix))
          prefix = "menu-";
        if (widget && !widget->isActiveWindow())
          suffix.append("-inactive");
        renderElement(painter, prefix+ispec.element+suffix, option->rect);
        if (!(option->state & State_Enabled))
          painter->restore();
      }

      break;
    }

    case PE_FrameFocusRect : {
      if (qstyleoption_cast<const QStyleOptionFocusRect*>(option)
          /* this would be ugly, IMO */
          /*&& !qobject_cast<const QAbstractItemView*>(widget)*/)
      {
        frame_spec fspec = getFrameSpec("Focus");
        fspec.expansion = 0;
        fspec.left = qMin(fspec.left,2);
        fspec.right = qMin(fspec.right,2);
        fspec.top = qMin(fspec.top,2);
        fspec.bottom = qMin(fspec.bottom,2);
        renderFrame(painter,option->rect,fspec,fspec.element);
      }

      break;
    }

    case PE_IndicatorBranch : {
      const indicator_spec dspec = getIndicatorSpec("TreeExpander");
      QRect r = option->rect;
      bool rtl(option->direction == Qt::RightToLeft);
      int expanderAdjust = 0;

      if (option->state & State_Children)
      {
        frame_spec fspec;
        default_frame_spec(fspec);


        QString status = getState(option,widget);
        QString eStatus = "normal";
        if (!(option->state & State_Enabled))
          eStatus = "disabled";
        else if (option->state & State_MouseOver)
          eStatus = "focused";
        else if (status.startsWith("toggled") || status.startsWith("pressed"))
          eStatus = "pressed";
        if (widget && !widget->isActiveWindow())
          eStatus.append("-inactive");
        if (option->state & State_Open)
          renderIndicator(painter,r,fspec,dspec,dspec.element+"-minus-"+eStatus,option->direction);
        else
        {
          if (rtl)
          { // flip the indicator horizontally because it may be an arrow
            painter->save();
            QTransform m;
            r.setRect(x, y, w, h);
            m.translate(2*x + w, 0); m.scale(-1,1);
            painter->setTransform(m, true);
          }
          renderIndicator(painter,r,fspec,dspec,dspec.element+"-plus-"+eStatus,option->direction);
          if (rtl)
            painter->restore();
        }

        if (tspec_.tree_branch_line)
        {
          int sizeLimit = qMin(qMin(r.width(), r.height()), dspec.size);
          if(!( sizeLimit&1)) --sizeLimit; // make it odd
          expanderAdjust = sizeLimit/2 + 1;
        }
      }

      if (tspec_.tree_branch_line) // taken from Oxygen
      {
        const QPoint center(r.center());
        const int centerX = center.x();
        const int centerY = center.y();

        QColor col;
        if (qGray(option->palette.color(QPalette::Base).rgb()) <= 100)
          col = option->palette.color(QPalette::Light);
        else
          col = option->palette.color(QPalette::Dark);
        if (!col.isValid()) break;
        painter->save();
        painter->setPen(col);
        if (option->state & (State_Item | State_Children | State_Sibling))
        {
          const QLine line(QPoint(centerX, r.top()), QPoint(centerX, centerY - expanderAdjust));
          painter->drawLine( line );
        }
        // the right/left (depending on dir) line gets drawn if we have an item
        if (option->state & State_Item)
        {
          const QLine line = rtl ?
                QLine(QPoint(r.left(), centerY), QPoint(centerX - expanderAdjust, centerY)) :
                QLine(QPoint(centerX + expanderAdjust, centerY), QPoint(r.right(), centerY));
          painter->drawLine(line);
        }
        // the bottom if we have a sibling
        if (option->state & State_Sibling)
        {
          const QLine line(QPoint(centerX, centerY + expanderAdjust), QPoint(centerX, r.bottom()));
          painter->drawLine(line);
        }
        painter->restore();
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
      if ((widget // it's NULL in the case of QML menus
           && !qobject_cast<const QMenu*>(widget))
          || isLibreoffice_) // really not needed
        break;

      const QString group = "Menu";
      frame_spec fspec = getFrameSpec(group);
      fspec.expansion = 0;
      const interior_spec ispec = getInteriorSpec(group);

      fspec.left = fspec.right = pixelMetric(PM_MenuHMargin,option,widget);
      fspec.top = fspec.bottom = pixelMetric(PM_MenuVMargin,option,widget);

      theme_spec tspec_now = settings_->getCompositeSpec();
      if (!noComposite_ && tspec_now.menu_shadow_depth > 0
          && fspec.left >= tspec_now.menu_shadow_depth // otherwise shadow will have no meaning
          && widget && translucentWidgets_.contains(widget))
      {
        renderFrame(painter,option->rect,fspec,fspec.element+"-shadow");
      }
      else
      {
        if (!widget) // QML
          painter->fillRect(option->rect, QApplication::palette().color(QPalette::Window));
        renderFrame(painter,option->rect,fspec,fspec.element+"-normal");
      }

      renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-normal");

      break;
    }

    case PE_FrameWindow : {
      QColor col = QApplication::palette().color(QPalette::Window);
      if (!col.isValid()) break;
      QRect r = option->rect;

      painter->save();

      // left
      painter->setPen(QPen(col.lighter(130), 0));
      painter->drawLine(QPoint(r.left()+1, r.top()+1),
                        QPoint(r.left()+1, r.bottom()-1));
      painter->setPen(QPen(col.darker(120), 0));
      painter->drawLine(QPoint(r.left(), r.top()),
                        QPoint(r.left(), r.bottom()));
      // bottom
      painter->setPen(QPen(col.darker(120), 0));
      painter->drawLine(QPoint(r.left()+1, r.bottom()-1),
                        QPoint(r.right()-1, r.bottom()-1));
      painter->setPen(QPen(col.darker(140), 0));
      painter->drawLine(QPoint(r.left(), r.bottom()),
                        QPoint(r.right(), r.bottom()));
      // right
      painter->setPen(QPen(col.darker(110), 0));
      painter->drawLine(QPoint(r.right()-1, r.top()+1),
                        QPoint(r.right()-1, r.bottom()-1));
      painter->setPen(QPen(col.darker(120), 0));
      painter->drawLine(QPoint(r.right(), r.top()),
                        QPoint(r.right(), r.bottom()));

      painter->restore();

      break;
    }

    //case PE_FrameButtonBevel :
    case PE_Frame : {
      const QAbstractScrollArea *sa = qobject_cast<const QAbstractScrollArea*>(widget);
      if (!widget // it's NULL with QML
          || sa
          || widget->inherits("QWellArray") // color dialog's color rects
          || widget->inherits("QComboBoxPrivateContainer")) // frame for combo menus
      {
        if (widget)
        {
          if (isDolphin_)
          {
            if (QWidget *pw = widget->parentWidget())
            {
              if (hspec_.transparent_dolphin_view
                  // not renaming area
                  && !qobject_cast<QAbstractScrollArea*>(pw)
                  // only Dolphin's view
                  && QString(pw->metaObject()->className()).startsWith("Dolphin"))
              {
                break;
              }
            }
          }
          else if (isPcmanfm_ && (hspec_.transparent_pcmanfm_view || hspec_.transparent_pcmanfm_sidepane))
          {
            if (QWidget *pw = widget->parentWidget())
            {
              if ((hspec_.transparent_pcmanfm_view && pw->inherits("Fm::FolderView"))
                  || (hspec_.transparent_pcmanfm_sidepane && pw->inherits("Fm::SidePane")))
              {
                break;
              }
            }
          }
        }

        frame_spec fspec = getFrameSpec("GenericFrame");
        fspec.expansion = 0;

        if (!(option->state & State_Enabled))
        {
          painter->save();
          painter->setOpacity(DISABLED_OPACITY);
        }
        QString fStatus = "normal";
        // Can it be animated? (which means a flat background too -> polish(QWidget *widget))
        bool hasFlatBg(sa && themeRndr_ && themeRndr_->isValid()
                       && (sa->backgroundRole() == QPalette::Window
                           || sa->backgroundRole() == QPalette::Button)
                       && (!sa->viewport()
                           || (sa->viewport()->backgroundRole() != QPalette::Window
                               && sa->viewport()->backgroundRole() != QPalette::Button)));
        if (widget && widget->hasFocus() && hasFlatBg
            && !widget->inherits("QWellArray")) // color rects always have focus!
        {
          fStatus = "focused";
        }
        if (widget && !widget->isActiveWindow())
          fStatus = "normal-inactive"; // the focus state is meaningless here
        if (!widget) // QML again!
          painter->fillRect(option->rect, QApplication::palette().color(QPalette::Base));
        bool animate(widget && widget->isEnabled()
                     && ((animatedWidget_ == widget && !fStatus.startsWith("normal"))
                         || (animatedWidgetOut_ == widget && fStatus.startsWith("normal"))));
        QString animationStartState(animationStartState_);
        int animationOpacity = animationOpacity_;
        if (animate)
        {
          if (fStatus.startsWith("normal")) // -> QEvent::FocusOut
          {
            animationStartState = animationStartStateOut_;
            animationOpacity = animationOpacityOut_;
          }
          if (animationStartState == fStatus)
          {
            animationOpacity = 100;
            if (fStatus.startsWith("normal"))
              animationOpacityOut_ = 100;
            else
              animationOpacity_ = 100;
          }
          else if (animationOpacity < 100)
            renderFrame(painter,option->rect,fspec,fspec.element+"-"+animationStartState);
          painter->save();
          painter->setOpacity((qreal)animationOpacity/100);
        }
        renderFrame(painter,option->rect,fspec,fspec.element+"-"+fStatus);
        if (animate)
        {
          painter->restore();
          if (animationOpacity >= 100)
          {
            if (fStatus.startsWith("normal"))
              animationStartStateOut_ = fStatus;
            else
              animationStartState_ = fStatus;
          }
        }
        if (!(option->state & State_Enabled))
          painter->restore();
      }

      break;
    }

    /* draw corner area only if scrollbars are drawn inside frame
       and viewport doesn't have a customized background */
    case PE_PanelScrollAreaCorner : {
      if (!tspec_.scrollbar_in_view)
        return;
      QColor col;
      if (const QAbstractScrollArea *sa = qobject_cast<const QAbstractScrollArea*>(widget))
      {
        if (QWidget *vp = sa->viewport())
        {
          if (!vp->autoFillBackground()
              || (!vp->styleSheet().isEmpty() && vp->styleSheet().contains("background")))
          {
            return;
          }
          col = vp->palette().color(vp->backgroundRole());
        }
      }
      if (!col.isValid())
        col = option->palette.color(QPalette::Window);
      painter->fillRect(option->rect, col);
      break;
    }

    case PE_FrameGroupBox : {
      const QString group = "GroupBox";
      frame_spec fspec = getFrameSpec(group);
      const interior_spec ispec = getInteriorSpec(group);
      if (!tspec_.groupbox_top_label
          || !widget) // WARNING: QML has anchoring!
        fspec.expansion = 0;

      if (!(option->state & State_Enabled))
      {
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      QString suffix = "-normal";
      if (widget && !widget->isActiveWindow())
        suffix = "-normal-inactive";
      renderFrame(painter,option->rect,fspec,fspec.element+suffix);
      if (tspec_.groupbox_top_label
          && widget) // QML anchoring
        renderInterior(painter,option->rect,fspec,ispec,ispec.element+suffix);
      if (!(option->state & State_Enabled))
        painter->restore();

      break;
    }

    case PE_FrameTabWidget : {
      const QString group = "TabFrame";
      frame_spec fspec = getFrameSpec(group);
      const interior_spec ispec = getInteriorSpec(group);
      fspec.expansion = 0;

      frame_spec fspec1 = fspec;
      int d = 0;
      int l = 0;
      int tp = 0;

      if (tspec_.attach_active_tab)
      {
        /* WARNING: We use "floating tabs" when QTabWidget is NULL because
           QStyleOptionTabWidgetFrame::selectedTabRect.x() is wrong for QML.
           For the sake of consistency, we don't draw tab widget frame either
           in such cases. Other styles are too simple to have this problem. */
        const QTabWidget *tw = qobject_cast<const QTabWidget*>(widget);
        if (!tw) return;
        else
        {
          QRect tr;
          tp = tw->tabPosition();
          if (const QStyleOptionTabWidgetFrame_v2 *twf =
              qstyleoption_cast<const QStyleOptionTabWidgetFrame_v2*>(option))
          {
            if (!twf->tabBarSize.isEmpty()) // it's empty in Kdenlive
              tr = twf->selectedTabRect;
          }
          // as in GoldenDict's Preferences dialog
#if QT_VERSION < 0x050000
          else if (QTabBar *tb = widget->findChild<QTabBar*>(QLatin1String("qt_tabwidget_tabbar")))
#else
          else if (QTabBar *tb = tw->tabBar())
#endif
          {
            int index = tw->currentIndex();
            if (index >= 0)
            {
              tr = tb->tabRect(index);
              if (tr.isValid())
              {
                if (tp == QTabWidget::North || tp == QTabWidget::South)
                  tr.translate(tb->x(),0);
                else
                  tr.translate(0,tb->y());
              }
            }
          }
          if (tr.isValid())
          {
            switch (tp) {
              case QTabWidget::North: {
                fspec1.top = 0;
                d = tr.x();
                l = tr.width();
                break;
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
                break;
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
      }

      if (!(option->state & State_Enabled))
      {
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      QString suffix = "-normal";
      if (widget && !widget->isActiveWindow())
        suffix = "-normal-inactive";
      if (widget) // WARNING: QML has anchoring!
        renderInterior(painter,option->rect,fspec1,ispec,ispec.element+suffix);
      const frame_spec fspecT = getFrameSpec("Tab");
      renderFrame(painter,
                  option->rect,
                  fspec,fspec.element+suffix,
                  d, l, fspecT.left, fspecT.right,tp);
      if (!(option->state & State_Enabled))
        painter->restore();

      break;
    }

    /* frame is forced on lineedits at PE_PanelLineEdit */
    case PE_FrameLineEdit : {return;}

    case PE_PanelLineEdit : {
      /* don't draw the interior or frame of a Plasma spinbox */
      if (isPlasma_ && widget && widget->window()->testAttribute(Qt::WA_NoSystemBackground))
        break;

      QWidget *p = getParent(widget,1);

      /* We draw lineedits of editable comboboxes only in drawComboLineEdit().
         It seems that some style plugins draw it twice. */
      if (qobject_cast<const QLineEdit*>(widget) && qobject_cast<QComboBox*>(p))
        break;

      const QString group = "LineEdit";
      interior_spec ispec = getInteriorSpec(group);
      frame_spec fspec = getFrameSpec(group);
      label_spec lspec = getLabelSpec(group);
      lspec.top = qMax(0,lspec.top-1);
      lspec.bottom = qMax(0,lspec.bottom-1);
      const size_spec sspec = getSizeSpec(group);

      if (!widget) // WARNING: QML has anchoring!
      {
        fspec.expansion = 0;
        ispec.px = ispec.py = 0;
      }

      if (isLibreoffice_)
      {
        fspec.left = fspec.right = fspec.top = fspec.bottom = qMin(fspec.left,3);
      }
      else if (qobject_cast<const QLineEdit*>(widget))
      {
        if (!widget->styleSheet().isEmpty() && widget->styleSheet().contains("padding"))
        {
          fspec.left = fspec.right = fspec.top = fspec.bottom = qMin(fspec.left,3);
        }
        else
        {
          if (widget->minimumWidth() == widget->maximumWidth())
          {
            fspec.left = qMin(fspec.left,3);
            fspec.right = qMin(fspec.right,3);
          }
          if (widget->height() < sizeCalculated(widget->font(),fspec,lspec,sspec,"W",QSize()).height())
          {
            fspec.top = qMin(fspec.top,3);
            fspec.bottom = qMin(fspec.bottom,3);
          }
        }
      }
      /* no frame when editing itemview texts */
      if (qobject_cast<QAbstractItemView*>(getParent(p,1)))
      {
        fspec.left = fspec.right = fspec.top = fspec.bottom = fspec.expansion = 0;
      }
      QAbstractSpinBox *sb = qobject_cast<QAbstractSpinBox*>(p);
      const QStyleOptionSpinBox *sbOpt = qstyleoption_cast<const QStyleOptionSpinBox*>(option);
      if (sb || sbOpt
          || (p && (p->inherits("KisAbstractSliderSpinBox") || p->inherits("Digikam::DAbstractSliderSpinBox")))
          || (isLibreoffice_ && sbOpt))
      {
        if (!sb || sb->buttonSymbols() != QAbstractSpinBox::NoButtons)
        {
          fspec.hasCapsule = true;
          fspec.capsuleH = -1;
          fspec.capsuleV = 2;
        }

        // the measure we used for CC_SpinBox at drawComplexControl()
        if (tspec_.vertical_spin_indicators || (!widget && sbOpt && sbOpt->frame))
        {
          fspec.left = fspec.right = fspec.top = fspec.bottom = qMin(fspec.left,3);
          fspec.expansion = 0;
        }
        else if (sb)
        {
          QString maxTxt = spinMaxText(sb);
          if (maxTxt.isEmpty()
              || option->rect.width() < textSize(sb->font(),maxTxt).width() + fspec.left
                                        + (sb->buttonSymbols() == QAbstractSpinBox::NoButtons ? fspec.right : 0)
              || (sb->buttonSymbols() != QAbstractSpinBox::NoButtons
                  && sb->width() < widget->width() + 2*tspec_.spin_button_width + getFrameSpec("IndicatorSpinBox").right))
          {
            fspec.left = qMin(fspec.left,3);
            fspec.right = qMin(fspec.right,3);
          }
          if (sb->height() < fspec.top+fspec.bottom+QFontMetrics(widget->font()).height())
          {
            fspec.top = qMin(fspec.top,3);
            fspec.bottom = qMin(fspec.bottom,3);
            //fspec.expansion = 0;
          }
        }
      }
      else if (qobject_cast<QComboBox*>(p)) // FIXME: without being a QLineEdit?!
      {
        fspec.hasCapsule = true;
        /* see if there is any icon on the left of the combo box (for LTR) */
        if (option->direction == Qt::RightToLeft)
        {
          if (widget->width() < p->width() - COMBO_ARROW_LENGTH
                                - (tspec_.combo_as_lineedit ? fspec.left : getFrameSpec("ComboBox").left))
            fspec.capsuleH = 0;
          else fspec.capsuleH = 1;
        }
        else
        {
          if (widget->x() > 0) fspec.capsuleH = 0;
          else fspec.capsuleH = -1;
        }
        fspec.capsuleV = 2;
      }

      // lineedits only have normal and focused states in Kvantum
      QString leStatus = (option->state & State_HasFocus) ? "focused" : "normal";
      if (widget && !widget->isActiveWindow())
        leStatus.append("-inactive");
      if (!(option->state & State_Enabled))
      {
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      bool animateSpin(qobject_cast<QAbstractSpinBox*>(p)
                       && ((animatedWidget_ == p && !leStatus.startsWith("normal"))
                           || (animatedWidgetOut_ == p && leStatus.startsWith("normal"))));
      bool animate(!isLibreoffice_ && widget && widget->isEnabled()
                   && !qobject_cast<const QAbstractScrollArea*>(widget)
                   && ((animatedWidget_ == widget && !leStatus.startsWith("normal"))
                       || (animatedWidgetOut_ == widget && leStatus.startsWith("normal"))
                       || animateSpin));
      QString animationStartState(animationStartState_);
      int animationOpacity = animationOpacity_;
      if (animate)
      {
        if (leStatus.startsWith("normal")) // -> QEvent::FocusOut
        {
          animationStartState = animationStartStateOut_;
          animationOpacity = animationOpacityOut_;
        }
        if (animationStartState == leStatus)
        {
          animationOpacity = 100;
          if (leStatus.startsWith("normal"))
            animationOpacityOut_ = 100;
          else
            animationOpacity_ = 100;
        }
        else if (animationOpacity < 100)
        {
          renderFrame(painter,option->rect,fspec,fspec.element+"-"+animationStartState);
          renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+animationStartState);
        }
        painter->save();
        painter->setOpacity((qreal)animationOpacity/100);
      }
      /* force frame */
      renderFrame(painter,
                  isLibreoffice_ && !sbOpt ?
                    option->rect.adjusted(fspec.left,fspec.top,-fspec.right,-fspec.bottom) :
                    option->rect,
                  fspec,
                  fspec.element+"-"+leStatus);
      renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+leStatus);
      if (animate)
      {
        painter->restore();
        if (animationOpacity >= 100)
        {
          if (leStatus.startsWith("normal"))
            animationStartStateOut_ = leStatus;
          else
            animationStartState_ = leStatus;
        }
      }
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

      QRect r = option->rect;
      if (!(option->state & State_Horizontal))
      {
        r.setRect(y, x, h, w);
        painter->save();
        QTransform m;
        m.scale(1,-1);
        m.rotate(-90);
        painter->setTransform(m, true);
      }
      if (element == PE_IndicatorToolBarHandle && tspec_.center_toolbar_handle)
        renderIndicator(painter,r,fspec,dspec,dspec.element+"-handle",option->direction);
      else
        renderInterior(painter,r,fspec,ispec,
                       dspec.element
                         +(element == PE_IndicatorToolBarHandle ? "-handle" : "-separator"));

      if (!(option->state & State_Horizontal))
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
      const QStyleOptionSpinBox *opt = qstyleoption_cast<const QStyleOptionSpinBox*>(option);
      // the measure we used in CC_SpinBox at drawComplexControl() (for QML)
      bool verticalIndicators(tspec_.vertical_spin_indicators || (!widget && opt && opt->frame));

      frame_spec fspec;
      if (!verticalIndicators)
      {
        fspec = getFrameSpec(group);
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
      }
      else
      {
        fspec = getFrameSpec("LineEdit");
        fspec.left = fspec.right = fspec.top = fspec.bottom = qMin(fspec.left,3);
        fspec.expansion = 0;
      }

      if (isLibreoffice_)
      {
        fspec.left = fspec.right = fspec.top = fspec.bottom = qMin(fspec.left,3);
        fspec.expansion = 0;
      }
      // -> CC_SpinBox
      else if (opt && !verticalIndicators)
      {
        if (up)
        {
          int m = opt->rect.width() - tspec_.spin_button_width;
          if (fspec.right > m)
          {
            m = qMax(m,2);
            fspec.right = qMin(fspec.right,m);
            fspec.expansion = 0;
          }
        }
        else if (w < tspec_.spin_button_width) fspec.expansion = 0;
        if (const QAbstractSpinBox *sb = qobject_cast<const QAbstractSpinBox*>(widget))
        {
          if (spinMaxText(sb).isEmpty())
          {
            fspec.right = qMin(fspec.right,3);
            //fspec.expansion = 0;
          }
        }
        if (opt->rect.height() < fspec.top + fspec.bottom)
        {
          fspec.top = fspec.bottom = qMin(fspec.top,3);
          //fspec.expansion = 0;
        }
      }

      QString iStatus = getState(option,widget);; // indicator state
      QString bStatus = iStatus; // button state
      if (option->state & State_Enabled)
      {
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

        if (widget && !widget->isActiveWindow())
        {
          if (!iStatus.endsWith("-inactive"))
            iStatus.append("-inactive");
          if (!bStatus.endsWith("-inactive"))
            bStatus.append("-inactive");
        }
      }

      /* a workaround for LibreOffice;
         also see subControlRect() -> CC_SpinBox */
      if (isLibreoffice_)
      {
        bStatus = iStatus = "normal";
        /*if (up) iString = "-plus-";
        else iString = "-minus-";*/
      }

      QString iString; // indicator string
      if (element == PE_IndicatorSpinPlus) iString = "-plus-";
      else if (element == PE_IndicatorSpinMinus) iString = "-minus-";
      else if (element == PE_IndicatorSpinUp) iString = "-up-";
      else  iString = "-down-";

      QRect r = option->rect;
      indicator_spec dspec = getIndicatorSpec(group);

      if (!verticalIndicators && !tspec_.inline_spin_indicators)
      {
        if (bStatus.startsWith("disabled"))
        {
          bStatus.replace("disabled","normal");
          painter->save();
          painter->setOpacity(DISABLED_OPACITY);
        }
        interior_spec ispec = getInteriorSpec(group);
        ispec.px = ispec.py = 0;
        renderFrame(painter,r,fspec,fspec.element+"-"+bStatus,0,0,0,0,0,true);
        renderInterior(painter,r,fspec,ispec,ispec.element+"-"+bStatus,true);
        if (element == PE_IndicatorSpinDown || element == PE_IndicatorSpinMinus)
        { // draw spinbox separator if it exists
          QString sepName = dspec.element + "-separator";
          QRect sep;
          sep.setRect(x, y+fspec.top, fspec.left, h-fspec.top-fspec.bottom);
          if (renderElement(painter, sepName+"-"+bStatus, sep))
          {
            sep.adjust(0, -fspec.top, 0, -h+fspec.top+fspec.bottom);
            renderElement(painter, sepName+"-top-"+bStatus, sep);
            sep.adjust(0, h-fspec.bottom, 0, h-fspec.top);
            renderElement(painter, sepName+"-bottom-"+bStatus, sep);
          }
        }
        if (!(option->state & State_Enabled))
          painter->restore();
      }

      Qt::Alignment align;
      // horizontally center both indicators
      if (!verticalIndicators)
        align = Qt::AlignCenter;
      else
      {
        fspec.left = 0;
        if (up) fspec.bottom = 0;
        else fspec.top = 0;
        align = Qt::AlignRight | Qt::AlignVCenter;
      }
      if ((verticalIndicators || tspec_.inline_spin_indicators)
          && themeRndr_ && themeRndr_->isValid())
      {
        QColor col = getFromRGBA(getLabelSpec(group).normalColor);
        if (!col.isValid())
          col = QApplication::palette().color(QPalette::ButtonText);
        if (enoughContrast(col, QApplication::palette().color(QPalette::Text))
            && themeRndr_->elementExists("flat-"+dspec.element+"-down-normal"))
          dspec.element = "flat-"+dspec.element;
      }
      renderIndicator(painter,
                      r,
                      fspec,dspec,
                      dspec.element+iString+iStatus,
                      option->direction,
                      align);

      break;
    }

    case PE_IndicatorHeaderArrow : {
      const QStyleOptionHeader *opt =
        qstyleoption_cast<const QStyleOptionHeader*>(option);
      if (opt)
      {
        const QString group = "HeaderSection";
        frame_spec fspec = getFrameSpec(group);
        const indicator_spec dspec = getIndicatorSpec(group);
        const label_spec lspec = getLabelSpec(group);

        /* this is compensated in CE_HeaderLabel;
           also see SE_HeaderArrow */
        if (option->direction == Qt::RightToLeft)
        {
          fspec.right = 0;
          if (opt->position == QStyleOptionHeader::Beginning || opt->position == QStyleOptionHeader::Middle)
            fspec.left = lspec.left;
          else
            fspec.left += lspec.left;
        }
        else
        {
          fspec.left = 0;
          if (opt->position == QStyleOptionHeader::Beginning || opt->position == QStyleOptionHeader::Middle)
            fspec.right = lspec.right;
          else
            fspec.right += lspec.right;
        }

        QString aStatus = "normal";
        if (!(option->state & State_Enabled))
          aStatus = "disabled";
        else if ((option->state & State_On) || (option->state & State_Sunken) || (option->state & State_Selected))
          aStatus = "pressed";
        else if (option->state & State_MouseOver)
          aStatus = "focused";
        if (widget && !widget->isActiveWindow())
          aStatus.append("-inactive");
        if (opt->sortIndicator == QStyleOptionHeader::SortDown)
          renderIndicator(painter,option->rect,fspec,dspec,dspec.element+"-down-"+aStatus,option->direction);
        else if (opt->sortIndicator == QStyleOptionHeader::SortUp)
          renderIndicator(painter,option->rect,fspec,dspec,dspec.element+"-up-"+aStatus,option->direction);
      }

      break;
    }

    case PE_IndicatorButtonDropDown : {
      QRect r = option->rect;
      QString group = "DropDownButton";

      const QToolButton *tb = qobject_cast<const QToolButton*>(widget);
      if (tb)
      {
        QWidget *p = getParent(widget,1);
        if (p && (isStylableToolbar(p) || isStylableToolbar(getParent(p,1)))
            && (!getFrameSpec("ToolbarButton").element.isEmpty()
                || !getInteriorSpec("ToolbarButton").element.isEmpty()))
        {
          group = "ToolbarButton";
        }
      }

      frame_spec fspec = getFrameSpec(group);
      fspec.expansion = 0; // depends on the containing widget
      interior_spec ispec = getInteriorSpec(group);
      indicator_spec dspec = getIndicatorSpec(group);
      if (group == "ToolbarButton")
        dspec.element += "-down";

      QString status = getState(option,widget);
      bool rtl(option->direction == Qt::RightToLeft);

      const QStyleOptionComboBox *combo =
            qstyleoption_cast<const QStyleOptionComboBox*>(option);
      const QComboBox *cb = qobject_cast<const QComboBox*>(widget);
      if (cb /*&& !cb->duplicatesEnabled()*/)
      {
        if (tspec_.combo_as_lineedit && combo && combo->editable && cb->lineEdit())
        {
          fspec = getFrameSpec("LineEdit");
          ispec = getInteriorSpec("LineEdit");
          const indicator_spec dspec1 = getIndicatorSpec("LineEdit");
          if (themeRndr_ && themeRndr_->isValid()
              && themeRndr_->elementExists(dspec1.element+"-normal"))
          {
            dspec = dspec1;
          }
        }
        else
        {
          fspec = getFrameSpec("ComboBox");
          ispec = getInteriorSpec("ComboBox");
          const indicator_spec dspec1 = getIndicatorSpec("ComboBox");
          if (themeRndr_ && themeRndr_->isValid()
              && themeRndr_->elementExists(dspec1.element+"-normal"))
          {
            dspec = dspec1;
          }
        }
        if (!(combo && combo->editable && cb->lineEdit()
              // someone may want transparent lineedits (as the developer of Cantata does)
              && cb->lineEdit()->palette().color(cb->lineEdit()->backgroundRole()).alpha() == 0))
        {
          fspec.hasCapsule = true;
          if (rtl)
            fspec.capsuleH = -1;
          else
            fspec.capsuleH = 1;
          fspec.capsuleV = 2;
        }

        status = (option->state & State_Enabled) ?
                  (option->state & State_On) ? "toggled" :
                  ((option->state & State_Sunken) || cb->hasFocus()) ? "pressed" :
                  (option->state & State_MouseOver) ? "focused" : "normal"
                : "disabled";
        if (!widget->isActiveWindow())
          status.append("-inactive");

        if ((combo && !combo->editable) || !cb->lineEdit())
        {
          /* in this case, the state definition isn't the usual one */
          status = (option->state & State_Enabled) ?
                    (option->state & State_On) ? "toggled" :
                    (option->state & State_MouseOver)
                      && widget->rect().contains(widget->mapFromGlobal(QCursor::pos())) // hover bug
                    ? "focused" :
                    (option->state & State_Sunken)
                    || (option->state & State_Selected) ? "pressed" : "normal"
                   : "disabled";
          if (!widget->isActiveWindow())
            status.append("-inactive");
          /* when there isn't enough space */
          QSize txtSize = textSize(painter->font(),combo->currentText);
          label_spec lspec1 = getLabelSpec("ComboBox");
          lspec1.left = qMax(0,lspec1.left-1);
          lspec1.right = qMax(0,lspec1.right-1);
          lspec1.top = qMax(0,lspec1.top-1);
          lspec1.bottom = qMax(0,lspec1.bottom-1);
          if (cb->width() < fspec.left+lspec1.left+txtSize.width()+lspec1.right+COMBO_ARROW_LENGTH+fspec.right
              || cb->height() < fspec.top+lspec1.top+txtSize.height()+fspec.bottom+lspec1.bottom)
          {
            if (rtl)
              r.adjust(0,0,-qMax(fspec.left-3,0),0);
            else
              r.adjust(qMax(fspec.right-3,0),0,0,0);
            fspec.left = qMin(fspec.left,3);
            fspec.right = qMin(fspec.right,3);
            fspec.top = qMin(fspec.top,3);
            fspec.bottom = qMin(fspec.bottom,3);
            if (rtl)
              r.adjust(0,0,-qMax(qMin(dspec.size,cb->height()-fspec.top-fspec.bottom)-9,0),0);
            else
              r.adjust(qMax(qMin(dspec.size,cb->height()-fspec.top-fspec.bottom)-9,0),0,0,0);
            dspec.size = qMin(dspec.size,9);
          }
        }
      }

      if (tb)
      {
        if (status.startsWith("focused")
            && !widget->rect().contains(widget->mapFromGlobal(QCursor::pos()))) // hover bug
        {
          status.replace("focused","normal");
        }
        const QToolBar *toolBar = qobject_cast<const QToolBar*>(tb->parentWidget());
        const frame_spec fspec1 = getFrameSpec("PanelButtonTool");
        fspec.top = fspec1.top; fspec.bottom = fspec1.bottom;
        bool drawRaised = false;
        if (tspec_.group_toolbar_buttons
            && toolBar && toolBar->orientation() != Qt::Vertical)
        {
          drawRaised = true;

          //const QStyleOptionToolButton *opt = qstyleoption_cast<const QStyleOptionToolButton*>(option);
          int kind = whichToolbarButton (tb, toolBar);
          if (kind != 2)
          {
            fspec.hasCapsule = true;
            fspec.capsuleV = 2;
            fspec.capsuleH = kind;
          }
        }

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

        /* lack of space */
        const QStyleOptionToolButton *opt = qstyleoption_cast<const QStyleOptionToolButton*>(option);
        if (opt && opt->toolButtonStyle == Qt::ToolButtonIconOnly && !opt->icon.isNull()
            && tb->popupMode() == QToolButton::MenuButtonPopup)
        {
          if (tb->width() < opt->iconSize.width()+fspec1.left
                            +(rtl ? fspec.left : fspec.right)
                            +TOOL_BUTTON_ARROW_SIZE+2*TOOL_BUTTON_ARROW_MARGIN)
          {
            if (rtl)
              fspec.left = qMin(fspec.left,3);
            else
            {
              fspec.right = qMin(fspec.right,3);
            }
            dspec.size = qMin(dspec.size,TOOL_BUTTON_ARROW_SIZE);
          }
        }

        if (fspec1.expansion <= 0) // otherwise drawn at PE_PanelButtonTool
        {
          if (!(option->state & State_Enabled))
          {
            status = "normal";
            if (option->state & State_On)
              status = "toggled";
            if (widget && !widget->isActiveWindow())
              status.append("-inactive");
            if (!drawRaised)
            {
              painter->save();
              painter->setOpacity(DISABLED_OPACITY);
            }
          }
          bool animate(widget->isEnabled() && animatedWidget_ == widget);
          if (!tb->autoRaise() || !status.startsWith("normal") || drawRaised)
          {
            if (animate)
            {
              if (animationStartState_ == status)
                animationOpacity_ = 100;
              else if (animationOpacity_ < 100
                       && (!tb->autoRaise() || !animationStartState_.startsWith("normal") || drawRaised))
              {
                renderFrame(painter,r,fspec,fspec.element+"-"+animationStartState_);
                renderInterior(painter,r,fspec,ispec,ispec.element+"-"+animationStartState_);
              }
              painter->save();
              painter->setOpacity((qreal)animationOpacity_/100);
            }
            renderInterior(painter,r,fspec,ispec,ispec.element+"-"+status);
            renderFrame(painter,r,fspec,fspec.element+"-"+status);
            if (animate)
            {
              painter->restore();
              if (animationOpacity_ >= 100)
                animationStartState_ = status;
            }
          }
          // fade out animation
          else if (animate)
          {
            if (animationStartState_ == status)
              animationOpacity_ = 100;
            else if (animationOpacity_ < 100)
            {
              painter->save();
              painter->setOpacity(1.0 - (qreal)animationOpacity_/100);
              renderFrame(painter,r,fspec,fspec.element+"-"+animationStartState_);
              renderInterior(painter,r,fspec,ispec,ispec.element+"-"+animationStartState_);
              painter->restore();
            }
            if (animationOpacity_ >= 100)
              animationStartState_ = status;
          }
          if (!(option->state & State_Enabled))
          {
            status = "disabled";
            if (widget && !widget->isActiveWindow())
              status.append("-inactive");
            if (!drawRaised)
              painter->restore();
          }
        }

        /* use the "flat" indicator with flat buttons if it exists */
        if (status.startsWith("normal") && tb->autoRaise()
            && themeRndr_ && themeRndr_->isValid())
        {
          QString group1 = "PanelButtonTool";
          if (group == "ToolbarButton")
            group1 = group;
          const indicator_spec dspec1 = getIndicatorSpec(group1);
          if (themeRndr_->elementExists("flat-"+dspec1.element+"-down-normal"))
          {
            QColor col = getFromRGBA(getLabelSpec(group1).normalColor);
            if (!col.isValid())
              col = QApplication::palette().color(QPalette::ButtonText);
            QWidget *p = tb->parentWidget();
            QWidget *gp = getParent(widget,2);
            QWidget* menubar = NULL;
            if (qobject_cast<QMenuBar*>(gp))
              menubar = gp;
            else if (qobject_cast<QMenuBar*>(p))
              menubar = p;
            if (menubar)
            {
              group1 = "MenuBar";
              if (mergedToolbarHeight(menubar))
                group1 = "Toolbar";
              if (enoughContrast(col, getFromRGBA(getLabelSpec(group1).normalColor)))
                dspec.element = "flat-"+dspec1.element+"-down";
            }
            else if ((qobject_cast<QMainWindow*>(gp) && isStylableToolbar(p)
                      && !p->findChild<QTabBar*>())
                     || (qobject_cast<QMainWindow*>(getParent(gp,1)) && isStylableToolbar(gp)
                         && !gp->findChild<QTabBar*>()))
            {
              if ((!tspec_.group_toolbar_buttons || (toolBar && toolBar->orientation() == Qt::Vertical))
                  && enoughContrast(col, getFromRGBA(getLabelSpec("Toolbar").normalColor)))
              {
                dspec.element = "flat-"+dspec1.element+"-down";
              }
            }
            else if (p && enoughContrast(col, p->palette().color(p->foregroundRole())))
              dspec.element = "flat-"+dspec1.element+"-down";
          }
        }
      }
      else if ((combo && combo->editable && !(cb && !cb->lineEdit())) // otherwise drawn at CC_ComboBox
               && (!(option->state & State_AutoRaise)
                   || (!status.startsWith("normal") && (option->state & State_Enabled))))
      {
        if (cb && tspec_.combo_as_lineedit)
        {
          if (cb->hasFocus())
          {
            if (!widget->isActiveWindow()) status = "focused-inactive";
            else status = "focused";
          }
          else if (status.startsWith("focused"))
            status.replace("focused","normal");
          else if (status.startsWith("toggled"))
            status.replace("toggled","normal");
        }
        if (!(option->state & State_Enabled))
        {
          status.replace("disabled","normal");
          painter->save();
          painter->setOpacity(DISABLED_OPACITY);
        }
        bool mouseAnimation(animatedWidget_ == widget
                            && (!status.startsWith("normal")
                                || ((!tspec_.combo_as_lineedit
                                     || (cb && cb->view() && cb->view()->isVisible()))
                                    && animationStartState_.startsWith("focused"))));
        bool animate(cb && cb->isEnabled()
                     && !qobject_cast<const QAbstractScrollArea*>(widget)
                     && (mouseAnimation
                         || (animatedWidgetOut_ == widget && status.startsWith("normal"))));
        QString animationStartState(animationStartState_);
        int animationOpacity = animationOpacity_;
        if (animate)
        {
          if (!mouseAnimation) // -> QEvent::FocusOut
          {
            animationStartState = animationStartStateOut_;
            animationOpacity = animationOpacityOut_;
          }
          if (animationStartState == status)
          {
            animationOpacity = 100;
            if (!mouseAnimation)
              animationOpacityOut_ = 100;
            else
              animationOpacity_ = 100;
          }
          else if (animationOpacity < 100)
          {
            renderInterior(painter,r,fspec,ispec,ispec.element+"-"+animationStartState);
            renderFrame(painter,r,fspec,fspec.element+"-"+animationStartState);
          }
          painter->save();
          painter->setOpacity((qreal)animationOpacity/100);
        }
        renderInterior(painter,r,fspec,ispec,ispec.element+"-"+status);
        renderFrame(painter,r,fspec,fspec.element+"-"+status);
        if (animate)
        {
          painter->restore();
          if (animationOpacity >= 100)
          {
            if (animatedWidget_ == widget)
              animationStartState_ = status;
            if (!mouseAnimation)
              animationStartStateOut_ = status;
          }
        }
        if (!(option->state & State_Enabled))
        {
          painter->restore();
          status = "disabled";
          if (widget && !widget->isActiveWindow())
            status.append("-inactive");
        }
      }

      /* distinguish between the toggled and pressed states
         only if a toggled down arrow element exists */
      if (status.startsWith("toggled")
          && !(themeRndr_ && themeRndr_->isValid()
               && themeRndr_->elementExists(dspec.element+"-toggled")))
      {
        status.replace("toggled","pressed");
      }
      /* Konqueror may have added an icon to the right of lineedit (for LTR),
         in which case, the arrow rectangle whould be widened at CC_ComboBox */
      if (combo && combo->editable && cb && cb->lineEdit())
      {
        int extra = r.width()-COMBO_ARROW_LENGTH-(rtl ? fspec.left : fspec.right);
        if (extra > 0)
        {
          if (rtl) r.adjust(0,0,-extra,0);
          else r.adjust(extra,0,0,0);
        }
      }
      renderIndicator(painter,
                      r,
                      fspec,dspec,dspec.element+"-"+status,
                      option->direction);

      break;
    }

    case PE_PanelMenuBar : {
      break;
    }

    case PE_IndicatorTabTear : {
      indicator_spec dspec = getIndicatorSpec("Tab");
      renderElement(painter,dspec.element+"-tear",option->rect);

      break;
    }

    case PE_IndicatorTabClose : {
      frame_spec fspec;
      default_frame_spec(fspec);
      const indicator_spec dspec = getIndicatorSpec("Tab");

      QString status = !(option->state & State_Enabled) ? "disabled" :
                         option->state & State_Sunken ? "pressed" :
                           option->state & State_MouseOver ? "focused" : "normal";
      if (widget && !widget->isActiveWindow())
        status.append("-inactive");
      renderIndicator(painter,option->rect,fspec,dspec,dspec.element+"-close-"+status,option->direction);

      break;
    }

    case PE_IndicatorArrowUp :
    case PE_IndicatorArrowDown :
    case PE_IndicatorArrowLeft :
    case PE_IndicatorArrowRight : {
      frame_spec fspec;
      default_frame_spec(fspec);

      QString aStatus = "normal";
      if (!(option->state & State_Enabled))
        aStatus = "disabled";
      else if ((option->state & State_On) || (option->state & State_Sunken) || (option->state & State_Selected))
        aStatus = "pressed";
      else if (option->state & State_MouseOver)
        aStatus = "focused";
      /* it's disabled in KColorChooser; why? */
      if (widget && widget->inherits("KSelector") && !(option->state & State_Enabled))
        aStatus = "pressed";

      QString dir;
      if (element == PE_IndicatorArrowUp)
        dir = "-up-";
      else if (element == PE_IndicatorArrowDown)
        dir = "-down-";
      else if (element == PE_IndicatorArrowLeft)
        dir = "-left-";
      else
        dir = "-right-";

      indicator_spec dspec = getIndicatorSpec("IndicatorArrow");

      /* menuitems may have their own right/left arrows */
      if (qstyleoption_cast<const QStyleOptionMenuItem*>(option)
          && (element == PE_IndicatorArrowLeft || element == PE_IndicatorArrowRight))
      {
        const indicator_spec dspec1 = getIndicatorSpec("MenuItem");
        dspec.size = dspec1.size;
        /* the arrow rectangle is set at CE_MenuItem appropriately */
        if (renderElement(painter, dspec1.element+dir+aStatus,option->rect))
        {
          break;
        }
      }

      if (aStatus == "normal"
          && painter->pen().color() == QColor(Qt::white) // -> SP_ToolBarHorizontalExtensionButton
          && themeRndr_ && themeRndr_->isValid()
          && themeRndr_->elementExists("flat-"+dspec.element+"-down-normal"))
      {
        dspec.element = "flat-"+dspec.element;
      }

      if (widget && !widget->isActiveWindow())
        aStatus.append("-inactive");
      renderIndicator(painter,option->rect,fspec,dspec,dspec.element+dir+aStatus,option->direction);

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
      // this may be better for QML but I don't like it
      /*if (!widget)
      {
        QCommonStyle::drawPrimitive(element,option,painter,widget);
        return;
      }*/

      /*
         Here frame has no real meaning but we force one by adjusting
         PM_FocusFrameHMargin and PM_FocusFrameVMargin for viewitems.
      */

      const QString group = "ItemView";
      frame_spec fspec = getFrameSpec(group);
      interior_spec ispec = getInteriorSpec(group);
      ispec.px = ispec.py = 0;

      /* QCommonStyle uses something like this: */
      /*QString ivStatus = (widget ? widget->isEnabled() : (option->state & QStyle::State_Enabled)) ?
                         (option->state & QStyle::State_Selected) ?
                         (option->state & QStyle::State_Active) ? "pressed" : "toggled" :
                         (option->state & State_MouseOver) ? "focused" : "normal" : "disabled";*/
      /* but we want to know if the widget itself has focus */
      QString ivStatus = (option->state & State_Enabled) ?
                         // as in Okular's navigation panel
                         ((option->state & State_Selected)
                          && (option->state & State_HasFocus)
                          && (option->state & State_Active)) ? "pressed" :
                         // as in most widgets
                         (widget && widget->hasFocus() && (option->state & State_Selected)) ? "pressed" :
                         (option->state & State_Selected) ? "toggled" :
                         (option->state & State_MouseOver) ? "focused" : "normal"
                         : "disabled";

      const QStyleOptionViewItem_v4 *opt = qstyleoption_cast<const QStyleOptionViewItem_v4*>(option);
      const QAbstractItemView *iv = qobject_cast<const QAbstractItemView*>(widget);
      if (opt)
      {
        switch (opt->viewItemPosition) {
          case QStyleOptionViewItem_v4::OnlyOne:
          case QStyleOptionViewItem_v4::Invalid: break;
          case QStyleOptionViewItem_v4::Beginning: {
            fspec.hasCapsule = true;
            fspec.capsuleV = 2;
            if (opt->direction == Qt::RightToLeft)
              fspec.capsuleH = 1;
            else
              fspec.capsuleH = -1;
            fspec.expansion = 0;
            break;
          }
          case QStyleOptionViewItem_v4::End: {
            fspec.hasCapsule = true;
            fspec.capsuleV = 2;
            if (opt->direction == Qt::RightToLeft)
              fspec.capsuleH = -1;
            else
              fspec.capsuleH = 1;
            fspec.expansion = 0;
            break;
          }
          case QStyleOptionViewItem_v4::Middle: {
            fspec.hasCapsule = true;
            fspec.capsuleV = 2;
            fspec.capsuleH = 0;
            fspec.expansion = 0;
            break;
          }
          default: break;
        }
        if (opt->backgroundBrush.style() != Qt::NoBrush)
        {
          /* in this case, the item is colored intentionally
             (as in Konsole's color scheme editing dialog) */
          fspec.expansion = 0;
          if (opt->state & State_HasFocus)
            renderFrame(painter,option->rect,fspec,fspec.element+"-pressed",0,0,0,0,0,fspec.hasCapsule,true);
          else if (ivStatus != "normal" && ivStatus != "disabled")
          {
            if (widget && !widget->isActiveWindow())
              ivStatus.append("-inactive");
            renderFrame(painter,option->rect,fspec,fspec.element+"-"+ivStatus,0,0,0,0,0,fspec.hasCapsule,true);
          }
          QBrush brush = opt->backgroundBrush;
          QColor col = brush.color();
          if (col.alpha() < 255)
          {
            /* this is for deciding on the text color at CE_ItemViewItem later */
            col.setRgb(col.red(),col.green(),col.blue());
            brush.setColor(col);
          }
          QPointF oldBO = painter->brushOrigin();
          painter->setBrushOrigin(opt->rect.topLeft()); // sometimes needed (as in Basket)
          painter->fillRect(interiorRect(opt->rect,fspec), brush);
          painter->setBrushOrigin(oldBO);
          break;
        }
        else if (opt->index.isValid() && !(opt->index.flags() & Qt::ItemIsEditable)
                 && iv && (option->state & State_Enabled))
        {
          /* force colors when text isn't drawn at CE_ItemViewItem (as in VLC) */
          if (QWidget *iw = iv->indexWidget(opt->index))
          {
            const label_spec lspec = getLabelSpec(group);
            QColor col;
            if (ivStatus == "normal")
            {
              QColor tmpCol = getFromRGBA(lspec.normalColor);
              if (enoughContrast(opt->palette.color(QPalette::Base), tmpCol))
                col = tmpCol;
            }
            else if (ivStatus == "focused")
            {
              QColor tmpCol = getFromRGBA(lspec.focusColor);
              if (enoughContrast(QApplication::palette().color(QPalette::Text), tmpCol)
                  // supposing that the focus interior is translucent, take care of contrast
                  || enoughContrast(opt->palette.color(QPalette::Base), tmpCol))
              {
                col = tmpCol;
              }
            }
            else if (ivStatus == "pressed")
              col = getFromRGBA(lspec.pressColor);
            else if (ivStatus == "toggled")
              col = getFromRGBA(lspec.toggleColor);
            if (!col.isValid())
              col = QApplication::palette().color(QPalette::Text);
            if (col.isValid())
            {
              QPalette palette = iw->palette();
              palette.setColor(QPalette::Active,QPalette::Text,col);
              palette.setColor(QPalette::Inactive,QPalette::Text,col);
              iw->setPalette(palette);
            }
          }
        }
      }

      if (ivStatus == "normal" || ivStatus == "disabled")
        break; // for the sake of consistency, we don't draw any background here

      if (widget && !widget->isActiveWindow())
        ivStatus.append("-inactive");

      /* this is needed for elegance */
      if (option->rect.height() < 2)
        fspec.expansion = 0;
      else
        fspec.expansion = qMin(fspec.expansion,option->rect.height()/2);
      /* since Dolphin's view-items have problem with QSvgRenderer, we set usePixmap to true */
      renderFrame(painter,option->rect,fspec,fspec.element+"-"+ivStatus,0,0,0,0,0,fspec.hasCapsule,true);
      renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+ivStatus,fspec.hasCapsule,true);

      if (opt && opt->state & State_HasFocus)
      {
        QStyleOptionFocusRect fropt;
        fropt.QStyleOption::operator=(*opt);
        fropt.rect = interiorRect(opt->rect, fspec);
        drawPrimitive(PE_FrameFocusRect, &fropt, painter, widget);
      }

      break;
    }

    case PE_PanelTipLabel : {
      const QString group = "ToolTip";

      frame_spec fspec = getFrameSpec(group);
      fspec.expansion = 0;
      const interior_spec ispec = getInteriorSpec(group);
      fspec.left = fspec.right = fspec.top = fspec.bottom = pixelMetric(PM_ToolTipLabelFrameWidth,option,widget);

      theme_spec tspec_now = settings_->getCompositeSpec();
      if (!noComposite_ && tspec_now.tooltip_shadow_depth > 0
          && fspec.left >= tspec_now.tooltip_shadow_depth
          && widget && translucentWidgets_.contains(widget))
      {
        renderFrame(painter,option->rect,fspec,fspec.element+"-shadow");
      }
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

QIcon::Mode Style::getIconMode(int state, label_spec lspec) const
{
  QIcon::Mode icnMode;
  if (state == 0)
    icnMode = QIcon::Disabled;
  else
    icnMode = QIcon::Normal;

  QColor txtCol;
  if (state == 1)
    txtCol = getFromRGBA(lspec.normalColor);
  if (state == 2)
    txtCol = getFromRGBA(lspec.focusColor);
  if (state == 3)
    txtCol = getFromRGBA(lspec.pressColor);
  else if (state == 4)
    txtCol = getFromRGBA(lspec.toggleColor);

  if (enoughContrast(txtCol, QApplication::palette().color(QPalette::ButtonText)))
    icnMode = QIcon::Selected;

  return icnMode;
}

void Style::drawControl(ControlElement element,
                        const QStyleOption *option,
                        QPainter *painter,
                        const QWidget *widget) const
{
  int x,y,h,w;
  option->rect.getRect(&x,&y,&w,&h);

  const QIcon::Mode iconmode =
        (option->state & State_Enabled) ?
        (option->state & State_Selected) ? QIcon::Selected :
        (option->state & State_Sunken) ? QIcon::Active :
        (option->state & State_MouseOver) ? QIcon::Active : QIcon::Normal
      : QIcon::Disabled;

  const QIcon::State iconstate =
      (option->state & State_On) ? QIcon::On : QIcon::Off;

  switch ((unsigned)element) { // unsigned because of CE_Kv_KCapacityBar
    case CE_MenuTearoff : {
      QString status = (option->state & State_Selected) ? "focused" : "normal";
      // see PM_MenuTearoffHeight and also PE_PanelMenu
      int marginH = pixelMetric(PM_MenuHMargin,option,widget);
      QRect r(option->rect.x() + marginH,
              option->rect.y() + pixelMetric(PM_MenuVMargin,option,widget),
              option->rect.width() - 2*marginH,
              8);
      const indicator_spec dspec = getIndicatorSpec("MenuItem");
      renderElement(painter,dspec.element+"-tearoff-"+status,r,20,0);

      break;
    }

    case CE_MenuItem : {
      const QStyleOptionMenuItem *opt =
          qstyleoption_cast<const QStyleOptionMenuItem*>(option);

      if (opt) {
        QString status = getState(option,widget);
        const QString group = "MenuItem";

        const frame_spec fspec = getFrameSpec(group);
        const interior_spec ispec = getInteriorSpec(group);
        const indicator_spec dspec = getIndicatorSpec(group);
        const label_spec lspec = getLabelSpec(group);

        if (opt->menuItemType == QStyleOptionMenuItem::Separator)
          renderElement(painter,dspec.element+"-separator",option->rect,20,0);
        //else if (opt->menuItemType == QStyleOptionMenuItem::TearOff)
          //renderElement(painter,dspec.element+"-tearoff",option->rect,20,0);
        else
        {
          bool libreoffice = false;
          if (isLibreoffice_ && (option->state & State_Enabled)
            && enoughContrast(getFromRGBA(lspec.focusColor), QApplication::palette().color(QPalette::WindowText)))
          {
            libreoffice = true;
            painter->save();
            painter->setOpacity(0.5);
          }
          /* don't draw panel for normal and disabled states */
          if (!status.startsWith("normal") && (option->state & State_Enabled))
          {
            renderFrame(painter,option->rect,fspec,fspec.element+"-"+status);
            renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+status);
          }
          if (libreoffice) painter->restore();

          const QStringList l = opt->text.split('\t');

          int smallIconSize = pixelMetric(PM_SmallIconSize);
          int talign = Qt::AlignVCenter | Qt::TextSingleLine;
          if (!styleHint(SH_UnderlineShortcut, opt, widget))
            talign |= Qt::TextHideMnemonic;
          else
            talign |= Qt::TextShowMnemonic;

          int state = 1;
          if (!(option->state & State_Enabled))
            state = 0;
          else if (status.startsWith("toggled") || status.startsWith("pressed"))
            state = 2;

          bool rtl(option->direction == Qt::RightToLeft);

          int iw = pixelMetric(PM_IndicatorWidth,option,widget);
          int ih = pixelMetric(PM_IndicatorHeight,option,widget);
          if (l.size() > 0) // menu label
          {
            int checkSpace = 0;
            if ((widget && opt->menuHasCheckableItems)
                /* QML menus only use checkType, while
                   the default value of menuHasCheckableItems is true. */
                || opt->checkType != QStyleOptionMenuItem::NotCheckable)
              checkSpace = iw + lspec.tispace;
            if (opt->icon.isNull() || (hspec_.iconless_menu && !l[0].isEmpty()))
            {
              int iconSpace = 0;
              if ((opt->maxIconWidth
                   || !widget) // QML menus set maxIconWidth to 0, although they have icon
                  && !hspec_.iconless_menu)
                iconSpace = smallIconSize + lspec.tispace;
              renderLabel(option,painter,
                          option->rect.adjusted(rtl ? 0 : iconSpace+checkSpace,
                                                0,
                                                rtl ? -iconSpace-checkSpace : 0,
                                                0),
                          fspec,lspec,
                          Qt::AlignLeft | talign,
                          l[0],QPalette::Text,
                          state);
            }
            else
            {
              int lxqtMenuIconSize = hspec_.lxqtmainmenu_iconsize;
              if (lxqtMenuIconSize >= 16
                  && lxqtMenuIconSize != smallIconSize
                  && qobject_cast<const QMenu*>(widget))
              {
                if (widget->objectName() == "TopLevelMainMenu")
                  smallIconSize = lxqtMenuIconSize;
                else if (QMenu *menu = qobject_cast<QMenu*>(getParent(widget, 1)))
                {
                  if (menu->objectName() == "TopLevelMainMenu")
                    smallIconSize = lxqtMenuIconSize;
                  else
                  {
                    while (qobject_cast<QMenu*>(getParent(menu, 1)))
                    {
                      menu = qobject_cast<QMenu*>(getParent(menu, 1));
                      if (menu->objectName() == "TopLevelMainMenu")
                      {
                        smallIconSize = lxqtMenuIconSize;
                        break;
                      }
                    }
                  }
                }
              }
              QSize iconSize = QSize(smallIconSize,smallIconSize);
              QRect r = option->rect.adjusted(rtl ? 0 : checkSpace,
                                              0,
                                              rtl ? -checkSpace : 0,
                                              0);
              if (l[0].isEmpty()) // textless menuitem, as in Kdenlive's play button menu
                r = alignedRect(option->direction,Qt::AlignVCenter | Qt::AlignLeft,
                                iconSize,labelRect(r,fspec,lspec));
              renderLabel(option,painter,r,
                          fspec,lspec,
                          Qt::AlignLeft | talign,
                          l[0],QPalette::Text,
                          state,
                          getPixmapFromIcon(opt->icon, getIconMode(state,lspec), iconstate, iconSize),
                          iconSize);
            }
          }
          if (l.size() > 1) // shortcut
          {
            int space = 0;
            if (opt->menuItemType == QStyleOptionMenuItem::SubMenu)
              space = dspec.size + lspec.tispace + 2; // we add a 2px right margin at CT_MenuItem
            renderLabel(option,painter,
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
          /* change the selected and pressed states to mouse-over */
          if (o.state & QStyle::State_Selected)
            o.state = (o.state & ~QStyle::State_Selected) | QStyle::State_MouseOver;
          if (o.state & QStyle::State_Sunken)
            o.state = (o.state & ~QStyle::State_Sunken) | QStyle::State_MouseOver;

          /* submenu arrow */
          if (opt->menuItemType == QStyleOptionMenuItem::SubMenu)
          {
            o.rect = alignedRect(option->direction,
                                 Qt::AlignRight | Qt::AlignVCenter,
                                 QSize(dspec.size,dspec.size),
                                 /* we add a 2px right margin at CT_MenuItem */
                                 interiorRect(opt->rect,fspec).adjusted(rtl ? 2 : 0,
                                                                        0,
                                                                        rtl ? 0 : -2,
                                                                        0));
            drawPrimitive(rtl ? PE_IndicatorArrowLeft : PE_IndicatorArrowRight,
                          &o,painter);
          }

          /* checkbox or radio button */
          if (opt->checkType != QStyleOptionMenuItem::NotCheckable)
          {
            o.rect = alignedRect(option->direction,
                                 Qt::AlignLeft | Qt::AlignVCenter,
                                 QSize(iw,ih),
                                 isLibreoffice_ ?
                                   opt->rect.adjusted(6,-2,0,0) // FIXME why?
                                   : interiorRect(opt->rect,fspec).adjusted(rtl ? 0 : lspec.left,
                                                                            0,
                                                                            rtl ? -lspec.right : 0,
                                                                            0));
            if (opt->checkType == QStyleOptionMenuItem::Exclusive)
            {
              if (opt->checked)
                o.state |= State_On;
              drawPrimitive(PE_IndicatorRadioButton,&o,painter,widget);
            }
            else if (opt->checkType == QStyleOptionMenuItem::NonExclusive)
            {
              if (opt->checked)
                o.state |= State_On;
              drawPrimitive(PE_IndicatorCheckBox,&o,painter,widget);
            }
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
      if (const QStyleOptionViewItem_v4 *opt = qstyleoption_cast<const QStyleOptionViewItem_v4*>(option))
      {
        QPalette palette(opt->palette);
        if (!opt->text.isEmpty()
            /* If another color has been set intentionally,
               as in Akregator's unread feeds or in Kate's
               text style preferences, use it! */
            && (!widget // QML
                || palette == widget->palette()))
        {
          // as in PE_PanelItemViewItem
          int state = (option->state & State_Enabled) ?
                      ((option->state & State_Selected)
                       && (option->state & State_HasFocus)
                       && (option->state & State_Active)) ? 3 :
                      (widget && widget->hasFocus() && (option->state & State_Selected)) ? 3 :
                      (option->state & State_Selected) ? 4 :
                      (option->state & State_MouseOver) ? 2 : 1 : 0;
          if (state != 0)
          {
            const label_spec lspec = getLabelSpec("ItemView");
            QColor normalColor = getFromRGBA(lspec.normalColor);
            QColor focusColor = getFromRGBA(lspec.focusColor);
            QColor pressColor = getFromRGBA(lspec.pressColor);
            QColor toggleColor = getFromRGBA(lspec.toggleColor);
            QColor col;
            if (opt->backgroundBrush.style() != Qt::NoBrush) //-> PE_PanelItemViewItem
            {
              col = QColor(Qt::white);
              Qt::BrushStyle bs = opt->backgroundBrush.style();
              if (bs != Qt::LinearGradientPattern
                  && bs != Qt::ConicalGradientPattern
                  && bs != Qt::RadialGradientPattern)
              {
                if (qGray(opt->backgroundBrush.color().rgb()) >= 127)
                  col = QColor(Qt::black);
              }
              else // FIXME: this isn't an accurate method
              {
                QGradientStops gs = opt->backgroundBrush.gradient()->stops();
                for (int i = 0; i < gs.size(); ++i)
                {
                  if (qGray(gs.at(i).second.rgb()) >= 127)
                  {
                    col = QColor(Qt::black);
                    break;
                  }
                }
              }
              normalColor = focusColor = pressColor = toggleColor = col;
            }
            if (state == 1 && normalColor.isValid()
                /* since we don't draw the normal interior,
                   a minimum amount of contrast is needed */
                && (col.isValid() || enoughContrast(palette.color(QPalette::Base), normalColor)))
            {
              QStyleOptionViewItem_v4 o(*opt);
              palette.setColor(QPalette::Text, normalColor);
              o.palette = palette;
              QCommonStyle::drawControl(element,&o,painter,widget);
              return;
            }
            else if (state == 2 && focusColor.isValid()
                     && (col.isValid()
                         || enoughContrast(QApplication::palette().color(QPalette::Text), focusColor)
                         // supposing that the focus interior is translucent, take care of contrast
                         || enoughContrast(palette.color(QPalette::Base), focusColor)))
            {
              QStyleOptionViewItem_v4 o(*opt);
              palette.setColor(QPalette::Text, focusColor);
              palette.setColor(QPalette::HighlightedText, focusColor);
              o.palette = palette;
              qreal tintPercentage = hspec_.tint_on_mouseover;
              if (tintPercentage > 0
                  && (opt->features & QStyleOptionViewItem_v2::HasDecoration)
                  && !opt->decorationSize.isEmpty())
              {
                QPixmap px = tintedPixmap(option, opt->icon.pixmap(opt->decorationSize), tintPercentage);
                o.icon = QIcon(px);
              }
              QCommonStyle::drawControl(element,&o,painter,widget);
              return;
            }
            else if (state == 3 && pressColor.isValid())
            {
              QStyleOptionViewItem_v4 o(*opt);
              palette.setColor(QPalette::HighlightedText, pressColor);
              o.palette = palette;
              QCommonStyle::drawControl(element,&o,painter,widget);
              return;
            }
            else if (state == 4 && toggleColor.isValid())
            {
              QStyleOptionViewItem_v4 o(*opt);
              palette.setColor(QPalette::HighlightedText, toggleColor);
              o.palette = palette;
              QCommonStyle::drawControl(element,&o,painter,widget);
              return;
            }
          }
          else
          {
            qreal opacityPercentage = hspec_.disabled_icon_opacity;
            if (opacityPercentage < 100
                && (opt->features & QStyleOptionViewItem_v2::HasDecoration)
                && !opt->decorationSize.isEmpty())
            {
              QStyleOptionViewItem_v4 o(*opt);
              QPixmap px = translucentPixmap(opt->icon.pixmap(opt->decorationSize), opacityPercentage);
              o.icon = QIcon(px);
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
      QString group = "MenuBarItem";
      label_spec lspec = getLabelSpec(group);
      if (isLibreoffice_
          && enoughContrast(getFromRGBA(lspec.normalColor),
                            QApplication::palette().color(QPalette::WindowText)))
      {
        break; // dark-and-light themes
      }
      const QStyleOptionMenuItem *opt =
          qstyleoption_cast<const QStyleOptionMenuItem*>(option);
      if (opt) {
        QString status = getState(option,widget);
#if QT_VERSION >= 0x050000
        if (!styleHint(SH_MenuBar_MouseTracking, opt, widget))
        {
          if (status.startsWith("toggled"))
            status.replace("toggled","normal");
          if (status.startsWith("focused"))
            status.replace("focused","normal");
        }
#endif

        group = "MenuBar";
        QRect r = opt->menuRect; // menubar svg element may not be simple
        if (r.isNull()) r = option->rect;
        if (int th = mergedToolbarHeight(widget))
        {
          group = "Toolbar";
          r.adjust(0,0,0,th);
        }

        frame_spec fspec = getFrameSpec(group);
        if (tspec_.merge_menubar_with_toolbar && group != "Toolbar")
        {
          const frame_spec fspec1 = getFrameSpec("Toolbar");
          fspec.left = fspec1.left;
          fspec.top = fspec1.top;
          fspec.right = fspec1.right;
          fspec.bottom = fspec1.bottom;
        }
        int topFrame = fspec.top;
        int bottomFrame = fspec.bottom;
        interior_spec ispec = getInteriorSpec(group);

        /* fill the non-empty regions of the menubar */
        if (!widget) // WARNING: QML has anchoring!
        {
          fspec.expansion = 0;
          ispec.px = ispec.py = 0;
        }
        renderFrame(painter,r,fspec,fspec.element+"-normal");
        renderInterior(painter,r,fspec,ispec,ispec.element+"-normal");

        fspec = getFrameSpec("MenuBarItem");
        ispec = getInteriorSpec("MenuBarItem");

        if (isPlasma_ && widget && widget->window()->testAttribute(Qt::WA_NoSystemBackground))
        {
          lspec.left = lspec.right = lspec.top = lspec.bottom = 0;
          fspec.left = fspec.right = fspec.top = fspec.bottom = qMin(fspec.left,3);
          fspec.expansion = 0;
        }

        /* topFrame and bottomFrame are added at CT_MenuBarItem */
        r = option->rect.adjusted(0,topFrame,0,-bottomFrame);

        /* draw a panel for the menubar-item only if it's focused or pressed */
        if (!status.startsWith("normal") && (option->state & State_Enabled))
        {
          bool libreoffice = false;
          if (isLibreoffice_ && (option->state & State_Enabled)
            && enoughContrast(getFromRGBA(lspec.focusColor), QApplication::palette().color(QPalette::WindowText)))
          {
            libreoffice = true;
            painter->save();
            painter->setOpacity(0.5);
          }
          renderFrame(painter,r,fspec,fspec.element+"-"+status);
          renderInterior(painter,r,fspec,ispec,ispec.element+"-"+status);
          if (libreoffice) painter->restore();
        }
        else
          lspec.normalColor = getLabelSpec(group).normalColor;

        int talign = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine;
        if (!styleHint(SH_UnderlineShortcut, opt, widget))
          talign |= Qt::TextHideMnemonic;
        else
          talign |= Qt::TextShowMnemonic;
        int state = 1;
        if (!(option->state & State_Enabled))
          state = 0;
#if QT_VERSION < 0x050000
        else if (status.startsWith("toggled") || status.startsWith("pressed"))
#else
        else if ((!styleHint(SH_MenuBar_MouseTracking, opt, widget) && status.startsWith("pressed"))
                  || (styleHint(SH_MenuBar_MouseTracking, opt, widget)
                      && (status.startsWith("toggled") || status.startsWith("pressed"))))
#endif
          state = 2;
        renderLabel(option,painter,r,
                    fspec,lspec,
                    talign,opt->text,QPalette::WindowText,
                    state);
      }

      break;
    }

    case CE_MenuBarEmptyArea : {
      if (isLibreoffice_
          && enoughContrast(getFromRGBA(getLabelSpec("MenuBarItem").normalColor),
                            QApplication::palette().color(QPalette::WindowText)))
      {
        break;
      }
      QString group = "MenuBar";
      QRect r = option->rect;
      if (int th = mergedToolbarHeight(widget))
      {
        group = "Toolbar";
        r.adjust(0,0,0,th);
      }

      frame_spec fspec = getFrameSpec(group);
      interior_spec ispec = getInteriorSpec(group);
      if (!widget) // WARNING: QML has anchoring!
      {
        fspec.expansion = 0;
        ispec.px = ispec.py = 0;
      }
      if (tspec_.merge_menubar_with_toolbar && group != "Toolbar")
      {
        const frame_spec fspec1 = getFrameSpec("Toolbar");
        fspec.left = fspec1.left;
        fspec.top = fspec1.top;
        fspec.right = fspec1.right;
        fspec.bottom = fspec1.bottom;
      }

      renderFrame(painter,r,fspec,fspec.element+"-normal");
      renderInterior(painter,r,fspec,ispec,ispec.element+"-normal");

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

    case CE_RadioButton:
    case CE_CheckBox: {
      const QStyleOptionButton *opt =
          qstyleoption_cast<const QStyleOptionButton*>(option);

      if (opt) {
        bool isRadio = (element == CE_RadioButton);
        QStyleOptionButton subopt = *opt;
        subopt.rect = subElementRect(isRadio ? SE_RadioButtonIndicator
                                             : SE_CheckBoxIndicator, opt, widget);
        drawPrimitive(isRadio ? PE_IndicatorRadioButton : PE_IndicatorCheckBox,
                      &subopt, painter, widget);
        subopt.rect = subElementRect(isRadio ? SE_RadioButtonContents
                                             : SE_CheckBoxContents, opt, widget);
        drawControl(isRadio ? CE_RadioButtonLabel : CE_CheckBoxLabel, &subopt, painter, widget);

        if (opt->state & State_HasFocus)
        {
          QStyleOptionFocusRect fropt;
          fropt.QStyleOption::operator=(*opt);
          fropt.rect = subElementRect(isRadio ? SE_RadioButtonFocusRect
                                              : SE_CheckBoxFocusRect, opt, widget);
          drawPrimitive(PE_FrameFocusRect, &fropt, painter, widget);
        }
      }

      break;
    }

    case CE_RadioButtonLabel : {
      const QStyleOptionButton *opt =
          qstyleoption_cast<const QStyleOptionButton*>(option);

      if (opt) {
        frame_spec fspec;
        default_frame_spec(fspec);
        const label_spec lspec = getLabelSpec("RadioButton");

        int talign = Qt::AlignLeft | Qt::AlignVCenter;
        if (!styleHint(SH_UnderlineShortcut, opt, widget))
          talign |= Qt::TextHideMnemonic;
        else
          talign |= Qt::TextShowMnemonic;
        renderLabel(option,painter,
                    option->rect,
                    fspec,lspec,
                    talign,opt->text,QPalette::WindowText,
                    option->state & State_Enabled ? option->state & State_MouseOver ? 2 : 1 : 0,
                    getPixmapFromIcon(opt->icon,iconmode,iconstate,opt->iconSize),
                    opt->iconSize);
      }

      break;
    }

    case CE_CheckBoxLabel : {
      const QStyleOptionButton *opt =
          qstyleoption_cast<const QStyleOptionButton*>(option);

      if (opt) {
        frame_spec fspec;
        default_frame_spec(fspec);
        const label_spec lspec = getLabelSpec("CheckBox");

        int talign = Qt::AlignLeft | Qt::AlignVCenter;
        if (!styleHint(SH_UnderlineShortcut, opt, widget))
          talign |= Qt::TextHideMnemonic;
        else
          talign |= Qt::TextShowMnemonic;
        renderLabel(option,painter,
                    option->rect,
                    fspec,lspec,
                    talign,opt->text,QPalette::WindowText,
                    option->state & State_Enabled ? option->state & State_MouseOver ? 2 : 1 : 0,
                    getPixmapFromIcon(opt->icon,iconmode,iconstate,opt->iconSize),
                    opt->iconSize);
      }

      break;
    }

    case CE_ComboBoxLabel : { // not editable
      const QStyleOptionComboBox *opt =
          qstyleoption_cast<const QStyleOptionComboBox*>(option);

      if (opt && !opt->editable) {
        QString status =
                 (option->state & State_Enabled) ?
                  (option->state & State_On) ? "toggled" :
                  (option->state & State_MouseOver)
                    && (!widget || widget->rect().contains(widget->mapFromGlobal(QCursor::pos()))) // hover bug
                  ? "focused" :
                  (option->state & State_Sunken)
                  // to know it has focus
                  || (option->state & State_Selected) ? "pressed" : "normal"
                 : "disabled";
        if (widget && !widget->isActiveWindow())
          status.append("-inactive");

        const QString group = "ComboBox";
        frame_spec fspec = getFrameSpec(group);
        label_spec lspec = getLabelSpec(group);
        lspec.left = qMax(0,lspec.left-1);
        lspec.top = qMax(0,lspec.top-1);
        lspec.right = qMax(0,lspec.right-1);
        lspec.bottom = qMax(0,lspec.bottom-1);

        QRect r = subControlRect(CC_ComboBox,opt,SC_ComboBoxEditField,widget);
        int talign = Qt::AlignLeft | Qt::AlignVCenter;
        if (!styleHint(SH_UnderlineShortcut, opt, widget))
          talign |= Qt::TextHideMnemonic;
        else
          talign |= Qt::TextShowMnemonic;

        int state = 1;
        if (!(option->state & State_Enabled))
          state = 0;
        else if (status.startsWith("pressed"))
          state = 3;
        else if (status.startsWith("toggled"))
          state = 4;
        else if (status.startsWith("focused"))
          state = 2;

        if (const QComboBox *cb = qobject_cast<const QComboBox*>(widget))
        { // when there isn't enough space
          if(!cb->lineEdit())
          {
            QSize txtSize = textSize(painter->font(),opt->currentText);
            const indicator_spec dspec = getIndicatorSpec("DropDownButton");
            int deltaR = 0; int deltaL = 0;
            int iSize = qMin(dspec.size,cb->height()-fspec.top-fspec.bottom);
            if (cb->width() < fspec.left+lspec.left+txtSize.width()+lspec.right+COMBO_ARROW_LENGTH+fspec.right
                || cb->height() < fspec.top+lspec.top+txtSize.height()+fspec.bottom+lspec.bottom)
            {
              deltaR = fspec.right > 3 ? fspec.right - 3 : 0;
              deltaL = fspec.left > 3 ? fspec.left - 3 : 0;

              fspec.left = qMin(fspec.left,3);
              fspec.right = qMin(fspec.right,3);
              fspec.top = qMin(fspec.top,3);
              fspec.bottom = qMin(fspec.bottom,3);

              lspec.left = qMin(lspec.left,2);
              lspec.right = qMin(lspec.right,2);
              lspec.top = qMin(lspec.top,2);
              lspec.bottom = qMin(lspec.bottom,2);
              lspec.tispace = qMin(lspec.tispace,2);

              /* indicator size is reduced to 9 at PE_IndicatorButtonDropDown */
              iSize = qMin(qMin(dspec.size,cb->height()-fspec.top-fspec.bottom),9);
            }
            /* give all available space to the label */
            if (opt->direction == Qt::RightToLeft)
              r.adjust(-deltaL-qMax(COMBO_ARROW_LENGTH-iSize,0), 0, 0, 0);
            else
              r.adjust(0, 0, deltaR+qMax(COMBO_ARROW_LENGTH-iSize,0), 0);
          }
        }

        int vFrame = qMax(fspec.top,fspec.bottom);
        QStyleOptionComboBox o(*opt);
        if ((option->state & State_MouseOver) && !status.startsWith("focused"))
          o.state = o.state & ~QStyle::State_MouseOver; // hover bug
        renderLabel(&o,painter,
                    /* since the label is vertically centered inside the label rectangle,
                       this doesn't do any harm and is good for Qt Designer and similar cases */
                    r.adjusted(0, -vFrame-lspec.top, 0, vFrame+lspec.bottom),
                    fspec,lspec,
                    talign,opt->currentText,QPalette::ButtonText,
                    state,
                    getPixmapFromIcon(opt->currentIcon, getIconMode(state,lspec), iconstate, opt->iconSize),
                    opt->iconSize);
      }

      break;
    }

    case CE_TabBarTabShape : {
      const QStyleOptionTab *opt =
        qstyleoption_cast<const QStyleOptionTab*>(option);

      if (opt)
      {
        /* Let's forget about the pressed state. It's useless here and
           makes trouble in KDevelop. The disabled state is useless too. */
        QString status =
                (option->state & State_On) ? "toggled" :
                 (option->state & State_Selected) ? "toggled" :
                 (option->state & State_MouseOver) ?
                  (option->state & State_Enabled) ? "focused" : "normal"
                : "normal";
        if (widget && !widget->isActiveWindow())
          status.append("-inactive");

        frame_spec fspec = getFrameSpec("Tab");
        interior_spec ispec = getInteriorSpec("Tab");

        QRect r = option->rect;
        bool verticalTabs = false;
        bool mirroredBottomTab = false;
        bool docMode = false;

        if (opt->shape == QTabBar::RoundedEast
            || opt->shape == QTabBar::RoundedWest
            || opt->shape == QTabBar::TriangularEast
            || opt->shape == QTabBar::TriangularWest)
        {
            verticalTabs = true;
        }
        QTabWidget *tw = qobject_cast<QTabWidget*>(getParent(widget,1));
        if (!tw || tw->documentMode()) docMode = true;
        bool mirror = (!docMode && tspec_.attach_active_tab) ? true : tspec_.mirror_doc_tabs;
        if (mirror && (opt->shape == QTabBar::RoundedSouth || opt->shape == QTabBar::TriangularSouth))
          mirroredBottomTab = true;
        bool rtl(opt->direction == Qt::RightToLeft && !verticalTabs);
        bool joinedActiveTab(joinedActiveTab_);
        bool noActiveTabSep = tspec_.no_active_tab_separator;
        QString sepName = fspec.element + "-separator";

        if (docMode && hasFloatingTabs_)
        {
          ispec.element="floating-"+ispec.element;
          fspec.element="floating-"+fspec.element;
          joinedActiveTab = joinedActiveFloatingTab_;
          sepName = "floating-"+sepName;
        }

        if (joinedActiveTab) // only use normal and toggled states
        {
          if ((option->state & State_On) || (option->state & State_Selected)
               // use toggled separator on both sides of selected tabs if possible
              || (opt->position != QStyleOptionTab::OnlyOneTab
                  && (!mirroredBottomTab ? (!rtl ? opt->selectedPosition == QStyleOptionTab::NextIsSelected
                                             : opt->selectedPosition == QStyleOptionTab::PreviousIsSelected)
                      : (!rtl ? opt->selectedPosition == QStyleOptionTab::PreviousIsSelected
                         : opt->selectedPosition == QStyleOptionTab::NextIsSelected))))
          {
            sepName = sepName+"-toggled"; // falls back to normal state automatically
          }
          else
            sepName = sepName+"-normal";
        }

        if ((joinedActiveTab && !noActiveTabSep)
            || status.startsWith("normal") || status.startsWith("focused"))
        {
          if (tspec_.joined_inactive_tabs
              && opt->position != QStyleOptionTab::OnlyOneTab)
          {
            int capsule = 2;
            if (opt->position == QStyleOptionTab::Beginning)
            {
              if ((joinedActiveTab && !noActiveTabSep) || opt->selectedPosition != QStyleOptionTab::NextIsSelected)
              {
                fspec.hasCapsule = true;
                capsule = -1;
              }
            }
            else if (opt->position == QStyleOptionTab::Middle)
            {
              fspec.hasCapsule = true;
              if ((joinedActiveTab && !noActiveTabSep))
                capsule = 0;
              else if (opt->selectedPosition == QStyleOptionTab::NextIsSelected)
                capsule = 1;
              else if (opt->selectedPosition == QStyleOptionTab::PreviousIsSelected)
                capsule = -1;
              else
                capsule = 0;
            }
            else if (opt->position == QStyleOptionTab::End)
            {
              if ((joinedActiveTab && !noActiveTabSep) || opt->selectedPosition != QStyleOptionTab::PreviousIsSelected)
              {
                fspec.hasCapsule = true;
                capsule = 1;
              }
            }
            /* will be flipped both vertically and horizontally */
            if (mirroredBottomTab)
              capsule = -1*capsule;
            /* I've seen this only in KDevelop */
            if (rtl)
              capsule = -1*capsule;
            fspec.capsuleH = capsule;
            fspec.capsuleV = 2;
          }
        }

        bool drawSep(joinedActiveTab
                     && opt->position != QStyleOptionTab::OnlyOneTab
                     && (mirroredBottomTab
                           ? rtl ? opt->position != QStyleOptionTab::End
                                 : opt->position != QStyleOptionTab::Beginning
                           : rtl ? opt->position != QStyleOptionTab::Beginning
                                 : opt->position != QStyleOptionTab::End));
        QRect sep, sepTop, sepBottom;
        bool isActiveTabSep = ((option->state & State_Selected)
                               || (mirroredBottomTab
                                     ? (!rtl ? opt->selectedPosition == QStyleOptionTab::PreviousIsSelected
                                             : opt->selectedPosition == QStyleOptionTab::NextIsSelected)
                                     : (!rtl ? opt->selectedPosition == QStyleOptionTab::NextIsSelected
                                             : opt->selectedPosition == QStyleOptionTab::PreviousIsSelected)));
        if (drawSep && !(noActiveTabSep && isActiveTabSep))
        {
          sep.setRect(x+w-fspec.right,
                      y+fspec.top,
                      fspec.right,
                      h-fspec.top-fspec.bottom);
          sepTop.setRect(x+w-fspec.right, y, fspec.right, fspec.top);
          sepBottom.setRect(x+w-fspec.right,
                            y+h-fspec.bottom,
                            fspec.right,
                            fspec.bottom);
        }

        if (verticalTabs)
        {
          /* painter saving/restoring is needed not only to
             render texts of left and bottom tabs correctly
             but also because there are usually mutiple tabs */
          painter->save();
          int X, Y, rot;
          int xTr = 0; int xScale = 1;
          if ((!(docMode || !tspec_.attach_active_tab) || mirror)
              && (opt->shape == QTabBar::RoundedEast || opt->shape == QTabBar::TriangularEast))
          {
            if (drawSep && !(noActiveTabSep && isActiveTabSep))
            {
              /* finding the correct sep rects is a little tricky and, especially,
                 the difference between right and left as well as top and bottom
                 frames should be taken into acount with care */
              if (!mirror)
              {
                sep.setRect(x+h-fspec.right,
                            y+fspec.top,
                            fspec.right,
                            w-fspec.top-fspec.bottom);
                sepTop.setRect(x+h-fspec.right,
                               y,
                               fspec.right,
                               fspec.top);
                sepBottom.setRect(x+h-fspec.right,
                                  y+w-fspec.bottom,
                                  fspec.right,
                                  fspec.bottom);
              }
              else
              {
                sep.setRect(h-fspec.right,
                            fspec.top,
                            fspec.right,
                            w-fspec.top-fspec.bottom);
                sepTop.setRect(h-fspec.right,
                               0,
                               fspec.right,
                               fspec.top);
                sepBottom.setRect(h-fspec.right,
                                  w-fspec.bottom,
                                  fspec.right,
                                  fspec.bottom);
              }
            }
            X = w;
            Y = y;
            rot = 90;
          }
          else
          {
            if (drawSep && !(noActiveTabSep && isActiveTabSep))
            {
              sep.setRect(h-fspec.right,
                          fspec.top,
                          fspec.right,
                          w-fspec.top-fspec.bottom);
              sepTop.setRect(h-fspec.right,
                             0,
                             fspec.right,
                             fspec.top);
              sepBottom.setRect(h-fspec.right,
                                w-fspec.bottom,
                                fspec.right,
                                fspec.bottom);
            }
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
        else if (mirroredBottomTab)
        {
          if (drawSep && mirror && !(noActiveTabSep && isActiveTabSep))
          {
            sep.setRect(w-fspec.right,
                        fspec.top,
                        fspec.right,
                        h-fspec.top-fspec.bottom);
            sepTop.setRect(w-fspec.right,
                           0,
                           fspec.right,
                           fspec.top);
            sepBottom.setRect(w-fspec.right,
                              h-fspec.bottom,
                              fspec.right,
                              fspec.bottom);
          }
          painter->save();
          QTransform m;
          /* flip bottom tabs both vertically and horizontally */
          r.setRect(0, 0, w, h);
          m.translate(x + w, h); m.scale(-1,-1);
          painter->setTransform(m, true);
        }

        if (!(option->state & State_Enabled))
        {
          painter->save();
          painter->setOpacity(DISABLED_OPACITY);
        }
        if (fspec.hasCapsule)
        {
          ispec.px = ispec.py = 0;
        }
        if (!widget) // WARNING: Why QML tabs are cut by 1px from below?
        {
          if (!verticalTabs)
          {
            if (tspec_.mirror_doc_tabs
                && (opt->shape == QTabBar::RoundedSouth || opt->shape == QTabBar::TriangularSouth))
            {
              r.adjust(0,1,0,0);
              sep.adjust(0,1,0,0);
              sepTop.adjust(0,1,0,1);
            }
            else
            {
              r.adjust(0,0,0,-1);
              sep.adjust(0,0,0,-1);
              sepBottom.adjust(0,-1,0,-1);
            }
          }
          else // this is redundant because TabView's tabPosition is either Qt.TopEdge or Qt.BottomEdge
          {
            if (tspec_.mirror_doc_tabs
                && (opt->shape == QTabBar::RoundedEast || opt->shape == QTabBar::TriangularEast))
            {
              if (tspec_.mirror_doc_tabs)
              {
                r.adjust(0,1,0,0);
                sep.adjust(0,1,0,0);
                sepTop.adjust(0,1,0,1);
              }
            }
            else
            {
              r.adjust(0,0,0,-1);
              sep.adjust(0,0,0,-1);
              sepBottom.adjust(0,-1,0,-1);
            }
          }
        }
        renderInterior(painter,r,fspec,ispec,ispec.element+"-"+status,fspec.hasCapsule);
        renderFrame(painter,r,fspec,fspec.element+"-"+status,0,0,0,0,0,fspec.hasCapsule);
        if (drawSep)
        {
          renderElement(painter,sepName,sep);
          renderElement(painter,sepName+"-top",sepTop);
          renderElement(painter,sepName+"-bottom",sepBottom);
        }
        if (!(option->state & State_Enabled))
          painter->restore();

        if (verticalTabs || mirroredBottomTab)
          painter->restore();
      }

      break;
    }

    case CE_TabBarTabLabel : {
      const QStyleOptionTab *opt =
        qstyleoption_cast<const QStyleOptionTab*>(option);

      if (opt) {
        QString status =
            (option->state & State_Enabled) ?
              (option->state & State_On) ? "toggled" :
              (option->state & State_Selected) ? "toggled" :
              (option->state & State_MouseOver) ? "focused" : "normal"
            : "disabled";
        if (widget && !widget->isActiveWindow())
          status.append("-inactive");

        const QString group = "Tab";
        frame_spec fspec = getFrameSpec(group);
        label_spec lspec = getLabelSpec(group);

        int talign;
        if (!widget) // QML
          talign = Qt::AlignCenter;
        else
          talign = Qt::AlignLeft | Qt::AlignVCenter;
        if (!styleHint(SH_UnderlineShortcut, opt, widget))
          talign |= Qt::TextHideMnemonic;
        else
          talign |= Qt::TextShowMnemonic;

        QRect r = option->rect;
        bool verticalTabs = false;
        bool bottomTabs = false;
        bool mirror = true;

        if (opt->shape == QTabBar::RoundedEast
            || opt->shape == QTabBar::RoundedWest
            || opt->shape == QTabBar::TriangularEast
            || opt->shape == QTabBar::TriangularWest)
        {
          verticalTabs = true;
        }
        QTabWidget *tw = qobject_cast<QTabWidget*>(getParent(widget,1));
        if ((!tw || tw->documentMode() || !tspec_.attach_active_tab) && !tspec_.mirror_doc_tabs)
          mirror = false;
        if (mirror && (opt->shape == QTabBar::RoundedSouth || opt->shape == QTabBar::TriangularSouth))
          bottomTabs = true;
        
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
            if (!mirror)
            { // without mirroring, the top and bottom margins should be swapped
              int t = fspec.bottom;
              fspec.bottom = fspec.top;
              fspec.top = t;
              t = lspec.bottom;
              lspec.bottom = lspec.top;
              lspec.top = t;
            }
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
        else if (bottomTabs)
        { // the top and bottom margins should be swapped
          int t = fspec.bottom;
          fspec.bottom = fspec.top;
          fspec.top = t;
          t = lspec.bottom;
          lspec.bottom = lspec.top;
          lspec.top = t;
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

        QStyleOptionTab_v2 tabV2(*opt);
        QSize iconSize;
        if (!tabV2.icon.isNull())
          iconSize = tabV2.iconSize;

        int state = 1;
        if (!(option->state & State_Enabled))
          state = 0;
        else if (status.startsWith("pressed"))
          state = 3;
        else if (status.startsWith("toggled"))
          state = 4;
        else if (option->state & State_MouseOver)
          state = 2;

        bool closable = false;
        if (const QTabBar *tb = qobject_cast<const QTabBar*>(widget))
        {
          if (tb->tabsClosable())
            closable = true;
        }

        /* since we draw text and icon together as label,
           for RTL we need to move the label to right */
        if (opt->direction == Qt::RightToLeft && !verticalTabs && closable)
          r = alignedRect(Qt::RightToLeft, Qt::AlignLeft,
                          QSize(w-pixelMetric(PM_TabCloseIndicatorWidth,option,widget), h),
                          option->rect);

        int icnSize = iconSize.isValid() ? 
                        qMax(iconSize.width(), iconSize.height())
                        : pixelMetric(PM_TabBarIconSize);

        /* eliding */
        QString txt = opt->text;
        if (!txt.isEmpty())
        {
          int txtWidth = r.width()-lspec.right-lspec.left-fspec.left-fspec.right
                         - (closable ? lspec.tispace : 0)
                         - (opt->icon.isNull() ? 0 : icnSize);
          QFont F(painter->font());
          if (lspec.boldFont) F.setBold(true);
          QSize txtSize = textSize(F,txt);
          if (txtSize.width() > txtWidth)
          {
            QFontMetrics fm(F);
            txt = fm.elidedText(txt, Qt::ElideRight, txtWidth);
          }
          if (txtSize.height() > r.height()-lspec.top-lspec.bottom-fspec.top-fspec.bottom)
          { // try to work around design flaws as far as possible
            lspec.top = lspec.bottom = 0;
          }
        }

        if (!widget) // QML
        {
          if (!verticalTabs)
          {
            if (tspec_.mirror_doc_tabs
                && (opt->shape == QTabBar::RoundedSouth || opt->shape == QTabBar::TriangularSouth))
              r.adjust(0,0,0,-1);
            else
              r.adjust(0,1,0,0);
          }
          else // redundant
          {
            if (tspec_.mirror_doc_tabs
                && (opt->shape == QTabBar::RoundedEast || opt->shape == QTabBar::TriangularEast))
              r.adjust(0,0,0,-1);
            else
              r.adjust(0,1,0,0);
          }
        }
        iconSize = QSize(icnSize,icnSize);
        renderLabel(option,painter,
                    r,
                    fspec,lspec,
                    talign,txt,QPalette::WindowText,
                    state,
                    getPixmapFromIcon(opt->icon, getIconMode(state,lspec), iconstate, iconSize),
                    iconSize);

        if (tabV2.state & State_HasFocus)
        {
          QStyleOptionFocusRect fropt;
          fropt.QStyleOption::operator=(*opt);
          QRect FR = opt->rect;
          if (verticalTabs)
            FR.setRect(0, 0, h, w);
          fropt.rect = interiorRect(FR, fspec);
          drawPrimitive(PE_FrameFocusRect, &fropt, painter, widget);
        }

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
      QString status = getState(option,widget);
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
      if (const QStyleOptionToolBox *opt = qstyleoption_cast<const QStyleOptionToolBox*>(option))
      {
        if (!opt->text.isEmpty())
        {
          int state = 1;
          if (!(option->state & State_Enabled))
            state = 0;
          else if ((option->state & State_On) || (option->state & State_Sunken) || (option->state & State_Selected))
            state = 3;
          else if (option->state & State_MouseOver)
            state = 2;
          if (state != 0)
          {
            const label_spec lspec = getLabelSpec("ToolboxTab");
            QColor normalColor = getFromRGBA(lspec.normalColor);
            QColor focusColor = getFromRGBA(lspec.focusColor);
            QColor pressColor = getFromRGBA(lspec.pressColor);
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
              qreal tintPercentage = hspec_.tint_on_mouseover;
              if (tintPercentage > 0 && !opt->icon.isNull())
              {
                QPixmap px = tintedPixmap(option,
                                          opt->icon.pixmap(pixelMetric(QStyle::PM_SmallIconSize,opt,widget)),
                                          tintPercentage);
                o.icon = QIcon(px);
              }
              QCommonStyle::drawControl(element,&o,painter,widget);
              return;
            }
            else if (state == 3 && pressColor.isValid())
            {
              QStyleOptionToolBox o(*opt);
              QPalette palette(opt->palette);
              palette.setColor(QPalette::ButtonText, pressColor);
              o.palette = palette;
              qreal tintPercentage = hspec_.tint_on_mouseover;
              if (tintPercentage > 0 && (option->state & State_MouseOver) && !opt->icon.isNull())
              {
                QPixmap px = tintedPixmap(option,
                                          opt->icon.pixmap(pixelMetric(QStyle::PM_SmallIconSize,opt,widget)),
                                          tintPercentage);
                o.icon = QIcon(px);
              }
              QCommonStyle::drawControl(element,&o,painter,widget);
              return;
            }
          }
          else
          {
            qreal opacityPercentage = hspec_.disabled_icon_opacity;
            if (opacityPercentage < 100 && !opt->icon.isNull())
            {
              QStyleOptionToolBox o(*opt);
              QPixmap px = translucentPixmap(opt->icon.pixmap(pixelMetric(QStyle::PM_SmallIconSize,opt,widget)),
                                             opacityPercentage);
              o.icon = QIcon(px);
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
      QString group;
      if (tspec_.vertical_spin_indicators && isKisSlider_)
        group = "LineEdit";
      else group = "Progressbar";

      frame_spec fspec = getFrameSpec(group);
      fspec.left = fspec.right = qMin(fspec.left,fspec.right);
      const interior_spec ispec = getInteriorSpec(group);
      if (isKisSlider_)
      {
        fspec.hasCapsule = true;
        fspec.capsuleH = -1;
        fspec.capsuleV = 2;
        if (tspec_.vertical_spin_indicators)
        {
          fspec.left = fspec.right = fspec.top = fspec.bottom = qMin(fspec.left,3);
          fspec.expansion = 0;
        }
      }

      QRect r = option->rect;

      /* checking State_Horizontal wouldn't work with
         Krita's progress-spin boxes (KisSliderSpinBox) */
      const QProgressBar *pb = qobject_cast<const QProgressBar*>(widget);
      bool isVertical(false);
      if (pb && pb->orientation() == Qt::Vertical)
      {
        isVertical = true;
        /* we don't save and restore the painter to draw
           the contents and the label correctly below */
        r.setRect(y, x, h, w);
        QTransform m;
        m.scale(-1,1);
        m.rotate(90);
        painter->setTransform(m, true);
      }

      /* When the maximum progressbar thickness isn't greater than
         the frame expansion, it means that progressbars should be
         always rounded. Here, we force this rule on KCapacityBar. */
      if (tspec_.progressbar_thickness > 0 && tspec_.progressbar_thickness <= fspec.expansion)
        fspec.expansion = qMax(fspec.expansion, isVertical ? w : h);

      if (!(option->state & State_Enabled))
      {
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      QString suffix = "-normal";
      if (widget && !widget->isActiveWindow())
        suffix = "-normal-inactive";
      renderFrame(painter,r,fspec,fspec.element+suffix,0,0,0,0,0,true);
      renderInterior(painter,r,fspec,ispec,ispec.element+suffix,true);
      if (!(option->state & State_Enabled))
        painter->restore();

      break;
    }

    case CE_ProgressBarContents : {
      const QStyleOptionProgressBar *opt =
          qstyleoption_cast<const QStyleOptionProgressBar*>(option);

      if (opt)
      {
        QString status =
                 (option->state & State_Enabled) ?
                   (option->state & State_Selected) ? "toggled" :
                   (option->state & State_MouseOver) ? "focused" : "normal"
                 : "disabled";
        if (widget && !widget->isActiveWindow())
          status.append("-inactive");

        const QString group = "ProgressbarContents";
        frame_spec fspec = getFrameSpec(group);
        if (isKisSlider_)
        {
          //fspec.right = 0;
          fspec.hasCapsule = true;
          fspec.capsuleH = -1;
          fspec.capsuleV = 2;
        }
        else
        {
          fspec.left = fspec.right = qMin(fspec.left,fspec.right);
        }
        const interior_spec ispec = getInteriorSpec(group);

        bool isVertical = false;
        bool inverted = false;
        const QProgressBar *pb = qobject_cast<const QProgressBar*>(widget);
        if (pb)
        {
          if (pb->orientation() == Qt::Vertical)
            isVertical = true;
          if (pb->invertedAppearance())
            inverted = true;
        }

        /* if the progressbar is rounded, its contents should be so too */
        bool isRounded = false;
        if (tspec_.vertical_spin_indicators && isKisSlider_)
        {
          fspec.left = fspec.right = fspec.top = fspec.bottom = qMin(fspec.left,3);
          fspec.expansion = 0;
        }
        else
        {
          const frame_spec fspec1 = getFrameSpec("Progressbar");
          fspec.expansion = fspec1.expansion - (tspec_.spread_progressbar? 0 : fspec1.top+fspec1.bottom);
          // like in CE_ProgressBarGroove
          if (tspec_.progressbar_thickness > 0 && tspec_.progressbar_thickness <= fspec1.expansion)
            fspec.expansion = qMax(fspec.expansion, isVertical ? w : h);
          if (fspec.expansion >= qMin(h,w)) isRounded = true;
        }

        QRect r = option->rect;
        // after this, we could visualize horizontally...
        if (isVertical)
          r.setRect(y, x, h, w);

        bool thin = false;
        if (opt->maximum != 0 || opt->minimum != 0)
        {
          int length = isVertical ? h : w;
          int empty = length
                      - sliderPositionFromValue(opt->minimum,
                                                opt->maximum,
                                                qMax(opt->progress,opt->minimum),
                                                length,
                                                false);
          if (isVertical ? inverted : !inverted)
            r.adjust(0,0,-empty,0);
          else
            r.adjust(empty,0,0,0);

          // take care of thin indicators
          if (r.width() > 0)
          {
            if (isRounded)
            {
              if ((!isVertical && r.width() < h) || (isVertical && r.width() < w))
              {
                painter->save();
                painter->setClipRegion(r);
                if (!isVertical && !inverted)
                  r.setWidth(h);
                else if (isVertical && inverted)
                  r.setWidth(w);
                else if (!isVertical && inverted)
                  r.adjust(r.width()-h,0,0,0);
                else// if (isVertical && !inverted)
                  r.adjust(r.width()-w,0,0,0);
                thin = true;
              }
            }
            else if (r.width() < fspec.left+fspec.right)
            {
              painter->save();
              painter->setClipRegion(r);
              if ((!isVertical && !inverted) || (isVertical && inverted))
                r.setWidth(fspec.left+fspec.right);
              else
                r.adjust(r.width()-fspec.left-fspec.right,0,0,0);
              thin = true;
            }
          }
          if (r.height() < fspec.top+fspec.bottom)
          {
            fspec.top = fspec.bottom = r.height()/2;
          }
          renderFrame(painter,r,fspec,fspec.element+"-"+status,0,0,0,0,0,true);
          renderInterior(painter,r,fspec,ispec,ispec.element+"-"+status,true);
          if (thin)
            painter->restore();
        }
        else if (widget)
        { // busy progressbar
          QWidget *wi = (QWidget *)widget;
          int animcount = progressbars_[wi];
          int pm = qMin(qMax(pixelMetric(PM_ProgressBarChunkWidth),fspec.left+fspec.right),r.width()/2-2);         
          QRect R = r.adjusted(animcount,0,0,0);
          if (isVertical ? inverted : !inverted)
            R.setX(r.x()+(animcount%r.width()));
          else
            R.setX(r.x()+r.width()-(animcount%r.width()));
          if (!isRounded)
            R.setWidth(pm);
          else
          {
            if (!isVertical)
              R.setWidth(h);
            else
              R.setWidth(w);
          }
          if (R.height() < fspec.top+fspec.bottom)
          {
            fspec.top = fspec.bottom = r.height()/2;
          }
            
          if (R.x()+R.width() > r.x()+r.width())
          {
            R.setWidth(r.x() + r.width() - R.x());

            // keep external corners rounded
            thin = false;
            QRect R1(R);
            if (R1.width() > 0)
            {
              if (isRounded)
              {
                painter->save();
                painter->setClipRegion(R);
                if (!isVertical)
                  R1.adjust(R.width()-h,0,0,0);
                else
                  R1.adjust(R.width()-w,0,0,0);
                thin = true;
              }
              else if (R1.width() < fspec.left+fspec.right)
              {
                painter->save();
                painter->setClipRegion(R1);
                R1.adjust(R.width()-fspec.left-fspec.right,0,0,0);
                thin = true;
              }
            }

            renderFrame(painter,R1,fspec,fspec.element+"-"+status,0,0,0,0,0,true);
            renderInterior(painter,R1,fspec,ispec,ispec.element+"-"+status,true);
            if (thin)
              painter->restore();

            R = QRect(r.x(), r.y(), (!isRounded ? pm : !isVertical? h : w)-R.width(), r.height());

            thin = false;
            if (R.width() > 0)
            {
              if (isRounded)
              {
                painter->save();
                painter->setClipRegion(R);
                if (!isVertical)
                  R.setWidth(h);
                else
                  R.setWidth(w);
                thin = true;
              }
              else if (R.width() < fspec.left+fspec.right)
              {
                painter->save();
                painter->setClipRegion(R);
                R.setWidth(fspec.left+fspec.right);
                thin = true;
              }
            }

            renderFrame(painter,R,fspec,fspec.element+"-"+status,0,0,0,0,0,true);
            renderInterior(painter,R,fspec,ispec,ispec.element+"-"+status,true);
            if (thin)
              painter->restore();
          }
          else
          {
            renderFrame(painter,R,fspec,fspec.element+"-"+status,0,0,0,0,0,true);
            renderInterior(painter,R,fspec,ispec,ispec.element+"-"+status,true);
          }
        }
        else
          QCommonStyle::drawControl(element,option,painter,widget);
      }

      break;
    }

    case CE_ProgressBarLabel : {
      const QStyleOptionProgressBar *opt =
          qstyleoption_cast<const QStyleOptionProgressBar*>(option);
      const QStyleOptionProgressBar_v2 *opt2 =
          qstyleoption_cast<const QStyleOptionProgressBar_v2*>(option);

      if (opt && opt->textVisible)
      {
        frame_spec fspec;
        default_frame_spec(fspec);
        label_spec lspec = getLabelSpec("Progressbar");
        lspec.left = lspec.right = lspec.top = lspec.bottom = 0;

        QFont f(painter->font());
        if (lspec.boldFont) f.setBold(true);
        bool isVertical = false;
        if (opt2 && opt2->orientation == Qt::Vertical)
          isVertical = true;

        if (tspec_.progressbar_thickness > 0
            && QFontMetrics(f).height() >= (isVertical ? w : h)
            // KCapacityBar and KisSliderSpinBox don't obey thickness setting
            && !(widget && widget->inherits("KCapacityBar")) && !isKisSlider_)
          break;

        int length = w;
        QRect r = option->rect;
        if (isVertical)
        {
          length = h;
          r.setRect(y, x, h, w);
          QTransform m;
          if (!opt2->bottomToTop)
          {
            m.translate(0, w+2*x); m.scale(1,-1);
          }
          else
          {
            m.translate(h+2*y, 0); m.scale(-1,1);
          }
          painter->setTransform(m, true);
        }

        QString txt = opt->text;
        if (!txt.isEmpty())
          txt = QFontMetrics(f).elidedText(txt, Qt::ElideRight, length);

        int state = option->state & State_Enabled ?
                      (option->state & State_Selected) ? 4
                      : option->state & State_MouseOver ? 2 : 1 : 0;

        /* find the part inside the indicator */
        QRect R;
        QColor col = getFromRGBA(cspec_.progressIndicatorTextColor);
        if (state != 0 && !txt.isEmpty() && col.isValid())
        {
          QColor txtCol;
          if (state == 1) txtCol = getFromRGBA(lspec.normalColor);
          else if (state == 2) txtCol = getFromRGBA(lspec.focusColor);
          else if (state == 4) txtCol = getFromRGBA(lspec.toggleColor);
          // do nothing if the colors are the same
          if ((!txtCol.isValid() || col != txtCol)
              && (txtCol.isValid() || col != QApplication::palette().color(QPalette::WindowText)))
          {
            int full = sliderPositionFromValue(opt->minimum,
                                               opt->maximum,
                                               qMax(opt->progress,opt->minimum),
                                               length,
                                               false);
            bool inverted = false;
            const QProgressBar *pb = qobject_cast<const QProgressBar*>(widget);
            if (pb && pb->invertedAppearance()) inverted = true;
            if (!isVertical)
            {
              if (inverted)
                R = r.adjusted(w-full,0,0,0);
              else
                R = r.adjusted(0,0,full-w,0);
            }
            else
            {
              if (inverted)
              {
                if (!opt2 || !opt2->bottomToTop)
                  R = r.adjusted(0,0,full-h,0);
                else
                  R = r.adjusted(h-full,0,0,0);
              }
              else
              {
                if (!opt2 || !opt2->bottomToTop)
                  R = r.adjusted(h-full,0,0,0);
                else
                  R = r.adjusted(0,0,full-h,0);
              }
            }
          }
        }

        if (R.isValid())
        {
          painter->save();
          painter->setClipRegion(QRegion(r).subtracted(QRegion(R)));
        }
        renderLabel(option,painter,
                    r,
                    fspec,lspec,
                    Qt::AlignCenter,txt,QPalette::WindowText,state);
        if (R.isValid())
        {
          painter->restore();
          painter->save();
          painter->setClipRect(R);
          renderLabel(option,painter,
                      r,
                      fspec,lspec,
                      Qt::AlignCenter,txt,QPalette::WindowText,-1);
          painter->restore();
        }
      }

      break;
    }

    case CE_Splitter : {
      const QString group = "Splitter";
      const frame_spec fspec = getFrameSpec(group);
      const interior_spec ispec = getInteriorSpec(group);
      const indicator_spec dspec = getIndicatorSpec(group);
      QString status =
          (option->state & State_Enabled) ?
            (option->state & State_Sunken) ? "pressed" :
            (option->state & State_MouseOver) ? "focused" : "normal"
          : "disabled";
      if (widget && !widget->isActiveWindow())
        status.append("-inactive");

      QRect r = option->rect;
      /* we don't check State_Horizontal because it may
         lead to wrong results (like in Qt4 Designer) */
      if (h < w)
      {
        /* we enter x and y into our calculations because
           there may be more than one splitter handle */
        r.setRect(y, x, h, w);
        painter->save();
        QTransform m;
        m.scale(1,-1);
        m.rotate(-90);
        painter->setTransform(m, true);
      }

      if (!(option->state & State_Enabled))
      {
        status.replace("disabled","normal");
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
                    alignedRect(option->direction,
                                Qt::AlignCenter,
                                QSize(iW,dspec.size),
                                r));
      if (!(option->state & State_Enabled))
        painter->restore();
      if (h < w)
        painter->restore();

      break;
    }

    case CE_ScrollBarAddLine :
    case CE_ScrollBarSubLine : {
      QRect r = option->rect;
      if (!r.isValid()) return;
      bool add = true;
      if (element == CE_ScrollBarSubLine)
        add = false;

      frame_spec fspec;
      default_frame_spec(fspec);
      const indicator_spec dspec = getIndicatorSpec("Scrollbar");

      QString iStatus = getState(option,widget); // indicator state
      if (option->state & State_Enabled)
      {
        const QStyleOptionSlider *opt = qstyleoption_cast<const QStyleOptionSlider*>(option);
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

          if (widget && !widget->isActiveWindow() && !iStatus.endsWith("-inactive"))
            iStatus.append("-inactive");
        }
      }

      bool hrtl = false;
      if (option->state & State_Horizontal)
      {
        if (option->direction == Qt::RightToLeft)
          hrtl = true;
        r.setRect(y, x, h, w);
        painter->save();
        QTransform m;
        m.scale(1,-1);
        m.rotate(-90);
        painter->setTransform(m, true);
      }

      renderIndicator(painter,r,
                      fspec,dspec,
                      dspec.element+(add ?
                                       hrtl ? "-up-" : "-down-"
                                       : hrtl ? "-down-" : "-up-")
                                   +iStatus,
                      option->direction);

      if (option->state & State_Horizontal)
        painter->restore();

      break;
    }

    case CE_ScrollBarSlider : {
      QString status = getState(option,widget);
      if (status.startsWith("focused")
          && widget && !widget->rect().contains(widget->mapFromGlobal(QCursor::pos()))) // hover bug
      {
        status.replace("focused","normal");
      }
      QString sStatus = status; // slider state
      if (!tspec_.animate_states // when animated, focus it when the cursor enters the groove
          && (option->state & State_Enabled))
      {
        const QStyleOptionSlider *opt = qstyleoption_cast<const QStyleOptionSlider*>(option);
        if (opt && opt->activeSubControls != QStyle::SC_ScrollBarSlider)
        {
          sStatus = "normal";
          if (widget && !widget->isActiveWindow())
            sStatus = "normal-inactive";
        }
      }

      const QString group = "ScrollbarSlider";

      frame_spec fspec = getFrameSpec(group);
      fspec.expansion = 0; // no need to frame expansion because the thickness is known
      const interior_spec ispec = getInteriorSpec(group);
      const indicator_spec dspec = getIndicatorSpec(group);

      QRect r = option->rect;
      if (option->state & State_Horizontal)
      {
        /* the painter was saved at CC_ScrollBar,
           so no transformation here */
        r.setRect(y, x, h, w);
      }

      if (!(option->state & State_Enabled))
      {
        sStatus.replace("disabled","normal");
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      bool animate(widget && widget->isEnabled() && animatedWidget_ == widget
                   && !qobject_cast<const QAbstractScrollArea*>(widget));
      if (animate)
      {
        if (animationStartState_ == sStatus)
          animationOpacity_ = 100;
        else if (animationOpacity_ < 100)
        {
          renderFrame(painter,r,fspec,fspec.element+"-"+animationStartState_);
          renderInterior(painter,r,fspec,ispec,ispec.element+"-"+animationStartState_);
        }
        painter->save();
        painter->setOpacity((qreal)animationOpacity_/100);
      }
      renderFrame(painter,r,fspec,fspec.element+"-"+sStatus);
      renderInterior(painter,r,fspec,ispec,ispec.element+"-"+sStatus);
      if (animate)
      {
        painter->restore();
        if (animationOpacity_ >= 100)
          animationStartState_ = sStatus;
      }
      renderElement(painter,
                    dspec.element+"-"+status, // let the grip change on mouse-over for the whole scrollbar
                    alignedRect(option->direction,
                                Qt::AlignCenter,
                                QSize(pixelMetric(PM_ScrollBarExtent)-fspec.left-fspec.right,
                                      qMin(dspec.size,r.height()-fspec.top-fspec.bottom)),
                                r));
      if (!(option->state & State_Enabled))
        painter->restore();

      break;
    }

    case CE_HeaderSection : {
      const QString group = "HeaderSection";
      frame_spec fspec = getFrameSpec(group);
      const interior_spec ispec = getInteriorSpec(group);

      bool horiz = true;
      QRect sep;
      if (const QStyleOptionHeader *opt = qstyleoption_cast<const QStyleOptionHeader*>(option))
      {
        bool rtl(option->direction == Qt::RightToLeft);
        if (opt->orientation != Qt::Horizontal) horiz = false;
        switch (opt->position) {
          case QStyleOptionHeader::End:
            fspec.hasCapsule = true;
            fspec.capsuleV = 2;
            fspec.capsuleH = rtl ? -1 : 1;
            if (horiz)
            {
              if (rtl)
              {
                sep.setRect(x+w-fspec.right,
                            y+fspec.top,
                            fspec.right,
                            h-fspec.top-fspec.bottom);
              }
              else
              {
                sep.setRect(x,
                            y+fspec.top,
                            fspec.left,
                            h-fspec.top-fspec.bottom);
              }
            }
            else
            {
              if (rtl) fspec.capsuleH = 1;
              sep.setRect(x+fspec.top, // -> CT_HeaderSection
                          y,
                          w-fspec.top-fspec.bottom,
                          fspec.left);
            }
            break;
          case QStyleOptionHeader::Beginning:
            fspec.hasCapsule = true;
            fspec.capsuleV = 2;
            fspec.capsuleH = rtl ? 1 : -1;
            if (!horiz && rtl) fspec.capsuleH = 1;
            break;
          case QStyleOptionHeader::Middle:
            fspec.hasCapsule = true;
            fspec.capsuleV = 2;
            fspec.capsuleH = 0;
            if (horiz)
            {
              if (rtl)
                sep.setRect(x+w-fspec.right,
                            y+fspec.top,
                            fspec.right,
                            h-fspec.top-fspec.bottom);
              else
                sep.setRect(x,
                            y+fspec.top,
                            fspec.left,
                            h-fspec.top-fspec.bottom);
            }
            else
            {
              sep.setRect(x+fspec.top, // -> CT_HeaderSection
                          y,
                          w-fspec.top-fspec.bottom,
                          fspec.left);
            }
            break;
         default: break;
        }
      }

      QRect r = option->rect;
      if (!horiz)
      {
        r.setRect(y, x, h, w);
        sep.setRect(sep.y(), sep.x(), sep.height(), sep.width());
        painter->save();
        QTransform m;
        m.scale(1,-1);
        m.rotate(-90);
        painter->setTransform(m, true);
      }

      QString status = getState(option,widget);
      if (!(option->state & State_Enabled))
      {
        status.replace("disabled","normal");
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      /* for elegance */
      /*if (r.height() < 2)
        fspec.expansion = 0;
      else
        fspec.expansion = qMin(fspec.expansion,r.height()/2);*/
      fspec.expansion = 0; // vertical headers have variable heights
      renderFrame(painter,r,fspec,fspec.element+"-"+status,0,0,0,0,0,true);
      renderInterior(painter,r,fspec,ispec,ispec.element+"-"+status,true);
      /* if there's no header separator, use the right frame */
      if (themeRndr_ && themeRndr_->isValid() && !themeRndr_->elementExists("header-separator"))
        renderElement(painter,fspec.element+"-"+status+"-left",sep);
      else
        renderElement(painter,"header-separator",sep);
      if (!(option->state & State_Enabled))
        painter->restore();
      if (!horiz)
        painter->restore();

      break;
    }

    case CE_HeaderLabel : {
      const QStyleOptionHeader *opt =
        qstyleoption_cast<const QStyleOptionHeader*>(option);

      if (opt) {
        const QString group = "HeaderSection";

        frame_spec fspec = getFrameSpec(group);
        label_spec lspec = getLabelSpec(group);

        bool rtl(opt->direction == Qt::RightToLeft);

        if (opt->orientation != Qt::Horizontal)
        { // -> CT_HeaderSection
          int t = fspec.left;
          fspec.left = fspec.top;
          fspec.top = t;
          t = fspec.right;
          fspec.right = fspec.bottom;
          fspec.bottom = t;
        }
        if (opt->position == QStyleOptionHeader::Beginning || opt->position == QStyleOptionHeader::Middle)
        {
          if (opt->orientation == Qt::Horizontal)
          {
            if (rtl) fspec.left = 0;
            else fspec.right = 0;
          }
          else
            fspec.bottom = 0;
        }
        if (opt->textAlignment & Qt::AlignLeft)
        {
          if (rtl) lspec.left = 0;
          else lspec.right = 0;
        }
        else if (opt->textAlignment & Qt::AlignRight)
        {
          if (rtl) lspec.right = 0;
          else lspec.left = 0;
        }
        else if (opt->textAlignment & Qt::AlignHCenter)
        {
          lspec.right = lspec.left = 0;
        }
        if (opt->sortIndicator != QStyleOptionHeader::None)
        { // the frame is taken care of at SE_HeaderArrow
          if (rtl)
            fspec.left = 0;
          else
            fspec.right = 0;
        }

        /* for thin headers, like in Dolphin's details view */
        if (opt->icon.isNull())
        {
          fspec.top = fspec.bottom = lspec.top = lspec.bottom = 0;
        }

        QString status = getState(option,widget);
        int state = 1;
        if (!(option->state & State_Enabled))
          state = 0;
        else if (status.startsWith("pressed"))
          state = 3;
        else if (status.startsWith("toggled"))
          state = 4;
        else if (status.startsWith("focused"))
          state = 2;

        int smallIconSize = pixelMetric(PM_SmallIconSize);
        QSize iconSize = QSize(smallIconSize,smallIconSize);
        renderLabel(option,painter,
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
                    getPixmapFromIcon(opt->icon,iconmode,iconstate,iconSize),
                    iconSize);
      }

      break;
    }

    case CE_ToolBar : {
      if (!qstyleoption_cast<const QStyleOptionToolBar*>(option))
        break;
      /* practically not a toolbar (Kaffeine's sidebar) */
      if (widget && widget->findChild<QTabBar*>())
        break;
      /* don't draw in places like KAboutDialog (> KAboutData > KAboutPerson) */
      if (!qobject_cast<QMainWindow*>(getParent(widget,1)))
        break;

      bool stylable(true);
      if (hspec_.single_top_toolbar && !isStylableToolbar(widget))
        stylable = false;

      QRect r = option->rect;
      if (stylable && !(option->state & State_Horizontal))
      {
        r.setRect(0, 0, h, w);
        painter->save();
        QTransform m;
        m.scale(1,-1);
        m.rotate(-90);
        painter->setTransform(m, true);
      }

      if (tspec_.merge_menubar_with_toolbar)
      {
        if (QMainWindow *mw = qobject_cast<QMainWindow*>(getParent(widget,1)))
        {
          if (QWidget *mb = mw->menuWidget())
          {
            if (mb->isVisible())
            {
              mb->update();
              if (stylable && (option->state & State_Horizontal)
                  && mb->y()+mb->height() == widget->y())
              {
                r.adjust(0,-mb->height(),0,0);
              }
            }
          }
        }
      }

      if (!stylable) break;

      const QString group = "Toolbar";
      frame_spec fspec = getFrameSpec(group);
      interior_spec ispec = getInteriorSpec(group);
      if (!widget) // WARNING: QML has anchoring!
      {
        fspec.expansion = 0;
        ispec.px = ispec.py = 0;
      }

      if (!(option->state & State_Enabled))
      {
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      QString suffix = "-normal";
      if (widget && !widget->isActiveWindow())
        suffix = "-normal-inactive";

      renderFrame(painter,r,fspec,fspec.element+suffix);
      renderInterior(painter,r,fspec,ispec,ispec.element+suffix);
      if (!(option->state & State_Enabled))
        painter->restore();
      if (!(option->state & State_Horizontal))
        painter->restore();

      break;
    }

    case CE_SizeGrip : {
      const indicator_spec dspec = getIndicatorSpec("SizeGrip");
      frame_spec fspec;
      default_frame_spec(fspec);

      Qt::Corner corner;
      if (const QStyleOptionSizeGrip *sgOpt = qstyleoption_cast<const QStyleOptionSizeGrip*>(option))
        corner = sgOpt->corner;
      else if (option->direction == Qt::RightToLeft)
        corner = Qt::BottomLeftCorner;
      else
        corner = Qt::BottomRightCorner;
      if (corner == Qt::BottomLeftCorner)
      {
        painter->save();
        QTransform m;
        m.translate(w,0);
        m.scale(-1,1);
        painter->setTransform(m, true);
      }
      else if (corner == Qt::TopRightCorner)
      {
        painter->save();
        QTransform m;
        m.translate(0,h);
        m.scale(1,-1);
        painter->setTransform(m, true);
      }
      else if (corner == Qt::TopLeftCorner)
      {
        painter->save();
        QTransform m;
        m.translate(w,h);
        m.scale(-1,-1);
        painter->setTransform(m, true);
      }
      QString status = getState(option,widget);
      renderIndicator(painter,option->rect,fspec,dspec,dspec.element+"-"+status,option->direction);
      if (corner != Qt::BottomRightCorner)
        painter->restore();

      break;
    }

    case CE_PushButton : {
      const QStyleOptionButton *opt =
          qstyleoption_cast<const QStyleOptionButton*>(option);
      if (opt) {
        if (qobject_cast<const QPushButton*>(widget)
            && !standardButton.contains(widget))
        {
          standardButton.insert(widget);
          connect(widget, SIGNAL(destroyed(QObject*)), SLOT(removeFromSet(QObject*)), Qt::UniqueConnection);
        }
        drawControl(QStyle::CE_PushButtonBevel,opt,painter,widget);
        QStyleOptionButton subopt(*opt);
        subopt.rect = subElementRect(SE_PushButtonContents,opt,widget);
        drawControl(QStyle::CE_PushButtonLabel,&subopt,painter,widget);
      }

      break;
    }

    case CE_PushButtonLabel : {
      const QStyleOptionButton *opt =
          qstyleoption_cast<const QStyleOptionButton*>(option);

      if (opt) {
        const QString group = "PanelButtonCommand";
        frame_spec fspec = getFrameSpec(group);
        const indicator_spec dspec = getIndicatorSpec(group);
        label_spec lspec = getLabelSpec(group);
        lspec.left = qMax(0,lspec.left-1);
        lspec.top = qMax(0,lspec.top-1);
        lspec.right = qMax(0,lspec.right-1);
        lspec.bottom = qMax(0,lspec.bottom-1);
        if (isPlasma_ && widget && widget->window()->testAttribute(Qt::WA_NoSystemBackground))
        {
          lspec.left = lspec.right = lspec.top = lspec.bottom = 0;
          fspec.left = fspec.right = fspec.top = fspec.bottom = 0;
        }
        else if (qobject_cast<QAbstractItemView*>(getParent(widget,2)))
        {
          lspec.left = lspec.right = lspec.top = lspec.bottom = qMin(lspec.left,2);
          fspec.left = fspec.right = fspec.top = fspec.bottom = qMin(fspec.left,3);
          lspec.tispace = qMin(lspec.tispace,3);
        }

        const QPushButton *pb = qobject_cast<const QPushButton*>(widget);
        if (!hspec_.normal_default_pushbutton
            && (option->state & State_Enabled) && pb && pb->isDefault()) {
          QFont f(pb->font());
          f.setBold(true);
          painter->setFont(f);
        }

        QSize txtSize;
        if (!opt->text.isEmpty())
        {
          QFont fnt(painter->font());
          if ((opt->features & QStyleOptionButton::AutoDefaultButton) || lspec.boldFont)
            fnt.setBold(true);
          txtSize = textSize(fnt,opt->text);
           /* in case there isn't enough space */
          if (pb)
          {
            if (pb->width() < txtSize.width()
                              +(opt->icon.isNull() ? 0 : opt->iconSize.width()+lspec.tispace)
                              +lspec.left+lspec.right+fspec.left+fspec.right)
            {
              lspec.left = lspec.right = 0;
              fspec.left = qMin(fspec.left,3);
              fspec.right = qMin(fspec.right,3);
              lspec.tispace = qMin(lspec.tispace,3);
            }
            if (pb->height() < txtSize.height() +lspec.top+lspec.bottom+fspec.top+fspec.bottom)
            {
              lspec.top = lspec.bottom = 0;
              fspec.top = qMin(fspec.top,3);
              fspec.bottom = qMin(fspec.bottom,3);
              lspec.tispace = qMin(lspec.tispace,3);
            }
          }
        }

        /* We should enlarge opt->rect because it's shrinked by PM_DefaultFrameWidth.
           We also take into account the possibility of the presence of an indicator. */
        int frame = pixelMetric(PM_DefaultFrameWidth, option, widget);
        int ind = opt->features & QStyleOptionButton::HasMenu ? dspec.size+pixelMetric(PM_HeaderMargin) : 0;
        QRect r = option->rect.adjusted(-frame + (opt->direction == Qt::RightToLeft ? ind : 0),
                                        -frame,
                                        frame - (opt->direction == Qt::RightToLeft ? 0 : ind),
                                        frame);
        QString status = getState(option,widget);
        if (status.startsWith("toggled") || status.startsWith("pressed"))
        {
          int hShift = pixelMetric(PM_ButtonShiftHorizontal);
          int vShift = pixelMetric(PM_ButtonShiftVertical);
          r = r.adjusted(hShift,vShift,hShift,vShift);
        }
        int talign = Qt::AlignHCenter | Qt::AlignVCenter;
        if (!styleHint(SH_UnderlineShortcut, opt, widget))
          talign |= Qt::TextHideMnemonic;
        else
          talign |= Qt::TextShowMnemonic;

        int state = 1;
        if (!(option->state & State_Enabled))
          state = 0;
        else if (status.startsWith("pressed"))
          state = 3;
        else if (status.startsWith("toggled"))
          state = 4;
        else if ((option->state & State_MouseOver)
                 && (!widget || widget->rect().contains(widget->mapFromGlobal(QCursor::pos())))) // hover bug
          state = 2;

        if (opt->features & QStyleOptionButton::Flat) // respect the text color of the parent widget
          lspec.normalColor = QApplication::palette().color(QPalette::WindowText).name();

        QStyleOptionButton o(*opt);
        if ((option->state & State_MouseOver) && state != 2)
          o.state = o.state & ~QStyle::State_MouseOver; // hover bug

        /* center text+icon */
        QRect R = r;
        if (txtSize.isValid() && !opt->icon.isNull())
        {
          int margin = (r.width() - txtSize.width() - opt->iconSize.width()
                        - fspec.left - fspec.right - lspec.left - lspec.right - lspec.tispace
                        - (lspec.hasShadow ? qAbs(lspec.xshift)+lspec.depth : 0)) / 2;
          if (margin > 0)
            R.adjust(margin, 0, -margin, 0);
        }

        renderLabel(&o,painter,
                    R,
                    fspec,lspec,
                    talign,opt->text,QPalette::ButtonText,
                    state,
                    (hspec_.iconless_pushbutton && !opt->text.isEmpty()) ? QPixmap()
                      : getPixmapFromIcon(opt->icon, getIconMode(state,lspec), iconstate, opt->iconSize),
                    opt->iconSize);
      }

      break;
    }

    case CE_PushButtonBevel : { // bevel and indicator
      const QStyleOptionButton *opt =
          qstyleoption_cast<const QStyleOptionButton*>(option);

      if (opt) {
        QString status = getState(option,widget);
        if (status.startsWith("focused")
            && widget && !widget->rect().contains(widget->mapFromGlobal(QCursor::pos()))) // hover bug
        {
          status.replace("focused","normal");
        }
        const QString group = "PanelButtonCommand";
        frame_spec fspec = getFrameSpec(group);
        const interior_spec ispec = getInteriorSpec(group);
        indicator_spec dspec = getIndicatorSpec(group);
        label_spec lspec = getLabelSpec(group);
        lspec.left = qMax(0,lspec.left-1);
        lspec.top = qMax(0,lspec.top-1);
        lspec.right = qMax(0,lspec.right-1);
        lspec.bottom = qMax(0,lspec.bottom-1);

        /* force text color if the button isn't drawn in a standard way */
        if (widget && !standardButton.contains(widget)
            && (option->state & State_Enabled))
        {
          QColor col;
          if (!(opt->features & QStyleOptionButton::Flat) || !status.startsWith("normal"))
          {
            col = getFromRGBA(lspec.normalColor);
            if (status.startsWith("pressed"))
              col = getFromRGBA(lspec.pressColor);
            else if (status.startsWith("toggled"))
              col = getFromRGBA(lspec.toggleColor);
            else if (option->state & State_MouseOver)
              col = getFromRGBA(lspec.focusColor);
          }
          else // FIXME: the foreground color of the parent widget should be used
            col = QApplication::palette().color(QPalette::WindowText);
          forceButtonTextColor(widget,col);
        }

        if (qobject_cast<QAbstractItemView*>(getParent(widget,2)))
        {
          lspec.left = lspec.right = lspec.top = lspec.bottom = qMin(lspec.left,2);
          fspec.left = fspec.right = fspec.top = fspec.bottom = qMin(fspec.left,3);
          //fspec.expansion = 0;
          lspec.tispace = qMin(lspec.tispace,3);
        }

        const QPushButton *pb = qobject_cast<const QPushButton*>(widget);

        if (pb && !opt->text.isEmpty()) // -> CE_PushButtonLabel
        {
          QFont fnt(painter->font());
          if ((opt->features & QStyleOptionButton::AutoDefaultButton) || lspec.boldFont)
            fnt.setBold(true);
          QSize txtSize = textSize(fnt,opt->text);
          if (pb->width() < txtSize.width()
                            +(opt->icon.isNull() ? 0 : opt->iconSize.width()+lspec.tispace)
                            +lspec.left+lspec.right+fspec.left+fspec.right)
          {
            lspec.left = lspec.right = 0;
            fspec.left = qMin(fspec.left,3);
            fspec.right = qMin(fspec.right,3);
          }
          if (pb->height() < txtSize.height()+lspec.top+lspec.bottom+fspec.top+fspec.bottom)
          {
            lspec.top = lspec.bottom = 0;
            fspec.top = qMin(fspec.top,3);
            fspec.bottom = qMin(fspec.bottom,3);
          }
        }

        // FIXME why does Qt4 designer use CE_PushButtonBevel for its Widget Box headers?
        if (widget && !pb)
        {
          drawPrimitive(PE_Frame,option,painter,widget);
          break;
        }
        // KColorButton (color button in general)
        if (opt->text.size() == 0 && opt->icon.isNull()) fspec.expansion = 0;
        if (!(option->state & State_Enabled))
        {
          status = "normal";
          if (option->state & State_On)
            status = "toggled";
          if (widget && !widget->isActiveWindow())
            status.append("-inactive");
          painter->save();
          painter->setOpacity(DISABLED_OPACITY);
        }
        if (widget && !(opt->features & QStyleOptionButton::Flat)
            && ((!widget->styleSheet().isEmpty() && widget->styleSheet().contains("background"))
                || (opt->icon.isNull()
                    && widget->palette().color(QPalette::Button) != QApplication::palette().color(QPalette::Button))))
        { // color button!?
          fspec.expansion = 0;
          renderFrame(painter,option->rect,fspec,fspec.element+"-"+status);
          painter->fillRect(interiorRect(opt->rect,fspec), widget->palette().brush(QPalette::Button));
        }
        else
        {
          bool libreoffice = false;
          if (isLibreoffice_ && (option->state & State_Enabled)
              && enoughContrast(getFromRGBA(lspec.normalColor), QApplication::palette().color(QPalette::ButtonText)))
          {
            libreoffice = true;
            painter->save();
            painter->setOpacity(0.5);
          }
          bool animate(widget && widget->isEnabled() && animatedWidget_ == widget
                       && !qobject_cast<const QAbstractScrollArea*>(widget));
          if (!(opt->features & QStyleOptionButton::Flat) || !status.startsWith("normal"))
          {
            if (animate)
            {
              if (animationStartState_ == status)
                animationOpacity_ = 100;
              else if (animationOpacity_ < 100
                       && (!(opt->features & QStyleOptionButton::Flat)
                           || !animationStartState_.startsWith("normal")))
              {
                renderFrame(painter,option->rect,fspec,fspec.element+"-"+animationStartState_);
                renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+animationStartState_);
              }
              painter->save();
              painter->setOpacity((qreal)animationOpacity_/100);
            }
            renderFrame(painter,option->rect,fspec,fspec.element+"-"+status);
            renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+status);
            if (animate)
            {
              painter->restore();
              if (animationOpacity_ >= 100)
                animationStartState_ = status;
            }
          }
          // fade out animation
          else if (animate)
          {
            if (animationStartState_ == status)
              animationOpacity_ = 100;
            else if (animationOpacity_ < 100)
            {
              painter->save();
              painter->setOpacity(1.0 - (qreal)animationOpacity_/100);
              renderFrame(painter,option->rect,fspec,fspec.element+"-"+animationStartState_);
              renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+animationStartState_);
              painter->restore();
            }
            if (animationOpacity_ >= 100)
              animationStartState_ = status;
          }
          if (libreoffice) painter->restore();
        }
        if (!(option->state & State_Enabled))
        {
          painter->restore();
          status = "disabled";
        }

        if (opt->features & QStyleOptionButton::HasMenu)
        {
          QString aStatus = "normal";
          /* use the "flat" indicator with flat buttons if it exists */
          if ((opt->features & QStyleOptionButton::Flat) && status.startsWith("normal"))
          {
            if (themeRndr_ && themeRndr_->isValid())
            {
              QColor ncol = getFromRGBA(lspec.normalColor);
              if (!ncol.isValid())
                ncol = QApplication::palette().color(QPalette::ButtonText);
              if (enoughContrast(ncol, QApplication::palette().color(QPalette::WindowText))
                  && themeRndr_->elementExists("flat-"+dspec.element+"-down-normal"))
                dspec.element = "flat-"+dspec.element;
            }
          }
          else
          {
            if (!(option->state & State_Enabled))
              aStatus = "disabled";
            else if (status.startsWith("toggled") || status.startsWith("pressed"))
              aStatus = "pressed";
            else if ((option->state & State_MouseOver)
                     && (!widget || widget->rect().contains(widget->mapFromGlobal(QCursor::pos())))) // hover bug
              aStatus = "focused";
          }
          if (widget && !widget->isActiveWindow())
            aStatus.append("-inactive");
          renderIndicator(painter,
                          option->rect.adjusted(opt->direction == Qt::RightToLeft ? lspec.left : 0,
                                                0,
                                                opt->direction == Qt::RightToLeft ? 0 : -lspec.right,
                                                0),
                          fspec,dspec,dspec.element+"-down-"+aStatus,
                          option->direction,
                          Qt::AlignRight | Qt::AlignVCenter);
        }

        if (pb && pb->isDefault() && (option->state & State_Enabled))
        {
          renderFrame(painter,option->rect,fspec,fspec.element+"-default");
          renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-default");
          QString di = "button-default-indicator";
          if ((opt->features & QStyleOptionButton::Flat) && status.startsWith("normal")
              && themeRndr_ && themeRndr_->isValid()
              && themeRndr_->elementExists("flat-button-default-indicator"))
          {
            di = "flat-button-default-indicator";
          }
          renderIndicator(painter,
                          option->rect,
                          fspec,dspec,di,option->direction,
                          Qt::AlignBottom | (opt->direction == Qt::RightToLeft ?
                                             Qt::AlignLeft : Qt::AlignRight));
        }

        if (opt->state & State_HasFocus)
        {
          QStyleOptionFocusRect fropt;
          fropt.QStyleOption::operator=(*opt);
          fropt.rect = interiorRect(subElementRect(SE_PushButtonFocusRect, opt, widget), fspec);
          drawPrimitive(PE_FrameFocusRect, &fropt, painter, widget);
        }
      }

      break;
    }

    case CE_ToolButtonLabel : {
      const QStyleOptionToolButton *opt =
          qstyleoption_cast<const QStyleOptionToolButton*>(option);

      if (opt) {
        QString txt = opt->text;
        QString status = getState(option,widget);
        QString group = "PanelButtonTool";
        QWidget *p = getParent(widget,1);
        QWidget *gp = getParent(p,1);
        if (p && (isStylableToolbar(p) || isStylableToolbar(gp))
            && (!getFrameSpec("ToolbarButton").element.isEmpty()
                || !getInteriorSpec("ToolbarButton").element.isEmpty()))
        {
          group = "ToolbarButton";
        }
        frame_spec fspec = getFrameSpec(group);
        indicator_spec dspec = getIndicatorSpec(group);
        label_spec lspec = getLabelSpec(group);
        lspec.left = qMax(0,lspec.left-1);
        lspec.top = qMax(0,lspec.top-1);
        lspec.right = qMax(0,lspec.right-1);
        lspec.bottom = qMax(0,lspec.bottom-1);
        /*bool inPlasma = false;
        QWidget *p = getParent(widget,1);
        if (isPlasma_ && widget
            && (widget->window()->testAttribute(Qt::WA_NoSystemBackground)
                // toolbuttons on PanelController
                || (p && p->palette().color(p->backgroundRole()) == QColor(Qt::transparent))))
        {
          inPlasma = true;
          lspec.left = lspec.right = lspec.top = lspec.bottom = 0;
          fspec.left = fspec.right = fspec.top = fspec.bottom = 0;
        }*/

        /* where there may not be enough space,
           especially in KDE new-stuff dialogs */
        if (qobject_cast<QAbstractItemView*>(gp))
        {
          fspec.left = qMin(fspec.left,3);
          fspec.right = qMin(fspec.right,3);
          fspec.top = qMin(fspec.top,3);
          fspec.bottom = qMin(fspec.bottom,3);

          //lspec.left = qMin(lspec.left,2);
          //lspec.right = qMin(lspec.right,2);
          lspec.top = qMin(lspec.top,2);
          lspec.bottom = qMin(lspec.bottom,2);
          lspec.tispace = qMin(lspec.tispace,2);
        }

        const Qt::ToolButtonStyle tialign = opt->toolButtonStyle;
        const QToolButton *tb = qobject_cast<const QToolButton*>(widget);

        if (status.startsWith("focused"))
        {
          if (widget)
          {
            QRect R = option->rect;
            if (fspec.expansion > 0 || (tb && tb->popupMode() != QToolButton::MenuButtonPopup))
              R = widget->rect();
            if (!R.contains(widget->mapFromGlobal(QCursor::pos()))) // hover bug
              status.replace("focused","normal");
          }
        }

        if (tb)
        {
          /* always show menu titles in the toggled state */
          bool transMenuTitle(hspec_.transparent_menutitle);
          if (!transMenuTitle
              && tb->isDown() && tb->toolButtonStyle() == Qt::ToolButtonTextBesideIcon
              && qobject_cast<QMenu*>(p))
          {
            status.replace("pressed","toggled");
          }

          /* the right arrow is attached */
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

          /* no pressed state if only the dropdown arrow is pressed */
          if (fspec.expansion <= 0 // otherwise the drop-down part will be integrated
              && tb->popupMode() == QToolButton::MenuButtonPopup && !tb->isDown()
              && status.startsWith("pressed"))
          {
            status.replace("pressed","normal");
          }

          /* respect the text color of the parent widget */
          if (tb->autoRaise() /*|| inPlasma*/ || !paneledButtons.contains(widget))
          {
            bool isNormal(status.startsWith("normal"));
            QColor ncol = getFromRGBA(lspec.normalColor);
            if (!ncol.isValid())
              ncol = QApplication::palette().color(QPalette::ButtonText);
            QWidget* menubar = NULL;
            if (qobject_cast<QMenuBar*>(gp))
              menubar = gp;
            else if (qobject_cast<QMenuBar*>(p))
              menubar = p;
            if (menubar)
            {
              if (isNormal)
              {
                QString group1("MenuBar");
                if (mergedToolbarHeight(menubar))
                  group1 = "Toolbar";
                const label_spec lspec1 = getLabelSpec(group1);
                if (themeRndr_ && themeRndr_->isValid()
                    && enoughContrast(ncol, getFromRGBA(lspec1.normalColor))
                    && themeRndr_->elementExists("flat-"+dspec.element+"-down-normal"))
                {
                  dspec.element = "flat-"+dspec.element;
                }
                lspec.normalColor = lspec1.normalColor;
              }
            }
            else if ((qobject_cast<QMainWindow*>(gp) && isStylableToolbar(p)
                      && !p->findChild<QTabBar*>())
                     || (qobject_cast<QMainWindow*>(getParent(gp,1)) && isStylableToolbar(gp)
                         && !gp->findChild<QTabBar*>()))
            {
              if (isNormal)
              {
                const QToolBar *toolBar = qobject_cast<QToolBar*>(p);
                if (!tspec_.group_toolbar_buttons || (toolBar && toolBar->orientation() == Qt::Vertical))
                {
                  const label_spec lspec1 = getLabelSpec("Toolbar");
                  if (themeRndr_ && themeRndr_->isValid()
                      && enoughContrast(ncol, getFromRGBA(lspec1.normalColor))
                      && themeRndr_->elementExists("flat-"+dspec.element+"-down-normal"))
                  {
                    dspec.element = "flat-"+dspec.element;
                  }
                  lspec.normalColor = lspec1.normalColor;
                }
              }
            }
            else if (p)
            {
              QColor col;
              if (!tb->autoRaise() && !paneledButtons.contains(widget)) // an already styled toolbutton
                col = opt->palette.color(QPalette::ButtonText);
              else
                col = p->palette().color(p->foregroundRole());
              if (!col.isValid())
                col = QApplication::palette().color(QPalette::WindowText);
              if (isNormal)
              {
                if (themeRndr_ && themeRndr_->isValid()
                    && enoughContrast(ncol, col)
                    && themeRndr_->elementExists("flat-"+dspec.element+"-down-normal"))
                {
                  dspec.element = "flat-"+dspec.element;
                }
                lspec.normalColor = col.name();
              }
              if (/*inPlasma ||*/ !paneledButtons.contains(widget))
              {
                lspec.focusColor = col.name();
                lspec.toggleColor = col.name();
                /* take care of Plasma menu titles */
                if (!qobject_cast<QMenu*>(p))
                  lspec.pressColor = col.name();
                else if (transMenuTitle)
                  lspec.pressColor = getLabelSpec("Menu").normalColor;
              }
            }
          }
          /* KDE menu titles */
          else if (qobject_cast<QMenu*>(p) && transMenuTitle)
            lspec.pressColor = getLabelSpec("Menu").normalColor;

          /* when there isn't enough space (as in Qupzilla's bookmark toolbar) */
          if (tialign != Qt::ToolButtonIconOnly)
          {
            if (!txt.isEmpty())
            {
              QSize txtSize = textSize(painter->font(), txt);
              if (tialign == Qt::ToolButtonTextBesideIcon || tialign == Qt::ToolButtonTextUnderIcon)
              {
                QSize availableSize = opt->rect.size() - opt->iconSize
                                      - QSize(fspec.left+fspec.right+lspec.left+lspec.right,
                                              fspec.top+fspec.bottom+lspec.top+lspec.bottom);
                if (tialign == Qt::ToolButtonTextBesideIcon)
                  availableSize -= QSize(lspec.tispace, 0);
                else
                  availableSize -= QSize(0, lspec.tispace);
                if (txtSize.height() > availableSize.height())
                {
                  lspec.top = lspec.bottom = 0;
                  fspec.top = fspec.bottom = 0;
                  if (tialign == Qt::ToolButtonTextUnderIcon)
                    lspec.tispace = 0;
                }
                if (txtSize.width() > availableSize.width())
                {
                  if (tialign == Qt::ToolButtonTextUnderIcon)
                  {
                    lspec.left = lspec.right = 0;
                    fspec.left = fspec.right = 0;
                  }
                  else if (txtSize.width() <= availableSize.width() + fspec.left + fspec.right
                                                                    + lspec.left + lspec.right + lspec.tispace)
                  {
                    lspec.left = lspec.right = lspec.tispace = 0;
                    fspec.left = fspec.right = 0;
                  }
                  else // If the text is beside the icon but doesn't fit in,...
                  { // ... elide it!
                    QFontMetrics fm(painter->font());
                    txt = fm.elidedText(txt, Qt::ElideRight, availableSize.width());
                  }
                }
              }
              else if (tialign == Qt::ToolButtonTextOnly)
              { // again, elide the text if it doesn't fit in
                int availableWidth = opt->rect.width() - fspec.left - fspec.right - lspec.left - lspec.right;
                if (txtSize.width() > availableWidth)
                {
                  QFontMetrics fm(painter->font());
                  txt = fm.elidedText(txt, Qt::ElideRight, availableWidth);
                }
              }
            }
          }
          /* lack of space (as in some of Krita's KisToolButtons) */
          else if (!opt->icon.isNull())
          {
            if (tb->popupMode() != QToolButton::MenuButtonPopup)
            {
              if ((tb->popupMode() == QToolButton::InstantPopup
                   || tb->popupMode() == QToolButton::DelayedPopup)
                  && (opt->features & QStyleOptionToolButton::HasMenu))
              {
                if (tb->width() < opt->iconSize.width()+fspec.left+fspec.right
                                  +dspec.size+ pixelMetric(PM_HeaderMargin)+lspec.tispace)
                {
                  if (opt->direction == Qt::RightToLeft)
                    fspec.right = qMin(fspec.right,3);
                  else
                    fspec.left = qMin(fspec.left,3);
                  dspec.size = qMin(dspec.size,TOOL_BUTTON_ARROW_SIZE-TOOL_BUTTON_ARROW_OVERLAP); // not needed
                  lspec.tispace=0; // not needed
                }
              }
              else
              {
                if (tb->width() < opt->iconSize.width()+fspec.left+fspec.right)
                {
                  fspec.left = qMin(fspec.left,3);
                  fspec.right = qMin(fspec.right,3);
                }
                if (tb->height() < opt->iconSize.height()+fspec.top+fspec.bottom)
                {
                  fspec.top = qMin(fspec.top,3);
                  fspec.bottom = qMin(fspec.bottom,3);
                }
              }
            }
            else
            {
              const frame_spec fspec1 = getFrameSpec("DropDownButton");
              if (tb->width() < opt->iconSize.width()+fspec.left
                                +(opt->direction == Qt::RightToLeft ? fspec1.left : fspec1.right)
                                +TOOL_BUTTON_ARROW_SIZE+2*TOOL_BUTTON_ARROW_MARGIN)
              {
                fspec.left = qMin(fspec.left,3);
                fspec.right = qMin(fspec.right,3);
              }
            }
          }
        }
        else // because of a mess in kate5/new KMultiTabBarTab
        {
          lspec.left = lspec.right = lspec.top = lspec.bottom = lspec.tispace = 0;
          fspec.left = fspec.right = fspec.top = fspec.bottom = 0;
          lspec.normalColor = opt->palette.color(QPalette::ButtonText).name();
        }

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
          fspec.left = fspec.right = fspec.top = fspec.bottom = 0;
          lspec.left = lspec.right = lspec.top = lspec.bottom = 0;
        }
        else
        {
          int state = 1;
          if (!(option->state & State_Enabled))
            state = 0;
          else if (status.startsWith("pressed"))
            state = 3;
          else if (status.startsWith("toggled"))
            state = 4;
          else if (status.startsWith("focused"))
            state = 2;
          QStyleOptionToolButton o(*opt);
          if ((option->state & State_MouseOver) && state != 2)
            o.state = o.state & ~QStyle::State_MouseOver; // hover bug
          renderLabel(&o,painter,
                      !(opt->features & QStyleOptionToolButton::Arrow)
                          || opt->arrowType == Qt::NoArrow
                          || tialign == Qt::ToolButtonTextOnly ?
                        r : // may still have arrow for a menu but that's dealt with at CC_ToolButton
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
                      talign,txt,QPalette::ButtonText,
                      state,
                      getPixmapFromIcon(opt->icon, getIconMode(state,lspec), iconstate, opt->iconSize),
                      opt->iconSize,tialign);
          iAlignment |= Qt::AlignLeft;
        }

        /* we treat arrows as icons */
        if (!(opt->features & QStyleOptionToolButton::Arrow) || tialign == Qt::ToolButtonTextOnly)
          break;

        if (status.startsWith("toggled")
            && (!themeRndr_ || !themeRndr_->isValid()
                || !themeRndr_->elementExists(dspec.element+"-down-toggled")))
        {
          /* distinguish between the toggled and pressed states
             only if a toggled down arrow element exists */
          status.replace("toggled","pressed");
        }
        if (!txt.isEmpty()) // it's empty for QStackedWidget
          r.adjust(lspec.left,lspec.top,-lspec.right,-lspec.bottom);
        switch (opt->arrowType) {
          case Qt::NoArrow :
            break;
          case Qt::UpArrow :
            renderIndicator(painter,r,
                            fspec,dspec,dspec.element+"-up-"+status,
                            option->direction,
                            iAlignment);
            break;
          case Qt::DownArrow :
            renderIndicator(painter,r,
                            fspec,dspec,dspec.element+"-down-"+status,
                            option->direction,
                            iAlignment);
            break;
          case Qt::LeftArrow :
            renderIndicator(painter,r,
                            fspec,dspec,dspec.element+"-left-"+status,
                            option->direction,
                            iAlignment);
            break;
          case Qt::RightArrow :
            renderIndicator(painter,r,
                            fspec,dspec,dspec.element+"-right-"+status,
                            option->direction,
                            iAlignment);
            break;
        }
      }

      break;
    }

    case CE_DockWidgetTitle : {
      const QStyleOptionDockWidget *opt =
          qstyleoption_cast<const QStyleOptionDockWidget*>(option);
      const QDockWidget *dw = qobject_cast<const QDockWidget*>(widget);

      if (opt) {
        const QString group = "DockTitle";

        frame_spec fspec = getFrameSpec(group);
        interior_spec ispec = getInteriorSpec(group);
        label_spec lspec = getLabelSpec(group);
        fspec.expansion = 0;

        QRect r = option->rect;
        QRect tRect =subElementRect(SE_DockWidgetTitleBarText, option, widget);
        bool hasVertTitle = false;
        if (dw && (dw->features() & QDockWidget::DockWidgetVerticalTitleBar))
          hasVertTitle = true;

        if (hasVertTitle)
        {
          r.setRect(0, 0, h, w);
          tRect.setRect(tRect.y(), tRect.x(),
                        tRect.height(), tRect.width());
          painter->save();
          QTransform m;          
          m.scale(1,-1);
          m.rotate(-90);
          painter->setTransform(m, true);
        }

        QString status = getState(option,widget);
        if (!(option->state & State_Enabled))
        {
          status.replace("disabled","normal");
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

        QString title = opt->title;
        if (!title.isEmpty())
        {
          QFont F(painter->font());
          if (lspec.boldFont) F.setBold(true);
          title = QFontMetrics(F).elidedText(title, Qt::ElideRight, tRect.width());
        }
        int talign = Qt::AlignHCenter | Qt::AlignVCenter;
        if (!styleHint(SH_UnderlineShortcut, opt, widget))
          talign |= Qt::TextHideMnemonic;
        else
          talign |= Qt::TextShowMnemonic;
        renderLabel(option,painter,
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
      if (const QStyleOptionFrame_v3 *f = qstyleoption_cast<const QStyleOptionFrame_v3*>(option))
      {
        /* skip ugly frames */
        if (f->frameShape != QFrame::HLine
            && f->frameShape != QFrame::VLine
            && (f->state & QStyle::State_Sunken || f->state & QStyle::State_Raised
                || (!widget // it's NULL in the case of a QML combobox
                    || widget->inherits("QComboBoxPrivateContainer"))))
        {
          if (f->frameShape == QFrame::Box)
          { // the default box frame is ugly too
            QColor col;
            if (f->state & QStyle::State_Sunken)
              col = f->palette.mid().color();
            else
              col = f->palette.midlight().color();
            if (!f->rect.isValid() || f->lineWidth == 0 || !col.isValid())
              break;
            painter->save();
            QRegion reg(f->rect);
            QRegion internalReg(f->rect.adjusted(f->lineWidth,f->lineWidth,-f->lineWidth,-f->lineWidth));
            painter->setClipRegion(reg.subtracted(internalReg));
            painter->fillRect(f->rect,col);
            painter->restore();
          }
          else
            QCommonStyle::drawControl(element,option,painter,widget);
        }
      }

      break;
    }

    case CE_Kv_KCapacityBar : {
      if (const QStyleOptionProgressBar *opt = qstyleoption_cast<const QStyleOptionProgressBar*>(option))
      {
        QStyleOptionProgressBar o(*opt);
        frame_spec fspec = getFrameSpec("Progressbar");
        fspec.left = fspec.right = qMin(fspec.left,fspec.right);
        drawControl(CE_ProgressBarGroove, &o, painter, widget);
        if (!tspec_.spread_progressbar)
          o.rect.adjust(fspec.left, fspec.top, -fspec.right, -fspec.bottom);
        drawControl(CE_ProgressBarContents, &o, painter, widget);
        drawControl(CE_ProgressBarLabel, &o, painter, widget);
      }

      break;
    }

    default : QCommonStyle::drawControl(element,option,painter,widget);
  }
}

void Style::drawComplexControl(ComplexControl control,
                               const QStyleOptionComplex *option,
                               QPainter *painter,
                               const QWidget *widget) const
{
  int x,y,h,w;
  option->rect.getRect(&x,&y,&w,&h);

  switch (control) {
    case CC_ToolButton : {
      const QStyleOptionToolButton *opt =
        qstyleoption_cast<const QStyleOptionToolButton*>(option);

      if (opt)
      {
        const QToolButton *tb = qobject_cast<const QToolButton*>(widget);
        if (tb && !standardButton.contains(widget))
        {
          standardButton.insert(widget);
          connect(widget, SIGNAL(destroyed(QObject*)), SLOT(removeFromSet(QObject*)), Qt::UniqueConnection);
        }
        const QString group = "PanelButtonTool";
        frame_spec fspec = getFrameSpec(group);
        QStyleOptionToolButton o(*opt);

        QRect r = subControlRect(CC_ToolButton,opt,SC_ToolButton,widget);
        o.rect = r;

        /* make an exception for (KDE) menu titles */
        if (hspec_.transparent_menutitle
            && tb && tb->isDown() && tb->toolButtonStyle() == Qt::ToolButtonTextBesideIcon
            && qobject_cast<QMenu*>(getParent(widget,1)))
        {
          drawControl(CE_ToolButtonLabel,&o,painter,widget);
          break;
        }

        /* to have a consistent look, integrate the drop-down part
           with the rest of the tool button if it's maximally rounded */
        if (fspec.expansion > 0 && tb && tb->popupMode() == QToolButton::MenuButtonPopup)
          o.rect = r.united(subControlRect(CC_ToolButton,opt,SC_ToolButtonMenu,widget));
        /* when SH_DockWidget_ButtonsHaveFrame is set to true (default), dock button panels
           are also drawn at PE_PanelButtonTool with all needed states (-> qdockwidget.cpp) */
        if (!widget || !widget->inherits("QDockWidgetTitleButton"))
          drawPrimitive(PE_PanelButtonTool,&o,painter,widget);
        //drawPrimitive(PE_FrameButtonTool,&o,painter,widget);
        o.rect = r;
        drawControl(CE_ToolButtonLabel,&o,painter,widget);

        if (tb)
        {
          o.rect = subControlRect(CC_ToolButton,opt,SC_ToolButtonMenu,widget);
          /* for a maximally rounded button, only the indicator
             will be drawn at PE_IndicatorButtonDropDown */
          if (tb->popupMode() == QToolButton::MenuButtonPopup)
            drawPrimitive(PE_IndicatorButtonDropDown,&o,painter,widget);
          else if ((tb->popupMode() == QToolButton::InstantPopup
                    || tb->popupMode() == QToolButton::DelayedPopup)
                   && (opt->features & QStyleOptionToolButton::HasMenu))
          {
            QWidget *p = getParent(widget,1);
            QWidget *gp = getParent(p,1);
            QString group1 = group;
            if (p && (isStylableToolbar(p) || isStylableToolbar(gp))
                && (!getFrameSpec("ToolbarButton").element.isEmpty()
                    || !getInteriorSpec("ToolbarButton").element.isEmpty()))
            {
              group1 = "ToolbarButton";
            }
            indicator_spec dspec = getIndicatorSpec(group1);
            const label_spec lspec = getLabelSpec(group1);

            QString aStatus = "normal";
            if (!(option->state & State_Enabled))
              aStatus = "disabled";
            else if ((option->state & State_On) || (option->state & State_Sunken) || (option->state & State_Selected))
              aStatus = "pressed";
            else if ((option->state & State_MouseOver)
                     && widget->rect().contains(widget->mapFromGlobal(QCursor::pos()))) // hover bug
              aStatus = "focused";

            /* use the "flat" indicator with flat buttons if it exists */
            if (aStatus == "normal"
                && tb->autoRaise()
                && themeRndr_ && themeRndr_->isValid()
                && themeRndr_->elementExists("flat-"+dspec.element+"-down-normal"))
            {
              QColor col = getFromRGBA(lspec.normalColor);
              if (!col.isValid())
                col = QApplication::palette().color(QPalette::ButtonText);
              QWidget* menubar = NULL;
              if (qobject_cast<QMenuBar*>(gp))
                menubar = gp;
              else if (qobject_cast<QMenuBar*>(p))
                menubar = p;
              if (menubar)
              {
                group1 = "MenuBar";
                if (mergedToolbarHeight(menubar))
                  group1 = "Toolbar";
                if (enoughContrast(col, getFromRGBA(getLabelSpec(group1).normalColor)))
                  dspec.element = "flat-"+dspec.element;
              }
              else if ((qobject_cast<QMainWindow*>(gp) && isStylableToolbar(p)
                        && !p->findChild<QTabBar*>())
                       || (qobject_cast<QMainWindow*>(getParent(gp,1)) && isStylableToolbar(gp)
                           && !gp->findChild<QTabBar*>()))
              {
                const QToolBar *toolBar = qobject_cast<QToolBar*>(p);
                if ((!tspec_.group_toolbar_buttons || (toolBar && toolBar->orientation() == Qt::Vertical))
                    && enoughContrast(col, getFromRGBA(getLabelSpec("Toolbar").normalColor)))
                {
                  dspec.element = "flat-"+dspec.element;
                }
              }
              else if (p && enoughContrast(col, p->palette().color(p->foregroundRole())))
                dspec.element = "flat-"+dspec.element;
            }
            fspec.right = fspec.left = 0;
            Qt::Alignment ialign = Qt::AlignLeft | Qt::AlignVCenter;
            // -> CE_ToolButtonLabel
            if (qobject_cast<QAbstractItemView*>(gp))
            {
              fspec.top = qMin(fspec.top,3);
              fspec.bottom = qMin(fspec.bottom,3);
            }
            /* lack of space */
            if (opt->toolButtonStyle == Qt::ToolButtonIconOnly && !opt->icon.isNull())
            {
              if (tb->width() < opt->iconSize.width()+fspec.left+fspec.right
                                +dspec.size+ pixelMetric(PM_HeaderMargin)+lspec.tispace)
              {
                dspec.size = qMin(dspec.size,TOOL_BUTTON_ARROW_SIZE);
                ialign = Qt::AlignRight | Qt::AlignBottom;
              }
            }
            if (!widget->isActiveWindow())
              aStatus.append("-inactive");
            renderIndicator(painter,
                            o.rect,
                            fspec,dspec,
                            dspec.element+"-down-"+aStatus,
                            option->direction,
                            ialign);
          }
        }

        if (opt->state & State_HasFocus)
        {
          QStyleOptionFocusRect fropt;
          fropt.QStyleOption::operator=(*opt);
          fropt.rect = interiorRect(opt->rect, fspec);
          drawPrimitive(PE_FrameFocusRect, &fropt, painter, widget);
        }
      }

      break;
    }

    case CC_SpinBox : {
      const QStyleOptionSpinBox *opt =
        qstyleoption_cast<const QStyleOptionSpinBox*>(option);

      if (opt) {
        QStyleOptionSpinBox o(*opt);
        /* If a null widget is fed into this method but the spinbox
           has a frame (QML), we'll draw buttons vertically. Fortunately,
           KisSliderSpinBox never fulfills this condition. */
        bool verticalIndicators(tspec_.vertical_spin_indicators || (!widget && opt->frame));
        QRect editRect = subControlRect(CC_SpinBox,opt,SC_SpinBoxEditField,widget);

        /* The field is automatically drawn as lineedit at PE_PanelLineEdit.
           So, we don't duplicate it here but there are some exceptions. */
        if (isLibreoffice_
            || (!widget && opt->frame && (opt->subControls & SC_SpinBoxFrame)))
        {
          o.rect = editRect;
          drawPrimitive(PE_PanelLineEdit,&o,painter,widget);
        }

        if ((verticalIndicators || tspec_.inline_spin_indicators)
            && opt->subControls & SC_SpinBoxUp)
        {
          const interior_spec ispec = getInteriorSpec("LineEdit");
          frame_spec fspec = getFrameSpec("LineEdit");
          fspec.hasCapsule = true;
          fspec.capsuleH = 1;
          fspec.capsuleV = 2;
          if (verticalIndicators)
          {
            fspec.left = fspec.right = fspec.top = fspec.bottom = qMin(fspec.left,3);
            fspec.expansion = 0;
          }
          QRect r = subControlRect(CC_SpinBox,opt,SC_SpinBoxUp,widget);
          r.setHeight(editRect.height());
          if (!verticalIndicators) // inline
          {
            r.setLeft(subControlRect(CC_SpinBox,opt,SC_SpinBoxDown,widget).left());

            // exactly as in PE_PanelLineEdit
            if (isLibreoffice_)
            {
              fspec.left = fspec.right = fspec.top = fspec.bottom = qMin(fspec.left,3);
              fspec.expansion = 0;
            }
            else if (QLineEdit *child = widget->findChild<QLineEdit*>())
            {
              label_spec lspec = getLabelSpec("LineEdit");
              lspec.top = qMax(0,lspec.top-1);
              lspec.bottom = qMax(0,lspec.bottom-1);
              const size_spec sspec = getSizeSpec("LineEdit");
              if (!child->styleSheet().isEmpty() && child->styleSheet().contains("padding"))
              {
                fspec.left = fspec.right = fspec.top = fspec.bottom = qMin(fspec.left,3);
              }
              else
              {
                if (child->minimumWidth() == child->maximumWidth())
                {
                  fspec.left = qMin(fspec.left,3);
                  fspec.right = qMin(fspec.right,3);
                }
                if (child->height() < sizeCalculated(child->font(),fspec,lspec,sspec,"W",QSize()).height())
                {
                  fspec.top = qMin(fspec.top,3);
                  fspec.bottom = qMin(fspec.bottom,3);
                }
              }
            }
            if (const QAbstractSpinBox *sb = qobject_cast<const QAbstractSpinBox*>(widget))
            {
              QString maxTxt = spinMaxText(sb);
              if (maxTxt.isEmpty()
                  || editRect.width() < textSize(sb->font(),maxTxt).width() + fspec.left
                                        + (sb->buttonSymbols() == QAbstractSpinBox::NoButtons
                                             ? fspec.right : 0)
                  || (sb->buttonSymbols() != QAbstractSpinBox::NoButtons
                      && sb->width() < editRect.width() + 2*tspec_.spin_button_width
                                                        + getFrameSpec("IndicatorSpinBox").right))
              {
                fspec.left = qMin(fspec.left,3);
                fspec.right = qMin(fspec.right,3);
              }
              if (sb->height() < fspec.top+fspec.bottom+QFontMetrics(widget->font()).height())
              {
                fspec.top = qMin(fspec.top,3);
                fspec.bottom = qMin(fspec.bottom,3);
              }
            }
          }
          QString leStatus;
          if (isKisSlider_) leStatus = "normal";
          else leStatus = (option->state & State_HasFocus) ? "focused" : "normal";
          if (widget && !widget->isActiveWindow())
            leStatus .append("-inactive");
          if (!(option->state & State_Enabled))
          {
            painter->save();
            painter->setOpacity(DISABLED_OPACITY);
          }
          bool animate(widget && widget->isEnabled()
                       && !qobject_cast<const QAbstractScrollArea*>(widget)
                       && ((animatedWidget_ == widget && !leStatus.startsWith("normal"))
                           || (animatedWidgetOut_ == widget && leStatus.startsWith("normal"))));

          QString animationStartState(animationStartState_);
          int animationOpacity = animationOpacity_;
          if (animate)
          {
            if (leStatus.startsWith("normal")) // -> QEvent::FocusOut
            {
              animationStartState = animationStartStateOut_;
              animationOpacity = animationOpacityOut_;
            }
            if (animationStartState == leStatus)
            {
              animationOpacity = 100;
              if (leStatus.startsWith("normal"))
                animationOpacityOut_ = 100;
              else
                animationOpacity_ = 100;
            }
            else if (animationOpacity < 100)
            {
              renderFrame(painter,r,fspec,fspec.element+"-"+animationStartState);
              renderInterior(painter,r,fspec,ispec,ispec.element+"-"+animationStartState);
            }
            painter->save();
            painter->setOpacity((qreal)animationOpacity/100);
          }
          renderFrame(painter,r,fspec,fspec.element+"-"+leStatus);
          renderInterior(painter,r,fspec,ispec,ispec.element+"-"+leStatus);
          if (animate)
          {
            painter->restore();
            if (animationOpacity >= 100)
            {
              if (leStatus.startsWith("normal"))
                animationStartStateOut_ = leStatus;
              else
                animationStartState_ = leStatus;
            }
          }
          if (!(option->state & State_Enabled))
            painter->restore();
        }

        if (opt->subControls & SC_SpinBoxUp)
        {
          o.rect = subControlRect(CC_SpinBox,opt,SC_SpinBoxUp,widget);
          if (opt->buttonSymbols == QAbstractSpinBox::UpDownArrows)
            drawPrimitive(PE_IndicatorSpinUp,&o,painter,widget);
          else if (opt->buttonSymbols == QAbstractSpinBox::PlusMinus)
            drawPrimitive(PE_IndicatorSpinPlus,&o,painter,widget);
        }
        if (opt->subControls & SC_SpinBoxDown)
        {
          o.rect = subControlRect(CC_SpinBox,opt,SC_SpinBoxDown,widget);
          if (opt->buttonSymbols == QAbstractSpinBox::UpDownArrows)
            drawPrimitive(PE_IndicatorSpinDown,&o,painter,widget);
          else if (opt->buttonSymbols == QAbstractSpinBox::PlusMinus)
            drawPrimitive(PE_IndicatorSpinMinus,&o,painter,widget);
        }
      }

      break;
    }

    case CC_ComboBox : {
      /* WARNING: QML comboboxes have lineedit even when they aren't editable.
         Hence, the existence of a lineedit isn't a sufficient but only a necessary
         condition for editability. */
      const QStyleOptionComboBox *opt =
          qstyleoption_cast<const QStyleOptionComboBox*>(option);

      if (opt) {
        QStyleOptionComboBox o(*opt);
        QRect arrowRect = subControlRect(CC_ComboBox,opt,SC_ComboBoxArrow,widget);
        const QComboBox *cb = qobject_cast<const QComboBox*>(widget);
        bool rtl(opt->direction == Qt::RightToLeft);
        bool editable(opt->editable && cb && cb->lineEdit());
        const QString group = "ComboBox";

        label_spec lspec = getLabelSpec(group);
        lspec.left = qMax(0,lspec.left-1);
        lspec.top = qMax(0,lspec.top-1);
        lspec.right = qMax(0,lspec.right-1);
        lspec.bottom = qMax(0,lspec.bottom-1);
        frame_spec fspec = getFrameSpec(group);
        interior_spec ispec = getInteriorSpec(group);
        if (!widget) // WARNING: QML has anchoring!
        {
          fspec.expansion = 0;
          ispec.px = ispec.py = 0;
        }
        if (editable) // otherwise the arrow part will be integrated
        {
          if (tspec_.combo_as_lineedit)
          {
            fspec = getFrameSpec("LineEdit");
            ispec = getInteriorSpec("LineEdit");
          }
          fspec.hasCapsule = true;
          fspec.capsuleH = rtl ? 1 : -1;
          fspec.capsuleV = 2;
        }

        int extra = 0;
        if (editable)
        {
          QLineEdit *le = cb->lineEdit();
          /* Konqueror may add an icon to the right of lineedit (for LTR) */
          extra  = rtl ? le->x() - (COMBO_ARROW_LENGTH+fspec.left)
                       : w - (COMBO_ARROW_LENGTH+fspec.right) - (le->x()+le->width());
          if (extra > 0)
          {
            if (rtl) arrowRect.adjust(0,0,extra,0);
            else arrowRect.adjust(-extra,0,0,0);
          }
        }

        if (opt->subControls & SC_ComboBoxFrame) // frame
        {
          QString status =
                   (option->state & State_Enabled) ?
                    (option->state & State_On) ? "toggled" :
                    (option->state & State_MouseOver)
                      && (!widget || widget->rect().contains(widget->mapFromGlobal(QCursor::pos()))) // hover bug
                    ? "focused" :
                    (option->state & State_Sunken)
                    // to know it has focus
                    || (option->state & State_Selected) ? "pressed" : "normal"
                   : "disabled";
          if (widget && !widget->isActiveWindow())
            status.append("-inactive");

          int margin = 0; // see CC_ComboBox at subControlRect
          if (opt->editable && !opt->currentIcon.isNull())
            margin = (rtl ? fspec.right+lspec.right : fspec.left+lspec.left) + lspec.tispace
                      - (tspec_.combo_as_lineedit ? 0
                         : 3); // it's 4px in qcombobox.cpp -> QComboBoxPrivate::updateLineEditGeometry()
          else if (isLibreoffice_)
            margin = fspec.left;
          // SC_ComboBoxEditField includes the icon too
          o.rect = subControlRect(CC_ComboBox,opt,SC_ComboBoxEditField,widget)
                   .adjusted(rtl ? 0 : -margin,
                             0,
                             rtl ? margin : 0,
                             0);

          if (!(option->state & State_Enabled))
          {
            status.replace("disabled","normal");
            painter->save();
            painter->setOpacity(DISABLED_OPACITY);
          }
          if (isLibreoffice_ && opt->editable)
          {
            painter->fillRect(o.rect, option->palette.brush(QPalette::Base));
            const frame_spec fspec1 = getFrameSpec("LineEdit");
            renderFrame(painter,o.rect,fspec,fspec1.element+"-normal");
          }
          else // ignore framelessness
          {
            /* don't cover the lineedit area */
            int editWidth = 0;
            if (cb)
            {
              if (editable)
              {
                if (!tspec_.combo_as_lineedit) // otherwise, the frame and edit field are drawn together as a lineedit
                  editWidth = cb->lineEdit()->width();
                if (extra > 0)
                  editWidth += extra;
                if (cb->hasFocus())
                {
                  if (tspec_.combo_as_lineedit)
                  {
                    if (!widget->isActiveWindow()) status = "focused-inactive"; // impossible
                    else status = "focused";
                  }
                  else
                  {
                    if (!widget->isActiveWindow()) status = "pressed-inactive";
                    else status = "pressed";
                  }
                }
                else if (tspec_.combo_as_lineedit)
                {
                  if (status.startsWith("focused"))
                    status.replace("focused","normal");
                  else if (status.startsWith("toggled"))
                    status.replace("toggled","normal");
                }
              }
              else // when there isn't enough space
              {
                QSize txtSize = textSize(painter->font(),opt->currentText);
                if (cb->width() < fspec.left+lspec.left+txtSize.width()+lspec.right+COMBO_ARROW_LENGTH+fspec.right
                    || cb->height() < fspec.top+lspec.top+txtSize.height()+fspec.bottom+lspec.bottom)
                {
                  fspec.left = qMin(fspec.left,3);
                  fspec.right = qMin(fspec.right,3);
                  fspec.top = qMin(fspec.top,3);
                  fspec.bottom = qMin(fspec.bottom,3);
                  //fspec.expansion = 0;

                  lspec.left = qMin(lspec.left,2);
                  lspec.right = qMin(lspec.right,2);
                  lspec.top = qMin(lspec.top,2);
                  lspec.bottom = qMin(lspec.bottom,2);
                  lspec.tispace = qMin(lspec.tispace,2);
                }
              }
            }
            QRect r = o.rect.adjusted(rtl ? editWidth : 0, 0, rtl ? 0 : -editWidth, 0);
            /* integrate the arrow part if the combo isn't editable */
            if (!opt->editable || (cb && !cb->lineEdit())) r = r.united(arrowRect);
            bool libreoffice = false;
            if (isLibreoffice_ && (option->state & State_Enabled))
            {
              if (enoughContrast(getFromRGBA(lspec.normalColor), QApplication::palette().color(QPalette::ButtonText)))
              {
                libreoffice = true;
                painter->save();
                painter->setOpacity(0.5);
              }
            }
            if (!editable
                // nothing should be drawn here if the lineedit is transparent (as in Cantata)
                || cb->lineEdit()->palette().color(cb->lineEdit()->backgroundRole()).alpha() != 0)
            {
              QStyleOptionComboBox leOpt(*opt);
              if (!tspec_.combo_as_lineedit && editable)
              {
                leOpt.rect = o.rect.adjusted(rtl ? 0 : o.rect.width()-editWidth, 0, 0,
                                             rtl ? editWidth-o.rect.width() : 0);
              }
              bool mouseAnimation(animatedWidget_ == widget
                                  && (!status.startsWith("normal")
                                      || ((!editable || !tspec_.combo_as_lineedit
                                           || (cb->view() && cb->view()->isVisible()))
                                          && animationStartState_.startsWith("focused"))));
              bool animate(cb && cb->isEnabled()
                           && (mouseAnimation
                               || (animatedWidgetOut_ == widget && status.startsWith("normal"))));
              QString animationStartState(animationStartState_);
              int animationOpacity = animationOpacity_;
              if (animate)
              {
                if (!mouseAnimation) // -> QEvent::FocusOut
                {
                  animationStartState = animationStartStateOut_;
                  animationOpacity = animationOpacityOut_;
                }
                if (animationStartState == status)
                {
                  animationOpacity = 100;
                  if (!mouseAnimation)
                    animationOpacityOut_ = 100;
                  else
                    animationOpacity_ = 100;
                }
                else if (animationOpacity < 100)
                {
                  renderFrame(painter,r,fspec,fspec.element+"-"+animationStartState);
                  renderInterior(painter,r,fspec,ispec,ispec.element+"-"+animationStartState);
                  if (!tspec_.combo_as_lineedit && editable)
                  {
                    if (!mouseAnimation)
                      leOpt.state = State_Enabled | State_Active | State_HasFocus;
                    else
                      leOpt.state = opt->state & (State_Enabled);
                    drawComboLineEdit(&leOpt, painter, cb->lineEdit(), widget);
                  }
                }
                painter->save();
                painter->setOpacity((qreal)animationOpacity/100);
              }
              renderFrame(painter,r,fspec,fspec.element+"-"+status);
              renderInterior(painter,r,fspec,ispec,ispec.element+"-"+status);
              if (!tspec_.combo_as_lineedit && editable)
              {
                leOpt.state = (opt->state & (State_Enabled | State_MouseOver | State_HasFocus))
                              | State_KeyboardFocusChange;
                drawComboLineEdit(&leOpt, painter, cb->lineEdit(), widget);
              }
              if (animate)
              {
                painter->restore();
                if (animationOpacity >= 100)
                {
                  if (!mouseAnimation)
                  {
                    animationStartStateOut_ = status;
                    if (!editable && animatedWidget_ == widget)
                      animationStartState_ = status;
                  }
                  else
                    animationStartState_ = status;
                }
              }
            }
            if (libreoffice) painter->restore();
            /* draw focus rect */
            if (!editable
                && (option->state & State_Enabled) && !(option->state & State_On)
                && ((option->state & State_Sunken) || (option->state & State_Selected)))
            {
              QStyleOptionFocusRect fropt;
              fropt.QStyleOption::operator=(*opt);
              fropt.rect = interiorRect(r, fspec);
              drawPrimitive(PE_FrameFocusRect, &fropt, painter, widget);
            }
            /* force label color (as in Krusader) */
            if (cb && (option->state & State_Enabled))
            {
              QList<QLabel*> llist = cb->findChildren<QLabel*>();
              if (!llist.isEmpty())
              {
                QColor col;
                col = getFromRGBA(lspec.normalColor);
                if (status.startsWith("pressed"))
                  col = getFromRGBA(lspec.pressColor);
                else if (status.startsWith("toggled"))
                  col = getFromRGBA(lspec.toggleColor);
                else if (option->state & State_MouseOver)
                  col = getFromRGBA(lspec.focusColor);
                if (col.isValid())
                {
                  for (int i = 0; i < llist.count(); ++i)
                  {
                    QPalette palette = llist.at(i)->palette();
                    if (col != palette.color(QPalette::WindowText))
                    {
                      palette.setColor(QPalette::Active,QPalette::WindowText,col);
                      palette.setColor(QPalette::Inactive,QPalette::WindowText,col);
                      llist.at(i)->setPalette(palette);
                    }
                  }
                }
              }
            }
          }
          if (!(option->state & State_Enabled))
            painter->restore();

          /* since the icon of an editable combo-box isn't drawn
             at CE_ComboBoxLabel, we draw and center it here */
          if (opt->editable && !opt->currentIcon.isNull())
          {
            const QIcon::State iconstate =
              (option->state & State_On) ? QIcon::On : QIcon::Off;

            int state = 1;
            if (!(option->state & State_Enabled))
              state = 0;
            else if (status.startsWith("pressed"))
              state = 3;
            else if (status.startsWith("toggled"))
              state = 4;
            else if (status.startsWith("focused"))
              state = 2;

            /*fspec.top = fspec.bottom = 0;
              lspec.top = lspec.bottom = 0;*/
            QPixmap icn = getPixmapFromIcon(opt->currentIcon, getIconMode(state,lspec), iconstate, opt->iconSize);
            QRect ricn = alignedRect(option->direction,
                                     Qt::AlignVCenter | Qt::AlignLeft,
                                     opt->iconSize,
                                     labelRect(option->rect,fspec,lspec));
            QRect iconRect = alignedRect(option->direction,
                                         Qt::AlignCenter,
                                         QSize(icn.width(),icn.height())/pixelRatio_, ricn);
            if (!(option->state & State_Enabled))
            {
              qreal opacityPercentage = hspec_.disabled_icon_opacity;
              if (opacityPercentage < 100)
                icn = translucentPixmap(icn, opacityPercentage);
            }
            else if (option->state & State_MouseOver)
            {
              qreal tintPercentage = hspec_.tint_on_mouseover;
              if (tintPercentage > 0)
                icn = tintedPixmap(option, icn, tintPercentage);
            }
            painter->drawPixmap(iconRect,icn);
          }
        } // end of frame

        if (opt->subControls & SC_ComboBoxArrow) // arrow
        {
          o.rect = arrowRect;
          drawPrimitive(PE_IndicatorButtonDropDown,&o,painter,widget);
        }
      }

      break;
    }

    case CC_ScrollBar : {
      const QStyleOptionSlider *opt =
        qstyleoption_cast<const QStyleOptionSlider*>(option);

      if (opt) {
        QStyleOptionSlider o(*opt);

        o.rect = subControlRect(CC_ScrollBar,opt,SC_ScrollBarGroove,widget);
        QRect r = o.rect;
        bool horiz = (option->state & State_Horizontal);
        /* arrows may be forced by another style, as in Gwenview
          (-> CC_ScrollBar at drawComplexControl) */
        int extent = pixelMetric(PM_ScrollBarExtent,option,widget);
        int arrowSize = 0;
        if (!tspec_.scroll_arrows && (horiz ? r.width() == w-2*extent : r.height() == h-2*extent))
          arrowSize = extent;

        /*******************
          Grrove and Slider
        ********************/
        if (opt->subControls & SC_ScrollBarSlider)
        {
          const QString group = "ScrollbarGroove";
          frame_spec fspec = getFrameSpec(group);
          fspec.expansion = 0; // no need to frame expansion because the thickness is known
          const interior_spec ispec = getInteriorSpec(group);

          if (horiz)
          {
            r.setRect(r.y(), r.x(), r.height(), r.width());
            painter->save();
            QTransform m;
            m.scale(1,-1);
            m.rotate(-90);
            painter->setTransform(m, true);
          }

          if (!(option->state & State_Enabled))
          {
            painter->save();
            painter->setOpacity(DISABLED_OPACITY);
          }

          QString suffix = "-normal";
          if (widget && !widget->isActiveWindow())
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
          if (option->state & State_Enabled)
          {
            const frame_spec sFspec = getFrameSpec("ScrollbarSlider");
            int glowH = 2*extent;
            int topGlowY, bottomGlowY, topGlowH, bottomGlowH;
            if (horiz)
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
            bottomGlowH = glowH+sFspec.bottom
                          - qMax(bottomGlowY+glowH+sFspec.bottom - (r.y()+r.height()-fspec.bottom), 0);
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

          if (opt->state & State_HasFocus)
          {
            QStyleOptionFocusRect fropt;
            fropt.QStyleOption::operator=(*opt);
            fropt.rect = r;
            drawPrimitive(PE_FrameFocusRect, &fropt, painter, widget);
          }

          if (horiz)
            painter->restore();
        }
        /***********
          Sub-Line
        ************/
        if (opt->subControls & SC_ScrollBarSubLine)
        {
          if (arrowSize > 0)
          {
            if (horiz)
              o.rect = QRect(option->direction == Qt::RightToLeft ? x : x+w-arrowSize, y,
                             arrowSize, arrowSize);
            else
              o.rect = QRect(x, y+h-arrowSize, arrowSize, arrowSize);
          }
          else
            o.rect = subControlRect(CC_ScrollBar,opt,SC_ScrollBarAddLine,widget);
          drawControl(CE_ScrollBarAddLine,&o,painter,widget);
        }
        /***********
          Add-Line
        ************/
        if (opt->subControls & SC_ScrollBarSubLine)
        {
          if (arrowSize > 0)
          {
            if (horiz)
              o.rect = QRect(option->direction == Qt::RightToLeft ? x+w-arrowSize : x, y,
                             arrowSize, arrowSize);
            else
              o.rect = QRect(x, y, arrowSize, arrowSize);
          }
          else
            o.rect = subControlRect(CC_ScrollBar,opt,SC_ScrollBarSubLine,widget);
          drawControl(CE_ScrollBarSubLine,&o,painter,widget);
        }
      }

      break;
    }

    case CC_Slider : {
      const QStyleOptionSlider *opt =
        qstyleoption_cast<const QStyleOptionSlider*>(option);

      if (opt)
      {
        QString group = "Slider";
        frame_spec fspec = getFrameSpec(group);
        interior_spec ispec = getInteriorSpec(group);
        fspec.expansion = 0;

        bool horiz = opt->orientation == Qt::Horizontal; // this is more reliable than option->state
        int ticks = opt->tickPosition;
        const int len = pixelMetric(PM_SliderLength,option,widget);
        const int thick = pixelMetric(PM_SliderControlThickness,option,widget);

       /************
        ** Groove **
        ************/
        if (opt->subControls & SC_SliderGroove) // QtColorPicker doesn't need the groove
        {
          /* find the groove rect, taking into accout slider_width */
          QRect grooveRect = subControlRect(CC_Slider,opt,SC_SliderGroove,widget);
          const int grooveThickness = qMin(tspec_.slider_width,thick);
          int delta;
          if (horiz)
          {
            delta = (grooveRect.height()-grooveThickness)/2;
            grooveRect.adjust(0,delta,0,-delta);
          }
          else
          {
            delta = (grooveRect.width()-grooveThickness)/2;
            grooveRect.adjust(delta,0,-delta,0);
          }

          QRect empty = grooveRect;
          QRect full = grooveRect;
          QRect slider = subControlRect(CC_Slider,opt,SC_SliderHandle,widget);
          QPoint sliderCenter = slider.center();

          /* take into account the inversion */
          if (horiz)
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
          if (horiz)
          {
            int H = empty.height();
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
            painter->save();
            QTransform m;
            m.scale(1,-1);
            m.rotate(-90);
            painter->setTransform(m, true);
          }

          /* now draw the groove */
          QString suffix = "-normal";
          if (widget && !widget->isActiveWindow())
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
            suffix.replace("normal","toggled");
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

          if (opt && opt->state & State_HasFocus)
          {
            QStyleOptionFocusRect fropt;
            fropt.QStyleOption::operator=(*opt);
            QRect FR = opt->rect;
            if (horiz)
              FR.setRect(0, 0, h, w);
            fropt.rect = FR;
            drawPrimitive(PE_FrameFocusRect, &fropt, painter, widget);
          }

          if (horiz)
            painter->restore();
        }

       /***************
        ** Tickmarks **
        ***************/
        if (opt->subControls & SC_SliderTickmarks)
        {
          /* slider ticks */
          QRect r = option->rect;
          if (horiz)
          {
            r.setRect(y, x, h, w);
            painter->save();
            QTransform m;
            m.scale(1,-1);
            m.rotate(-90);
            painter->setTransform(m, true);
          }
          if (!(option->state & State_Enabled))
          {
            painter->save();
            painter->setOpacity(0.4);
          }
          QString suffix = "-normal";
          if (widget && !widget->isActiveWindow())
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
          if (ticks & QSlider::TicksAbove)
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
          if (ticks & QSlider::TicksBelow)
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
          if (horiz)
            painter->restore();
        }

       /************
        ** Handle **
        ************/
        if (opt->subControls & SC_SliderHandle) // I haven't seen a slider without handle
        {
          group = "SliderCursor";
          fspec = getFrameSpec(group);
          ispec = getInteriorSpec(group);
          fspec.expansion = 0;

          QRect r = subControlRect(CC_Slider,opt,SC_SliderHandle,widget);
          /* workaround for bad hard-coded styling (as in Sayonara) */
          QRect R = option->rect;
          if (horiz)
          {
            if (r.y() < R.y())
              r.moveTop(R.y());
            if (r.bottom() > R.bottom())
            {
              r.setHeight(R.height() - r.y());
              fspec.left = qMin(fspec.left,3);
              fspec.right = qMin(fspec.right,3);
              fspec.top = qMin(fspec.top,3);
              fspec.bottom = qMin(fspec.bottom,3);
            }
          }
          else
          {
            if (r.x() < R.x())
              r.moveLeft(R.x());
            if (r.right() > R.right())
            {
              r.setWidth(R.width() - r.x());
              fspec.left = qMin(fspec.left,3);
              fspec.right = qMin(fspec.right,3);
              fspec.top = qMin(fspec.top,3);
              fspec.bottom = qMin(fspec.bottom,3);
            }
          }
          /* derive other handles from the main one only when necessary */
          bool derive = false;
          if (len != thick)
          {
            if (horiz)
            {
              derive = true;
              int sY = r.y();
              int sH = r.height();
              r.setRect(sY, r.x(), sH, r.width());
              painter->save();
              QTransform m;
              if (ticks == QSlider::TicksAbove)
              {
                m.translate(0, 2*sY+sH);
                m.scale(1,-1);
              }
              m.scale(1,-1);
              m.rotate(-90);
              painter->setTransform(m, true);
            }
            else if (ticks == QSlider::TicksAbove)
            {
              derive = true;
              painter->save();
              QTransform m;
              m.translate(2*r.x()+r.width(), 0);
              m.scale(-1,1);
              painter->setTransform(m, true);
            }
          }

          QString status = getState(option,widget);
          bool animate(widget && widget->isEnabled() && animatedWidget_ == widget
                       && !qobject_cast<const QAbstractScrollArea*>(widget));
          if (animate)
          {
            if (animationStartState_ == status)
              animationOpacity_ = 100;
            else if (animationOpacity_ < 100)
            {
              renderFrame(painter,r,fspec,fspec.element+"-"+animationStartState_);
              renderInterior(painter,r,fspec,ispec,ispec.element+"-"+animationStartState_);
            }
            painter->save();
            painter->setOpacity((qreal)animationOpacity_/100);
          }
          renderFrame(painter,r,fspec,fspec.element+"-"+status);
          renderInterior(painter,r,fspec,ispec,ispec.element+"-"+status);
          if (animate)
          {
            painter->restore();
            if (animationOpacity_ >= 100)
              animationStartState_ = status;
          }

          // a decorative indicator if its element exists
          const indicator_spec dspec = getIndicatorSpec(group);
          renderIndicator(painter,r,fspec,dspec,dspec.element+"-"+status,option->direction);

          if (derive)
            painter->restore();
        }
      }

      break;
    }

    case CC_Dial : {
      const QStyleOptionSlider *opt =
          qstyleoption_cast<const QStyleOptionSlider*>(option);

      if (opt)
      {
        QRect dial(subControlRect(CC_Dial,opt,SC_DialGroove,widget));
        QRect handle(subControlRect(CC_Dial,opt,SC_DialHandle,widget));

        QString suffix;
        if (widget && !widget->isActiveWindow())
          suffix = "-inactive";

        renderElement(painter,"dial"+suffix,dial);
        renderElement(painter,"dial-handle"+suffix,handle);
        
        if (const QDial *d = qobject_cast<const QDial*>(widget))
        {
          if (d->notchesVisible())
            renderElement(painter,"dial-notches"+suffix,dial);
        }

        if (opt->state & State_HasFocus)
        {
          QStyleOptionFocusRect fropt;
          fropt.QStyleOption::operator=(*opt);
          fropt.rect = dial;
          drawPrimitive(PE_FrameFocusRect, &fropt, painter, widget);
        }
      }

      break;
    }

    case CC_TitleBar : {
      const QStyleOptionTitleBar *opt =
        qstyleoption_cast<const QStyleOptionTitleBar*>(option);

      if (opt) {
        int ts = opt->titleBarState;
        const QString tbStatus =
              (ts & Qt::WindowActive) ? "focused" : "normal";

        const QString group = "TitleBar";
        frame_spec fspec;
        default_frame_spec(fspec);
        const interior_spec ispec = getInteriorSpec(group);

        if (opt->subControls & SC_TitleBarLabel)
        {
          const label_spec lspec = getLabelSpec(group);
          QStyleOptionTitleBar o(*opt);
          // SH_TitleBar_NoBorder is set to be true
          //QString status = getState(option,widget);
          //renderFrame(painter,o.rect,fspec,fspec.element+"-"+status);
          renderInterior(painter,o.rect,fspec,ispec,ispec.element+"-"+tbStatus);

          o.rect = subControlRect(CC_TitleBar,opt,SC_TitleBarLabel,widget);
          QString title = o.text;
          if (!title.isEmpty())
          {
            QFont F(painter->font());
            if (lspec.boldFont) F.setBold(true);
            QFontMetrics fm(F);
            title = fm.elidedText(title, Qt::ElideRight,
                                  o.rect.width()-(pixelMetric(PM_TitleBarHeight)-4+lspec.tispace)
                                                // titlebars have no frame
                                                -lspec.right-lspec.left);
          }
          int icnSize = pixelMetric(PM_TitleBarHeight) - 4; // 2-px margins for the icon
          QSize iconSize = QSize(icnSize,icnSize);
          renderLabel(option,painter,
                      o.rect,
                      fspec,lspec,
                      Qt::AlignCenter,title,QPalette::WindowText,
                      tbStatus == "normal" ? 1 : 2,
                      getPixmapFromIcon(o.icon,QIcon::Normal,QIcon::Off,iconSize),
                      iconSize);
        }

        indicator_spec dspec = getIndicatorSpec(group);
        Qt::WindowFlags tf = opt->titleBarFlags;

        if ((opt->subControls & SC_TitleBarCloseButton) && (tf & Qt::WindowSystemMenuHint))
          renderIndicator(painter,
                          subControlRect(CC_TitleBar,opt,SC_TitleBarCloseButton,widget),
                          fspec,dspec,
                          dspec.element+"-close-"
                            + ((opt->activeSubControls & QStyle::SC_TitleBarCloseButton) ?
                                (option->state & State_Sunken) ? "pressed" : "focused"
                                  : tbStatus == "focused" ? "normal" : "disabled"),
                          option->direction);
        if ((opt->subControls & SC_TitleBarMaxButton) && (tf & Qt::WindowMaximizeButtonHint)
            && !(ts & Qt::WindowMaximized))
          renderIndicator(painter,
                          subControlRect(CC_TitleBar,opt,SC_TitleBarMaxButton,widget),
                          fspec,dspec,
                          dspec.element+"-maximize-"
                            + ((opt->activeSubControls & QStyle::SC_TitleBarMaxButton) ?
                                (option->state & State_Sunken) ? "pressed" : "focused"
                                  : tbStatus == "focused" ? "normal" : "disabled"),
                          option->direction);
        if ((opt->subControls & SC_TitleBarMinButton) && (tf & Qt::WindowMinimizeButtonHint)
            && !(ts & Qt::WindowMinimized))
          renderIndicator(painter,
                          subControlRect(CC_TitleBar,opt,SC_TitleBarMinButton,widget),
                          fspec,dspec,
                          dspec.element+"-minimize-"
                            + ((opt->activeSubControls & QStyle::SC_TitleBarMinButton) ?
                                (option->state & State_Sunken) ? "pressed" : "focused"
                                  : tbStatus == "focused" ? "normal" : "disabled"),
                          option->direction);
        if ((opt->subControls & SC_TitleBarNormalButton)
            && (((tf & Qt::WindowMinimizeButtonHint) && (ts & Qt::WindowMinimized))
                || ((tf & Qt::WindowMaximizeButtonHint) && (ts & Qt::WindowMaximized))))
          renderIndicator(painter,
                          subControlRect(CC_TitleBar,opt,SC_TitleBarNormalButton,widget),
                          fspec,dspec,
                          dspec.element+"-restore-"
                            + ((opt->activeSubControls & QStyle::SC_TitleBarNormalButton) ?
                                (option->state & State_Sunken) ? "pressed" : "focused"
                                  : tbStatus == "focused" ? "normal" : "disabled"),
                          option->direction);
        if ((opt->subControls & SC_TitleBarShadeButton) && (tf & Qt::WindowShadeButtonHint)
            && !(ts & Qt::WindowMinimized))
          renderIndicator(painter,
                          subControlRect(CC_TitleBar,opt,SC_TitleBarShadeButton,widget),
                          fspec,dspec,
                          dspec.element+"-shade-"
                            + ((opt->activeSubControls & QStyle::SC_TitleBarShadeButton) ?
                                (option->state & State_Sunken) ? "pressed" : "focused"
                                  : tbStatus == "focused" ? "normal" : "disabled"),
                          option->direction);
        if ((opt->subControls & SC_TitleBarUnshadeButton) && (tf & Qt::WindowShadeButtonHint)
            && (ts & Qt::WindowMinimized))
          renderIndicator(painter,
                          subControlRect(CC_TitleBar,opt,SC_TitleBarUnshadeButton,widget),
                          fspec,dspec,
                          dspec.element+"-restore-"
                            + ((opt->activeSubControls & QStyle::SC_TitleBarUnshadeButton) ?
                                (option->state & State_Sunken) ? "pressed" : "focused"
                                  : tbStatus == "focused" ? "normal" : "disabled"),
                          option->direction);
        if ((opt->subControls & SC_TitleBarContextHelpButton)&& (ts & Qt::WindowContextHelpButtonHint))
          break;
        /* FIXME Why is SP_TitleBarMenuButton used here? */
        if ((opt->subControls & SC_TitleBarSysMenu) && (tf & Qt::WindowSystemMenuHint))
        {
          /*if (!opt->icon.isNull())
            opt->icon.paint(painter,subControlRect(CC_TitleBar,opt,SC_TitleBarSysMenu,widget));
          else
            renderIndicator(painter,
                            subControlRect(CC_TitleBar,opt,SC_TitleBarSysMenu,widget),
                            fspec,dspec,
                            dspec.element+"-menu-normal",option->direction);*/
          break;
        }
      }

      break;
    }

    case CC_MdiControls: { // on menubar
      QStyleOptionButton btnOpt;
      btnOpt.QStyleOption::operator=(*option);
      btnOpt.state &= ~State_MouseOver;
      const QIcon::Mode iconmode =
        (option->state & State_Enabled) ?
        (option->state & State_Sunken) ? QIcon::Active :
        (option->state & State_MouseOver) ? QIcon::Active : QIcon::Normal
        : QIcon::Disabled;
      const QIcon::State iconstate =
        (option->state & State_On) ? QIcon::On : QIcon::Off;
      if (option->subControls & QStyle::SC_MdiCloseButton)
      {
        if (option->activeSubControls & QStyle::SC_MdiCloseButton)
        {
          if (option->state & State_Sunken)
          {
            btnOpt.state |= State_Sunken;
            btnOpt.state &= ~State_Raised;
          }
          else
          {
            btnOpt.state |= State_MouseOver;
            btnOpt.state &= ~State_Raised;
            btnOpt.state &= ~State_Sunken;
          }
        }
        else
        {
          btnOpt.state |= State_Raised;
          btnOpt.state &= ~State_Sunken;
          btnOpt.state &= ~State_MouseOver;
        }
        btnOpt.rect = subControlRect(CC_MdiControls, option, SC_MdiCloseButton, widget);
        //drawPrimitive(PE_PanelButtonCommand, &btnOpt, painter, widget);
        QPixmap pm = getPixmapFromIcon(standardIcon(SP_TitleBarCloseButton,&btnOpt,widget),
                                       iconmode,iconstate,QSize(16,16));
        QRect iconRect = alignedRect(option->direction, Qt::AlignCenter, pm.size()/pixelRatio_, btnOpt.rect);
        painter->drawPixmap(iconRect,pm);
      }
      if (option->subControls & QStyle::SC_MdiNormalButton)
      {
        if (option->activeSubControls & QStyle::SC_MdiNormalButton)
        {
          if (option->state & State_Sunken)
          {
            btnOpt.state |= State_Sunken;
            btnOpt.state &= ~State_Raised;
            btnOpt.state &= ~State_MouseOver;
          }
          else
          {
            btnOpt.state |= State_MouseOver;
            btnOpt.state &= ~State_Raised;
            btnOpt.state &= ~State_Sunken;
          }
        }
        else
        {
          btnOpt.state |= State_Raised;
          btnOpt.state &= ~State_Sunken;
          btnOpt.state &= ~State_MouseOver;
        }
        btnOpt.rect = subControlRect(CC_MdiControls, option, SC_MdiNormalButton, widget);
        //drawPrimitive(PE_PanelButtonCommand, &btnOpt, painter, widget);
        QPixmap pm = getPixmapFromIcon(standardIcon(SP_TitleBarNormalButton,&btnOpt,widget),
                                       iconmode,iconstate,QSize(16,16));
        QRect iconRect = alignedRect(option->direction, Qt::AlignCenter, pm.size()/pixelRatio_, btnOpt.rect);
        painter->drawPixmap(iconRect,pm);
      }
      if (option->subControls & QStyle::SC_MdiMinButton)
      {
        if (option->activeSubControls & QStyle::SC_MdiMinButton)
        {
          if (option->state & State_Sunken)
          {
            btnOpt.state |= State_Sunken;
            btnOpt.state &= ~State_Raised;
            btnOpt.state &= ~State_MouseOver;
          }
          else
          {
            btnOpt.state |= State_MouseOver;
            btnOpt.state &= ~State_Raised;
            btnOpt.state &= ~State_Sunken;
          }
        }
        else
        {
          btnOpt.state |= State_Raised;
          btnOpt.state &= ~State_Sunken;
          btnOpt.state &= ~State_MouseOver;
        }
        btnOpt.rect = subControlRect(CC_MdiControls, option, SC_MdiMinButton, widget);
        //drawPrimitive(PE_PanelButtonCommand, &btnOpt, painter, widget);
        QPixmap pm = getPixmapFromIcon(standardIcon(SP_TitleBarMinButton,&btnOpt,widget),
                                                    iconmode,iconstate,QSize(16,16));
        QRect iconRect = alignedRect(option->direction, Qt::AlignCenter, pm.size()/pixelRatio_, btnOpt.rect);
        painter->drawPixmap(iconRect,pm);
      }

      break;
    }

    case CC_GroupBox: { // added only for correcting RTL text alignment
      const QStyleOptionGroupBox *opt =
        qstyleoption_cast<const QStyleOptionGroupBox*>(option);
      if (opt) {
        // Draw frame
        QRect textRect = subControlRect(CC_GroupBox, opt, SC_GroupBoxLabel, widget);
        QRect checkBoxRect = proxy()->subControlRect(CC_GroupBox, opt, SC_GroupBoxCheckBox, widget);
        if (opt->subControls & QStyle::SC_GroupBoxFrame)
        {
          QStyleOptionFrame_v3 frame;
          frame.QStyleOption::operator=(*opt);
          frame.features = opt->features;
          frame.lineWidth = opt->lineWidth;
          frame.midLineWidth = opt->midLineWidth;
          frame.rect = subControlRect(CC_GroupBox, opt, SC_GroupBoxFrame, widget);
          painter->save();
          QRegion region(opt->rect);
          if (!opt->text.isEmpty())
          {
            bool ltr = opt->direction == Qt::LeftToRight;
            QRect finalRect;
            if (opt->subControls & QStyle::SC_GroupBoxCheckBox)
            {
              finalRect = checkBoxRect.united(textRect);
              finalRect.adjust(ltr ? -4 : 0, 0, ltr ? 0 : 4, 0);
            }
            else
              finalRect = textRect;

            region -= finalRect;
          }
          painter->setClipRegion(region);
          drawPrimitive(PE_FrameGroupBox, &frame, painter, widget);
          painter->restore();
        }

        // Draw title
        if ((opt->subControls & QStyle::SC_GroupBoxLabel) && !opt->text.isEmpty())
        {
          const label_spec lspec = getLabelSpec("GroupBox");
          QColor col;
          if (!(option->state & State_Enabled))
            col = getFromRGBA(cspec_.disabledTextColor);
          else if (option->state & State_MouseOver)
            col = getFromRGBA(lspec.focusColor);
          else
            col = getFromRGBA(lspec.normalColor);
          if (!col.isValid())
            col = option->palette.color(QPalette::Text);

          int talign = Qt::AlignHCenter | Qt::AlignVCenter;
          if (!styleHint(SH_UnderlineShortcut, opt, widget))
            talign |= Qt::TextHideMnemonic;
          else
            talign |= Qt::TextShowMnemonic;

          if (lspec.boldFont || lspec.italicFont)
          {
            QFont font(painter->font());
            if (lspec.boldFont)
              font.setBold(true);
            if (lspec.italicFont)
              font.setItalic(true);
            painter->save();
            painter->setFont(font);
          }

          if (lspec.hasShadow)
          {
            QColor shadowColor = getFromRGBA(lspec.shadowColor);
            /* the shadow should have enough contrast with the text */
            if (shadowColor.isValid() && enoughContrast(col, shadowColor))
            {
              painter->save();
              if (lspec.a < 255)
                shadowColor.setAlpha(lspec.a);
              painter->setPen(QPen(shadowColor));
              for (int i=0; i<lspec.depth; i++)
                painter->drawText(textRect.adjusted(lspec.xshift+i,lspec.yshift+i,0,0),
                                  talign,opt->text);
              painter->restore();
            }
          }

          if (col.isValid())
          {
            painter->save();
            painter->setPen(col);
          }

          drawItemText(painter, textRect, talign,
                       opt->palette, opt->state & State_Enabled, opt->text,
                       col.isValid() ? QPalette::NoRole : QPalette::WindowText);

          if (col.isValid())
            painter->restore();
          if (lspec.boldFont || lspec.italicFont)
            painter->restore();

          if (opt->state & State_HasFocus)
          {
            QStyleOptionFocusRect fropt;
            fropt.QStyleOption::operator=(*opt);
            fropt.rect = textRect;
            drawPrimitive(PE_FrameFocusRect, &fropt, painter, widget);
          }
        }

        // Draw checkbox
        if (opt->subControls & SC_GroupBoxCheckBox)
        {
          QStyleOptionButton box;
          box.QStyleOption::operator=(*opt);
          box.rect = checkBoxRect;
          drawPrimitive(PE_IndicatorCheckBox, &box, painter, widget);
        }
      }

      break;
    }

    default : QCommonStyle::drawComplexControl(control,option,painter,widget);
  }
}

int Style::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
  switch (metric) {
    case PM_ButtonMargin : return 0;

    case PM_ButtonShiftHorizontal :
    case PM_ButtonShiftVertical : return tspec_.button_contents_shift ? 1 : 0;

    case PM_DefaultFrameWidth : {
      QString group;
      if (qstyleoption_cast<const QStyleOptionButton*>(option))
        group = "PanelButtonCommand";
      else
        group = "GenericFrame";

      const frame_spec fspec = getFrameSpec(group);
      if (qobject_cast<const QAbstractItemView*>(widget)
          && qstyleoption_cast<const QStyleOptionButton*>(option))
      { // as in Kate's preferences for its default text style
        return qMin(fspec.left,3);
      }
      return qMax(qMax(fspec.top,fspec.bottom),qMax(fspec.left,fspec.right));
    }

    case PM_SpinBoxFrameWidth :
    case PM_ComboBoxFrameWidth : return 0;

    case PM_MdiSubWindowFrameWidth : return 4;
    case PM_MdiSubWindowMinimizedWidth : return 200;

    case PM_LayoutLeftMargin :
    case PM_LayoutRightMargin :
    case PM_LayoutTopMargin :
    case PM_LayoutBottomMargin : return tspec_.layout_margin;

    case PM_LayoutHorizontalSpacing :
    case PM_LayoutVerticalSpacing : return tspec_.layout_spacing;

    case PM_MenuBarPanelWidth :
    case PM_MenuBarVMargin :
    case PM_MenuBarHMargin :  return 0;

    case PM_MenuBarItemSpacing : {
      /* needed for putting menubar-items inside menubar frame */
      if (tspec_.merge_menubar_with_toolbar)
        return getFrameSpec("Toolbar").left;
      else
        return getFrameSpec("MenuBar").left;
    }

    case PM_MenuPanelWidth :
    case PM_MenuDesktopFrameWidth: return 0;

    case PM_SubMenuOverlap : {
#if QT_VERSION >= 0x050000
      if (QApplication::layoutDirection() == Qt::RightToLeft)
        return 0; // RTL submenu positioning is a mess in Qt5
#endif
      int so = tspec_.submenu_overlap;
      if (so >= 0)
      {
        /* Even when PM_SubMenuOverlap is set to zero, there's an overlap
           equal to PM_MenuHMargin. So, we make the overlap accurate here. */
        so -= getMenuMargin(true);
        if (!noComposite_
            && settings_->getCompositeSpec().composite
            && menuHShadows_.count() == 2
            && (!qobject_cast<const QMenu*>(widget)
                || translucentWidgets_.contains(widget)
                || widget->testAttribute(Qt::WA_X11NetWmWindowTypeMenu)))
        {
          so += (menuHShadows_.at(0) + menuHShadows_.at(1));
        }
        return -so;
      }
      else
      {
        if (!noComposite_
            && settings_->getCompositeSpec().composite
            && (!qobject_cast<const QMenu*>(widget)
                || translucentWidgets_.contains(widget)
                || widget->testAttribute(Qt::WA_X11NetWmWindowTypeMenu)))
        {
          const frame_spec fspec = getFrameSpec("Menu");
          return -qMax(fspec.left,fspec.right);
        }
        return 0;
      }
    }

    case PM_MenuHMargin : 
    case PM_MenuVMargin:
    case PM_MenuTearoffHeight : {
      if (qobject_cast<const QComboBox*>(widget) && metric == PM_MenuVMargin)
        return 0; // added to the height of combo menu in qcombobox.cpp -> showPopup()

      const frame_spec fspec = getFrameSpec("Menu");
      int v = qMax(fspec.top,fspec.bottom);
      int h = qMax(fspec.left,fspec.right);
      theme_spec tspec_now = settings_->getCompositeSpec();
      if (!noComposite_ && tspec_now.composite
          && widget && translucentWidgets_.contains(widget))
      {
        v += tspec_now.menu_shadow_depth;
        h += tspec_now.menu_shadow_depth;
      }
      /* a margin > 2px could create ugly corners without compositing */
      if (/*!tspec_now.composite ||*/ isLibreoffice_
          || (!widget && option) // QML menus (see PE_PanelMenu)
          /*|| (qobject_cast<const QMenu*>(widget) && !translucentWidgets_.contains(widget))*/)
      {
        v = qMin(2,v);
        h = qMin(2,h);
      }

      /* Sometimes (like in VLC or SVG Cleaner), developers make this
         mistake that they give a stylesheet to a subclassed lineedit
         but forget to prevent its propagation to the context menu.
         What follows is a simple workaround for such cases. */
      if (qobject_cast<const QMenu*>(widget)
          && widget->style() != this
          && !widget->testAttribute(Qt::WA_X11NetWmWindowTypeMenu))
      {
        QString css;
        if (QWidget *p = widget->parentWidget())
        {
          if (qobject_cast<QLineEdit*>(p))
            css = p->styleSheet();
          else if (qobject_cast<QMenu*>(p))
          {
            if (QLineEdit *pp = qobject_cast<QLineEdit*>(p->parentWidget()))
              css = pp->styleSheet();
          }
        }
        if (!css.isEmpty() && css.contains("padding") && !css.contains("{"))
        {
          v = qMin(2,v);
          h = qMin(2,h);
        }
      }

      if (metric == PM_MenuTearoffHeight)
        /* we set the height of tearoff indicator to be 8px */
        return v + 8;
      else if (metric == PM_MenuHMargin)
        return h;
      else return v;
    }

    case PM_MenuScrollerHeight : {
      const indicator_spec dspec = getIndicatorSpec("MenuItem");
      return qMax(pixelMetric(PM_MenuVMargin,option,widget), dspec.size);
    }

    case PM_ToolBarFrameWidth : return 0;
    case PM_ToolBarItemSpacing : {
      if (tspec_.group_toolbar_buttons)
        return 0;
      else
      {
        label_spec lspec = getLabelSpec("PanelButtonTool");
        lspec.left = qMax(0,lspec.left-1);
        lspec.right = qMax(0,lspec.right-1);
        return qMax(0, 5-lspec.left-lspec.right);
      }
    }
    case PM_ToolBarHandleExtent : {
      if (tspec_.center_toolbar_handle)
      {
        const indicator_spec dspec = getIndicatorSpec("Toolbar");
        return dspec.size ? 2*dspec.size : 16;
      }
      return 8;
    }
    case PM_ToolBarSeparatorExtent : {
      const indicator_spec dspec = getIndicatorSpec("Toolbar");
      return dspec.size ? dspec.size : 8;
    }
    case PM_ToolBarIconSize : return tspec_.toolbar_icon_size;
    case PM_ToolBarExtensionExtent : return 16;
    case PM_ToolBarItemMargin : {
      const frame_spec fspec = getFrameSpec("Toolbar");
      int v = qMax(fspec.top,fspec.bottom);
      int h = qMax(fspec.left,fspec.right);
      return qMax(v,h);
    }

    /* PM_TabBarTabHSpace provides an appropriate space between the
       close button and frame but PM_TabBarTabVSpace isn't needed */
    case PM_TabBarTabHSpace : {
      const frame_spec fspec = getFrameSpec("Tab");
      int hSpace = fspec.left + fspec.right;
      if (!widget) // QML
      {
        const label_spec lspec = getLabelSpec("Tab");
        int common = QCommonStyle::pixelMetric(metric,option,widget);
        hSpace += lspec.left + lspec.right;
        hSpace = qMax(hSpace, common);
      }
      return hSpace;
    }
    case PM_TabBarTabVSpace : {
      if (!widget) // QML
      {
        const frame_spec fspec = getFrameSpec("Tab");
        const label_spec lspec = getLabelSpec("Tab");
        int common = QCommonStyle::pixelMetric(metric,option,widget);
        return qMax(fspec.top+fspec.bottom + lspec.top+lspec.bottom
                      + 1, // WARNING: Why QML tabs are cut by 1px from below?
                    common);
      }
      else return 0;
    }

    case PM_TabBarTabOverlap :
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

    case PM_TabBarIconSize :
    case PM_ListViewIconSize :
    case PM_ButtonIconSize : return tspec_.button_icon_size; 
    case PM_SmallIconSize : return tspec_.small_icon_size;

    case PM_IconViewIconSize:
    case PM_LargeIconSize : return tspec_.large_icon_size;

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

    case PM_SplitterWidth :
      return tspec_.splitter_width;

    case PM_ScrollBarExtent :
      return tspec_.scroll_width;
    case PM_ScrollBarSliderMin :
      return tspec_.scroll_min_extent;

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
    case PM_SliderLength :
      return tspec_.slider_handle_length;
    case PM_SliderControlThickness :
      return tspec_.slider_handle_width;

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
      int b = 0;
      if (widget && lspec.boldFont)
      {
        QFont f = widget->font();
        QSize s = textSize(f, "W");
        f.setBold(true);
        b = (textSize(f, "W") - s).height();
      }
      return qMax(widget ? widget->fontMetrics().lineSpacing()+v+b
                           : option ? option->fontMetrics.lineSpacing()+v : 0,
                  24);
    }

    case PM_TextCursorWidth : return 1;

    case PM_HeaderMargin : return 2;

    case PM_ToolTipLabelFrameWidth : {
      const frame_spec fspec = getFrameSpec("ToolTip");

      int v = qMax(fspec.top,fspec.bottom);
      int h = qMax(fspec.left,fspec.right);
      theme_spec tspec_now = settings_->getCompositeSpec();
      if (!noComposite_ && tspec_now.composite
          && (!widget || translucentWidgets_.contains(widget)))
      {
        v += tspec_now.tooltip_shadow_depth;
        h += tspec_now.tooltip_shadow_depth;
      }
      /* a margin > 2px could create ugly
         corners without compositing */
      if (/*!tspec_now.composite ||*/ isLibreoffice_
          /*|| (widget && !translucentWidgets_.contains(widget))*/)
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
      /* make exception for menuitems and viewitems */
      if (isLibreoffice_
          || qstyleoption_cast<const QStyleOptionMenuItem*>(option)
          || qobject_cast<QAbstractItemView*>(getParent(widget,2)))
      {
        return qMin(QCommonStyle::pixelMetric(PM_IndicatorWidth,option,widget)*pixelRatio_,
                    tspec_.check_size);
      }
      return tspec_.check_size;
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
  to use createTLExtra() and createTLSysExtra(). There are to ways for that:

  (1) Using of private headers, which isn't a good idea for obvious reasons;

  (2) Setting Qt::AA_DontCreateNativeWidgetSiblings, so that the method
      enforceNativeChildren() isn't used in setAttribute() (-> qwidget.cpp).
*/
void Style::setSurfaceFormat(QWidget *widget) const
{
#if QT_VERSION < 0x050000
  Q_UNUSED(widget);
  return;
#else
  if (noComposite_ || !tspec_.composite
      || !widget || widget->testAttribute(Qt::WA_WState_Created)
      || qobject_cast<QMenu*>(widget) // WARNING: prevent a hang in KFileDialog!
      || subApp_ || isLibreoffice_)
    return;
  if (widget->inherits("QTipLabel")) ; // see polish(QWidget*)
  else if (!widget->isWindow()
           /* FIXME: if transluceny is enabled, hard-coded translucency
                     will also be included but that seems harmless */
           || !tspec_.translucent_windows
           || isPlasma_ || isOpaque_)
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

/*
  This is a workaround for a Qt5 bug (QTBUG-47043), because of which,
  _NET_WM_WINDOW_TYPE is set to _NET_WM_WINDOW_TYPE_NORMAL for QMenu.
*/
void Style::setMenuType(const QWidget *widget) const
{
#if QT_VERSION < 0x050500 || !(defined Q_WS_X11 || defined Q_OS_LINUX)
  Q_UNUSED(widget);
  return;
#else
  if (!qobject_cast<const QMenu*>(widget)
      || widget->testAttribute(Qt::WA_X11NetWmWindowTypeMenu)
      || !widget->windowHandle())
    return;

  int wmWindowType = 0;
  if (widget->testAttribute(Qt::WA_X11NetWmWindowTypeDropDownMenu))
    wmWindowType |= QXcbWindowFunctions::DropDownMenu;
  if (widget->testAttribute(Qt::WA_X11NetWmWindowTypePopupMenu))
    wmWindowType |= QXcbWindowFunctions::PopupMenu;
  if (wmWindowType == 0) return;
  QXcbWindowFunctions::setWmWindowType(widget->windowHandle(),
                                       static_cast<QXcbWindowFunctions::WmWindowType>(wmWindowType));
#endif
}

int Style::styleHint(StyleHint hint,
                     const QStyleOption *option,
                     const QWidget *widget,
                     QStyleHintReturn *returnData) const
{
  setSurfaceFormat(widget); /* FIXME Why here and nowhere else?
                                     Perhaps because of its use in qapplication.cpp. */
  setMenuType(widget);

  switch (hint) {
    case SH_EtchDisabledText :
    case SH_DitherDisabledText :
    case SH_Menu_AllowActiveAndDisabled :
    case SH_MenuBar_AltKeyNavigation :
    case SH_ItemView_ShowDecorationSelected :
    case SH_ItemView_ArrowKeysNavigateIntoChildren : return false;

    case SH_ItemView_ActivateItemOnSingleClick : return !tspec_.double_click;

    case SH_ToolButton_PopupDelay :
    case SH_Menu_SubMenuPopupDelay : return 250;
    case SH_Menu_Scrollable : return false; // let's see the whole menu
    case SH_Menu_SloppySubMenus : return true;
#if QT_VERSION >= 0x050500
    case SH_Menu_SubMenuSloppyCloseTimeout : return 1000;
    case SH_Menu_SubMenuResetWhenReenteringParent : return false;
    case SH_Menu_SubMenuDontStartSloppyOnLeave : return false;
    case SH_Menu_SubMenuSloppySelectOtherActions : return true;
#endif
    /* when set to true, only the last submenu is
       hidden on clicking anywhere outside the menu */
    case SH_Menu_FadeOutOnHide : return false;

    case SH_ComboBox_ListMouseTracking :
    case SH_Menu_MouseTracking : return true;

    case SH_ComboBox_PopupFrameStyle: return QFrame::StyledPanel | QFrame::Plain;
    case SH_ComboBox_Popup : return tspec_.combo_menu;

    case SH_MenuBar_MouseTracking :
      return tspec_.menubar_mouse_tracking;

    case SH_TabBar_Alignment : {
      if (tspec_.left_tabs)
      {
        if (tspec_.center_doc_tabs)
        {
          const QTabWidget *tw = qobject_cast<const QTabWidget*>(widget);
          if (!tw || tw->documentMode())
            return Qt::AlignCenter;
        }
        return Qt::AlignLeft;
      }
      else
        return Qt::AlignCenter;
    }

    //case SH_ScrollBar_BackgroundMode : return Qt::OpaqueMode;

    case SH_ScrollBar_ContextMenu :
    case SH_ScrollBar_LeftClickAbsolutePosition : return true;

    case SH_Slider_StopMouseOverSlider : return true;

    case SH_ScrollView_FrameOnlyAroundContents : return !tspec_.scrollbar_in_view;

    case SH_UnderlineShortcut:
      return (widget && itsShortcutHandler_) ? itsShortcutHandler_->showShortcut(widget)
                                             : false; // no underline by default or for QML widgets

    case SH_TitleBar_NoBorder: return true;
    case SH_TitleBar_AutoRaise: return true;

    case SH_GroupBox_TextLabelVerticalAlignment : {
      if (tspec_.groupbox_top_label)
        return Qt::AlignTop;
      return Qt::AlignVCenter;
    }

    case SH_GroupBox_TextLabelColor: {
      const label_spec lspec = getLabelSpec("GroupBox");
      QColor col;
      if (!(option->state & State_Enabled))
      {
        col = getFromRGBA(cspec_.disabledTextColor);
        if (col.isValid())
          return col.rgba();
      }
      else if (option->state & State_MouseOver)
      {
        col = getFromRGBA(lspec.focusColor);
        if (col.isValid())
          return col.rgba();
      }
      else
      {
        col = getFromRGBA(lspec.normalColor);
        if (col.isValid())
          return col.rgba();
      }

      return QCommonStyle::styleHint(hint,option,widget,returnData);
    }

    case SH_ToolButtonStyle : {
      switch (tspec_.toolbutton_style) {
        case 0 : return QCommonStyle::styleHint(hint,option,widget,returnData);
        case 1 : return Qt::ToolButtonIconOnly;
        case 2 : return Qt::ToolButtonTextOnly;
        case 3 : return Qt::ToolButtonTextBesideIcon;
        case 4 : return Qt::ToolButtonTextUnderIcon;
        default :return QCommonStyle::styleHint(hint,option,widget,returnData);
      }
    }

    case SH_RubberBand_Mask : {
      const QStyleOptionRubberBand *opt = qstyleoption_cast<const QStyleOptionRubberBand*>(option);
      if (!opt) return false;
      if (QStyleHintReturnMask *mask = qstyleoption_cast<QStyleHintReturnMask*>(returnData))
      {
        mask->region = option->rect;
        if (!qobject_cast<QGraphicsView*>(getParent(widget,1)) // as in Oxygen
            && (!tspec_.fill_rubberband || !qobject_cast<QMainWindow*>(getParent(widget,1))))
        {
          mask->region -= option->rect.adjusted(1,1,-1,-1);
        }
        return true;
      }
      return false;
    }

    //case SH_DialogButtonLayout: return QDialogButtonBox::GnomeLayout;

    //case SH_SpinControls_DisableOnBounds: return true;

#if QT_VERSION >= 0x050000
    case SH_ToolTip_WakeUpDelay : {
      int delay = tspec_.tooltip_delay;
      if (tspec_.tooltip_delay >= 0)
        return delay;
      return QCommonStyle::styleHint(hint,option,widget,returnData);
    }
    case SH_ToolTip_FallAsleepDelay : {
      if (tspec_.tooltip_delay >= 0)
        return 0;
      return QCommonStyle::styleHint(hint,option,widget,returnData);
    }
#endif

    default : {
      if (hint >= SH_CustomBase && hspec_.kcapacitybar_as_progressbar
          && widget && widget->objectName() == "CE_CapacityBar")
      {
        return CE_Kv_KCapacityBar;
      }
      return QCommonStyle::styleHint(hint,option,widget,returnData);
    }
  }
}

QCommonStyle::SubControl Style::hitTestComplexControl(ComplexControl control,
                                                      const QStyleOptionComplex *option,
                                                      const QPoint &position,
                                                      const QWidget *widget) const
{
  return QCommonStyle::hitTestComplexControl(control,option,position,widget);
}

QSize Style::sizeFromContents(ContentsType type,
                              const QStyleOption *option,
                              const QSize &contentsSize,
                              const QWidget *widget) const
{
  QSize defaultSize = QCommonStyle::sizeFromContents(type,option,contentsSize,widget);
  QSize s = QSize(0,0);

  switch (type) {
    case CT_LineEdit : {
      QFont f = QApplication::font();
      if (widget) f = widget->font();

      const QString group = "LineEdit";
      const frame_spec fspec = getFrameSpec(group);
      const size_spec sspec = getSizeSpec(group);
      /* the label spec is only used for vertical spacing */
      label_spec lspec = getLabelSpec(group);
      lspec.top = qMax(0,lspec.top-1);
      lspec.bottom = qMax(0,lspec.bottom-1);

      s = sizeCalculated(f,fspec,lspec,sspec,"W",QSize());
      s.rwidth() = qMax(defaultSize.width() + lspec.left+lspec.right + qMax(fspec.left+fspec.right-2,0),
                        s.width());
      /* defaultSize may be a bit thicker because of frame, which doesn't matter
         to us. However, we'll make an exception for widgets like KCalcDisplay. */
      if (s.height() < defaultSize.height() && !qobject_cast<const QLineEdit*>(widget))
        s.rheight() = defaultSize.height();
      return s;

      break;
    }

    case CT_SpinBox : {
      /* Here we don't use defaultSize because, for Qt4, it's based on spinbox size hint,
         which in turn is based on SC_SpinBoxEditField (-> qabstractspinbox.cpp). That's
         corrected in Qt5 but the following method works for both. */
      frame_spec fspec = getFrameSpec("LineEdit");
      if (tspec_.vertical_spin_indicators)
      {
        fspec.left = fspec.right = fspec.top = fspec.bottom = qMin(fspec.left,3);
      }
      label_spec lspec = getLabelSpec("LineEdit");
      lspec.top = qMax(0,lspec.top-1);
      lspec.bottom = qMax(0,lspec.bottom-1);
      const frame_spec fspec1 = getFrameSpec("IndicatorSpinBox");
      const size_spec sspec = getSizeSpec("IndicatorSpinBox");
      if (const QAbstractSpinBox *sb = qobject_cast<const QAbstractSpinBox*>(widget))
      {
        QString maxTxt = spinMaxText(sb);
        if (!maxTxt.isEmpty())
        {
          maxTxt = maxTxt + QLatin1Char(' ');
          s = textSize(sb->font(),maxTxt)
              + QSize(fspec.left + (tspec_.vertical_spin_indicators ? 0 : lspec.left) + 2 // cursor padding
                                 + 2*tspec_.spin_button_width
                                 + (tspec_.vertical_spin_indicators
                                    || sb->buttonSymbols() == QAbstractSpinBox::NoButtons ? // as in qpdfview
                                      fspec.right : fspec1.right),
                      lspec.top + lspec.bottom
                      + (tspec_.vertical_spin_indicators
                         || sb->buttonSymbols() == QAbstractSpinBox::NoButtons ? fspec.top + fspec.bottom
                         : (qMax(fspec1.top,fspec.top) + qMax(fspec1.bottom,fspec.bottom))));
        }
        else
        {
          /* This is a for some apps (like Kdenlive with its
             TimecodeDisplay) that subclass only QAbstractSpinBox. */
          if (tspec_.vertical_spin_indicators || sb->buttonSymbols() == QAbstractSpinBox::NoButtons)
            s.rwidth() = sb->minimumWidth();
          else
            s.rwidth() = sb->minimumWidth() + tspec_.spin_button_width;
        }

        s = s.expandedTo(QSize(sspec.minW,sspec.minH));
      }

      break;
    }

    case CT_ComboBox : {
      const QStyleOptionComboBox *opt =
          qstyleoption_cast<const QStyleOptionComboBox*>(option);

      if (opt) {
        const QString group = "ComboBox";
        const frame_spec fspec = getFrameSpec(group);
        const size_spec sspec = getSizeSpec(group);
        label_spec lspec = getLabelSpec(group);
        lspec.left = qMax(0,lspec.left-1);
        lspec.top = qMax(0,lspec.top-1);
        lspec.right = qMax(0,lspec.right-1);
        lspec.bottom = qMax(0,lspec.bottom-1);
        const frame_spec fspec1 = getFrameSpec("LineEdit");
        const label_spec lspec1 = getLabelSpec("LineEdit");

        QFont f = QApplication::font();
        if (widget) f = widget->font();
        if (lspec.boldFont) f.setBold(true);

        bool hasIcon = false;
        if (const QComboBox *cb = qobject_cast<const QComboBox*>(widget))
        {
          for (int i = 0; i < cb->count(); i++)
          {
            if (!cb->itemIcon(i).isNull())
            {
              hasIcon = true;
              break;
            }
          }
        }
        else hasIcon = true;

        /* We don't add COMBO_ARROW_LENGTH (=20) to the width because
           qMax(23,X) is already added to it in qcommonstyle.cpp.

           We want that the left icon respect frame width,
           text margin and text-icon spacing in the editable mode too. */
        s = QSize(defaultSize.width() + fspec.left+fspec.right
                                      + (opt->editable ? lspec1.left+lspec1.right +
                                          (option->direction == Qt::RightToLeft ?
                                            fspec1.right + fspec.right + (hasIcon ? lspec.right : 0)
                                            : fspec1.left + fspec.left + (hasIcon ? lspec.left : 0))
                                          : lspec.left+lspec.right)
                                      + (hasIcon ? lspec.tispace : 0) ,
                  sizeCalculated(f,fspec,lspec,sspec,"W",
                                 hasIcon ? opt->iconSize : QSize()).height());

        /* consider the top and bottom frames
           of lineedits inside editable combos */
        if (opt->editable)
        {
          s.rheight() += (fspec1.top > fspec.top ? fspec1.top-fspec.top : 0)
                         + (fspec1.bottom > fspec.bottom ? fspec1.bottom-fspec.bottom : 0);
          if (tspec_.combo_as_lineedit)
            s.rwidth() += option->direction == Qt::RightToLeft ?
                           (fspec1.right > fspec.right ? fspec1.right-fspec.right : 0)
                           : (fspec1.left > fspec.left ? fspec1.left-fspec.left : 0);
        }

        if (s.width() < sspec.minW)
          s.setWidth(sspec.minW);
      }

      break;
    }

    case CT_PushButton : {
      const QStyleOptionButton *opt =
        qstyleoption_cast<const QStyleOptionButton*>(option);

      if (opt) {
        const QString group = "PanelButtonCommand";
        frame_spec fspec = getFrameSpec(group);
        const indicator_spec dspec = getIndicatorSpec(group);
        const size_spec sspec = getSizeSpec(group);
        label_spec lspec = getLabelSpec(group);
        lspec.left = qMax(0,lspec.left-1);
        lspec.top = qMax(0,lspec.top-1);
        lspec.right = qMax(0,lspec.right-1);
        lspec.bottom = qMax(0,lspec.bottom-1);

        if (qobject_cast<QAbstractItemView*>(getParent(widget,2)))
        {
          lspec.left = lspec.right = lspec.top = lspec.bottom = qMin(lspec.left,2);
          fspec.left = fspec.right = fspec.top = fspec.bottom = qMin(fspec.left,3);
          lspec.tispace = qMin(lspec.tispace,3);
        }

       /*
          Like with CT_ToolButton, don't use sizeCalculated()!
       */

        /* also take into account the possibility of the presence of an indicator */
        s = defaultSize
            + QSize(opt->features & QStyleOptionButton::HasMenu ? dspec.size+pixelMetric(PM_HeaderMargin) : 0, 0);

        /* Add the spacings! The frame widths are calculated
           as pixelMetric(PM_DefaultFrameWidth,option,widget). */
        const QString txt = opt->text;
        s = s + QSize(lspec.left+lspec.right,
                      lspec.top+lspec.bottom); // either text or icon
        if (!txt.isEmpty())
        {
          if (lspec.hasShadow)
            s = s + QSize(qAbs(lspec.xshift)+lspec.depth, qAbs(lspec.yshift)+lspec.depth);
          if (!opt->icon.isNull())
            s = s + QSize(lspec.tispace, 0);
        }

        /* this was for KColorButton but apparently
           it isn't needed when sizeCalculated() isn't used */
        /*if (txt.size() == 0 && opt->icon.isNull())
        {
          int smallIconSize = pixelMetric(PM_SmallIconSize);
          s = QSize(s.width() < smallIconSize ? smallIconSize : s.width(),
                    s.height() < smallIconSize ? smallIconSize : s.height());
        }*/

        if (!txt.isEmpty())
        {
          /* take in to account the boldness of default button text
             and also the possibility of boldness in general */
          if ((opt->features & QStyleOptionButton::AutoDefaultButton) || lspec.boldFont)
          {
            QFont f = QApplication::font();
            if (widget) f = widget->font();
            QSize s1 = textSize(f, txt);
            f.setBold(true);
            s = s + textSize(f, txt) - s1;
          }
          // consider a global min. width for push buttons
          s = s.expandedTo(QSize(2*pixelMetric(PM_DefaultFrameWidth,option,widget)
                                   + 6*QFontMetrics(QApplication::font()).width("W"),
                                 s.height()));
        }

        s = s.expandedTo(QSize(sspec.minW,sspec.minH));
      }

      break;
    }

    case CT_RadioButton : {
      const QStyleOptionButton *opt =
        qstyleoption_cast<const QStyleOptionButton*>(option);

      if (opt) {
        const QString group = "RadioButton";
        frame_spec fspec;
        default_frame_spec(fspec);
        const label_spec lspec = getLabelSpec(group);
        const size_spec sspec = getSizeSpec(group);

        QFont f = QApplication::font();
        if (widget) f = widget->font();
        if (lspec.boldFont) f.setBold(true);

        int ih = pixelMetric(PM_ExclusiveIndicatorHeight);
        if (!opt->text.isEmpty() || !opt->icon.isNull())
        s = sizeCalculated(f,fspec,lspec,sspec,opt->text,opt->iconSize)
            + QSize(pixelMetric(PM_RadioButtonLabelSpacing), 0);
        s = s + QSize(pixelMetric(PM_ExclusiveIndicatorWidth), (s.height() < ih ? ih : 0));
      }

      break;
    }

    case CT_CheckBox : {
      const QStyleOptionButton *opt =
        qstyleoption_cast<const QStyleOptionButton*>(option);

      if (opt) {
        const QString group = "CheckBox";
        frame_spec fspec;
        default_frame_spec(fspec);
        const label_spec lspec = getLabelSpec(group);
        const size_spec sspec = getSizeSpec(group);

        QFont f = QApplication::font();
        if (widget) f = widget->font();
        if (lspec.boldFont) f.setBold(true);

        int ih = pixelMetric(PM_IndicatorHeight);
        if (!opt->text.isEmpty() || !opt->icon.isNull())
          s = sizeCalculated(f,fspec,lspec,sspec,opt->text,opt->iconSize)
              + QSize(pixelMetric(PM_CheckBoxLabelSpacing), 0);
        s = s + QSize(pixelMetric(PM_IndicatorWidth), (s.height() < ih ? ih : 0));
      }

      break;
    }

    case CT_MenuItem : {
      const QStyleOptionMenuItem *opt =
        qstyleoption_cast<const QStyleOptionMenuItem*>(option);

      if (opt) {
        const QString group = "MenuItem";
        const frame_spec fspec = getFrameSpec(group);
        const label_spec lspec = getLabelSpec(group);
        const size_spec sspec = getSizeSpec(group);

        QFont f = QApplication::font();
        if (widget) f = widget->font();
        if (lspec.boldFont) f.setBold(true);

        int iconSize = qMax(pixelMetric(PM_SmallIconSize), opt->maxIconWidth);
        int lxqtMenuIconSize = hspec_.lxqtmainmenu_iconsize;
        if (lxqtMenuIconSize >= 16
            && lxqtMenuIconSize != iconSize
            && qobject_cast<const QMenu*>(widget))
        {
          if (widget->objectName() == "TopLevelMainMenu")
            iconSize = lxqtMenuIconSize;
          else if (QMenu *menu = qobject_cast<QMenu*>(getParent(widget, 1)))
          {
            if (menu->objectName() == "TopLevelMainMenu")
              iconSize = lxqtMenuIconSize;
            else
            {
              while (qobject_cast<QMenu*>(getParent(menu, 1)))
              {
                menu = qobject_cast<QMenu*>(getParent(menu, 1));
                if (menu->objectName() == "TopLevelMainMenu")
                {
                  iconSize = lxqtMenuIconSize;
                  break;
                }
              }
            }
          }
        }

        if (opt->menuItemType == QStyleOptionMenuItem::Separator)
          s = QSize(contentsSize.width(),10); /* FIXME there is no PM_MenuSeparatorHeight pixel metric */
        else
        {
          const QStringList l = opt->text.split('\t');
          s = sizeCalculated(f,fspec,lspec,sspec,opt->text,
                             (opt->icon.isNull() || (hspec_.iconless_menu && !(l.size() > 0 && l[0].isEmpty())))
                             ? QSize()
                             : QSize(iconSize,iconSize));
        }

        /* even when there's no icon, another menuitem may have icon
           and that isn't taken into account with sizeCalculated() */
        if(opt->icon.isNull() && !hspec_.iconless_menu && opt->maxIconWidth)
          s.rwidth() += iconSize + lspec.tispace;

        if (opt->menuItemType == QStyleOptionMenuItem::SubMenu)
        {
          const indicator_spec dspec = getIndicatorSpec(group);
          /* we also add 2px for the right margin. */
          s.rwidth() += dspec.size + lspec.tispace + 2;
          s.rheight() += (dspec.size > s.height() ? dspec.size : 0);
        }

        if (opt->menuHasCheckableItems)
        {
          int cSize = pixelMetric(PM_IndicatorWidth,option,widget);
          s.rwidth() += cSize + lspec.tispace;
          /* for the height, see if there's really a check/radio button */
          if (opt->checkType == QStyleOptionMenuItem::Exclusive
              || opt->checkType == QStyleOptionMenuItem::NonExclusive)
          {
            s.rheight() += (cSize > s.height() ? cSize : 0);
          }
        }
      }

      break;
    }

    case CT_MenuBarItem : {
      const QStyleOptionMenuItem *opt =
        qstyleoption_cast<const QStyleOptionMenuItem*>(option);

      if (opt) {
        QString group = "MenuBarItem";
        frame_spec fspec = getFrameSpec(group);
        const label_spec lspec = getLabelSpec(group);
        const size_spec sspec = getSizeSpec(group);
        frame_spec fspec1;
        if (tspec_.merge_menubar_with_toolbar)
          fspec1 = getFrameSpec("Toolbar");
        else
          fspec1 = getFrameSpec("MenuBar");
        /* needed for putting menubar-items inside menubar frame */
        fspec.top += fspec1.top+fspec1.bottom;

        QFont f = QApplication::font();
        if (widget) f = widget->font();
        if (lspec.boldFont) f.setBold(true);

        s = sizeCalculated(f,fspec,lspec,sspec,opt->text,
                           opt->icon.isNull() ? QSize() : QSize(opt->maxIconWidth,opt->maxIconWidth));
      }

      break;
    }

    case CT_ToolButton : {
      const QStyleOptionToolButton *opt =
        qstyleoption_cast<const QStyleOptionToolButton*>(option);

      if (opt) {
        const QString group = "PanelButtonTool";
        frame_spec fspec = getFrameSpec(group);
        const indicator_spec dspec = getIndicatorSpec(group);
        const size_spec sspec = getSizeSpec(group);
        label_spec lspec = getLabelSpec(group);
        lspec.left = qMax(0,lspec.left-1);
        lspec.top = qMax(0,lspec.top-1);
        lspec.right = qMax(0,lspec.right-1);
        lspec.bottom = qMax(0,lspec.bottom-1);

        // -> CE_ToolButtonLabel
        if (qobject_cast<QAbstractItemView*>(getParent(widget,2)))
        {
          fspec.left = qMin(fspec.left,3);
          fspec.right = qMin(fspec.right,3);
          fspec.top = qMin(fspec.top,3);
          fspec.bottom = qMin(fspec.bottom,3);

          //lspec.left = qMin(lspec.left,2);
          //lspec.right = qMin(lspec.right,2);
          lspec.top = qMin(lspec.top,2);
          lspec.bottom = qMin(lspec.bottom,2);
          lspec.tispace = qMin(lspec.tispace,2);
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
           Unfortunately, there's no standard way to determine
           how margins and frames are changed in such cases.
        */

        s = defaultSize
            /* Unlike the case of CT_PushButton, the frame widths aren't taken
               into account yet. Qt seems to consider toolbuttons frameless,
               althought it adds 6 and 5 px to their widths and heights respectively
               (-> qcommonstyle.cpp and qtoolbutton.cpp -> QSize QToolButton::sizeHint() const). */
            + QSize(fspec.left+fspec.right - 6 , fspec.top+fspec.bottom - 5)
            + QSize(!(opt->features & QStyleOptionToolButton::Arrow)
                        || opt->arrowType == Qt::NoArrow
                        || tialign == Qt::ToolButtonTextOnly ?
                      0
                      // also add a margin between indicator and text (-> CE_ToolButtonLabel)
                      : dspec.size+lspec.tispace+pixelMetric(PM_HeaderMargin),
                    0);

        /* add the spacings */
        s = s + QSize(lspec.left+lspec.right,
                      lspec.top+lspec.bottom);
        if (tialign == Qt::ToolButtonTextBesideIcon)
          s = s + QSize(lspec.tispace, 0);
        else if (tialign == Qt::ToolButtonTextUnderIcon)
          s = s + QSize(0, lspec.tispace);

        if (const QToolButton *tb = qobject_cast<const QToolButton*>(widget))
        {
          if (tb->popupMode() == QToolButton::MenuButtonPopup)
          {
            const QString group1 = "DropDownButton";
            const frame_spec fspec1 = getFrameSpec(group1);
            indicator_spec dspec1 = getIndicatorSpec(group1);
            dspec1.size = qMin(dspec1.size,qMin(defaultSize.height(),defaultSize.width()));
            s.rwidth() += (opt->direction == Qt::RightToLeft ?
                             fspec1.left-fspec.left
                             : fspec1.right-fspec.right) // there's a capsule
                          +dspec1.size+2*TOOL_BUTTON_ARROW_MARGIN
                          -pixelMetric(PM_MenuButtonIndicator); // added in qcommonstyle.cpp
          }
          else if ((tb->popupMode() == QToolButton::InstantPopup
                    || tb->popupMode() == QToolButton::DelayedPopup)
                   && (opt->features & QStyleOptionToolButton::HasMenu))
          {
              s.rwidth() += lspec.tispace+dspec.size + pixelMetric(PM_HeaderMargin);
          }

          /* extra space for shadow and bold text */
          if (!opt->text.isEmpty())
          {
            if (lspec.hasShadow)
              s = s + QSize(qAbs(lspec.xshift)+lspec.depth, qAbs(lspec.yshift)+lspec.depth);
            if (lspec.boldFont)
            {
              QFont f = tb->font();
              QSize s1 = textSize(f, opt->text);
              f.setBold(true);
              s = s + textSize(f, opt->text) - s1;
            }
          }
        }

        s = s.expandedTo(QSize(sspec.minW,sspec.minH));
      }

      break;
    }

    case CT_TabBarTab : {
      const QStyleOptionTab *opt =
        qstyleoption_cast<const QStyleOptionTab*>(option);

      if (opt) {
        const QString group = "Tab";
        const frame_spec fspec = getFrameSpec(group);
        const label_spec lspec = getLabelSpec(group);
        const size_spec sspec = getSizeSpec(group);

        QFont f = QApplication::font();
        if (widget) f = widget->font();
        if (lspec.boldFont) f.setBold(true);

        int iconSize = pixelMetric(PM_TabBarIconSize);
        s = sizeCalculated(f,fspec,lspec,sspec,opt->text,
                           opt->icon.isNull() ? QSize() : QSize(iconSize,iconSize));

        bool verticalTabs = false;
        if (opt->shape == QTabBar::RoundedEast
            || opt->shape == QTabBar::RoundedWest
            || opt->shape == QTabBar::TriangularEast
            || opt->shape == QTabBar::TriangularWest)
        {
          verticalTabs = true;
        }

        if (opt->text.isEmpty())
          s.rwidth() += lspec.left + lspec.right;

        if (const QTabBar *tb = qobject_cast<const QTabBar*>(widget))
        {
          if (tb->tabsClosable())
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
        qstyleoption_cast<const QStyleOptionHeader*>(option);

      if (opt) {
        const QString group = "HeaderSection";
        frame_spec fspec = getFrameSpec(group);
        const label_spec lspec = getLabelSpec(group);
        const size_spec sspec = getSizeSpec(group);
        const indicator_spec dspec = getIndicatorSpec(group);
        if (opt->orientation != Qt::Horizontal)
        {
          int t = fspec.left;
          fspec.left = fspec.top;
          fspec.top = t;
          t = fspec.right;
          fspec.right = fspec.bottom;
          fspec.bottom = t;
        }

        QFont f = QApplication::font();
        if (widget) f = widget->font();
        if (lspec.boldFont) f.setBold(true);

        int iconSize = pixelMetric(PM_SmallIconSize);
        s = sizeCalculated(f,fspec,lspec,sspec,opt->text,
                           opt->icon.isNull() ? QSize() : QSize(iconSize,iconSize));
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
          qstyleoption_cast<const QStyleOptionViewItem*>(option);
      if (opt)
      {
        const QString group = "ItemView";
        const frame_spec fspec = getFrameSpec(group);
        const label_spec lspec = getLabelSpec(group);
        const size_spec sspec = getSizeSpec(group);
        QStyleOptionViewItem::Position pos = opt->decorationPosition;

        s.rheight() += fspec.top + fspec.bottom;
        /* the width is already increased with PM_FocusFrameHMargin */
        //s.rwidth() += fspec.left + fspec.right;

        const QStyleOptionViewItem_v2 *vopt =
            qstyleoption_cast<const QStyleOptionViewItem_v2*>(option);
        bool hasIcon = false;
        if (vopt && (vopt->features & QStyleOptionViewItem_v2::HasDecoration))
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

        s = s.expandedTo(QSize(sspec.minW,sspec.minH));
      }

      // the item text may be inside a button like in Kate's font preferences (see SE_PushButtonContents)
      /*const QStyleOptionViewItem *opt =
          qstyleoption_cast<const QStyleOptionViewItem*>(option);
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
      const size_spec sspec = getSizeSpec("TabFrame");
      s = defaultSize + QSize(fspec.left+fspec.right,
                              fspec.top+fspec.bottom);
      s = s.expandedTo(QSize(sspec.minW,sspec.minH));

      break;
    }

    case CT_GroupBox : {
      const QString group = "GroupBox";

      frame_spec fspec;
      default_frame_spec(fspec);
      label_spec lspec = getLabelSpec(group);
      size_spec sspec;
      default_size_spec(sspec);

      const QStyleOptionGroupBox *opt =
        qstyleoption_cast<const QStyleOptionGroupBox*>(option);

      bool checkable(false);
      if (const QGroupBox *gb = qobject_cast<const QGroupBox*>(widget))
      {
        if (gb->isCheckable())
          checkable = true;
      }
      if (!checkable && opt && (opt->subControls & QStyle::SC_GroupBoxCheckBox)) // QML
        checkable = true;
      if (checkable)
      { // if checkable, don't use lspec.left, use PM_CheckBoxLabelSpacing for spacing
        if (option && option->direction == Qt::RightToLeft)
          lspec.right = 0;
        else
          lspec.left = 0;
      }

      QFont f = QApplication::font();
      if (widget) f = widget->font();
      if (lspec.boldFont) f.setBold(true);
      QSize textSize = sizeCalculated(f,fspec,lspec,sspec,opt? opt->text : QString(),QSize());
      fspec = getFrameSpec(group);
      lspec = getLabelSpec(group);
      int checkWidth = (checkable ? pixelMetric(PM_IndicatorWidth)+pixelMetric(PM_CheckBoxLabelSpacing) : 0);
      int spacing = (tspec_.groupbox_top_label ? 0 : 6 + 10); /* 3px between text and frame and
                                                                 text starts at 10px after the left frame */
      s = QSize(qMax(defaultSize.width(), textSize.width() + checkWidth + spacing)
                  + fspec.left + fspec.right + lspec.left + lspec.right,
                defaultSize.height() + fspec.top + fspec.bottom + lspec.top + lspec.bottom
                  + (tspec_.groupbox_top_label ? 0
                     : qMax(pixelMetric(PM_IndicatorHeight),textSize.height())/2));
      sspec = getSizeSpec(group);
      s = s.expandedTo(QSize(sspec.minW,sspec.minH));

      break;
    }

    /*case CT_ProgressBar : {
      s = defaultSize;
      if (!isKisSlider_ && tspec_.progressbar_thickness > 0)
      {
        const QProgressBar *pb = qobject_cast<const QProgressBar*>(widget);
        if (pb && pb->orientation() == Qt::Vertical)
          s.rwidth() = qMin(tspec_.progressbar_thickness,s.width());
        else
          s.rheight() = qMin(tspec_.progressbar_thickness,s.height());
        return s;
      }

      break;
    }*/

    default : return defaultSize;
  }

  // I'm too cautious to not add this:
  return s.expandedTo(defaultSize);
}

QSize Style::sizeCalculated(const QFont &font,
                            const frame_spec &fspec, // frame spec
                            const label_spec &lspec, // label spec
                            const size_spec &sspec, // size spec
                            const QString &text,
                            const QSize iconSize,
                            // text-icon alignment
                            const Qt::ToolButtonStyle tialign) const
{
  QSize s;
  s.setWidth(fspec.left+fspec.right+lspec.left+lspec.right);
  s.setHeight(fspec.top+fspec.bottom+lspec.top+lspec.bottom);
  if (!text.isEmpty() && lspec.hasShadow)
  {
    s.rwidth() += qAbs(lspec.xshift)+lspec.depth;
    s.rheight() += qAbs(lspec.yshift)+lspec.depth;
  }

  QSize ts = textSize(font, text);
  int tw = ts.width();
  int th = ts.height();

  if (tialign == Qt::ToolButtonIconOnly)
  {
    if (iconSize.isValid())
    {
      s.rwidth() += iconSize.width();
      s.rheight() += iconSize.height();
    }
  }
  else if (tialign == Qt::ToolButtonTextOnly)
  {
    s.rwidth() += tw;
    s.rheight() += th;
  }
  else if (tialign == Qt::ToolButtonTextBesideIcon)
  {
    if (iconSize.isValid())
    {
      s.rwidth() += iconSize.width() + (text.isEmpty() ? 0 : lspec.tispace) + tw;
      s.rheight() += qMax(iconSize.height(), th);
    }
    else
    {
      s.rwidth() +=  tw;
      s.rheight() += th;
    }
  }
  else if (tialign == Qt::ToolButtonTextUnderIcon)
  {
    if (iconSize.isValid())
    {
      s.rwidth() += qMax(iconSize.width(), tw);
      s.rheight() += iconSize.height() + (text.isEmpty() ? 0 : lspec.tispace) + th;
    }
    else
    {
      s.rwidth() += tw;
      s.rheight() += th;
    }
  }

  if (s.height() < sspec.minH)
    s.setHeight(sspec.minH);
  if (s.width() < sspec.minW)
    s.setWidth(sspec.minW);

  return s;
}

QRect Style::subElementRect(SubElement element, const QStyleOption *option, const QWidget *widget) const
{
  switch (element) {
    case SE_CheckBoxFocusRect :
    case SE_RadioButtonFocusRect : {
      const QStyleOptionButton *opt =
          qstyleoption_cast<const QStyleOptionButton*>(option);
      if (opt)
        return opt->rect.adjusted(opt->direction == Qt::RightToLeft ?
                                    0 : pixelMetric(PM_IndicatorWidth)+pixelMetric(PM_CheckBoxLabelSpacing),
                                  0,
                                  opt->direction == Qt::RightToLeft ?
                                    -(pixelMetric(PM_IndicatorWidth)+pixelMetric(PM_CheckBoxLabelSpacing)) : 0,
                                  0);
      else
        return QCommonStyle::subElementRect(element,option,widget);
    }

    case SE_PushButtonFocusRect : {
      const QStyleOptionButton *opt =
          qstyleoption_cast<const QStyleOptionButton*>(option);
      if (opt)
        return opt->rect;
    }

    case SE_ComboBoxFocusRect :
    case SE_SliderFocusRect :
    case SE_ItemViewItemFocusRect : return QRect();

    case SE_HeaderLabel : return option->rect;

    case SE_HeaderArrow : {
      const QString group = "HeaderSection";
      frame_spec fspec = getFrameSpec(group);
      const indicator_spec dspec = getIndicatorSpec(group);
      const label_spec lspec = getLabelSpec(group);
      if (const QStyleOptionHeader *opt = qstyleoption_cast<const QStyleOptionHeader*>(option))
      {
        if (opt->orientation != Qt::Horizontal) return QRect();
        if (opt->position == QStyleOptionHeader::Beginning || opt->position == QStyleOptionHeader::Middle)
        {
          if (option->direction == Qt::RightToLeft)
            fspec.left = 0;
          else
            fspec.right = 0;
        }
      }

      return alignedRect(option->direction,
                         Qt::AlignRight,
                         QSize(option->direction == Qt::RightToLeft ?
                                 fspec.left+lspec.left+dspec.size
                                 : fspec.right+lspec.right+dspec.size,
                               option->rect.height()),
                         option->rect);
    }

    case SE_ProgressBarGroove :
    case SE_ProgressBarLabel : {
       QRect r = option->rect;
       if (!isKisSlider_ && tspec_.progressbar_thickness > 0)
       {
         const QProgressBar *pb = qobject_cast<const QProgressBar*>(widget);
         QSize s;
         if (pb && pb->orientation() == Qt::Vertical)
           s = QSize(qMin(tspec_.progressbar_thickness,r.width()),r.height());
         else
           s = QSize(r.width(),qMin(tspec_.progressbar_thickness,r.height()));
         r = alignedRect(option->direction,Qt::AlignCenter,s,r);
       }
       return r;
    }

    case SE_ProgressBarContents : {
      if (tspec_.spread_progressbar)
        return subElementRect(SE_ProgressBarGroove,option,widget);

      frame_spec fspec = getFrameSpec("Progressbar");
      if (isKisSlider_)
        fspec.right = 0;
      else
      {
        fspec.left = fspec.right = qMin(fspec.left,fspec.right);
      }
      // the vertical progressbar will be made out of the horizontal one
      const QProgressBar *pb = qobject_cast<const QProgressBar*>(widget);
      if (pb && pb->orientation() == Qt::Vertical)
      {
        int top = fspec.top;
        fspec.top = fspec.right;
        int bottom = fspec.bottom;
        fspec.bottom = fspec.left;
        fspec.left = top;
        fspec.right = bottom;
      }

      return interiorRect(subElementRect(SE_ProgressBarGroove,option,widget), fspec);
    }

    case SE_LineEditContents : {
      frame_spec fspec = getFrameSpec("LineEdit");
      label_spec lspec = getLabelSpec("LineEdit");
      lspec.top = qMax(0,lspec.top-1);
      lspec.bottom = qMax(0,lspec.bottom-1);
      const size_spec sspec = getSizeSpec("LineEdit");
      /* no frame when editing itemview texts */
      if (qobject_cast<QAbstractItemView*>(getParent(widget,2)))
      {
        fspec.left = fspec.right = fspec.top = fspec.bottom = 0;
        lspec.left = lspec.right = lspec.top = lspec.bottom = 0;
      }
      else if (widget)
      {
        if (qobject_cast<const QLineEdit*>(widget)
            && !widget->styleSheet().isEmpty() && widget->styleSheet().contains("padding"))
        {
          fspec.left = fspec.right = fspec.top = fspec.bottom = qMin(fspec.left,3);
          lspec.left = lspec.right = qMin(lspec.left,2);
        }
        else
        {
          if (widget->minimumWidth() == widget->maximumWidth())
          {
            fspec.left = qMin(fspec.left,3);
            fspec.right = qMin(fspec.right,3);
            lspec.left = lspec.right = qMin(lspec.left,2);
          }
          if (widget->height() < sizeCalculated(widget->font(),fspec,lspec,sspec,"W",QSize()).height())
          {
            fspec.top = qMin(fspec.left,3);
            fspec.bottom = qMin(fspec.bottom,3);
          }
        }
      }
      if (QAbstractSpinBox *p = qobject_cast<QAbstractSpinBox*>(getParent(widget,1)))
      {
        lspec.right = 0;
        if (!tspec_.vertical_spin_indicators)
        {
          QString maxTxt = spinMaxText(p);
          if (maxTxt.isEmpty() 
              || option->rect.width() < textSize(p->font(),maxTxt).width() + fspec.left
                                        + (p->buttonSymbols() == QAbstractSpinBox::NoButtons ? fspec.right : 0)
              || (p->buttonSymbols() != QAbstractSpinBox::NoButtons
                  && p->width() < option->rect.width() + 2*tspec_.spin_button_width + getFrameSpec("IndicatorSpinBox").right))
          {
            fspec.left = qMin(fspec.left,3);
            fspec.right = qMin(fspec.right,3);
            lspec.left = 0;
            if (p->buttonSymbols() == QAbstractSpinBox::NoButtons)
              lspec.right = 0;
          }
          if (p->height() < fspec.top+fspec.bottom+QFontMetrics(widget->font()).height())
          {
            fspec.top = qMin(fspec.top,3);
            fspec.bottom = qMin(fspec.bottom,3);
          }
        }
        else
        {
          fspec.left = fspec.right = fspec.top = fspec.bottom = qMin(fspec.left,3);
          lspec.left = 0;
        }
      }
      lspec.top = lspec.bottom = 0;
      QRect rect = labelRect(option->rect, fspec, lspec);

      /* in these cases there are capsules */
      if (widget)
      {
        if (QComboBox *cb = qobject_cast<QComboBox*>(widget->parentWidget()))
        {
          rect.adjust(option->direction == Qt::RightToLeft ? -fspec.left : 0,
                      0,
                      option->direction == Qt::RightToLeft ? 0 : fspec.right,
                      0);
          if (option->direction == Qt::RightToLeft)
          {
            const frame_spec fspec1 = getFrameSpec("ComboBox");
            if (widget->width() < cb->width() - COMBO_ARROW_LENGTH - fspec1.left)
              rect.adjust(0,0,fspec.right,0);
          }
          else if (widget->x() > 0)
              rect.adjust(-fspec.left,0,0,0);
        }
        else if (QAbstractSpinBox *p = qobject_cast<QAbstractSpinBox*>(widget->parentWidget()))
        {
          if (p->buttonSymbols() != QAbstractSpinBox::NoButtons)
            rect.adjust(0,0,fspec.right,0);
        }
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
          qstyleoption_cast<const QStyleOptionViewItem*>(option);

      if (opt)
      {
        const QStyleOptionViewItem_v2 *vopt =
            qstyleoption_cast<const QStyleOptionViewItem_v2*>(option);
        bool hasIcon = false;
        if (vopt && (vopt->features & QStyleOptionViewItem_v2::HasDecoration))
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
            const QStyleOptionViewItem_v4 *vopt1 =
              qstyleoption_cast<const QStyleOptionViewItem_v4*>(option);
            if (vopt1)
            {
              QString txt = vopt1->text;
              if (!txt.isEmpty())
              {
                QStringList l = txt.split('\n');
                int txtHeight = 0;
                if (l.size() == 1)
                  txtHeight = QFontMetrics(opt->font).height()*(l.size());
                else
                {
                  txtHeight = QFontMetrics(opt->font).boundingRect(QLatin1Char('M')).height()*1.6;
                  txtHeight *= l.size();
                }
                r = alignedRect(option->direction,
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
          qstyleoption_cast<const QStyleOptionViewItem*>(option);
      if (opt)
      {
        // put the icon inside the frame
        const QStyleOptionViewItem_v2 *vopt =
            qstyleoption_cast<const QStyleOptionViewItem_v2*>(option);
        if (vopt && (vopt->features & QStyleOptionViewItem_v2::HasDecoration))
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
          qstyleoption_cast<const QStyleOptionButton*>(option);
      // Kate's preferences for its default text style
      if (opt && !opt->text.isEmpty() && widget)
      {
        if (qobject_cast<const QAbstractItemView*>(widget))
        {
          const frame_spec fspec = getFrameSpec("PanelButtonCommand");
          label_spec lspec = getLabelSpec("PanelButtonCommand");
          lspec.left = qMax(0,lspec.left-1);
          lspec.top = qMax(0,lspec.top-1);
          lspec.right = qMax(0,lspec.right-1);
          lspec.bottom = qMax(0,lspec.bottom-1);
          r.adjust(-fspec.left-lspec.left,
                   -fspec.top-lspec.top,
                   fspec.right+lspec.right,
                   fspec.bottom+lspec.bottom);
        }
      }
      return r;
    }

    case SE_TabWidgetTabBar : {
      /* Here, we fix some minute miscalculations in QCommonStyle, which can be
         relevant only for centered tabs and when the tabbar base panel is drawn. */
      QRect r;
      if (const QStyleOptionTabWidgetFrame *opt
              = qstyleoption_cast<const QStyleOptionTabWidgetFrame*>(option))
      {
        r.setSize(opt->tabBarSize);
        const uint alingMask = Qt::AlignLeft | Qt::AlignRight | Qt::AlignHCenter;
        QSize leftCornerSize = opt->leftCornerWidgetSize.isValid()
                                 ? opt->leftCornerWidgetSize : QSize(0, 0);
        QSize rightCornerSize = opt->rightCornerWidgetSize.isValid()
                                  ? opt->rightCornerWidgetSize : QSize(0, 0);
        switch (opt->shape) {
          case QTabBar::RoundedNorth:
          case QTabBar::TriangularNorth: {
            r.setWidth(qMin(r.width(), opt->rect.width() - leftCornerSize.width()
                                                         - rightCornerSize.width()));
            switch (styleHint(SH_TabBar_Alignment, opt, widget) & alingMask) {
              default:
              case Qt::AlignLeft:
                r.moveTopLeft(QPoint(leftCornerSize.width(), 0));
                break;
              case Qt::AlignHCenter: {
                r.moveTopLeft(QPoint(opt->rect.center().x() - qRound(r.width()/2.0f) + 1
                                                            + leftCornerSize.width()/2
                                                            - rightCornerSize.width()/2,
                                     0));
                break;
              }
              case Qt::AlignRight:
                r.moveTopLeft(QPoint(opt->rect.width() - opt->tabBarSize.width()
                                                       - rightCornerSize.width(),
                                     0));
                break;
            }
            r = visualRect(opt->direction, opt->rect, r);
            break;
          }
          case QTabBar::RoundedSouth:
          case QTabBar::TriangularSouth: {
            r.setWidth(qMin(r.width(),
                            opt->rect.width() - leftCornerSize.width()
                                              - rightCornerSize.width()));
            switch (styleHint(SH_TabBar_Alignment, opt, widget) & alingMask) {
              default:
              case Qt::AlignLeft: {
                r.moveTopLeft(QPoint(leftCornerSize.width(),
                                     opt->rect.height() - opt->tabBarSize.height()));
                break;
              }
              case Qt::AlignHCenter: {
                r.moveTopLeft(QPoint(opt->rect.center().x() - qRound(r.width() / 2.0f) + 1
                                                            + leftCornerSize.width()/2
                                                            - rightCornerSize.width()/2,
                                     opt->rect.height() - opt->tabBarSize.height()));
                break;
              }
              case Qt::AlignRight: {
                r.moveTopLeft(QPoint(opt->rect.width() - opt->tabBarSize.width()
                                                       - rightCornerSize.width(),
                                     opt->rect.height() - opt->tabBarSize.height()));
                break;
              }
            }
            r = visualRect(opt->direction, opt->rect, r);
            break;
          }
          case QTabBar::RoundedEast:
          case QTabBar::TriangularEast: {
            r.setHeight(qMin(r.height(),
                        opt->rect.height() - leftCornerSize.height()
                                           - rightCornerSize.height()));
            switch (styleHint(SH_TabBar_Alignment, opt, widget) & alingMask) {
              default:
              case Qt::AlignLeft: {
                r.moveTopLeft(QPoint(opt->rect.width() - opt->tabBarSize.width(),
                                     leftCornerSize.height()));
                break;
              }
              case Qt::AlignHCenter: {
                r.moveTopLeft(QPoint(opt->rect.width() - opt->tabBarSize.width(),
                                     opt->rect.center().y() - qRound(r.height() / 2.0f) + 1));
                break;
              }
              case Qt::AlignRight: {
                r.moveTopLeft(QPoint(opt->rect.width() - opt->tabBarSize.width(),
                                     opt->rect.height() - opt->tabBarSize.height()
                                                        - rightCornerSize.height()));
                break;
              }
            }
            break;
          }
          case QTabBar::RoundedWest:
          case QTabBar::TriangularWest: {
            r.setHeight(qMin(r.height(),
                        opt->rect.height() - leftCornerSize.height()
                                           - rightCornerSize.height()));
            switch (styleHint(SH_TabBar_Alignment, opt, widget) & alingMask) {
              default:
              case Qt::AlignLeft: {
                r.moveTopLeft(QPoint(0, leftCornerSize.height()));
                break;
              }
              case Qt::AlignHCenter: {
                r.moveTopLeft(QPoint(0, opt->rect.center().y() - qRound(r.height() / 2.0f) + 1));
                break;
              }
              case Qt::AlignRight: {
                r.moveTopLeft(QPoint(0, opt->rect.height() - opt->tabBarSize.height()
                                                           - rightCornerSize.height()));
                break;
              }
            }
            break;
          }
        }
      }
      return r;
    }

    case SE_TabWidgetTabContents : {
      const frame_spec fspec = getFrameSpec("TabFrame");
      return QCommonStyle::subElementRect(element,option,widget).adjusted(fspec.left,fspec.top,-fspec.right,-fspec.bottom);
    }

    case SE_TabWidgetLeftCorner : {
      QRect r;
      if (const QStyleOptionTabWidgetFrame *twf =
          qstyleoption_cast<const QStyleOptionTabWidgetFrame*>(option))
      {
        QRect paneRect = subElementRect(SE_TabWidgetTabPane, twf, widget);
        switch (twf->shape) {
          case QTabBar::RoundedNorth:
          case QTabBar::TriangularNorth:
            r = QRect(QPoint(paneRect.x(), paneRect.y()-twf->leftCornerWidgetSize.height()),
                      twf->leftCornerWidgetSize);
            break;
          case QTabBar::RoundedSouth:
          case QTabBar::TriangularSouth:
            r = QRect(QPoint(paneRect.x(), paneRect.y()+paneRect.height()),
                      twf->leftCornerWidgetSize);
            break;
          /* WARNING: The Qt documentation says, "Corner widgets are designed for North
             and South tab positions; other orientations are known to not work properly." */
          /*case QTabBar::RoundedWest:
          case QTabBar::TriangularWest:
            r = QRect(QPoint(paneRect.x()-twf->leftCornerWidgetSize.width()
                                         +(option->direction == Qt::RightToLeft ? paneRect.width() : 0),
                             paneRect.y()),
                      twf->leftCornerWidgetSize);
            break;
          case QTabBar::RoundedEast:
          case QTabBar::TriangularEast:
            r = QRect(QPoint(option->direction == Qt::RightToLeft ? paneRect.x()
                                                                  : paneRect.x()+paneRect.width(),
                             paneRect.y()),
                      twf->leftCornerWidgetSize);
            break;*/
          default: break;
        }
        r = visualRect(twf->direction, twf->rect, r);
      }
      if (r.isValid()) return r;
      else return QCommonStyle::subElementRect(element,option,widget);
    }

    case SE_TabWidgetRightCorner : {
      QRect r;
      if (const QStyleOptionTabWidgetFrame *twf =
          qstyleoption_cast<const QStyleOptionTabWidgetFrame*>(option))
      {
        QRect paneRect = subElementRect(SE_TabWidgetTabPane, twf, widget);
        switch (twf->shape) {
          case QTabBar::RoundedNorth:
          case QTabBar::TriangularNorth:
            r = QRect(QPoint(paneRect.x()+paneRect.width()-twf->rightCornerWidgetSize.width(),
                             paneRect.y()-twf->rightCornerWidgetSize.height()),
                      twf->rightCornerWidgetSize);
            break;
          case QTabBar::RoundedSouth:
          case QTabBar::TriangularSouth:
            r = QRect(QPoint(paneRect.x()+paneRect.width()-twf->rightCornerWidgetSize.width(),
                             paneRect.y()+paneRect.height()),
                      twf->rightCornerWidgetSize);
            break;
          /*case QTabBar::RoundedWest:
          case QTabBar::TriangularWest:
            r = QRect(QPoint(paneRect.x()-twf->rightCornerWidgetSize.width()
                                         +(option->direction == Qt::RightToLeft ? paneRect.width() : 0),
                             paneRect.y()+paneRect.height()-twf->rightCornerWidgetSize.height()),
                      twf->rightCornerWidgetSize);
            break;
          case QTabBar::RoundedEast:
          case QTabBar::TriangularEast:
            r = QRect(QPoint(option->direction == Qt::RightToLeft ? paneRect.x()
                                                                  : paneRect.x()+paneRect.width(),
                             paneRect.y()+paneRect.height()-twf->rightCornerWidgetSize.height()),
                      twf->rightCornerWidgetSize);
            break;*/
          default: break;
        }
        r = visualRect(twf->direction, twf->rect, r);
      }
      if (r.isValid()) return r;
      else return QCommonStyle::subElementRect(element,option,widget);
    }

    default : return QCommonStyle::subElementRect(element,option,widget);
  }
}

QRect Style::subControlRect(ComplexControl control,
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
          if (const QStyleOptionTitleBar *tb = qstyleoption_cast<const QStyleOptionTitleBar*>(option))
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
      int sw = tspec_.spin_button_width;
      frame_spec fspec = getFrameSpec("IndicatorSpinBox");
      frame_spec fspecLE = getFrameSpec("LineEdit");
      const QStyleOptionSpinBox *opt = qstyleoption_cast<const QStyleOptionSpinBox*>(option);
      // the measure we used in CC_SpinBox at drawComplexControl() (for QML)
      bool verticalIndicators(tspec_.vertical_spin_indicators || (!widget && opt && opt->frame));

      // a workaround for LibreOffice
      if (isLibreoffice_)
      {
        sw = 12;
        fspec.right = qMin(fspec.right,3);
      }
      else if (!verticalIndicators)
      {
        if (const QAbstractSpinBox *sb = qobject_cast<const QAbstractSpinBox*>(widget))
        {
          if (sb->buttonSymbols() == QAbstractSpinBox::NoButtons)
            sw = 0;
          else // when there isn't enough horizontal space (as in VLC and Pencil)
          {
            QString maxTxt = spinMaxText(sb);
            if (!maxTxt.isEmpty())
            {
              maxTxt = maxTxt + QLatin1Char(' ');
              int txtWidth = textSize(sb->font(),maxTxt).width();
              int rightFrame = w-txtWidth-2*sw-fspecLE.left-2; // 2 for padding
              if (fspec.right > rightFrame)
              {
                sw = 16;
                // in this case, lineedit frame width is set to 3 at PE_PanelLineEdit
                rightFrame = w-txtWidth-2*sw-3-2;
                if (fspec.right > rightFrame)
                {
                  rightFrame = qMax(rightFrame,2);
                  if (rightFrame > 2 || w >= txtWidth+ 2*8 + 2) // otherwise wouldn't help
                  {
                    if (rightFrame == 2) // see PE_IndicatorSpinUp
                      sw = 8;
                    fspec.right = qMin(fspec.right,
                                       qMin(rightFrame,3)); // for a uniform look
                  }
                  else fspec.right = qMin(fspec.right,3); // better than nothing
                }
              }
            }
            else fspec.right = qMin(fspec.right,3);
          }
        }
      }

      if (sw != 0 && verticalIndicators)
      {
        fspecLE.right = qMin(fspecLE.left,3);
        fspec = fspecLE;
        sw = 8;
      }

      // take into account the right frame width
      switch (subControl) {
        case SC_SpinBoxFrame :
          return option->rect;
        case SC_SpinBoxEditField : {
          int margin = 0;
          if (isLibreoffice_)
            margin = qMin(fspecLE.left,3);
          return QRect(x + margin,
                       y,
                       w - (sw + fspec.right) - (verticalIndicators ? 0 : sw),
                       h);
        }
        case SC_SpinBoxUp :
          return QRect(x + w - (sw + fspec.right),
                       y,
                       sw + fspec.right,
                       verticalIndicators ? h/2 + (h%2 ? 1 : 0) : h);
        case SC_SpinBoxDown :
          if (!verticalIndicators)
            return QRect(x + w - (sw + fspec.right) - sw,
                         y,
                         sw,
                         h);
          else
            return QRect(x + w - (sw + fspec.right),
                         y + h/2,
                         sw + fspec.right,
                         h/2 + (h%2 ? 1 : 0));

        default : return QCommonStyle::subControlRect(control,option,subControl,widget);
      }
      break;
    }

    case CC_ComboBox :
      switch (subControl) {
        case SC_ComboBoxFrame : return option->rect;
        case SC_ComboBoxEditField : {
          const QStyleOptionComboBox *opt =
              qstyleoption_cast<const QStyleOptionComboBox*>(option);
          int margin = 0;
          frame_spec fspec;
          if (tspec_.combo_as_lineedit && opt && opt->editable)
            fspec = getFrameSpec("LineEdit");
          else
            fspec = getFrameSpec("ComboBox");
          const label_spec lspec =  getLabelSpec("ComboBox");
          if (isLibreoffice_)
          {
            const frame_spec Fspec = getFrameSpec("LineEdit");
            margin = qMin(Fspec.left,3);
          }
          else
          {
            /* The left icon should respect frame width, text margin
               and text-icon spacing in the editable mode too */
            if (opt && opt->editable && !opt->currentIcon.isNull())
              margin = (option->direction == Qt::RightToLeft ? fspec.right+qMax(0,lspec.right-1)
                                                             : fspec.left+qMax(0,lspec.left-1))
                       + lspec.tispace
                       - (tspec_.combo_as_lineedit ? 0
                          : 3); // it's 4px in qcombobox.cpp -> QComboBoxPrivate::updateLineEditGeometry()
          }
          return QRect(option->direction == Qt::RightToLeft ?
                         x+COMBO_ARROW_LENGTH+fspec.left
                         : x+margin,
                       y,
                       option->direction == Qt::RightToLeft ?
                         w-(COMBO_ARROW_LENGTH+fspec.left)-margin
                         : w-(COMBO_ARROW_LENGTH+fspec.right)-margin,
                       h);
        }
        case SC_ComboBoxArrow : {
          const QStyleOptionComboBox *opt =
              qstyleoption_cast<const QStyleOptionComboBox*>(option);
          frame_spec fspec;
          if (tspec_.combo_as_lineedit && opt && opt->editable)
            fspec = getFrameSpec("LineEdit");
          else
            fspec = getFrameSpec("ComboBox");
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
          if (!tspec_.combo_menu)
          { // level the popup list with the bottom or top edge of the combobox
            int popupMargin = QCommonStyle::pixelMetric(PM_FocusFrameVMargin);
            return option->rect.adjusted(0, -popupMargin, 0, popupMargin);
          }
          else
          { // take into account the space needed by checkbox and icon
            int space = qMin(QCommonStyle::pixelMetric(PM_IndicatorWidth)*pixelRatio_, tspec_.check_size)
                        + tspec_.small_icon_size
                        + pixelMetric(PM_CheckBoxLabelSpacing);
            return option->rect.adjusted(-space, 0, 0, 0);
          }
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
      const QStyleOptionSlider *opt =
          qstyleoption_cast<const QStyleOptionSlider*>(option);
      if (!opt) break;

      int extent = pixelMetric(PM_ScrollBarExtent,option,widget);
      int arrowSize = 0;
      if (tspec_.scroll_arrows
          /* when arrows are present in a different style, as in Gwenview */
          || (widget && widget->style() != this
              && subControl != SC_ScrollBarSubLine && subControl != SC_ScrollBarAddLine // no infinite loop
              && widget->style()->subControlRect(CC_ScrollBar,option,SC_ScrollBarSubLine,widget).isValid()))
      {
        arrowSize = extent;
      }
      bool horiz = (option->state & State_Horizontal);

      int maxLength = 0; // max slider length
      int length = 0; // slider length
      int start = 0; // slider start
      if (subControl == SC_ScrollBarSlider
          || subControl == SC_ScrollBarAddPage
          || subControl == SC_ScrollBarSubPage)
      {
        QRect r = subControlRect(CC_ScrollBar,option,SC_ScrollBarGroove,widget);
        r.getRect(&x,&y,&w,&h);

        if (horiz)
          maxLength = w;
        else
          maxLength = h;
        int minLength = pixelMetric(PM_ScrollBarSliderMin,option,widget);
        if (minLength >= maxLength) minLength = qMax(maxLength-1,16); // 1px for scrolling down
        const int valueRange = opt->maximum - opt->minimum;
        length = maxLength;
        if (opt->minimum != opt->maximum)
        {
          length = (opt->pageStep*maxLength) / (valueRange+opt->pageStep);

          if ((length < minLength) || (valueRange > INT_MAX/2))
            length = minLength;
          if (length > maxLength)
            length = maxLength;
        }

        start = sliderPositionFromValue(opt->minimum,
                                        opt->maximum,
                                        opt->sliderPosition,
                                        maxLength - length,
                                        opt->upsideDown);
      }

      switch (subControl) {
        case SC_ScrollBarGroove :
          if (horiz)
            return QRect(x+arrowSize, y, w-2*arrowSize, h);
          else
            return QRect(x, y+arrowSize, w, h-2*arrowSize);
        case SC_ScrollBarSubLine :
          if (horiz)
            return QRect(option->direction == Qt::RightToLeft ? x+w-arrowSize : x, y,
                         arrowSize, arrowSize);
          else
            return QRect(x, y, arrowSize, arrowSize);
        case SC_ScrollBarAddLine :
          if (horiz)
            return QRect(option->direction == Qt::RightToLeft ? x : x+w-arrowSize, y,
                         arrowSize, arrowSize);
          else
            return QRect(x, y+h-arrowSize, arrowSize, arrowSize);
        case SC_ScrollBarSlider : {
          if (horiz)
            return QRect(opt->direction == Qt::RightToLeft ? x+w-start-length : x+start, y,
                         length, h);
          else
            return QRect(x, y+start, w, length);
        }
        case SC_ScrollBarAddPage : {
          if (horiz)
            return QRect(arrowSize+start+length, 0,
                         maxLength-start-length, extent);
          else
            return QRect(0, arrowSize+start+length,
                         extent, maxLength-start-length);
        }
        case SC_ScrollBarSubPage : {
          if (horiz)
            return QRect(arrowSize, 0, start, extent);
          else
            return QRect(0, arrowSize, extent, start);
        }

        default : return QCommonStyle::subControlRect(control,option,subControl,widget);
      }

      break;
    }

    case CC_Slider : {
      const QStyleOptionSlider *opt =
        qstyleoption_cast<const QStyleOptionSlider*>(option);
      switch (subControl) {
        case SC_SliderGroove : { // sets the clicking area
          if (opt)
          {
            bool horiz = opt->orientation == Qt::Horizontal; // this is more reliable than option->state
            int ticks = opt->tickPosition;
            const int handleThickness = pixelMetric(PM_SliderControlThickness, option, widget);
            if (horiz)
            {
              if (ticks == QSlider::TicksAbove)
                y += SLIDER_TICK_SIZE/2;
              else if (ticks == QSlider::TicksBelow)
                y -= SLIDER_TICK_SIZE/2;
              /* decrease the height of the clicking area to the handle thickness */
              return QRect(x,
                           y+(h-handleThickness)/2,
                           w,
                           handleThickness);
            }
            else
            {
              if (ticks == QSlider::TicksAbove) // left
                x += SLIDER_TICK_SIZE/2;
              else if (ticks == QSlider::TicksBelow) // right
                x -= SLIDER_TICK_SIZE/2;
              return QRect(x+(w-handleThickness)/2,
                           y,
                           handleThickness,
                           h);
            }
          }
        }

        case SC_SliderHandle : {
          if (opt)
          {
            bool horiz = opt->orientation == Qt::Horizontal;
            subControlRect(CC_Slider,option,SC_SliderGroove,widget).getRect(&x,&y,&w,&h);
            const int len = pixelMetric(PM_SliderLength, option, widget);
            const int sliderPos = sliderPositionFromValue (opt->minimum,
                                                           opt->maximum,
                                                           opt->sliderPosition,
                                                           (horiz ? w : h) - len,
                                                           opt->upsideDown);
            if (horiz)
              return QRect(x+sliderPos, y, len, h);
            else
              return QRect(x, y+sliderPos, w, len);
          }

        }

        default : return QCommonStyle::subControlRect(control,option,subControl,widget);
      }

      break;
    }

    case CC_Dial : {
      switch (subControl) {
        case SC_DialGroove : return alignedRect(option->direction,
                                                Qt::AlignHCenter | Qt::AlignVCenter,
                                                QSize(qMin(option->rect.width(),option->rect.height()),
                                                      qMin(option->rect.width(),option->rect.height())),
                                                option->rect);
        case SC_DialHandle : {
          const QStyleOptionSlider *opt =
            qstyleoption_cast<const QStyleOptionSlider*>(option);

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
            qstyleoption_cast<const QStyleOptionToolButton*>(option);

          if (opt)
          {
            if (const QToolButton *tb = qobject_cast<const QToolButton*>(widget))
            {
              bool rtl(opt->direction == Qt::RightToLeft);
              if (tb->popupMode() == QToolButton::MenuButtonPopup)
              {
                const QString group = "DropDownButton";
                frame_spec fspec = getFrameSpec(group);
                indicator_spec dspec = getIndicatorSpec(group);
                /* limit the arrow size */
                dspec.size = qMin(dspec.size, h);
                /* lack of space */
                if (opt && opt->toolButtonStyle == Qt::ToolButtonIconOnly && !opt->icon.isNull())
                {
                  const frame_spec fspec1 = getFrameSpec("PanelButtonTool");
                  if (w < opt->iconSize.width()+fspec1.left
                          +(rtl ? fspec.left : fspec.right)+dspec.size+2*TOOL_BUTTON_ARROW_MARGIN)
                  {
                    if (rtl)
                      fspec.left = qMin(fspec.left,3);
                    else
                      fspec.right = qMin(fspec.right,3);
                    dspec.size = qMin(dspec.size,TOOL_BUTTON_ARROW_SIZE);
                  }
                }
                return option->rect.adjusted(rtl ? fspec.left+dspec.size+2*TOOL_BUTTON_ARROW_MARGIN : 0,
                                             0,
                                             rtl ? 0 : -fspec.right-dspec.size-2*TOOL_BUTTON_ARROW_MARGIN,
                                             0);
              }
              else if ((tb->popupMode() == QToolButton::InstantPopup
                        || tb->popupMode() == QToolButton::DelayedPopup)
                       && (opt->features & QStyleOptionToolButton::HasMenu))
              {
                const QString group = "PanelButtonTool";
                frame_spec fspec = getFrameSpec(group);
                indicator_spec dspec = getIndicatorSpec(group);
                label_spec lspec = getLabelSpec(group);
                // -> CE_ToolButtonLabel
                if (qobject_cast<QAbstractItemView*>(getParent(widget,2)))
                {
                  fspec.left = qMin(fspec.left,3);
                  fspec.right = qMin(fspec.right,3);
                  lspec.tispace = qMin(lspec.tispace,3);
                }
                /* lack of space */
                if (opt->toolButtonStyle == Qt::ToolButtonIconOnly && !opt->icon.isNull())
                {
                  if (w < opt->iconSize.width()+fspec.left+fspec.right
                          +dspec.size+ pixelMetric(PM_HeaderMargin)+lspec.tispace)
                  {
                    if (rtl)
                      fspec.left = qMin(fspec.left,3);
                    else
                      fspec.right = qMin(fspec.right,3);
                    dspec.size = qMin(dspec.size,TOOL_BUTTON_ARROW_SIZE-TOOL_BUTTON_ARROW_OVERLAP);
                    lspec.tispace=0;
                  }
                }
                return option->rect.adjusted(rtl ?
                                               lspec.tispace+dspec.size
                                                 // -> CE_ToolButtonLabel
                                                 + (opt->toolButtonStyle == Qt::ToolButtonTextOnly ?
                                                    qMin(fspec.left,fspec.right) : fspec.left)
                                                 + pixelMetric(PM_HeaderMargin)
                                               : 0,
                                             0,
                                             rtl ?
                                               0
                                               : - lspec.tispace-dspec.size
                                                   - (opt->toolButtonStyle == Qt::ToolButtonTextOnly ?
                                                      qMin(fspec.left,fspec.right) : fspec.right)
                                                   - pixelMetric(PM_HeaderMargin),
                                             0);
              }
            }
          }

          return option->rect;
        }

        case SC_ToolButtonMenu : {
          const QStyleOptionToolButton *opt =
          qstyleoption_cast<const QStyleOptionToolButton*>(option);

          if (opt)
          {
            if (const QToolButton *tb = qobject_cast<const QToolButton*>(widget))
            {
              bool rtl(opt->direction == Qt::RightToLeft);
              if (tb->popupMode() == QToolButton::MenuButtonPopup)
              {
                const QString group = "DropDownButton";
                frame_spec fspec = getFrameSpec(group);
                indicator_spec dspec = getIndicatorSpec(group);
                /* limit the arrow size */
                dspec.size = qMin(dspec.size, h);
                /* lack of space */
                if (opt && opt->toolButtonStyle == Qt::ToolButtonIconOnly && !opt->icon.isNull())
                {
                  const frame_spec fspec1 = getFrameSpec("PanelButtonTool");
                  if (w < opt->iconSize.width()+fspec1.left
                          +(rtl ? fspec.left : fspec.right)+dspec.size+2*TOOL_BUTTON_ARROW_MARGIN)
                  {
                    if (rtl)
                      fspec.left = qMin(fspec.left,3);
                    else
                      fspec.right = qMin(fspec.right,3);
                    dspec.size = qMin(dspec.size,TOOL_BUTTON_ARROW_SIZE);
                  }
                }
                int l = (rtl ? fspec.left : fspec.right)+dspec.size+2*TOOL_BUTTON_ARROW_MARGIN;
                return QRect(rtl ? x : x+w-l,
                             y,l,h);
              }
              else if ((tb->popupMode() == QToolButton::InstantPopup
                        || tb->popupMode() == QToolButton::DelayedPopup)
                       && (opt->features & QStyleOptionToolButton::HasMenu))
              {
                const QString group = "PanelButtonTool";
                frame_spec fspec = getFrameSpec(group);
                indicator_spec dspec = getIndicatorSpec(group);
                // -> CE_ToolButtonLabel
                if (qobject_cast<QAbstractItemView*>(getParent(widget,2)))
                {
                  fspec.left = qMin(fspec.left,3);
                  fspec.right = qMin(fspec.right,3);
                }
                /* lack of space */
                if (opt->toolButtonStyle == Qt::ToolButtonIconOnly && !opt->icon.isNull())
                {
                  const label_spec lspec = getLabelSpec(group);
                  if (w < opt->iconSize.width()+fspec.left+fspec.right
                          +dspec.size+ pixelMetric(PM_HeaderMargin)+lspec.tispace)
                  {
                    if (rtl)
                      fspec.left = qMin(fspec.left,3);
                    else
                      fspec.right = qMin(fspec.right,3);
                    dspec.size = qMin(dspec.size,TOOL_BUTTON_ARROW_SIZE);
                  }
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

          return option->rect;
        }

        default : return QCommonStyle::subControlRect(control,option,subControl,widget);
      }

      break;
    }

    case CC_GroupBox : {
      const QStyleOptionGroupBox *opt =
        qstyleoption_cast<const QStyleOptionGroupBox*>(option);
      if (opt)
      {
        frame_spec fspec;
        default_frame_spec(fspec);
        label_spec lspec = getLabelSpec("GroupBox");
        size_spec sspec;
        default_size_spec(sspec);

        bool rtl(option->direction == Qt::RightToLeft);
        bool checkable = false;
        if (const QGroupBox *gb = qobject_cast<const QGroupBox*>(widget))
        {
          if (gb->isCheckable())
            checkable = true;
        }
        if (!checkable && (opt->subControls & QStyle::SC_GroupBoxCheckBox)) // QML
          checkable = true;
        if (checkable)
        { // if checkable, don't use lspec.left, use PM_CheckBoxLabelSpacing for spacing
          if (rtl)
            lspec.right = 0;
          else
            lspec.left = 0;
        }
        QFont f = QApplication::font();
        if (widget) f = widget->font();
        if (lspec.boldFont) f.setBold(true);
        QSize textSize = sizeCalculated(f,fspec,lspec,sspec,opt->text,QSize());
        int checkWidth = (checkable ? pixelMetric(PM_IndicatorWidth)+pixelMetric(PM_CheckBoxLabelSpacing) : 0);
        int checkHeight = pixelMetric(PM_IndicatorHeight);
        fspec = getFrameSpec("GroupBox");
        int labelMargin = (tspec_.groupbox_top_label ? 0 : (rtl ? fspec.right : fspec.left) + 10);

        switch (subControl) {
          case SC_GroupBoxCheckBox : {
            int delta = 0;
            if (textSize.height() > checkHeight)
              delta = (textSize.height() - checkHeight)/2;
            return QRect(rtl ? x+w - labelMargin - pixelMetric(PM_IndicatorWidth) : x + labelMargin,
                         y + delta,
                         pixelMetric(PM_IndicatorWidth),
                         checkHeight);
          }
          case SC_GroupBoxLabel : {
            int delta = 0;
            if (checkHeight > textSize.height())
              delta = (checkHeight - textSize.height())/2;
            int spacing = (tspec_.groupbox_top_label ? 0 : 6); // 3px between text and frame
            return QRect(rtl ? x+w - labelMargin - checkWidth - textSize.width() - spacing
                             : x + labelMargin + checkWidth,
                         y + delta,
                         textSize.width() + spacing,
                         textSize.height());
          }
          case SC_GroupBoxContents : {
            int top = 0;
            if (!tspec_.groupbox_top_label)
              top = qMax(checkHeight,textSize.height())/2;
            lspec = getLabelSpec("GroupBox");
            return labelRect(subControlRect(control,option,SC_GroupBoxFrame,widget), fspec, lspec)
                   .adjusted(0,top,0,0);
          }
          case SC_GroupBoxFrame : {
            int top = qMax(checkHeight,textSize.height());
            if (!tspec_.groupbox_top_label && fspec.top < top)
              top = (top - fspec.top)/2;
            return QRect(x,
                         y + top,
                         w,
                         h - top);
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
QIcon Style::standardIconImplementation(QStyle::StandardPixmap standardIcon,
                                        const QStyleOption *option,
                                        const QWidget *widget) const
#else
QIcon Style::standardIcon(QStyle::StandardPixmap standardIcon,
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

      /* If this is a dark-and-light theme, we set the pen color of
         the painter to white as a sign to use at PE_IndicatorArrowRight. */
      if (themeRndr_ && themeRndr_->isValid())
      { // for toolbar, widget is NULL but for menubar, it isn't
        QColor col;
        if (!widget || mergedToolbarHeight(widget) > 0)
          col = getFromRGBA(getLabelSpec("Toolbar").normalColor);
        else
          col = getFromRGBA(getLabelSpec("MenuBar").normalColor);
        if (enoughContrast(col, getFromRGBA(cspec_.windowTextColor)))
          painter.setPen(QColor(Qt::white));
      }

      QStyleOption opt;
      opt.rect = QRect(0,0,s,s);
      opt.state |= State_Enabled; // other states don't work for toolbar FIXME: why?

      drawPrimitive(QApplication::layoutDirection() == Qt::RightToLeft ?
                      PE_IndicatorArrowLeft : PE_IndicatorArrowRight,
                    &opt,&painter,0);

      return QIcon(pm);
    }
    case SP_ToolBarVerticalExtensionButton : {
      int s = pixelMetric(PM_ToolBarExtensionExtent);
      QPixmap pm(QSize(s,s));
      pm.fill(Qt::transparent);

      QPainter painter(&pm);

      if (!hspec_.single_top_toolbar
          && themeRndr_ && themeRndr_->isValid()
          && enoughContrast(getFromRGBA(getLabelSpec("Toolbar").normalColor),
                            getFromRGBA(cspec_.windowTextColor)))
      {
        painter.setPen(QColor(Qt::white));
      }

      QStyleOption opt;
      opt.rect = QRect(0,0,s,s);
      opt.state |= State_Enabled;

      drawPrimitive(PE_IndicatorArrowDown,&opt,&painter,0);

      return QIcon(pm);
    }
    case SP_TitleBarMinButton : {
      int s = 12*pixelRatio_;
      QPixmap pm(QSize(s,s));
      pm.fill(Qt::transparent);

      QPainter painter(&pm);

      QString status("normal");
      if (option)
        status = (option->state & State_Enabled) ?
                   (option->state & State_Sunken) ? "pressed" :
                   (option->state & State_MouseOver) ? "focused" : "normal"
                 : "disabled";
      if (renderElement(&painter,getIndicatorSpec("TitleBar").element+"-minimize-"+status,QRect(0,0,s,s)))
        return QIcon(pm);
      else break;
    }
    case SP_TitleBarMaxButton : {
      int s = 12*pixelRatio_;
      QPixmap pm(QSize(s,s));
      pm.fill(Qt::transparent);

      QPainter painter(&pm);

      if (renderElement(&painter,getIndicatorSpec("TitleBar").element+"-maximize-normal",QRect(0,0,s,s)))
        return QIcon(pm);
      else break;
    }
    case SP_DockWidgetCloseButton :
    case SP_TitleBarCloseButton : {
      int s = 12*pixelRatio_;
      QPixmap pm(QSize(s,s));
      pm.fill(Qt::transparent);

      QPainter painter(&pm);

      QString status("normal");
      if (qstyleoption_cast<const QStyleOptionButton*>(option))
        status = (option->state & State_Enabled) ?
                   (option->state & State_Sunken) ? "pressed" :
                   (option->state & State_MouseOver) ? "focused" : "normal"
                 : "disabled";
      if (renderElement(&painter,getIndicatorSpec("TitleBar").element+"-close-"+status,QRect(0,0,s,s)))
        return QIcon(pm);
      else break;
    }
    case SP_TitleBarMenuButton : {
      int s = 12*pixelRatio_;
      QPixmap pm(QSize(s,s));
      pm.fill(Qt::transparent);

      QPainter painter(&pm);

      if (renderElement(&painter,getIndicatorSpec("TitleBar").element+"-menu-normal",QRect(0,0,s,s)))
        return QIcon(pm);
      else break;
    }
    case SP_TitleBarNormalButton : {
      int s = 12*pixelRatio_;
      QPixmap pm(QSize(s,s));
      pm.fill(Qt::transparent);

      QPainter painter(&pm);

      QString status("normal");
      if (qstyleoption_cast<const QStyleOptionButton*>(option))
        status = (option->state & State_Enabled) ?
                   (option->state & State_Sunken) ? "pressed" :
                   (option->state & State_MouseOver) ? "focused" : "normal"
                 : "disabled";
      if (renderElement(&painter,getIndicatorSpec("TitleBar").element+"-restore-"+status,QRect(0,0,s,s)))
        return QIcon(pm);
      else break;
    }
    /* in these cases too, Qt sets the size to 24 in standardPixmap() */
    case SP_DialogCancelButton :
    case SP_DialogNoButton : {
       QIcon icn = QIcon::fromTheme(QLatin1String("dialog-cancel"),
                                    QIcon::fromTheme(QLatin1String("process-stop")));
       if (!icn.isNull()) return icn;
       else break;
    }
    case SP_DialogSaveButton : {
       QIcon icn = QIcon::fromTheme(QLatin1String("document-save"));
       if (!icn.isNull()) return icn;
       else break;
    }
    case SP_DialogResetButton : {
      QIcon icn = QIcon::fromTheme(QLatin1String("edit-clear"));
      if (!icn.isNull()) return icn;
      else break;
    }
    case SP_DialogHelpButton : {
      QIcon icn = QIcon::fromTheme(QLatin1String("help-contents"));
      if (!icn.isNull()) return icn;
      else break;
    }

    default : break;
  }

#if QT_VERSION < 0x050000
  return QCommonStyle::standardIconImplementation(standardIcon,option,widget);
#else
  return QCommonStyle::standardIcon(standardIcon,option,widget);
#endif
}

static inline uint qt_intensity(uint r, uint g, uint b)
{
  // 30% red, 59% green, 11% blue
  return (77 * r + 150 * g + 28 * b) / 255;
}

QPixmap Style::generatedIconPixmap(QIcon::Mode iconMode,
                                   const QPixmap &pixmap,
                                   const QStyleOption *opt) const
{
  switch (iconMode) {
    case QIcon::Disabled: {
      QImage im = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);

      // Create a colortable based on the background (black -> bg -> white)
      QColor bg = opt->palette.color(QPalette::Disabled, QPalette::Window);
      int red = bg.red();
      int green = bg.green();
      int blue = bg.blue();
      uchar reds[256], greens[256], blues[256];
      for (int i=0; i<128; ++i)
      {
        reds[i]   = uchar((red   * (i<<1)) >> 8);
        greens[i] = uchar((green * (i<<1)) >> 8);
        blues[i]  = uchar((blue  * (i<<1)) >> 8);
      }
      for (int i=0; i<128; ++i)
      {
        reds[i+128]   = uchar(qMin(red   + (i << 1), 255));
        greens[i+128] = uchar(qMin(green + (i << 1), 255));
        blues[i+128]  = uchar(qMin(blue  + (i << 1), 255));
      }

      int intensity = qt_intensity(red, green, blue);
      const int factor = 191;

      // High intensity colors needs dark shifting in the color table, while
      // low intensity colors needs light shifting. This is to increase the
      // percieved contrast.
      if ((red - factor > green && red - factor > blue)
          || (green - factor > red && green - factor > blue)
          || (blue - factor > red && blue - factor > green))
        intensity = qMin(255, intensity + 91);
      else if (intensity <= 128)
        intensity -= 51;

      for (int y=0; y<im.height(); ++y)
      {
        QRgb *scanLine = (QRgb*)im.scanLine(y);
        for (int x=0; x<im.width(); ++x)
        {
          QRgb pixel = *scanLine;
          // Calculate color table index, taking intensity adjustment
          // and a magic offset into account.
          uint ci = uint(qGray(pixel)/3 + (130 - intensity / 3));
          *scanLine = qRgba(reds[ci], greens[ci], blues[ci], qAlpha(pixel));
          ++scanLine;
        }
      }

      return QPixmap::fromImage(im);
    }
    case QIcon::Selected: {
      if (hspec_.no_selection_tint) break;
      QImage img = pixmap.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
      QColor color = opt->palette.color(QPalette::Normal, QPalette::Highlight);
      color.setAlphaF(qreal(0.3));
      QPainter painter(&img);
      painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
      painter.fillRect(0, 0, img.width(), img.height(), color);
      painter.end();
      return QPixmap::fromImage(img);
    }
    case QIcon::Active:
      return pixmap;
    default: break;
  }
  return pixmap;
}

QRect Style::squaredRect(const QRect &r) const {
  int e = (r.width() > r.height()) ? r.height() : r.width();
  return QRect(r.x(),r.y(),e,e);
}

/* Here, instead of using the render() method of QSvgRenderer
   directly, we first make a QPixmap for drawing SVG elements. */
static inline void drawSvgElement(QSvgRenderer *renderer, QPainter *painter, QRect bounds, QString element)
{
  QPixmap pixmap = QPixmap(bounds.width(), bounds.height());
  pixmap.fill(QColor(Qt::transparent));
  QPainter p;
  p.begin(&pixmap);
  renderer->render(&p,element);
  p.end();
  painter->drawPixmap(bounds,pixmap);
}

bool Style::renderElement(QPainter *painter,
                          const QString &element,
                          const QRect &bounds,
                          int hsize, int vsize, // pattern sizes
                          bool usePixmap // first make a QPixmap for drawing
                         ) const
{
  if (element.isEmpty() || !bounds.isValid() || painter->opacity() == 0)
    return false;

  QSvgRenderer *renderer = 0;
  QString element_ (element);

  if (themeRndr_ && themeRndr_->isValid()
      && (themeRndr_->elementExists(element_)
          || themeRndr_->elementExists(element_.remove("-inactive"))
          // fall back to the normal state if other states aren't found
          || themeRndr_->elementExists(element_.replace("-toggled","-normal")
                                               .replace("-pressed","-normal")
                                               .replace("-focused","-normal"))))
  {
    renderer = themeRndr_;
  }
  /* always use the default SVG image (which doesn't contain
     any object for the inactive state) as fallback */
  else if (defaultRndr_ && defaultRndr_->isValid())
  {
    element_ = element;
    if (defaultRndr_->elementExists(element_.remove("-inactive")))
      renderer = defaultRndr_;
  }
  if (!renderer) return false;

  if (hsize > 0 || vsize > 0)
  {
    /* draw the pattern over the background
       if a separate pattern element exists */
    if (renderer->elementExists(element_+"-pattern"))
    {
      if (usePixmap)
        drawSvgElement(renderer,painter,bounds,element_);
      else
        renderer->render(painter,element_,bounds);
      element_ = element_+"-pattern";
    }

    int width = hsize > 0 ? hsize : bounds.width();
    int height = vsize > 0 ? vsize : bounds.height();
    QString str = QString("%1-%2-%3").arg(element_)
                                     .arg(QString().setNum(width))
                                     .arg(QString().setNum(height));
    QPixmap pixmap;
    if (!QPixmapCache::find(str, &pixmap))
    {
      pixmap = QPixmap(width, height);
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
  {
    if (usePixmap)
      drawSvgElement(renderer,painter,bounds,element_);
    else
      renderer->render(painter,element_,bounds);
  }

  return true;
}

void Style::renderSliderTick(QPainter *painter,
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

  if (themeRndr_ && themeRndr_->isValid()
      && (themeRndr_->elementExists(element_)
          || (element_.contains("-inactive")
              && themeRndr_->elementExists(element_.remove("-inactive")))))
  {
    renderer = themeRndr_;
  }
  else if (defaultRndr_ && defaultRndr_->isValid()
           && defaultRndr_->elementExists(element_.remove("-inactive")))
  {
    renderer = defaultRndr_;
  }
  else
    return;

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

void Style::renderFrame(QPainter *painter,
                        const QRect &bounds, // frame bounds
                        frame_spec fspec, // frame spec
                        const QString &element, // frame SVG element
                        int d, // distance of the attached tab from the edge
                        int l, // length of the attached tab
                        int f1, // width of tab's left frame
                        int f2, // width of tab's right frame
                        int tp, // tab position
                        bool grouped, // is among grouped similar widgets?
                        bool usePixmap, // first make a QPixmap for drawing
                        bool drawBorder // draw a border with maximum rounding if possible
                       ) const
{
  if (!bounds.isValid() || !fspec.hasFrame || painter->opacity() == 0)
    return;

  int x0,y0,x1,y1,w,h;
  bounds.getRect(&x0,&y0,&w,&h);
  /* for "historical" reasons, we have to add 1
     (-> QRect documentation) */
  x1 = bounds.bottomRight().x() + 1;
  y1 = bounds.bottomRight().y() + 1;

  int Left,Top,Right,Bottom;
  Left = Top = Right = Bottom = 0;

  bool isInactive(false);
  QStringList states;
  states << "-normal" << "-focused" << "-pressed" << "-toggled";
  QString state;
  QStringList list = element.split("-");
  int count = list.count();
  if (count > 2 && list.at(count - 1) == "inactive")
  {
    state = "-" + list.at(count - 2);
    isInactive = true;
  }
  else if (count > 1)
  {
    state = "-" + list.at(count - 1);
    if (!states.contains(state))
      state = QString();
  }

  // search for expanded frame element
  QString realElement = fspec.expandedElement;
  if (fspec.expansion <= 0 || realElement.isEmpty())
    realElement = element;
  else if (!state.isEmpty())
  {
    realElement += state;
    if (isInactive)
      realElement += "-inactive";
  }
  else if (element.endsWith("-default")) // default button
    realElement += "-default";

  QString element1(realElement);
  QString element0(realElement); // used just for checking
  element0 = "expand-"+element0;
  if (fspec.hasCapsule && fspec.capsuleH != 2)
    grouped = true;
  int e = grouped ? h : qMin(h,w);
  bool drawExpanded = false;
  /* still round the corners if the "expand-" element is found */
  if (fspec.expansion > 0 &&
      (e <= fspec.expansion || (themeRndr_ && themeRndr_->isValid()
                                && (themeRndr_->elementExists(element0.remove("-inactive"))
                                    // fall back to the normal state
                                    || (!state.isEmpty()
                                        && themeRndr_->elementExists(element0.replace(state,"-normal")))))))
  {
    drawExpanded = true;
    fspec.left = fspec.leftExpanded;
    fspec.right = fspec.rightExpanded;
    fspec.top = fspec.topExpanded;
    fspec.bottom = fspec.bottomExpanded;
  }
  if (!isLibreoffice_ && fspec.expansion > 0 && drawExpanded
      && (!fspec.hasCapsule || fspec.capsuleV == 2)
      /* there's no right/left expanded element */
      && (h <= 2*w || (fspec.capsuleH != 1 && fspec.capsuleH != -1)))
  {
    e = qMin(e,fspec.expansion);
    int H = h;
    if (grouped) H = e;
    if (!fspec.hasCapsule || fspec.capsuleH == 2)
    {
      /* to get smoother gradients, we use QTransform in this special case
         but not when the rect is grouped (as in grouped toolbuttons inside
         vertical toolbars or small progressbar indicators */
      if (h > w && !grouped)
      {
        QRect r;
        r.setRect(y0, x0, h, w);
        QTransform m;
        m.scale(-1,1);
        m.rotate(90);
        painter->save();
        painter->setTransform(m, true);
        renderFrame(painter,r,fspec,realElement,d,l,f1,f2,tp,grouped,usePixmap);
        painter->restore();
        return;
      }
      if (h > w && grouped)
        e = qMin(e,w);  // only here e may be greater than w
      if (e%2 == 0)
      {
        Left = Top = Right = Bottom = e/2;
      }
      else
      {
        Left = Top = (e+1)/2;
        Right = Bottom = (e-1)/2;
      }
    }
    else
    {
      int X = 0;
      /* here, this is always true: (H <= 2*w || fspec.capsuleH == 0) */
      if (H%2 == 0)
      {
        X = Top = Bottom = H/2;
      }
      else
      {
        X = Top = (H+1)/2;
        Bottom = (H-1)/2;
      }
      if (fspec.capsuleH == -1)
      {
        Left = X;
        Right = qMin(fspec.right,w/2);
      }
      else if (fspec.capsuleH == 1)
      {
        Right = X;
        Left = qMin(fspec.left,w/2);
      }
    }
    element0 = "border-"+realElement;
    if (drawBorder && themeRndr_ && themeRndr_->isValid()
        && (themeRndr_->elementExists(element0.remove("-inactive")+"-top")
            || (!state.isEmpty() && themeRndr_->elementExists(element0.replace(state,"-normal")+"-top"))))
    {
      element1 = element0;
      if (isInactive)
        element1 = element1 + "-inactive";
    }
    else
    {
      element0 = "expand-"+realElement;
      if (themeRndr_ && themeRndr_->isValid()
          && (themeRndr_->elementExists(element0.remove("-inactive")+"-top")
              || (!state.isEmpty() && themeRndr_->elementExists(element0.replace(state,"-normal")+"-top"))))
      {
        element1 = element0;
        if (isInactive)
          element1 = element1 + "-inactive";
        drawBorder = false;
      }
      else drawBorder = false; // don't waste CPU time
    }
  }
  else
  {
    drawBorder = false;
    drawExpanded = false;
    Left = fspec.left;
    Right = fspec.right;
    Top = fspec.top;
    Bottom = fspec.bottom;

    /* extreme cases */
    if (fspec.left + fspec.right > w)
    {
      if (fspec.hasCapsule && fspec.capsuleH != 2)
      {
        if (fspec.capsuleH == -1)
        {
          if (fspec.left > w) Left = w;
        }
        else if (fspec.capsuleH == 1)
        {
          if (fspec.right > w) Right = w;
        }
      }
      else
      {
        if (w%2 == 0)
        {
          Left = Right = w/2;
        }
        else
        {
          Left = (w+1)/2;
          Right = (w-1)/2;
        }
      }
    }
    if (fspec.top + fspec.bottom > h)
    {
      if (fspec.hasCapsule && fspec.capsuleV != 2)
      {
        if (fspec.capsuleV == -1)
        {
          if (fspec.top > h) Top = h;
        }
        else if (fspec.capsuleV == 1)
        {
          if (fspec.bottom > h) Bottom = h;
        }
      }
      else
      {
        if (h%2 == 0)
        {
          Top = Bottom = h/2;
        }
        else
        {
          Top = (h+1)/2;
          Bottom = (h-1)/2;
        }
      }
    }

    if (Left == 0 && Top == 0 && Right == 0 && Bottom == 0) return;
  }

  if (!fspec.hasCapsule || (fspec.capsuleH == 2 && fspec.capsuleV == 2))
  {
    /*********
     ** Top **
     *********/
    if (l > 0 && tp == QTabWidget::North)
    {
      renderElement(painter,element1+"-top",
                    QRect(x0+Left,
                          y0,
                          d-x0-Left,
                          Top),
                    fspec.ps,0,usePixmap);
      renderElement(painter,element1+"-top",
                    QRect(d+l,
                          y0,
                          x0+w-Left-d-l,
                          Top),
                    fspec.ps,0,usePixmap);
     /* left and right junctions */
     if (d-x0-Left >= 0)
       renderElement(painter,element1+"-top-leftjunct",
                      QRect(d,
                            y0,
                            f1,
                            Top),
                      0,0,usePixmap);
     if (x0+w-Left-d-l >= 0)
       renderElement(painter,element1+"-top-rightjunct",
                      QRect(d+l-f2,
                            y0,
                            f2,
                            Top),
                      0,0,usePixmap);
    }
    else
      renderElement(painter,element1+"-top",
                    QRect(x0+Left,y0,w-Left-Right,Top),
                    fspec.ps,0,usePixmap);

    /************
     ** Bottom **
     ************/
    if (l > 0 && tp == QTabWidget::South)
    {
      renderElement(painter,element1+"-bottom",
                    QRect(x0+Left,
                          y1-Bottom,
                          d-x0-Left,
                          Bottom),
                    fspec.ps,0,usePixmap);
      renderElement(painter,element1+"-bottom",
                    QRect(d+l,
                          y1-Bottom,
                          x0+w-Left-d-l,
                          Bottom),
                    fspec.ps,0,usePixmap);
      if (d-x0-Left >= 0)
        renderElement(painter,element1+"-bottom-leftjunct",
                      QRect(d,
                            y1-Bottom,
                            f2,
                            Bottom),
                      0,0,usePixmap);
      if (x0+w-Left-d-l >= 0)
        renderElement(painter,element1+"-bottom-rightjunct",
                      QRect(d+l-f1,
                            y1-Bottom,
                            f1,
                            Bottom),
                      0,0,usePixmap);
    }
    else
      renderElement(painter,element1+"-bottom",
                    QRect(x0+Left,y1-Bottom,w-Left-Right,Bottom),
                    fspec.ps,0,usePixmap);

    /**********
     ** Left **
     **********/
    if (l > 0 && tp == QTabWidget::West)
    {
      renderElement(painter,element1+"-left",
                    QRect(x0,
                          y0+Top,
                          Left,
                          d-y0-Top),
                    0,fspec.ps,usePixmap);
      renderElement(painter,element1+"-left",
                    QRect(x0,
                          d+l,
                          Left,
                          y0+h-Bottom-d-l),
                    0,fspec.ps,usePixmap);
      if (y0+h-Bottom-d-l >= 0)
        renderElement(painter,element1+"-left-leftjunct",
                      QRect(x0,
                            d+l-f2,
                            Left,
                            f2),
                      0,0,usePixmap);
      if (d-y0-Top >= 0)
        renderElement(painter,element1+"-left-rightjunct",
                      QRect(x0,
                            d,
                            Left,
                            f1),
                      0,0,usePixmap);
    }
    else
      renderElement(painter,element1+"-left",
                    QRect(x0,y0+Top,Left,h-Top-Bottom),
                    0,fspec.ps,usePixmap);

    /***********
     ** Right **
     ***********/
    if (l > 0 && tp == QTabWidget::East)
    {
      renderElement(painter,element1+"-right",
                    QRect(x1-Right,
                          y0+Top,
                          Right,
                          d-y0-Top),
                    0,fspec.ps,usePixmap);
      renderElement(painter,element1+"-right",
                    QRect(x1-Right,
                          d+l,
                          Right,
                          y0+h-Bottom-d-l),
                    0,fspec.ps,usePixmap);
      if (d-y0-Top >= 0)
        renderElement(painter,element1+"-right-leftjunct",
                      QRect(x1-Right,
                            d,
                            Right,
                            f1),
                      0,0,usePixmap);
      if (y0+h-Bottom-d-l >= 0)
        renderElement(painter,element1+"-right-rightjunct",
                      QRect(x1-Right,
                            d+l-f2,
                            Right,
                            f2),
                      0,0,usePixmap);
    }
    else
      renderElement(painter,element1+"-right",
                    QRect(x1-Right,y0+Top,Right,h-Top-Bottom),
                    0,fspec.ps,usePixmap);

    /*************
     ** Topleft **
     *************/
    QString  element_ = element1+"-topleft";
    if (l > 0)
    {
      if (tp == QTabWidget::North && d < Left)
        element_ = element1+"-left";
      else if (tp == QTabWidget::West && d < Top)
        element_ = element1+"-top";
    }
    renderElement(painter,element_,
                  QRect(x0,y0,Left,Top),
                  0,0,usePixmap);

    /**************
     ** Topright **
     **************/
    element_ = element1+"-topright";
    if (l > 0)
    {
      if (tp == QTabWidget::North && w-d-l < Right)
        element_ = element1+"-right";
      else if (tp == QTabWidget::East && d < Top)
        element_ = element1+"-top";
    }
    renderElement(painter,element_,
                  QRect(x1-Right,y0,Right,Top),
                  0,0,usePixmap);

    /****************
     ** Bottomleft **
     ****************/
    element_ = element1+"-bottomleft";
    if (l > 0)
    {
      if (tp == QTabWidget::South && d < Left)
        element_ = element1+"-left";
      else if (tp == QTabWidget::West && h-d-l < Bottom)
        element_ = element1+"-bottom";
    }
    renderElement(painter,element_,
                  QRect(x0,y1-Bottom,Left,Bottom),
                  0,0,usePixmap);

    /*****************
     ** Bottomright **
     *****************/
    element_ = element1+"-bottomright";
    if (l > 0)
    {
      if (tp == QTabWidget::South && w-d-l < Right)
        element_ = element1+"-right";
      else if (tp == QTabWidget::East && h-d-l < Bottom)
        element_ = element1+"-bottom";
    }
    renderElement(painter,element_,
                  QRect(x1-Right,y1-Bottom,Right,Bottom),
                  0,0,usePixmap);
  }
  else // with capsule
  {
    if (fspec.capsuleH == 0 && fspec.capsuleV == 0)
      return;

    /* to simplify calculations, we first get margins */
    int left = 0, right = 0, top = 0, bottom = 0;
    if (fspec.capsuleH == -1 || fspec.capsuleH == 2)
      left = Left;
    if (fspec.capsuleH == 1 || fspec.capsuleH == 2)
      right = Right;
    if (fspec.capsuleV == -1  || fspec.capsuleV == 2)
      top = Top;
    if (fspec.capsuleV == 1 || fspec.capsuleV == 2)
      bottom = Bottom;

    /*********
     ** Top **
     *********/
    if (top > 0)
    {
      renderElement(painter,element1+"-top",
                    QRect(x0+left,y0,w-left-right,top),
                    fspec.ps,0,usePixmap);

      // topleft corner
      if (left > 0)
        renderElement(painter,element1+"-topleft",
                      QRect(x0,y0,left,top),
                      0,0,usePixmap);
      // topright corner
      if (right > 0)
        renderElement(painter,element1+"-topright",
                      QRect(x1-right,y0,right,top),
                      0,0,usePixmap);
    }

    /************
     ** Bottom **
     ************/
    if (bottom > 0)
    {
      renderElement(painter,element1+"-bottom",
                    QRect(x0+left,y1-bottom,w-left-right,bottom),
                    fspec.ps,0,usePixmap);

      // bottomleft corner
      if (left > 0)
        renderElement(painter,element1+"-bottomleft",
                      QRect(x0,y1-bottom,left,bottom),
                      0,0,usePixmap);
      // bottomright corner
      if (right > 0)
        renderElement(painter,element1+"-bottomright",
                      QRect(x1-right,y1-bottom,right,bottom),
                      0,0,usePixmap);
    }

    /**********
     ** Left **
     **********/
    if (left > 0)
      renderElement(painter,element1+"-left",
                    QRect(x0,y0+top,left,h-top-bottom),
                    0,fspec.ps,usePixmap);

    /***********
     ** Right **
     ***********/
    if (right > 0)
      renderElement(painter,element1+"-right",
                    QRect(x1-right,y0+top,right,h-top-bottom),
                    0,fspec.ps,usePixmap);
  }


  if (drawExpanded && Top + Bottom != h) // when needed and there is space...
  { // ... draw the "interior"
    if (grouped && fspec.hasCapsule)
    {
      if (fspec.capsuleH == 0)
        Right = Left = 0;
      else if (fspec.capsuleH == -1)
        Right = 0;
      else if (fspec.capsuleH == 1)
        Left = 0;
    }
    renderElement(painter,element1,
                  bounds.adjusted(Left,Top,-Right,-Bottom),
                  0,0,usePixmap);
  }
  if (drawBorder) // draw inside this rectangle to make a border
  {
    /* the expansion should be less here; otherwise, the border wouldn't be smooth */
    frame_spec Fspec = fspec;
    Fspec.expansion = fspec.expansion - fspec.top - fspec.bottom;
    if (Fspec.expansion <= 0) Fspec.expansion = 1;
    renderFrame(painter,
                bounds.adjusted((fspec.hasCapsule && (fspec.capsuleH == 1 || fspec.capsuleH == 0)) ? 0 : fspec.left,
                                fspec.top,
                                (fspec.hasCapsule && (fspec.capsuleH == -1 || fspec.capsuleH == 0)) ?  0: -fspec.right,
                                -fspec.bottom),
                Fspec,element,d,l,f1,f2,tp,grouped,usePixmap,false); // this time, don't draw any border
  }
}

void Style::renderInterior(QPainter *painter,
                           const QRect &bounds, // frame bounds
                           const frame_spec &fspec, // frame spec
                           const interior_spec &ispec, // interior spec
                           const QString &element, // interior SVG element
                           bool grouped, // is among grouped similar widgets?
                           bool usePixmap // first make a QPixmap for drawing
                          ) const
{
  if (!bounds.isValid() || !ispec.hasInterior || painter->opacity() == 0)
    return;

  int w = bounds.width(); int h = bounds.height();
  if (!isLibreoffice_ && fspec.expansion > 0 && !ispec.element.isEmpty())
  {
    if (fspec.hasCapsule && fspec.capsuleH != 2)
      grouped = true;
    int e = grouped ? h : qMin(h,w);
    QString frameElement(fspec.expandedElement);
    if (frameElement.isEmpty())
      frameElement = fspec.element;
    QString element0(element);
    /* the interior used for partial frame expansion has the frame name */
    element0 = element0.remove("-inactive").replace(ispec.element, frameElement);
    element0 = "expand-"+element0;
    if ((e <= fspec.expansion || (themeRndr_ && themeRndr_->isValid()
                                  && (themeRndr_->elementExists(element0)
                                      || themeRndr_->elementExists(element0.replace("-toggled","-normal")
                                                                           .replace("-pressed","-normal")
                                                                           .replace("-focused","-normal")))))
        && (!fspec.hasCapsule || fspec.capsuleV == 2)
        /* there's no right/left expanded element */
        && (h <= 2*w || (fspec.capsuleH != 1 && fspec.capsuleH != -1)))
    {
      return;
    }
  }

  /* extreme cases */
  if (fspec.hasCapsule// && (fspec.capsuleH != 2 || fspec.capsuleV != 2)
      && ((fspec.capsuleH == -1 && fspec.left >= w)
          || (fspec.capsuleH == 1 && fspec.right >= w)
          || (fspec.capsuleV == -1 && fspec.top >= h)
          || (fspec.capsuleV == 1 && fspec.bottom >= h)))
  {
      return;
  }

  renderElement(painter,element,interiorRect(bounds,fspec),
                ispec.px,ispec.py,usePixmap);
}

void Style::renderIndicator(QPainter *painter,
                            const QRect &bounds, // frame bounds
                            const frame_spec &fspec, // frame spec
                            const indicator_spec &dspec, // indicator spec
                            const QString &element, // indicator SVG element
                            Qt::LayoutDirection ld,
                            Qt::Alignment alignment) const
{
  if (!bounds.isValid()) return;
  const QRect interior = interiorRect(bounds,fspec);
  QRect sq = squaredRect(interior);
  if (!sq.isValid())
    sq = squaredRect(bounds);
  /* make the indicator smaller if there isn't enough space */
  int s = (sq.width() > dspec.size) ? dspec.size : sq.width();

  renderElement(painter,element,
                alignedRect(ld,alignment,QSize(s,s),interior));
}

void Style::renderLabel(
                        const QStyleOption *option,
                        QPainter *painter,
                        const QRect &bounds, // frame bounds
                        const frame_spec &fspec, // frame spec
                        const label_spec &lspec, // label spec
                        int talign, // text alignment
                        const QString &text,
                        QPalette::ColorRole textRole, // text color role
                        int state, // widget state (0->disabled, 1->normal, 2->focused, 3->pressed, 4->toggled)
                        const QPixmap &px,
                        QSize iconSize,
                        const Qt::ToolButtonStyle tialign // relative positions of text and icon
                       ) const
{
  // compute text and icon rect
  QRect r;
  if (/*!isPlasma_ &&*/ // we ignore Plasma text margins just for push and tool buttons and menubars
      tialign != Qt::ToolButtonIconOnly
      && !text.isEmpty())
    r = labelRect(bounds,fspec,lspec);
  else
    r = interiorRect(bounds,fspec);

  if (!r.isValid())
    return;

  if (px.isNull() || !iconSize.isValid())
    iconSize = QSize(0,0);

  QRect ricon = r;
  QRect rtext = r;
  Qt::LayoutDirection ld = option->direction;

  if (tialign == Qt::ToolButtonTextBesideIcon)
  {
    ricon = alignedRect(ld,
                        Qt::AlignVCenter | Qt::AlignLeft,
                        iconSize,
                        r);
    rtext = QRect(ld == Qt::RightToLeft ?
                    r.x()
                    : r.x()+iconSize.width() + (px.isNull() ? 0 : lspec.tispace),
                  r.y(),
                  r.width()-ricon.width() - (px.isNull() ? 0 : lspec.tispace),
                  r.height());
  }
  else if (tialign == Qt::ToolButtonTextUnderIcon)
  {
    ricon = alignedRect(ld,
                        Qt::AlignTop | Qt::AlignHCenter,
                        iconSize,
                        r);
    rtext = QRect(r.x(),
                  r.y()+iconSize.height() + (px.isNull() ? 0 : lspec.tispace),
                  r.width(),
                  r.height()-ricon.height() - (px.isNull() ? 0 : lspec.tispace));
  }
  else if (tialign == Qt::ToolButtonIconOnly)
  {
    ricon = alignedRect(ld,
                        Qt::AlignCenter,
                        iconSize,
                        r);
  }

  if (text.isEmpty())
  {
    ricon = alignedRect(ld,
                        Qt::AlignCenter,
                        iconSize,
                        r);
  }

  if (tialign != Qt::ToolButtonTextOnly && !px.isNull())
  {
    QRect iconRect = alignedRect(ld, Qt::AlignCenter, px.size()/pixelRatio_, ricon);

    if (!(option->state & State_Enabled))
    {
      qreal opacityPercentage = hspec_.disabled_icon_opacity;
      if (opacityPercentage < 100)
        painter->drawPixmap(iconRect,translucentPixmap(px, opacityPercentage));
      else
        painter->drawPixmap(iconRect,px);
    }
    else
    {
      qreal tintPercentage = hspec_.tint_on_mouseover;
      if (tintPercentage > 0 && (option->state & State_MouseOver))
        painter->drawPixmap(iconRect, tintedPixmap(option,px,tintPercentage));
      else
        painter->drawPixmap(iconRect,px);
    }
  }

  if (((isPlasma_ && px.isNull()) // Why do some Plasma toolbuttons pretend to have only icons?
       || tialign != Qt::ToolButtonIconOnly)
      && !text.isEmpty())
  {
    // draw text based on its direction, not based on the layout direction
    painter->save();
    if (text.isRightToLeft())
    {
      painter->setLayoutDirection(Qt::RightToLeft);
      if (option->direction == Qt::LeftToRight)
      {
        if (talign & Qt::AlignLeft)
        {
          talign &= ~Qt::AlignLeft;
          talign |= Qt::AlignRight;
        }
        else if (talign & Qt::AlignRight)
        {
          talign &= ~Qt::AlignRight;
          talign |= Qt::AlignLeft;
        }
      }
    }
    else
    {
      painter->setLayoutDirection(Qt::LeftToRight);
      if (option->direction == Qt::RightToLeft)
      {
        if (talign & Qt::AlignLeft)
        {
          talign &= ~Qt::AlignLeft;
          talign |= Qt::AlignRight;
        }
        else if (talign & Qt::AlignRight)
        {
          talign &= ~Qt::AlignRight;
          talign |= Qt::AlignLeft;
        }
      }
    }

    if (lspec.boldFont)
    {
      QFont f(painter->font());
      f.setBold(true);
      painter->save();
      painter->setFont(f);
    }
    if (lspec.italicFont)
    {
      QFont f(painter->font());
      f.setItalic(true);
      painter->save();
      painter->setFont(f);
    }

    if (state != 0 && !(isPlasma_ && tialign == Qt::ToolButtonIconOnly))
    {
      QColor normalColor = getFromRGBA(lspec.normalColor);
      QColor focusColor = getFromRGBA(lspec.focusColor);
      QColor pressColor = getFromRGBA(lspec.pressColor);
      QColor toggleColor = getFromRGBA(lspec.toggleColor);
      QColor progColor = getFromRGBA(cspec_.progressIndicatorTextColor);

      if (lspec.hasShadow)
      {
        QColor shadowColor = getFromRGBA(lspec.shadowColor);
        /* the shadow should have enough contrast with the text */
        if (shadowColor.isValid()
            && ((state == 1 && (!normalColor.isValid() || enoughContrast(normalColor, shadowColor)))
                || (state == 2 && (!focusColor.isValid() || enoughContrast(focusColor, shadowColor)))
                || (state == 3 && (!pressColor.isValid() || enoughContrast(pressColor, shadowColor)))
                || (state == 4 && (!toggleColor.isValid() || enoughContrast(toggleColor, shadowColor)))
                || (state == -1 && (!progColor.isValid() || enoughContrast(progColor, shadowColor)))))
        {
          painter->save();
          if (lspec.a < 255)
            shadowColor.setAlpha(lspec.a);
          painter->setPen(QPen(shadowColor));
          for (int i=0; i<lspec.depth; i++)
            painter->drawText(rtext.adjusted(lspec.xshift+i,lspec.yshift+i,0,0),talign,text);
          painter->restore();
        }
      }

      if (state == 1 && normalColor.isValid())
      {
        painter->save();
        painter->setPen(QPen(normalColor));
        painter->drawText(rtext,talign,text);
        painter->restore();
        if (lspec.boldFont)
          painter->restore();
        if (lspec.italicFont)
          painter->restore();
        painter->restore();
        return;
      }
      else if (state == 2 && focusColor.isValid())
      {
        painter->save();
        painter->setPen(QPen(focusColor));
        painter->drawText(rtext,talign,text);
        painter->restore();
        if (lspec.boldFont)
          painter->restore();
        if (lspec.italicFont)
          painter->restore();
        painter->restore();
        return;
      }
      else if (state == 3 && pressColor.isValid())
      {
        painter->save();
        painter->setPen(QPen(pressColor));
        painter->drawText(rtext,talign,text);
        painter->restore();
        if (lspec.boldFont)
          painter->restore();
        if (lspec.italicFont)
          painter->restore();
        painter->restore();
        return;
      }
      else if (state == 4 && toggleColor.isValid())
      {
        painter->save();
        painter->setPen(QPen(toggleColor));
        painter->drawText(rtext,talign,text);
        painter->restore();
        if (lspec.boldFont)
          painter->restore();
        if (lspec.italicFont)
          painter->restore();
        painter->restore();
        return;
      }
      else if (state == -1 && progColor.isValid())
      {
        painter->save();
        painter->setPen(progColor);
        painter->drawText(rtext,talign,text);
        painter->restore();
        if (lspec.boldFont)
          painter->restore();
        if (lspec.italicFont)
          painter->restore();
        painter->restore();
        return;
      }
    }

    QCommonStyle::drawItemText(painter,
                               rtext,
                               talign,
                               option->palette,
                               state == 0 ? false: true,
                               text,
                               textRole);

    if (lspec.boldFont)
      painter->restore();
    if (lspec.italicFont)
      painter->restore();
    painter->restore();
  }
}

QPixmap Style::getPixmapFromIcon(const QIcon &icon,
                                 const QIcon::Mode iconmode,
                                 const QIcon::State iconstate,
                                 QSize iconSize) const
{
  if (icon.isNull()) return QPixmap();
  /* we don't want a too big pixmap because it would
     result in a malformed icon when made smaller */
  bool hdpi(false);
#if QT_VERSION >= 0x050500
  if (qApp->testAttribute(Qt::AA_UseHighDpiPixmaps))
    hdpi = true;
#endif
  QPixmap px = icon.pixmap(hdpi ? iconSize/pixelRatio_ // QPixmap::setDevicePixelRatio() is used too (-> qicon.cpp)
                                : iconSize*pixelRatio_,iconmode,iconstate);
  if (hdpi && (px.size() == iconSize // not from icon theme
               || px.size().width() < iconSize.width())) // when pixelRatio_ is odd FIXME: why?!
    px = icon.pixmap(iconSize,iconmode,iconstate);
  return px;
}

QPixmap Style::tintedPixmap(const QStyleOption *option,
                            const QPixmap &px,
                            const qreal tintPercentage) const
{ // -> generatedIconPixmap()
  if (!option || px.isNull()) return QPixmap();
  if (tintPercentage <= 0) return px;
  QImage img = px.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
  QColor tintColor = option->palette.color(QPalette::Active, QPalette::Highlight);
  tintColor.setAlphaF(tintPercentage/100);
  QPainter p(&img);
  p.setCompositionMode(QPainter::CompositionMode_SourceAtop);
  p.fillRect(0, 0, img.width(), img.height(), tintColor);
  p.end();
  return QPixmap::fromImage(img);
}

QPixmap Style::translucentPixmap(const QPixmap &px,
                                 const qreal opacityPercentage) const
{ // -> generatedIconPixmap()
  if (px.isNull()) return QPixmap();
  QImage img = px.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::transparent);
  QPainter p(&img);
  p.setOpacity(opacityPercentage/100);
  p.drawPixmap(0, 0, px);
  p.end();
  return QPixmap::fromImage(img);
}

QRect Style::interiorRect(const QRect &bounds, frame_spec fspec) const
{
  if (!fspec.hasCapsule || (fspec.capsuleH == 2 && fspec.capsuleV == 2))
    return bounds.adjusted(fspec.left,fspec.top,-fspec.right,-fspec.bottom);
  else
  {
    int left = 0, right = 0, top = 0, bottom = 0;
    if (fspec.capsuleH == -1)
      left = fspec.left;
    else if (fspec.capsuleH == 1)
      right = fspec.right;
    else if (fspec.capsuleH == 2)
    {
      left = fspec.left;
      right = fspec.right;
    }
    if (fspec.capsuleV == -1)
      top = fspec.top;
    else if (fspec.capsuleV == 1)
      bottom = fspec.bottom;
    else if (fspec.capsuleV == 2)
    {
      top = fspec.top;
      bottom = fspec.bottom;
    }
    return bounds.adjusted(left,top,-right,-bottom);
  }
}

inline frame_spec Style::getFrameSpec(const QString &widgetName) const
{
  return settings_->getFrameSpec(widgetName);
}

inline interior_spec Style::getInteriorSpec(const QString &widgetName) const
{
  return settings_->getInteriorSpec(widgetName);
}

inline indicator_spec Style::getIndicatorSpec(const QString &widgetName) const
{
  return settings_->getIndicatorSpec(widgetName);
}

inline label_spec Style::getLabelSpec(const QString &widgetName) const
{
  label_spec lspec = settings_->getLabelSpec(widgetName);
  if (QApplication::layoutDirection() == Qt::RightToLeft)
  {
    int l = lspec.left;
    lspec.left = lspec.right;
    lspec.right = l;
  }
  return lspec;
}

inline size_spec Style::getSizeSpec(const QString &widgetName) const
{
  return settings_->getSizeSpec(widgetName);
}
}
