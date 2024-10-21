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

#include "Kvantum5.h"

#include <QDir>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>
#include <QTimer>
#include <QSvgRenderer>
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
//#include <QAbstractButton>
//#include <QAbstractItemView>
#include <QHeaderView>
#include <QDockWidget>
#include <QDial>
//#include <QMdiArea>
#include <QMdiSubWindow>
#include <QToolBox>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QDateTimeEdit>
//#include <QBitmap>
#include <QtMath>
#include <QMenuBar>
#include <QGraphicsView>
#include <QStandardPaths>
#include <QItemSelectionModel>
#include <QDialogButtonBox> // for dialog buttons layout
#include <QFormLayout>
#include <QSurfaceFormat>
#include <QWindow>
#include <QScreen> // for isCursorOutsideWidget()
#include <QProxyStyle> // only inside setSurfaceFormat()

#define M_PI 3.14159265358979323846
#define DISABLED_OPACITY 0.7
#define COMBO_ARROW_LENGTH 20
#define INDICATOR_MARGIN 2
#define BUTTON_ARROW_MARGIN 3 // for menu arrow
#define TOOL_BUTTON_ARROW_SIZE 10 // when there isn't enough space (~ PM_MenuButtonIndicator)
#define TOOL_BUTTON_ARROW_OVERLAP 4 // when there isn't enough space
#define LIMITED_EXPANSION 14 // when the frame expansion should be limited
#define MIN_CONTRAST_RATIO 3.5

#define OPACITY_STEP 20 // percent

namespace Kvantum
{

// Taken from https://www.w3.org/TR/WCAG20/.
// It isn't related to HSL lightness.
static inline qreal luminance(const QColor &col)
{
  /* all divided by 255 */
  qreal R = col.redF();
  qreal G = col.greenF();
  qreal B = col.blueF();

  if(R <= 0.03928) R = R/12.92; else R = qPow((R + 0.055)/1.055, 2.4);
  if(G <= 0.03928) G = G/12.92; else G = qPow((G + 0.055)/1.055, 2.4);
  if(B <= 0.03928) B = B/12.92; else B = qPow((B + 0.055)/1.055, 2.4);

  return 0.2126*R + 0.7152*G + 0.0722*B;
}

bool Style::enoughContrast(const QColor &col1, const QColor &col2) const
{
  if (!col1.isValid() || !col2.isValid()) return false;
  qreal rl1 = luminance(col1);
  qreal rl2 = luminance(col2);
  if ((qMax(rl1,rl2) + 0.05) / (qMin(rl1,rl2) + 0.05) < MIN_CONTRAST_RATIO)
    return false;
  return true;
}

QColor Style::overlayColor(const QColor &bgCol, const QColor &overlayCol) const
{
  if (!overlayCol.isValid()) return QColor(0,0,0);
  if (!bgCol.isValid()) return overlayCol;

  qreal a1 = overlayCol.alphaF();
  if (a1 == 1.0) return overlayCol;
  qreal a0  = bgCol.alphaF();
  qreal a = (1.0 - a1) * a0 + a1;

  QColor res;
  res.setAlphaF(a);
  res.setRedF(((1.0 - a1) * a0 * bgCol.redF() + a1 * overlayCol.redF()) / a);
  res.setGreenF(((1.0 - a1) * a0 *bgCol.greenF() + a1 * overlayCol.greenF()) / a);
  res.setBlueF(((1.0 - a1) * a0 * bgCol.blueF() + a1 * overlayCol.blueF()) / a);

  return res;
}

/* Qt >= 5.2 accepts #ARGB as the color name but most apps use #RGBA.
   Here we get the alpha from #RGBA if it exists. */
QColor Style::getFromRGBA(const QString &str) const
{
  QColor col(str);
  if (str.isEmpty() || !(str.size() == 9 && str.startsWith(QLatin1String("#"))))
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

/* Qt >= 5.2 gives #AARRGGBB, while we want #RRGGBBAA. */
static inline QString getName(const QColor &col)
{
  QString colName = col.name();
  long alpha = col.alpha();
  if (alpha < 255)
    colName += QString::number(alpha, 16);
  return colName;
}

Style::Style(bool useDark) : QCommonStyle()
{
  opacityTimer_ = opacityTimerOut_ = nullptr;
  animationOpacity_ = animationOpacityOut_ = 100;
  animationStartState_ = animationStartStateOut_ = "normal";
  animatedWidget_ = animatedWidgetOut_ = nullptr;

  settings_ = defaultSettings_ = themeSettings_ = nullptr;
  defaultRndr_ = themeRndr_ = nullptr;

  gtkDesktop_ = false;
  noComposite_ = false;

  QString homeDir = QDir::homePath();

  /* this is just for protection against a bad sudo */
  char * _xdg_config_home = getenv("XDG_CONFIG_HOME");
  if (!_xdg_config_home)
    xdg_config_home = QString("%1/.config").arg(homeDir);
  else
    xdg_config_home = QString(_xdg_config_home);

  QString theme;
  QString themeChooserFile = QString("%1/Kvantum/kvantum.kvconfig").arg(xdg_config_home);
  if (!QFile::exists(themeChooserFile))
  { // go to a global config file
    themeChooserFile = QString();
    QStringList confList = QStandardPaths::standardLocations(QStandardPaths::ConfigLocation);
    confList.removeOne(xdg_config_home);
    for (const QString &thisConf : static_cast<const QStringList&>(confList))
    {
      QString thisFile = QString("%1/Kvantum/kvantum.kvconfig").arg(thisConf);
      if (QFile::exists(thisFile))
      {
        themeChooserFile = thisFile;
        break;
      }
    }
  }
  if (!themeChooserFile.isEmpty())
  {
    QSettings themeChooser(themeChooserFile,QSettings::NativeFormat);
    if (themeChooser.status() == QSettings::NoError)
    {
      if (themeChooser.contains(QStringLiteral("theme")))
        theme = themeChooser.value("theme").toString();
      /* check if this app has a specific theme assigned to it */
      QString appName = qApp->applicationName();
      if (appName != "kvantummanager" && appName != "kvantumpreview")
      {
        themeChooser.beginGroup("Applications");
        QStringList list = themeChooser.childKeys();
        for (int i = 0; i < list.count(); ++i)
        {
          if (themeChooser.value(list.at(i)).toStringList().contains(appName, Qt::CaseInsensitive))
          {
            theme = list.at(i);
            break;
          }
        }
        themeChooser.endGroup();
      }
    }
  }

  setBuiltinDefaultTheme();
  setTheme(theme, useDark);

  tspec_ = settings_->getThemeSpec();
  hspec_ = settings_->getHacksSpec();
  cspec_ = settings_->getColorSpec();

  QList<QByteArray> desktopList = qgetenv("XDG_CURRENT_DESKTOP").toLower().split(':');
  QSet<QByteArray> desktop(desktopList.begin(), desktopList.end());
  QSet<QByteArray> gtkDesktops = QSet<QByteArray>() << "gnome" << "pantheon";
  gtkDesktop_ = gtkDesktops.intersects(desktop);

  if (tspec_.respect_DE)
  {
    if (gtkDesktop_)
    {
      hspec_.iconless_pushbutton = true;
      hspec_.iconless_menu = true;
      tspec_.blurring = false;
      //tspec_.x11drag = WindowManager::DRAG_MENUBAR_AND_PRIMARY_TOOLBAR;
    }
    else if (desktop.contains("kde"))
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
        KDESettings.beginGroup(QStringLiteral("DialogIcons"));
        v = KDESettings.value(QStringLiteral("Size"));
        KDESettings.endGroup();
        if (v.isValid())
        {
          iconSize = v.toInt();
          if (iconSize > 0 && iconSize <= 256)
            tspec_.large_icon_size = iconSize;
        }
        else
          tspec_.large_icon_size = 32;
        KDESettings.beginGroup(QStringLiteral("SmallIcons"));
        v = KDESettings.value(QStringLiteral("Size"));
        KDESettings.endGroup();
        if (v.isValid())
        {
          iconSize = v.toInt();
          if (iconSize > 0 && iconSize <= 256)
            tspec_.small_icon_size = iconSize;
        }
        else
          tspec_.small_icon_size = 16;
        KDESettings.beginGroup(QStringLiteral("KDE"));
        v = KDESettings.value(QStringLiteral("ScrollbarLeftClickNavigatesByPage"));
        KDESettings.endGroup();
        hspec_.middle_click_scroll = (!v.isValid() || v.toBool());
      }
    }
  }

  isPlasma_ = false;
  isLibreoffice_ = false;
  isDolphin_ = false;
  isPcmanfm_ = false;
  isKrita_ = false;
  subApp_ = false;
  isOpaque_ = false;
  ticklessSliderHandleSize_ = -1;
  isKisSlider_ = false;

  if (tspec_.animate_states)
  {
    opacityTimer_ = new QTimer(this);
    opacityTimerOut_ = new QTimer(this);
    connect(opacityTimer_, &QTimer::timeout, this, &Style::setAnimationOpacity);
    connect(opacityTimerOut_, &QTimer::timeout, this, &Style::setAnimationOpacityOut);
  }

  itsShortcutHandler_ = nullptr;
  if (tspec_.alt_mnemonic)
    itsShortcutHandler_ = new ShortcutHandler(this);

  // decide, once for all, about active indicator, connecting active tabs to others and using floating tabs
  hasActiveIndicator_ = joinedActiveTab_ = joinedActiveFloatingTab_ = hasFloatingTabs_ = false;
  if (themeRndr_ && themeRndr_->isValid())
  {
    if (themeRndr_->elementExists(getIndicatorSpec(QStringLiteral("Tab")).element+"-close-toggled"))
      hasActiveIndicator_ = true;
    if (themeRndr_->elementExists("floating-"+getInteriorSpec(QStringLiteral("Tab")).element+"-normal"))
      hasFloatingTabs_ = true;
    if (tspec_.joined_inactive_tabs)
    {
      QString sepName = getFrameSpec(QStringLiteral("Tab")).element + "-separator";
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

  // decide about view-item colors once for all
  hasInactiveSelItemCol_ = toggledItemHasContrast_ = false;
  if (!tspec_.no_inactiveness)
  {
    const label_spec lspec = getLabelSpec(QStringLiteral("ItemView"));
    QColor toggleInactiveCol = getFromRGBA(lspec.toggleInactiveColor);
    if (toggleInactiveCol.isValid())
    {
      QColor toggleActiveCol = getFromRGBA(lspec.toggleColor);
      if (toggleActiveCol.isValid() && toggleActiveCol != toggleInactiveCol)
      {
        hasInactiveSelItemCol_ = true;
        if (enoughContrast(toggleActiveCol, getFromRGBA(lspec.pressColor)))
          toggledItemHasContrast_ = true;
      }
    }
  }

  itsWindowManager_ = nullptr;
  blurHelper_ = nullptr;

  if (tspec_.x11drag)
  {
    itsWindowManager_ = new WindowManager(this, tspec_.x11drag, tspec_.drag_from_buttons);
    itsWindowManager_->initialize();
  }

  if (tspec_.blurring)
  {
    if (tspec_.menu_shadow_depth > 0)
      getShadow(QStringLiteral("Menu"), getMenuMargin(true), getMenuMargin(false));
    if (tspec_.tooltip_shadow_depth > 0)
    {
      const frame_spec fspec = getFrameSpec(QStringLiteral("ToolTip"));
      int thickness = qMax(qMax(fspec.top,fspec.bottom), qMax(fspec.left,fspec.right));
      thickness += tspec_.tooltip_shadow_depth;
      getShadow(QStringLiteral("ToolTip"), thickness);
    }
    blurHelper_ = new BlurHelper(this, menuShadow_, tooltipShadow_,
                                 tspec_.menu_blur_radius, tspec_.tooltip_blur_radius,
                                 tspec_.contrast, tspec_.intensity, tspec_.saturation,
                                 hspec_.blur_only_active_window);
  }

  cachedOption_ = nullptr;
}

Style::~Style()
{
  if (cachedOption_)
  {
    delete cachedOption_;
    cachedOption_ = nullptr;
  }

  QHash<const QObject*, Animation*>::iterator i = animations_.begin();
  while (i != animations_.end())
  {
    QHash<const QObject*, Animation*>::iterator prev = i;
    ++i;
    Animation *animation = animations_.take(prev.key());
    if (animation)
    { // deleting should be done after stopping
      animation->stop();
      delete animation;
      animation = nullptr;
    }
  }

  /* all the following timers have "this" as their parent
     but are explicitly deleted here only to be listed */
  if (opacityTimer_)
  {
    opacityTimer_->stop();
    delete opacityTimer_;
    opacityTimer_ = nullptr;
  }
  if (opacityTimerOut_)
  {
    opacityTimerOut_->stop();
    delete opacityTimerOut_;
    opacityTimerOut_ = nullptr;
  }

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
    defaultSettings_ = nullptr;
  }
  if (defaultRndr_)
  {
    delete defaultRndr_;
    defaultRndr_ = nullptr;
  }

  defaultSettings_ = new ThemeConfig(QStringLiteral(":/Kvantum/default.kvconfig"));
  defaultRndr_ = new QSvgRenderer();
  defaultRndr_->load(QStringLiteral(":/Kvantum/default.svg"));
}

static inline bool isThemeDir(const QString &path, const QString &themeName)
{
  if (themeName.isEmpty()) return false;
  if (path.endsWith(QLatin1String("/Kvantum")))
  {
    if (QFile::exists(path + QString("/%1/%1.kvconfig").arg(themeName))
        || QFile::exists(path + QString("/%1/%1.svg").arg(themeName)))
    {
      return true;
    }
  }
  else if (QFile::exists(path + QString("/%1/Kvantum/%1.kvconfig").arg(themeName))
           || QFile::exists(path + QString("/%1/Kvantum/%1.svg").arg(themeName)))
  {
    return true;
  }
  return false;
}

void Style::setTheme(const QString &baseThemeName, bool useDark)
{
  if (themeSettings_)
  {
    delete themeSettings_;
    themeSettings_ = nullptr;
  }
  if (themeRndr_)
  {
    delete themeRndr_;
    themeRndr_ = nullptr;
  }

  if (!baseThemeName.isNull() && !baseThemeName.isEmpty()
      /* "Default" is reserved by Kvantum Manager for copied default theme */
      && baseThemeName != "Default"
      /* "Kvantum" is reserved for the alternative installation paths */
      && baseThemeName != "Kvantum"
      /* no space in theme name */
      && !(baseThemeName.simplified()).contains(QLatin1String(" "))
      /* "#" is reserved by Kvantum Manager as an ending for copied root themes */
      && (!baseThemeName.contains(QLatin1String("#"))
          || (baseThemeName.length() > 1
              && baseThemeName.indexOf("#") == baseThemeName.size() - 1)))
  {
    QStringList themeMames;
    if (useDark)
    {
      QString name = baseThemeName;
      if (name.endsWith(QLatin1String("#")))
        name.chop(1);
      /* give priority to modified themes */
      themeMames.append(name + "Dark#");
      themeMames.append(name + "Dark");
    }
    themeMames.append(baseThemeName);
    for (const QString &themeName : static_cast<const QStringList&>(themeMames))
    {
      QString userConfig, userSvg, temp, lightName;

      if (themeName.length() > 4 && themeName.endsWith(QLatin1String("Dark")))
      { // dark theme inside light theme folder
        lightName = themeName.left(themeName.length() - 4);
      }

      temp = QString("%1/Kvantum/%2/%2.kvconfig")
             .arg(xdg_config_home).arg(themeName);
      if (QFile::exists(temp))
        userConfig = temp;
      temp = QString("%1/Kvantum/%2/%2.svg")
             .arg(xdg_config_home).arg(themeName);
      if (QFile::exists(temp))
        userSvg = temp;

      if (userConfig.isEmpty() && userSvg.isEmpty()
          /* dark themes should be inside valid light directories */
          && isThemeDir(QString("%1/Kvantum").arg(xdg_config_home), lightName))
      {
        temp = QString("%1/Kvantum/%2/%3.kvconfig")
               .arg(xdg_config_home).arg(lightName).arg(themeName);
        if (QFile::exists(temp))
          userConfig = temp;
        temp = QString("%1/Kvantum/%2/%3.svg")
               .arg(xdg_config_home).arg(lightName).arg(themeName);
        if (QFile::exists(temp))
          userSvg = temp;
      }

      if (themeName.endsWith(QLatin1String("#")))
      {
        if (themeName.length() > 5 && themeName.endsWith(QLatin1String("Dark#")))
        { // root dark theme inside root light theme folder
          lightName = themeName.left(themeName.length() - 5);
        }
      }
      /* search in the alternative theme installation paths
         only if there's no such theme in the config folder */
      else if (userConfig.isEmpty() && userSvg.isEmpty()) // copied themes don't come here
      {
        QString homeDir = QDir::homePath();
        temp = QString("%1/.themes/%2/Kvantum/%2.kvconfig")
               .arg(homeDir).arg(themeName);
        if (QFile::exists(temp))
          userConfig = temp;
        temp = QString("%1/.themes/%2/Kvantum/%2.svg")
               .arg(homeDir).arg(themeName);
        if (QFile::exists(temp))
          userSvg = temp;

        if (userConfig.isEmpty() && userSvg.isEmpty()
            && isThemeDir(QString("%1/.themes").arg(homeDir), lightName))
        {
          temp = QString("%1/.themes/%2/Kvantum/%3.kvconfig")
                 .arg(homeDir).arg(lightName).arg(themeName);
          if (QFile::exists(temp))
            userConfig = temp;
          temp = QString("%1/.themes/%2/Kvantum/%3.svg")
                 .arg(homeDir).arg(lightName).arg(themeName);
          if (QFile::exists(temp))
            userSvg = temp;
        }

        if (userConfig.isEmpty() && userSvg.isEmpty())
        {
          temp = QString("%1/.local/share/themes/%2/Kvantum/%2.kvconfig")
                 .arg(homeDir).arg(themeName);
          if (QFile::exists(temp))
            userConfig = temp;
          temp = QString("%1/.local/share/themes/%2/Kvantum/%2.svg")
                 .arg(homeDir).arg(themeName);
          if (QFile::exists(temp))
            userSvg = temp;

          if (userConfig.isEmpty() && userSvg.isEmpty()
              && isThemeDir(QString("%1/.local/share/themes").arg(homeDir), lightName))
          {
            temp = QString("%1/.local/share/themes/%2/Kvantum/%3.kvconfig")
                   .arg(homeDir).arg(lightName).arg(themeName);
            if (QFile::exists(temp))
              userConfig = temp;
            temp = QString("%1/.local/share/themes/%2/Kvantum/%3.svg")
                   .arg(homeDir).arg(lightName).arg(themeName);
            if (QFile::exists(temp))
              userSvg = temp;
          }
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
               && !themeName.endsWith(QLatin1String("#"))) // root theme names can't have the ending "#"
      { // root theme
        temp = QString(DATADIR)
               + QString("/Kvantum/%1/%1.kvconfig").arg(themeName);
        if (QFile::exists(temp))
          themeSettings_ = new ThemeConfig(temp);
        else if (!isThemeDir(QString(DATADIR) + "/Kvantum", themeName) // svg shouldn't be found
                 && isThemeDir(QString(DATADIR) + "/Kvantum", lightName))
        {
          temp = QString(DATADIR)
                 + QString("/Kvantum/%1/%2.kvconfig").arg(lightName).arg(themeName);
          if (QFile::exists(temp))
            themeSettings_ = new ThemeConfig(temp);
        }

        if (!QFile::exists(temp))
        {
          temp = QString(DATADIR)
                 + QString("/Kvantum/%1/%1.svg").arg(themeName);
          if (!QFile::exists(temp)) // otherwise the checked root theme was just an SVG image
          {
            temp = QString(DATADIR)
                   + QString("/themes/%1/Kvantum/%1.kvconfig").arg(themeName);
            if (QFile::exists(temp))
              themeSettings_ = new ThemeConfig(temp);
          }

          if (!QFile::exists(temp)
              && !isThemeDir(QString(DATADIR) + "/themes", themeName)
              && isThemeDir(QString(DATADIR) + "/themes", lightName))
          {
            temp = QString(DATADIR)
                   + QString("/Kvantum/%1/%2.svg").arg(lightName).arg(themeName);
            if (!QFile::exists(temp))
            {
              temp = QString(DATADIR)
                     + QString("/themes/%1/Kvantum/%2.kvconfig").arg(lightName).arg(themeName);
              if (QFile::exists(temp))
                themeSettings_ = new ThemeConfig(temp);
            }
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
        if (!themeName.endsWith(QLatin1String("#")))
        {
          if (userConfig.isEmpty()) // otherwise it's a user theme without SVG image
          { // root theme
            temp = QString(DATADIR)
                   + QString("/Kvantum/%1/%1.svg").arg(themeName);
            if (QFile::exists(temp))
            {
              themeRndr_ = new QSvgRenderer();
              themeRndr_->load(temp);
            }
            else if (!isThemeDir(QString(DATADIR) + "/Kvantum", themeName) // config shouldn't be found
                     && isThemeDir(QString(DATADIR) + "/Kvantum", lightName))
            {
              temp = QString(DATADIR)
                     + QString("/Kvantum/%1/%2.svg").arg(lightName).arg(themeName);
              if (QFile::exists(temp))
              {
                themeRndr_ = new QSvgRenderer();
                themeRndr_->load(temp);
              }
            }

            if (!QFile::exists(temp))
            {
              temp = QString(DATADIR)
                     + QString("/Kvantum/%1/%1.kvconfig").arg(themeName);
              if (!QFile::exists(temp)) // otherwise the checked root theme was just a config file
              {
                temp = QString(DATADIR)
                       + QString("/themes/%1/Kvantum/%1.svg").arg(themeName);
                if (QFile::exists(temp))
                {
                  themeRndr_ = new QSvgRenderer();
                  themeRndr_->load(temp);
                }
              }

              if (!QFile::exists(temp)
                  && !isThemeDir(QString(DATADIR) + "/themes", themeName)
                  && isThemeDir(QString(DATADIR) + "/themes", lightName))
              {
                temp = QString(DATADIR)
                       + QString("/Kvantum/%1/%2.kvconfig").arg(lightName).arg(themeName);
                if (!QFile::exists(temp))
                {
                  temp = QString(DATADIR)
                         + QString("/themes/%1/Kvantum/%2.svg").arg(lightName).arg(themeName);
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
        else if (!userConfig.isEmpty()) // otherwise, the folder has been emptied manually
        { // find the SVG image of the root theme, of which this is a copy
          QString _themeName = themeName.left(themeName.length() - 1);
          if (!_themeName.isEmpty() && !_themeName.contains(QLatin1String("#")))
          {
            temp = QString(DATADIR)
                   + QString("/Kvantum/%1/%1.svg").arg(_themeName);
            if (QFile::exists(temp))
            {
              themeRndr_ = new QSvgRenderer();
              themeRndr_->load(temp);
            }
            else if (isThemeDir(QString(DATADIR) + "/Kvantum", lightName))
            {
              temp = QString(DATADIR)
                     + QString("/Kvantum/%1/%2.svg").arg(lightName).arg(_themeName);
              if (QFile::exists(temp))
              {
                themeRndr_ = new QSvgRenderer();
                themeRndr_->load(temp);
              }
            }

            if (!QFile::exists(temp))
            {
              temp = QString(DATADIR)
                     + QString("/Kvantum/%1/%1.kvconfig").arg(_themeName);
              if (!QFile::exists(temp)) // otherwise the checked root theme was just a config file
              {
                temp = QString(DATADIR)
                       + QString("/themes/%1/Kvantum/%1.svg").arg(_themeName);
                if (QFile::exists(temp))
                {
                  themeRndr_ = new QSvgRenderer();
                  themeRndr_->load(temp);
                }
              }

              if (!QFile::exists(temp)
                  && !isThemeDir(QString(DATADIR) + "/themes", _themeName)
                  && isThemeDir(QString(DATADIR) + "/themes", lightName))
              {
                temp = QString(DATADIR)
                       + QString("/Kvantum/%1/%2.kvconfig").arg(lightName).arg(_themeName);
                if (!QFile::exists(temp))
                {
                  temp = QString(DATADIR)
                         + QString("/themes/%1/Kvantum/%2.svg").arg(lightName).arg(_themeName);
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
      if (themeRndr_)
        break;
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

void Style::forgetPopupOrigin(QObject *o)
{
  if (auto w = qobject_cast<QWidget*>(o))
  {
    if (popupOrigins_.contains(w))
    {
        disconnect(w, &QObject::destroyed, this, &Style::forgetPopupOrigin);
        popupOrigins_.remove(w);
    }
  }
}

void Style::forgetMovedMenu(QObject *o)
{
  if (auto w = qobject_cast<QWidget*>(o))
  {
    if (movedMenus_.contains(w))
    {
        disconnect(w, &QObject::destroyed, this, &Style::forgetMovedMenu);
        movedMenus_.remove(w);
    }
  }
}


void Style::startAnimation(Animation *animation) const
{
  if (animation->target() == nullptr) return;
  stopAnimation(animation->target());
  connect(animation, &QObject::destroyed, this, &Style::removeAnimation, Qt::UniqueConnection);
  animations_.insert(animation->target(), animation);
  animation->start();
}

void Style::stopAnimation(const QObject *target) const
{
  Animation *animation = animations_.take(target);
  if (animation)
  {
    animation->stop();
    delete animation;
    animation = nullptr;
  }
}

void Style::removeAnimation(QObject *animation)
{
  if (animation)
    animations_.remove(animation->parent());
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
  int margin = 0;
  theme_spec tspec_now = settings_->getCompositeSpec();
  /* NOTE: No horizontal margin when menu items are spread across an opaque or shadowless menu. */
  if (!horiz || !(tspec_.spread_menuitems
                  && (tspec_.shadowless_popup || noComposite_ || !tspec_now.composite)))
  {
    const frame_spec fspec = getFrameSpec(QStringLiteral("Menu"));
    margin = horiz ? qMax(fspec.left,fspec.right) : qMax(fspec.top,fspec.bottom);
  }
  if (!tspec_.shadowless_popup && !noComposite_) // used without compositing at PM_SubMenuOverlap
    margin += tspec_now.menu_shadow_depth;
  return margin;
}

// Should be called only when compositing is available.
void Style::getShadow(const QString &widgetName, int thicknessH, int thicknessV)
{
  if (widgetName == "Menu")
  {
    if (menuShadow_.count() == 4)
      return;
    if (tspec_.shadowless_popup)
    {
      menuShadow_ = {0,0,0,0};
      realMenuShadow_ = {0,0,0,0};
      setProperty("menu_shadow", QVariant::fromValue(realMenuShadow_));
    }
  }
  else
  {
    if (tooltipShadow_.count() == 4 || tspec_.shadowless_popup)
      return;
  }

  QList<qreal> s;
  s << 0 << 0 << 0 << 0; // [left, top, right, bottom]

  QSvgRenderer *renderer = 0;
  qreal divisor = 0;
  QList<QString> direction;
  direction << QStringLiteral("left") << QStringLiteral("top") << QStringLiteral("right") << QStringLiteral("bottom");
  frame_spec fspec = getFrameSpec(widgetName);
  QString element = fspec.element;

  for (int i = 0; i < 4; ++i)
  {
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
          s[i] = i%2 ? static_cast<qreal>(thicknessV)*(br.height()/divisor)
                     : static_cast<qreal>(thicknessH)*(br.width()/divisor);
        }
      }
    }
  }

  if (widgetName == "Menu")
  {
    menuShadow_ = s;
    QList<int> shadow;
    shadow << qRound(s[0]) << qRound(s[1]) << qRound(s[2]) << qRound(s[3]);
    realMenuShadow_ = shadow; // don't lose the real info
    if (tspec_.spread_menuitems)
    {
      shadow[0] = shadow[2] = thicknessH;
      menuShadow_[0] = menuShadow_[2] = static_cast<qreal>(thicknessH);
    }
    setProperty("menu_shadow", QVariant::fromValue(shadow));
  }
  else
    tooltipShadow_ = s;
}

// also checks for NULL widgets
QWidget* Style::getParent(const QWidget *widget, int level) const
{
  if (!widget || level <= 0) return nullptr;
  QWidget *w = widget->parentWidget();
  for (int i = 1; i < level && w; ++i)
    w = w->parentWidget();
  return w;
}

bool Style::isWidgetInactive(const QWidget *widget) const
{
  if (tspec_.no_inactiveness) return false;
  if (widget
      /* some widgets (like KCapacityBar in kdf) may be drawn while still invisible */
      && widget->isVisible()
      && !widget->window()->windowFlags().testFlag(Qt::WindowDoesNotAcceptFocus)
      && !widget->window()->windowFlags().testFlag(Qt::X11BypassWindowManagerHint)
      && !widget->isActiveWindow())
  {
    return true;
  }
  return false;
}

void Style::noTranslucency(QObject *o)
{
  if (auto w = qobject_cast<QWidget*>(o))
  {
    translucentWidgets_.remove(w);
    forcedTranslucency_.remove(w);
  }
}

void Style::forcePalette(QWidget *widget, const QPalette &p) const
{
  widget->setPalette(p);
  widget->setProperty("_kv_fPalette", true);
}

int Style::mergedToolbarHeight(const QWidget *menubar) const
{
  if (!tspec_.merge_menubar_with_toolbar || isPlasma_) return 0;
  QWidget *p = getParent(menubar,1);
  if (!p) return 0;
  QList<QToolBar*> tList = p->findChildren<QToolBar*>(QString(), Qt::FindDirectChildrenOnly);
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

bool Style::isStylableToolbar(const QWidget *w, bool allowInvisible) const
{
  const QToolBar *tb = qobject_cast<const QToolBar*>(w);
  if (!tb
      || w->autoFillBackground()
      || w->testAttribute(Qt::WA_StyleSheetTarget) // not drawn by Kvantum (CE_ToolBar may not be called)
      || isPlasma_)
  {
    return false;
  }
  if (QTabBar *tabBar = w->findChild<QTabBar*>())
  {
    if (tb->isAncestorOf(tabBar))
      return false; // practically not a toolbar (Kaffeine's sidebar)
  }
  QWidget *p = getParent(w,1);
  if (p != w->window()) return false; // inside a dock
  /* don't style toolbars in places like KAboutDialog (-> KAboutData -> KAboutPerson) */
  if (QMainWindow *mw = qobject_cast<QMainWindow*>(p))
  {
    if (!hspec_.single_top_toolbar)
      return (tb->orientation() != Qt::Vertical || hspec_.style_vertical_toolbars);
    if (tb->orientation() == Qt::Vertical) return false;
    if (QWidget *mb = mw->menuWidget()) // WARNING: an empty menubar may be created by menuBar()
    {
      if (mb->isVisible())
      {
        if (mb->y()+mb->height() == tb->y())
          return true;
      }
      else if (tb->y() == 0
               && (allowInvisible
                   || tb->isVisible())) // FIXME: Why can KtoolBar be invisible here?
      {
        return true;
      }
    }
    else if (tb->y() == 0) return true;
  }
  return false;
}

QWidget* Style::getStylableToolbarContainer(const QWidget *w, bool allowInvisible) const
{
  if (w == nullptr || qobject_cast<const QToolBar*>(w))
    return nullptr;
  QWidget *window = w->window();
  if (window == w) return nullptr;
  if (isStylableToolbar(window, allowInvisible)) // detached toolbar
    return window;
  const QList<QToolBar*> toolbars = window->findChildren<QToolBar*>(QString(), Qt::FindDirectChildrenOnly);
  for (QToolBar *tb : toolbars)
  {
    if (isStylableToolbar(tb, allowInvisible) && tb->isAncestorOf(w))
      return tb;
  }
  return nullptr;
}

// This is only called when the widget doesn't have an interior SVG element.
bool Style::hasHighContrastWithContainer(const QWidget *w, const QColor color) const
{
  QString container;
  if (getStylableToolbarContainer(w))
    container = "Toolbar";
  else if (QWidget *p = getParent(w,1))
  {
    /* check parent with menubar FIXME: isn't the container approach needed here? */
    if (qobject_cast<QMenuBar*>(p)
        || qobject_cast<QMenuBar*>(getParent(p,1)))
    {
      container = "MenuBar";
    }
    else if (qobject_cast<QAbstractItemView*>(p)
             || qobject_cast<QAbstractItemView*>(getParent(p,1))
             || qobject_cast<QAbstractItemView*>(getParent(p,2)))
    { // don't let view-item selection cover widgets without interior
      return true;
    }
    else if (qobject_cast<QMenu*>(w->window())) // as in QtAv
      container = "MenuItem";
  }
  if(!container.isEmpty()
     && enoughContrast(color, getFromRGBA(getLabelSpec(container).normalColor)))
  {
    return true;
  }
  return false;
}

enum arrowType
{
  arrowNormal,
  arrowDelayed,
  arrowNone
};

static inline int getArrowType(const QToolButton *tb, const QStyleOptionToolButton *opt)
{
  if (tb)
  {
    if (tb->popupMode() == QToolButton::MenuButtonPopup)
      return arrowNormal;
    if (opt && (opt->features & QStyleOptionToolButton::HasMenu)
        && tb->popupMode() == QToolButton::DelayedPopup)
    {
      return arrowDelayed;
    }
  }
  else if (opt && (opt->features & QStyleOptionToolButton::HasMenu))
  {
    if (opt->features & QStyleOptionToolButton::MenuButtonPopup)
      return arrowNormal;
    if (opt->features & QStyleOptionToolButton::PopupDelay)
      return arrowDelayed;
  }
  return arrowNone;
}

enum groupedTBtnKind
{
  tbLeft = -1,
  tbMiddle,
  tbRight,
  tbAlone
};

static QSet<QWidget*> btnSeparators;

int Style::whichGroupedTBtn(const QToolButton *tb, const QWidget *parentBar, bool &drawSeparator) const
{
  int res = tbAlone;
  drawSeparator = false;

  if (!tb || !parentBar
      /* When the toolbar extension button is to the immediate right of the last toolbutton,
         there's a 1px gap between them (see the comment below). So, the extension button
         is excluded here. */
      || tb->objectName() == "qt_toolbar_ext_button")
  {
    return res;
  }

  /* There's no method for getting a child widget that properly contains a point. Therefore,
     instead of adding more computation, we set the minimum of PM_ToolBarSeparatorExtent to
     2 and check the existence of adjacent children by going 1px to the right and left. */
  const QRect g = tb->geometry();
  QToolButton *left = qobject_cast<QToolButton*>(parentBar->childAt(g.x()-1, g.y()));
  if (left && left->objectName() == "qt_toolbar_ext_button")
    left = nullptr;
  const QToolButton *right =  qobject_cast<const QToolButton*>(parentBar->childAt(g.x()+g.width()+1, g.y()));
  if (right && right->objectName() == "qt_toolbar_ext_button")
    right = nullptr;

  /* only direct children should be considered */
  if (left && left->parentWidget() != parentBar)
    left = nullptr;
  if (right && right->parentWidget() != parentBar)
    right = nullptr;

  if (left && g.height() == left->height())
  {
    if (right && g.height() == right->height())
    {
      res = tbMiddle;
      if ((tb->isChecked() && !right->isChecked() && !right->isDown())
          || (!tb->isChecked() && !tb->isDown() && right->isChecked()))
      {
        drawSeparator = true;
      }
    }
    else
      res = tbRight;
  }
  else if (right && g.height() == right->height())
  {
    res = tbLeft;
    if ((tb->isChecked() && !right->isChecked() && !right->isDown())
        || (!tb->isChecked() && !tb->isDown() && right->isChecked()))
    {
      drawSeparator = true;
    }
  }

  if (drawSeparator)
  {
    btnSeparators.insert(const_cast<QToolButton*>(tb));
    connect(tb, &QObject::destroyed, this, &Style::removeFromSet, Qt::UniqueConnection);
  }
  else
    btnSeparators.remove(const_cast<QToolButton*>(tb));

  if (left)
  { // update the left button when needed
    if ((tb->isChecked()
         && ((!left->isChecked() && !left->isDown() && !btnSeparators.contains(left))
             || ((left->isChecked() || left->isDown()) && btnSeparators.contains(left))))
        ||
        (!tb->isChecked()
          && (((!left->isChecked() || tb->isDown()) && btnSeparators.contains(left))
              || (left->isChecked() && !btnSeparators.contains(left)))))
    {
      left->update();
    }
  }

  return res;
}

/* get the widest digit/day/month string for the locale if needed */
static QHash<const QLocale, QString> allDigits;
static QHash<const QPair<QLocale, QFont>, QString> maxDigit;
static QHash<const QPair<QLocale, QFont>, QString> maxDay;
static QHash<const QPair<QLocale, QFont>, QString> maxMonth;
static QHash<const QPair<QLocale, QFont>, QString> maxFullDay;
static QHash<const QPair<QLocale, QFont>, QString> maxFullMonth;
static QHash<const QPair<QLocale, QFont>, QString> maxAmPm;

static void getAllDigits(const QLocale &l)
{ // to be used in regular expressions
  QString all = l.toString(0);
  for (int i = 1; i < 10; ++i)
    all += "|" + l.toString(i);
  allDigits.insert(l, all);
}
static void getMaxDigit(const QPair<QLocale, QFont> &data)
{
  int widestDigit = 0;
  int max = 0;
  QFontMetrics fm(data.second);
  for (int i = 0; i < 10; ++i)
  {
    int size = fm.horizontalAdvance(data.first.toString(i));
    if (max < size)
    {
      max = size;
      widestDigit = i;
    }
  }
  maxDigit.insert(data, data.first.toString(widestDigit));
}
static void getMaxDay(const QPair<QLocale, QFont> &data, bool full)
{
  QString day;
  int max = 0;
  QLocale::FormatType format = full ? QLocale::LongFormat : QLocale::ShortFormat;
  QFontMetrics fm(data.second);
  for (int i=1; i<=7 ; ++i)
  {
    QString theDay = data.first.dayName(i,format);
    int size = fm.horizontalAdvance(theDay);
    if (max < size)
    {
      max = size;
      day = theDay;
    }
  }
  if (full) maxFullDay.insert(data, day);
  else maxDay.insert(data, day);
}
static void getMaxMonth(const QPair<QLocale, QFont> &data, bool full)
{
  QString month;
  int max = 0;
  QLocale::FormatType format = full ? QLocale::LongFormat : QLocale::ShortFormat;
  QFontMetrics fm(data.second);
  for (int i=1; i<=12 ; ++i)
  {
    QString theMonth = data.first.monthName(i,format);
    int size = fm.horizontalAdvance(theMonth);
    if (max < size)
    {
      max = size;
      month = theMonth;
    }
  }
  if (full) maxFullMonth.insert(data, month);
  else maxMonth.insert(data, month);
}
static void getMaxAmPm(const QPair<QLocale, QFont> &data)
{
  QFontMetrics fm(data.second);
  QString pmText = data.first.pmText();
  QString amText = data.first.amText();
  QString ap = fm.horizontalAdvance(pmText) > fm.horizontalAdvance(amText)
                 ? pmText : amText;
  maxAmPm.insert(data, ap);
}

static inline QString spinMaxText(const QAbstractSpinBox *sp)
{
  QString maxTxt;
  if (const QSpinBox *sb = qobject_cast<const QSpinBox*>(sp))
  {
    QLocale l = sp->locale();
    if (!sb->isGroupSeparatorShown())
      l.setNumberOptions(l.numberOptions() | QLocale::OmitGroupSeparator);

    QString maxStr = l.toString(sb->maximum());
    QString minStr = l.toString(sb->minimum());
    if (minStr.size() > maxStr.size())
      maxStr = minStr;
    /* QAbstractSpinBox::sizeHint() sets a maximum of 18 characters
       but truncating isn't really needed here because of MAX_INT */
    maxStr.truncate(20);

    if (!allDigits.contains(l)) getAllDigits(l);
    QPair<QLocale, QFont> data(l, sb->font());
    if (!maxDigit.contains(data)) getMaxDigit(data);

    maxStr.replace(QRegularExpression(allDigits.value(l)), maxDigit.value(data));
    maxTxt = sb->prefix() + maxStr + sb->suffix();
  }
  else if (const QDoubleSpinBox *sb = qobject_cast<const QDoubleSpinBox*>(sp))
  {
    QLocale l = sp->locale();
    if (!sb->isGroupSeparatorShown())
      l.setNumberOptions(l.numberOptions() | QLocale::OmitGroupSeparator);

    /* at first, only consider integers... */
    int max = sb->maximum();
    int min = sb->minimum();
    QString maxStr = l.toString(max);
    QString minStr = l.toString(min);
    if (minStr.size() > maxStr.size())
      maxStr = minStr;
    maxStr.truncate(20);

    if (!allDigits.contains(l)) getAllDigits(l);
    QPair<QLocale, QFont> data(l, sb->font());
    if (!maxDigit.contains(data)) getMaxDigit(data);
    QString widestDigit = maxDigit.value(data);

    maxStr.replace(QRegularExpression(allDigits.value(l)), widestDigit);
    maxTxt = sb->prefix() + maxStr + sb->suffix();

    /* ... then, take decimals into account */
    int dec = sb->decimals();
    if (dec > 0)
    {
      maxTxt += l.decimalPoint();
      for (int i = 0; i < dec ; ++i)
        maxTxt += widestDigit;
    }
  }
  else if (const QDateTimeEdit *sb = qobject_cast<const QDateTimeEdit*>(sp))
  {
    QLocale l = sp->locale();
    l.setNumberOptions(l.numberOptions() | QLocale::OmitGroupSeparator);

    QPair<QLocale, QFont> data(l, sb->font());
    if (!maxDigit.contains(data)) getMaxDigit(data);
    QString widestDigit = maxDigit.value(data);
    QString twoDigits = widestDigit + widestDigit; // 00
    QString threeDigits = twoDigits + widestDigit; // 000
    if (!maxAmPm.contains(data)) getMaxAmPm(data);

    maxTxt = sb->displayFormat();

    /* take into account leading zeros */
    maxTxt.replace(QRegularExpression(QStringLiteral("hh|HH|mm|ss")), twoDigits);
    maxTxt.replace(QRegularExpression(QStringLiteral("h|H|m|s")), twoDigits);
    maxTxt.replace(QLatin1String("zzz"), threeDigits);
    maxTxt.replace(QLatin1String("z"), threeDigits);
    /* year */
    maxTxt.replace(QLatin1String("yy"), twoDigits);
    maxTxt.replace(QLatin1String("yyyy"), twoDigits+twoDigits); // 0000
    /* am/pm */
    QString ap = maxAmPm.value(data);
    maxTxt.replace(QLatin1String("ap"), ap, Qt::CaseInsensitive);
    maxTxt.replace(QLatin1String("a"), ap, Qt::CaseInsensitive);
    /* these will be replaced later */
    maxTxt.replace(QLatin1String("dddd"), QLatin1String("eeee"));
    maxTxt.replace(QLatin1String("MMMM"), QLatin1String("ffff"));
    maxTxt.replace(QLatin1String("ddd"), QLatin1String("eee"));
    maxTxt.replace(QLatin1String("MMM"), QLatin1String("fff"));
    /* leading zeros */
    maxTxt.replace(QRegularExpression(QStringLiteral("dd|MM")), twoDigits);
    maxTxt.replace(QRegularExpression(QStringLiteral("d|M")), twoDigits);
    /* time zone */
    maxTxt.replace(QLatin1String("t"), sb->dateTime().toString("t"));
    /* full day/month name */
    if (maxTxt.contains(QLatin1String("eeee")))
    {
      if (!maxFullDay.contains(data)) getMaxDay(data, true);
      maxTxt.replace(QLatin1String("eeee"), maxFullDay.value(data));
    }
    if (maxTxt.contains(QLatin1String("ffff")))
    {
      if (!maxFullMonth.contains(data)) getMaxMonth(data, true);
      maxTxt.replace(QLatin1String("ffff"), maxFullMonth.value(data));
    }
    /* short day/month name */
    if (maxTxt.contains(QLatin1String("eee")))
    {
      if (!maxDay.contains(data)) getMaxDay(data, false);
      maxTxt.replace(QLatin1String("eee"), maxDay.value(data));
    }
    if (maxTxt.contains(QLatin1String("fff")))
    {
      if (!maxMonth.contains(data)) getMaxMonth(data, false);
      maxTxt.replace(QLatin1String("fff"), maxMonth.value(data));
    }
  }

  if (!maxTxt.isEmpty())
  {
    QString svt = sp->specialValueText();
    if (!svt.isEmpty())
    {
      QFontMetrics fm(sp->font());
      if (fm.horizontalAdvance(svt) > fm.horizontalAdvance(maxTxt))
        maxTxt = svt;
    }
  }

  return maxTxt;
}

static inline bool spinTextless(const QAbstractSpinBox *sp)
{
  if (const QDateTimeEdit *sb = qobject_cast<const QDateTimeEdit*>(sp))
    return sb->displayFormat().isEmpty();
  return (!qobject_cast<const QSpinBox*>(sp) && !qobject_cast<const QDoubleSpinBox*>(sp));
}

static inline QString progressMaxText(const QProgressBar *pb, const QStyleOptionProgressBar *opt)
{
  QString maxTxt;
  if (pb && pb->isTextVisible() && !pb->text().isEmpty())
  {
    QLocale l = pb->locale();
    l.setNumberOptions(l.numberOptions() | QLocale::OmitGroupSeparator);
    maxTxt = pb->format();
    maxTxt.replace(QLatin1String("%p"), l.toString(100));
    maxTxt.replace(QLatin1String("%v"), l.toString(pb->maximum()));
    maxTxt.replace(QLatin1String("%m"), l.toString(pb->maximum()));
  }
  else if (opt && opt->textVisible && !opt->text.isEmpty())
  {
    maxTxt = opt->text;
    QLocale l = QLocale::system();
    l.setNumberOptions(l.numberOptions() | QLocale::OmitGroupSeparator);
    QString percentTxt = QString(l.percent()) + l.toString(100);
    QFontMetrics fm = opt->fontMetrics;
    if (fm.horizontalAdvance(percentTxt) > fm.horizontalAdvance(maxTxt))
      maxTxt = percentTxt;
  }
  return maxTxt;
}

/* Does the (tool-)button have a panel drawn at PE_PanelButtonTool?
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
  if (auto w = qobject_cast<QWidget*>(o))
  {
    paneledButtons.remove(w);
    standardButton.remove(w);
    txtColForced.remove(w);
    btnSeparators.remove(w);
  }
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
    col = standardPalette().color(QPalette::ButtonText);
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
      forcePalette(b, palette);
      txtColForced.insert(widget,col);
      connect(widget, &QObject::destroyed, this, &Style::removeFromSet, Qt::UniqueConnection);
    }
  }
}

/* Calculate the size of a text. */
static inline QSize textSize(const QFont &font, const QString &text)
{
  int tw = 0, th = 0;

  if (!text.isEmpty())
  {
    QString t(text);
    /* remove tabs and also the '&' mnemonic character
       (two successive ampersands are reduced to one) */
    t.replace(QLatin1Char('\t'), QLatin1Char(' '));
    int i = 0;
    while (i < t.size())
    {
      if (t.at(i) == QLatin1Char('&'))
        t.remove(i, 1);
      i++;
    }

    /* deal with newlines */
    QStringList l = t.split(QLatin1Char('\n'));

    th = QFontMetrics(font).height()*(l.size());

    for (int n = 0; n < l.size(); n++)
      tw = qMax(tw, QFontMetrics(font).horizontalAdvance(l[n]));

    if (l.size() > 1)
    {
      QRect br = QFontMetrics(font).boundingRect(QRect(0,0,tw,th),
                                                 Qt::AlignCenter | Qt::TextShowMnemonic,
                                                 text);
      th = br.height();
    }
  }

  return QSize(tw, th);
}

/* Elide a non-empty (and, probably, multiline) text that doesn't include '\t'. */
static inline QString makeTextElided(const QFontMetrics &fm, const QString &text, int width)
{
  QStringList l = text.split(QLatin1Char('\n'));
  for (int i=0; i<l.size(); i++)
    l[i] = fm.elidedText(l[i], Qt::ElideRight, width, Qt::TextShowMnemonic);
  return l.join(QLatin1Char('\n'));
}

static inline void colorizeRect(QPainter *painter, const QColor &col, const QRect &rect)
{ // used in a workaround for bad codes that change line-edit base color
  QPainterPath path;
  path.addRoundedRect(rect, 3, 3);
  painter->save();
  painter->setRenderHints(QPainter::Antialiasing, true);
  painter->fillPath(path, col);
  painter->restore();
}

bool Style::btnDragInProgress() const
{
  return (tspec_.drag_from_buttons
          && itsWindowManager_
          && itsWindowManager_->dragInProgress());
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
  if (isWidgetInactive(widget))
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

void Style::drawFocusRect(QPainter *painter, const QRect &rect, const QString &element) const
{
  frame_spec fspec = getFrameSpec(QStringLiteral("Focus"));
  fspec.expansion = 0;
  fspec.left = qMin(fspec.left,2);
  fspec.right = qMin(fspec.right,2);
  fspec.top = qMin(fspec.top,2);
  fspec.bottom = qMin(fspec.bottom,2);
  if (!element.isEmpty())
    fspec.element = element;
  renderFrame(painter,rect,fspec,fspec.element);
}

/* This method is used, instead of drawPrimitive(PE_PanelLineEdit,...), for drawing the lineedit
   of an editable combobox because animatedWidget_ is always NULL for KUrlComboBox -> KLineEdit.
   Although it's only needed in such special cases, it can always be used safely. */
void Style::drawComboLineEdit(const QStyleOption *option,
                              QPainter *painter,
                              const QWidget *lineedit,
                              const QWidget *combo,
                              const QString &group,
                              bool fillInterior) const
{
  if (isPlasma_ && lineedit && lineedit->window()->testAttribute(Qt::WA_NoSystemBackground))
    return;

  const interior_spec ispec = getInteriorSpec(group);
  frame_spec fspec = getFrameSpec(group);
  label_spec lspec = getLabelSpec(group);
  const size_spec sspec = getSizeSpec(group);

  /*if (isLibreoffice_)
  {
    fspec.left = qMin(fspec.left,3);
    fspec.right = qMin(fspec.right,3);
    fspec.top = qMin(fspec.top,3);
    fspec.bottom = qMin(fspec.bottom,3);
  }
  else
  {*/
    bool noSpace(lineedit
                 && ((lineedit->testAttribute(Qt::WA_StyleSheetTarget)
                      && !lineedit->styleSheet().isEmpty()
                      && lineedit->styleSheet().contains(QLatin1String("padding")))
                     || lineedit->minimumWidth() == lineedit->maximumWidth()));
    if (!noSpace && lineedit
        && lineedit->height() < sizeCalculated(lineedit->font(),fspec,lspec,sspec,QStringLiteral("W"),QSize()).height())
    { // the label spacing isn't added at CT_ComboBox
      lspec.top = qMin(lspec.top,2);
      lspec.bottom = qMin(lspec.bottom,2);
      if (lineedit->height() < sizeCalculated(lineedit->font(),fspec,lspec,sspec,QStringLiteral("W"),QSize()).height())
        noSpace = true;
    }
    if (noSpace)
    {
      fspec.left = qMin(fspec.left,3);
      fspec.right = qMin(fspec.right,3);
      fspec.top = qMin(fspec.top,3);
      fspec.bottom = qMin(fspec.bottom,3);

      if (!hasExpandedBorder(fspec))
        fspec.expansion = qMin(fspec.expansion, LIMITED_EXPANSION);
      else
      {
        fspec.leftExpanded = qMin(fspec.leftExpanded,3);
        fspec.rightExpanded = qMin(fspec.rightExpanded,3);
        fspec.topExpanded = qMin(fspec.topExpanded,3);
        fspec.bottomExpanded = qMin(fspec.bottomExpanded,3);
      }
    }
  //}

  if (qobject_cast<QAbstractItemView*>(getParent(combo,1)))
  {
    fspec.left = fspec.right = fspec.top = fspec.bottom = fspec.expansion = 0;
  }

  fspec.isAttached = true;
  if (option->direction == Qt::RightToLeft)
  {
    int arrowFrameSize = tspec_.combo_as_lineedit ? fspec.left : getFrameSpec(QStringLiteral("ComboBox")).left;
    if (lineedit && combo
        && lineedit->width() < combo->width()
                               - (tspec_.square_combo_button
                                  ? qMax(COMBO_ARROW_LENGTH, combo->height()-arrowFrameSize)
                                  : COMBO_ARROW_LENGTH)
                               - arrowFrameSize)
    {
      fspec.HPos = 0;
    }
    else fspec.HPos = 1;
  }
  else
  {
    if (lineedit && lineedit->x() > 0) fspec.HPos = 0;
    else fspec.HPos = -1;
  }

  // lineedits only have normal and focused states in Kvantum
  QString leStatus = (option->state & State_HasFocus) ? "-focused" : "-normal";
  if (isWidgetInactive(lineedit))
    leStatus.append("-inactive");
  if (!(option->state & State_Enabled))
  {
    painter->save();
    painter->setOpacity(DISABLED_OPACITY);
  }
  renderFrame(painter,
              /*isLibreoffice_ ? // impossible
                option->rect.adjusted(fspec.left,fspec.top,-fspec.right,-fspec.bottom) :*/
                option->rect,
              fspec,
              fspec.element+leStatus);
  if (!fillInterior)
  {
    renderInterior(painter,option->rect,fspec,ispec,ispec.element+leStatus);

    /* a workaround for bad codes that change line-edit base color */
    if (!isPcmanfm_ && group == "LineEdit" && lineedit)
    {
      QColor baseCol = lineedit->palette().color(QPalette::Base);
      if (baseCol != standardPalette().color(lineedit->palette().currentColorGroup(), QPalette::Base)
          && baseCol.saturation() > 10) // should have enough saturation
      {
        baseCol.setAlpha(255);
        QStyleOption o(*option);
        o.rect = lineedit->rect();
        colorizeRect(painter, baseCol, subElementRect(SE_LineEditContents, &o, lineedit));
      }
    }
  }
  else
  {
    QColor baseCol = (lineedit ? lineedit->palette() : standardPalette())
                     .color(leStatus.contains(QLatin1String("-inactive"))
                                                ? QPalette::Inactive
                                                : QPalette::Active,
                                              QPalette::Base);
    baseCol.setAlpha(255);
    painter->fillRect(interiorRect(option->rect,fspec), baseCol);
  }
  if (!(option->state & State_Enabled))
    painter->restore();
}

static inline bool isCursorOutsideWidget(const QWidget *widget)
{ // used for woking around Qt's hover bug
  if (widget == nullptr) return false;
  QScreen *scr = widget->screen();
  return !widget->rect().contains(widget->mapFromGlobal(scr != nullptr ? QCursor::pos(scr)
                                                                       : QCursor::pos()));
}

static inline bool isCursorOutsideRect(const QWidget *widget, const QRect &rect)
{ // used for woking around Qt's hover bug
  if (widget == nullptr) return false;
  QScreen *scr = widget->screen();
  return !rect.contains(widget->mapFromGlobal(scr != nullptr ? QCursor::pos(scr)
                                                             : QCursor::pos()));
}

void Style::drawPrimitive(QStyle::PrimitiveElement element,
                          const QStyleOption *option,
                          QPainter *painter,
                          const QWidget *widget) const
{
  int x,y,h,w;
  option->rect.getRect(&x,&y,&w,&h);

  switch(element) {
    case PE_Widget : {
      if (qobject_cast<const QMdiSubWindow*>(widget))
      {
        painter->fillRect(option->rect, standardPalette().color(isWidgetInactive(widget)
                                                                  ? QPalette::Inactive
                                                                  : QPalette::Active,
                                                                QPalette::Window));
        break;
      }
      if (widget) // it's NULL with QML
      {
        if (widget->windowType() == Qt::ToolTip)
        {
          painter->fillRect(option->rect, standardPalette().color(QPalette::Window));
          break;
        }
        // only for windows and dialogs
        if (!widget->isWindow())
          break;
      }

      // we don't accept custom background colors for windows...
      if (!widget // QML
          || (option->palette.color(QPalette::Window) != standardPalette().color(QPalette::Window)
              && !widget->testAttribute(Qt::WA_TranslucentBackground)
              && !widget->testAttribute(Qt::WA_NoSystemBackground)))
      {
        if (option->palette.color(QPalette::Window) == option->palette.color(isWidgetInactive(widget)
                                                                               ? QPalette::Inactive
                                                                               : QPalette::Active,
                                                                             QPalette::Base))
          break; // ...but make an exception for apps like KNotes
        painter->fillRect(option->rect, standardPalette().color(isWidgetInactive(widget)
                                                                  ? QPalette::Inactive
                                                                  : QPalette::Active,
                                                                QPalette::Window));
      }

      interior_spec ispec = getInteriorSpec(QStringLiteral("Dialog"));
      size_spec sspec = getSizeSpec(QStringLiteral("Dialog"));
      if (widget && !ispec.element.isEmpty()
          && !widget->windowFlags().testFlag(Qt::FramelessWindowHint)) // not a panel)
      {
        if (QWidget *child = widget->childAt(0,0))
        {
          if (qobject_cast<QMenuBar*>(child) || qobject_cast<QToolBar*>(child))
          {
            ispec = getInteriorSpec(QStringLiteral("Window"));
            sspec = getSizeSpec(QStringLiteral("Window"));
          }
        }
      }
      else
      {
        ispec = getInteriorSpec(QStringLiteral("Window"));
        sspec = getSizeSpec(QStringLiteral("Window"));
      }
      frame_spec fspec;
      default_frame_spec(fspec);

      QString suffix = "-normal";
      if (isWidgetInactive(widget))
        suffix = "-normal-inactive";
      if (tspec_.no_window_pattern && (ispec.px > 0 || ispec.py > 0))
        ispec.px = -1; // no tiling pattern (without translucency)
      int dh = sspec.incrementH ? sspec.minH : qMax(sspec.minH - h, 0);
      int dw = sspec.incrementW ? sspec.minW : qMax(sspec.minW - w, 0);
      if (dh > 0 || dw > 0)
      {
        painter->save();
        painter->setClipRegion(option->rect, Qt::IntersectClip);
      }
      renderInterior(painter,option->rect.adjusted(0,0,dw,dh),fspec,ispec,ispec.element+suffix);
      if (dh > 0 || dw > 0)
        painter->restore();

      break;
    }

    case PE_FrameStatusBarItem : {return;}

    case PE_FrameDockWidget : {
      frame_spec fspec = getFrameSpec(QStringLiteral("Dock"));
      const interior_spec ispec = getInteriorSpec(QStringLiteral("Dock"));
      fspec.expansion = 0;

      QString status = getState(option,widget);
      if (!(option->state & State_Enabled))
      {
        status.replace(QLatin1String("disabled"),QLatin1String("normal"));
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
      if (const QStyleOptionTabBarBase *opt
              = qstyleoption_cast<const QStyleOptionTabBarBase*>(option))
      {
        bool verticalTabs = false;
        if (opt->shape == QTabBar::RoundedEast
            || opt->shape == QTabBar::RoundedWest
            || opt->shape == QTabBar::TriangularEast
            || opt->shape == QTabBar::TriangularWest)
        {
          verticalTabs = true;
        }
        QRect r = option->rect;
        // FIXME: Why does Qt draw redundant frames when there's a corner widget (button)?
        //if (!r.contains(opt->tabBarRect) || r == opt->tabBarRect)
        if ((!verticalTabs && (r.top() != opt->tabBarRect.top() || r.bottom() != opt->tabBarRect.bottom()))
            || (verticalTabs && (r.left() != opt->tabBarRect.left() || r.right() != opt->tabBarRect.right())))
          return;

        int l = 0; int d = 0;
        QRect tr = opt->selectedTabRect;
        if (tspec_.attach_active_tab)
        {
          if (tr.isEmpty()) return;
          d = tr.x();
          l = tr.width();
        }
        bool bottomTabs = false;
        // as with CE_TabBarTabShape
        if (verticalTabs)
        {
          if (tspec_.attach_active_tab)
          {
            l = tr.height();
            d = tr.y();
          }
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

        frame_spec fspec;

        /* don't consider the overlap (as in PE_FrameTabWidget) */
        /*if (l > 0)
        {
          int overlap = tspec_.active_tab_overlap;
          if (overlap > 0)
          {
            bool joinedActiveTab = hasFloatingTabs_ ? joinedActiveFloatingTab_ : joinedActiveTab_;
            if (!joinedActiveTab || tspec_.no_active_tab_separator)
            {
              fspec = getFrameSpec(QStringLiteral("Tab"));
              int exp = qMin(fspec.expansion, qMin(tr.width(), tr.height())) / 2 + 1;
              overlap = qMin(overlap, qMax(exp, qMax(fspec.left, fspec.right)));
              if (d == 0) // at the beginning
                l -= overlap;
              else
              {
                if (d == r.width() - l) // at the end
                  l -= overlap;
                else
                  l -= 2*overlap;
                d += overlap;
              }
              if (l < 0) l = 0;
            }
          }
        }*/

        fspec = getFrameSpec(QStringLiteral("TabBarFrame"));
        const interior_spec ispec = getInteriorSpec(QStringLiteral("TabBarFrame"));
        fspec.expansion = 0;

        QString status = getState(option,widget);
        if (!(option->state & State_Enabled))
        {
          status.replace(QLatin1String("disabled"),QLatin1String("normal"));
          painter->save();
          painter->setOpacity(DISABLED_OPACITY);
        }
        // TabBarFrame seems to have a redundant focus state
        else if (!status.startsWith(QLatin1String("normal")))
        {
          if (status.endsWith(QLatin1String("-inactive"))) status = "normal-inactive";
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

    /* PE_PanelButtonCommand is used by QCommonStyle only inside CE_PushButtonBevel
       and CC_MdiControls but we don't use it. It's here for special cases. */
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
        if (isWidgetInactive(widget))
          status.append("-inactive");
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      const QStyleOptionButton *opt =
          qstyleoption_cast<const QStyleOptionButton*>(option);
      if (opt == nullptr
          || !(opt->features & QStyleOptionButton::Flat) || !status.startsWith(QLatin1String("normal")))
      {
        renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+status);
        renderFrame(painter,option->rect,fspec,fspec.element+"-"+status);
      }
      if (!(option->state & State_Enabled))
        painter->restore();

      break;
    }

    case PE_PanelButtonTool : {
      const QStyleOptionToolButton *opt = qstyleoption_cast<const QStyleOptionToolButton*>(option);
      const QToolButton *tb = qobject_cast<const QToolButton*>(widget);
      if (widget != nullptr)
      {
        if ((option->state & State_Sunken)
            && ((opt && (opt->features & QStyleOptionToolButton::HasMenu))
                || (tb && tb->menu())))
        {
          sunkenButton_ = const_cast<QWidget*>(widget);
        }
        else if (sunkenButton_.data() == widget)
          sunkenButton_.clear();
        /* the extension button arrow has no state */
        if (widget->objectName() == "qt_toolbar_ext_button"
            || widget->objectName() == "qt_menubar_ext_button")
        {
          break;
        }
      }
      interior_spec ispec;
      QString group = "PanelButtonTool";
      QWidget *p = getParent(widget,1);
      bool autoraise(option->state & State_AutoRaise);
      bool fillWidgetInterior(false);
      QWidget *stb = getStylableToolbarContainer(widget);
      if (stb)
      {
        autoraise = true; // we make all toolbuttons auto-raised inside toolbars
        if (!getFrameSpec(QStringLiteral("ToolbarButton")).element.isEmpty()
            || !getInteriorSpec(QStringLiteral("ToolbarButton")).element.isEmpty())
        {
          group = "ToolbarButton";
        }
        ispec = getInteriorSpec(group);
        if (!ispec.hasInterior
            && enoughContrast(getFromRGBA(getLabelSpec(group).normalColor),
                              getFromRGBA(getLabelSpec(QStringLiteral("Toolbar")).normalColor)))
        { // high contrast on toolbar
          fillWidgetInterior = true;
        }
      }
      else
      {
        ispec = getInteriorSpec(group);
        if (p && !ispec.hasInterior)
        {
          QString containerGroup;
          if (qobject_cast<QMenuBar*>(p))
          {
            if (mergedToolbarHeight(p))
              containerGroup = "Toolbar";
            else
              containerGroup = "MenuBar";
          }
          else if (QMenuBar *gp = qobject_cast<QMenuBar*>(getParent(p,1)))
          {
            if (mergedToolbarHeight(gp))
              containerGroup = "Toolbar";
            else
              containerGroup = "MenuBar";
          }
          if (!containerGroup.isEmpty()
              && enoughContrast(getFromRGBA(getLabelSpec(group).normalColor),
                                getFromRGBA(getLabelSpec(containerGroup).normalColor)))
          { // high contrast on menubar
            fillWidgetInterior = true;
          }
        }
      }

      frame_spec fspec = getFrameSpec(group);
      QString status = getState(option,widget);

      /* prevent drawing pushbuttons as toolbuttons (as in QupZilla, KNotes or Kate) */
      if (const QPushButton *pb = qobject_cast<const QPushButton*>(widget))
      {
        fspec.expansion = 0;
        if (pb->text().isEmpty())
        {
          QColor col = widget->palette().color(status.contains(QLatin1String("-inactive"))
                                                 ? QPalette::Inactive
                                                 : QPalette::Active,
                                               QPalette::Button);
          if (pb->isChecked() || pb->isDown())
          {
            int gray = qGray(col.rgb());
            if (gray <= 100) gray += 30;
            else gray -= 30;
            col = QColor(gray,gray,gray);
          }
          painter->fillRect(option->rect, col);
          break;
        }
      }

      QObject *styleObject = option->styleObject;

      /* Due to a Qt5 bug (which I call "the hover bug"), comboboxes and buttons may have
         the WA_UnderMouse attribute without being under the cursor after their menus are
         closed or they are enabled. Hence we use the following logic in several places.
         It will be harmless if the bug is fixed (but shouldn't be used with dragging from buttons). */
      if (styleObject != nullptr)
      {
        if (!btnDragInProgress() && status.startsWith(QLatin1String("focused"))
            && isCursorOutsideWidget(widget))
        {
          styleObject->setProperty("_kv_hover_bug", true);
          status.replace(QLatin1String("focused"),QLatin1String("normal"));
        }
        else
          styleObject->setProperty("_kv_hover_bug", QVariant());
      }

      bool hasPanel = false;

      indicator_spec dspec = getIndicatorSpec(group);
      label_spec lspec = getLabelSpec(group);
      QRect r = option->rect;

      //const QStyleOptionTitleBar *titleBar = qstyleoption_cast<const QStyleOptionTitleBar*>(option);

      /*if (qobject_cast<QTabBar*>(p))
      {
        painter->fillRect(option->rect, option->palette.brush(QPalette::Window));
        //fspec.expansion = 0;
      }
      else if ((titleBar && (titleBar->titleBarFlags & Qt::WindowType_Mask) == Qt::Tool)
               || qobject_cast<QDockWidget*>(getParent(widget,1)))
      {
        return;
      }*/
      if (widget && widget->inherits("QDockWidgetTitleButton"))
      {
        if (status.startsWith(QLatin1String("focused")))
        {
          QColor col = widget->palette().color(QPalette::Active,QPalette::Highlight);
          col.setAlpha(50);
          painter->fillRect(r,col);
        }
        else if (status.startsWith(QLatin1String("pressed")))
        {
          QColor col = widget->palette().color(QPalette::Active,QPalette::Highlight);
          painter->save();
          QRegion reg(r);
          QRegion internalReg(r.adjusted(1,1,-1,-1));
          painter->setClipRegion(reg.subtracted(internalReg));
          painter->fillRect(r,col);
          painter->restore();
        }
        return;
      }

      int arType = getArrowType(tb, opt);

      QToolBar *toolBar = qobject_cast<QToolBar*>(p);
      if ((tb && (tb->toolButtonStyle() == Qt::ToolButtonIconOnly
                  || (tb->toolButtonStyle() == Qt::ToolButtonFollowStyle
                      && styleHint(SH_ToolButtonStyle,option,widget) == Qt::ToolButtonIconOnly)
                  || tb->text().isEmpty())
           && tb->icon().isNull())
          || (opt && (opt->toolButtonStyle == Qt::ToolButtonIconOnly || opt->text.isEmpty())
              && opt->icon.isNull()))
      {
        if ((tb && tb->arrowType() != Qt::NoArrow)
            || (opt && (opt->features & QStyleOptionToolButton::Arrow)
                && opt->arrowType != Qt::NoArrow)) // a button with just arrows
        {
          if (qobject_cast<QTabBar*>(p)) // tabbar scroll button (-> PM_TabBarScrollButtonWidth)
          {
            bool painterSaved = false;
            painter->fillRect(option->rect, standardPalette().color(status.contains(QLatin1String("-inactive"))
                                                                      ? QPalette::Inactive
                                                                      : QPalette::Active,
                                                                    QPalette::Window));
            const frame_spec fspec1 = getFrameSpec(QStringLiteral("Tab"));
            fspec.left = qMin(fspec.left, fspec1.left);
            fspec.right = qMin(fspec.right, fspec1.right);
            fspec.top = qMin(fspec.top, fspec1.top);
            fspec.bottom = qMin(fspec.bottom, fspec1.bottom);
            fspec.isAttached = true;
            Qt::ArrowType aType = opt ? opt->arrowType : tb->arrowType();
            QTransform m;
            switch (aType) {
              case Qt::LeftArrow :
                fspec.HPos = -1;
                break;
              case Qt::RightArrow :
                fspec.HPos = 1;
                break;
              case Qt::UpArrow :
                fspec.HPos = -1;
                r.setRect(y, x, h, w);
                painter->save();
                painterSaved = true;
                m.scale(1,-1);
                m.rotate(-90);
                painter->setTransform(m, true);
                break;
              case Qt::DownArrow :
                fspec.HPos = 1;
                r.setRect(y, x, h, w);
                painter->save();
                painterSaved = true;
                m.scale(1,-1);
                m.rotate(-90);
                painter->setTransform(m, true);
                break;
              default :
                break;
            }
            /* don't accept any state because some themes
               may not have SVG elements suitable for grouping */
            status = "normal";
            if (isWidgetInactive(widget))
              status.append("-inactive");
            renderFrame(painter,r,fspec,fspec.element+"-"+status);
            renderInterior(painter,r,fspec,ispec,ispec.element+"-"+status);
            if(painterSaved)
              painter->restore();
            if (!paneledButtons.contains(widget))
            {
              paneledButtons.insert(widget);
              connect(widget, &QObject::destroyed, this, &Style::removeFromSet, Qt::UniqueConnection);
            }
            return;
          }
          /* a button with just one arrow */
          else if (toolBar == nullptr && hspec_.transparent_arrow_button
                   && arType == arrowNone)
          {
            return; // not in paneledButtons
          }
        }
        else if ((!tb || tb->text().isEmpty())
                 && (!opt || opt->text.isEmpty()))
        {
          fspec.expansion = 0; // color button
        }
      }

      // -> CE_ToolButtonLabel
      if (qobject_cast<QAbstractItemView*>(getParent(p,1)))
      {
        fspec.left = qMin(fspec.left,3);
        fspec.right = qMin(fspec.right,3);
        fspec.top = qMin(fspec.top,3);
        fspec.bottom = qMin(fspec.bottom,3);

        if (!hasExpandedBorder(fspec))
          fspec.expansion = qMin(fspec.expansion, LIMITED_EXPANSION);
        else
        {
          fspec.leftExpanded = qMin(fspec.leftExpanded,3);
          fspec.rightExpanded = qMin(fspec.rightExpanded,3);
          fspec.topExpanded = qMin(fspec.topExpanded,3);
          fspec.bottomExpanded = qMin(fspec.bottomExpanded,3);
        }

        //lspec.left = qMin(lspec.left,2);
        //lspec.right = qMin(lspec.right,2);
        lspec.top = qMin(lspec.top,2);
        lspec.bottom = qMin(lspec.bottom,2);
        lspec.tispace = qMin(lspec.tispace,2);
      }

      bool isInactive(isWidgetInactive(widget));
      bool rtl(option->direction == Qt::RightToLeft);
      bool drawRaised = false;
      bool drawSep(false);
      if (!(option->state & State_Enabled))
      {
        status = "normal";
        if (option->state & State_On)
          status = "toggled";
        if (isInactive)
          status.append("-inactive");
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      if (tb)
      {
        /* always show menu titles in the toggled state */
        if (!hspec_.transparent_menutitle
            && tb->isDown()
            && (tb->toolButtonStyle() == Qt::ToolButtonTextBesideIcon
                || (tb->toolButtonStyle() == Qt::ToolButtonFollowStyle
                    && styleHint(SH_ToolButtonStyle,option,widget) == Qt::ToolButtonTextBesideIcon))
            && qobject_cast<QMenu*>(p))
        {
          status.replace(QLatin1String("pressed"),QLatin1String("toggled"));
        }

        /* only group tool buttons whose immediate parent is a horizontal toolbar */
        if ((toolBar && toolBar->orientation() != Qt::Vertical)
            /* but make an exception for libfm-qt's path buttons on a stylable toolbar */
            || (qobject_cast<QToolBar*>(stb)
                && qobject_cast<QToolBar*>(stb)->orientation() != Qt::Vertical
                && tb->inherits("Fm::PathButton")))
        {
          if (tspec_.group_toolbar_buttons)
          {
            /* the disabled state is ugly for grouped tool buttons */
            if (!(option->state & State_Enabled))
              painter->restore();
            drawRaised = true;
            ispec.px = ispec.py = 0;
            int kind = whichGroupedTBtn(tb, p, drawSep);
            if (kind != tbAlone)
            {
              fspec.isAttached = true;
              fspec.HPos = kind;
            }
          }
        }
        /* group libfm-qt's path buttons when they aren't on a toolbar */
        else if (tb->inherits("Fm::PathButton"))
        {
            //if (QWidget *ancestor = getParent(p,3))
            //{
              //if (ancestor->inherits("Fm::PathBar"))
              //{
                if (!(option->state & State_Enabled))
                  painter->restore();
                drawRaised = true;
                ispec.px = ispec.py = 0;
                int kind = whichGroupedTBtn(tb, p, drawSep);
                if (kind != tbAlone)
                {
                  fspec.isAttached = true;
                  fspec.HPos = kind;
                }
              //}
            //}
        }

        // lack of space  (-> CE_ToolButtonLabel)
        if (!fspec.isAttached
            && opt && opt->toolButtonStyle == Qt::ToolButtonIconOnly && !opt->icon.isNull())
        {
          if (arType != arrowNormal)
          {
            if (arType == arrowDelayed)
            {
              if (tb->width() < opt->iconSize.width()+fspec.left+fspec.right
                                +dspec.size+ BUTTON_ARROW_MARGIN+lspec.tispace)
              {
                if (rtl)
                  fspec.right = qMin(fspec.right,3);
                else
                  fspec.left = qMin(fspec.left,3);
                //fspec.expansion = 0;
                //dspec.size = qMin(dspec.size,TOOL_BUTTON_ARROW_SIZE-TOOL_BUTTON_ARROW_OVERLAP);
                lspec.tispace = 0;
              }
            }
            else if (tb->width() < opt->iconSize.width()+fspec.left+fspec.right
                     || tb->height() < opt->iconSize.height()+fspec.top+fspec.bottom)
            {
              fspec.left = qMin(fspec.left,3);
              fspec.right = qMin(fspec.right,3);
              fspec.top = qMin(fspec.top,3);
              fspec.bottom = qMin(fspec.bottom,3);

              if (!hasExpandedBorder(fspec))
                fspec.expansion = qMin(fspec.expansion, LIMITED_EXPANSION);
              else
              {
                fspec.leftExpanded = qMin(fspec.leftExpanded,3);
                fspec.rightExpanded = qMin(fspec.rightExpanded,3);
                fspec.topExpanded = qMin(fspec.topExpanded,3);
                fspec.bottomExpanded = qMin(fspec.bottomExpanded,3);
              }
            }
          }
          else
          {
            const frame_spec fspec1 = getFrameSpec(QStringLiteral("DropDownButton"));
            if (tb->width() < opt->iconSize.width()+fspec.left
                              +(rtl ? fspec1.left : fspec1.right)
                              +TOOL_BUTTON_ARROW_SIZE+2*BUTTON_ARROW_MARGIN)
            {
              fspec.left = qMin(fspec.left,3);
              fspec.right = qMin(fspec.right,3);
              fspec.top = qMin(fspec.top,3);
              fspec.bottom = qMin(fspec.bottom,3);

              if (!hasExpandedBorder(fspec))
                fspec.expansion = qMin(fspec.expansion, LIMITED_EXPANSION);
              else
              {
                fspec.leftExpanded = qMin(fspec.leftExpanded,3);
                fspec.rightExpanded = qMin(fspec.rightExpanded,3);
                fspec.topExpanded = qMin(fspec.topExpanded,3);
                fspec.bottomExpanded = qMin(fspec.bottomExpanded,3);
              }
            }
          }
        }

        const QString prevState = styleObject != nullptr
                                    ? styleObject->property("_kv_state").toString()
                                    : QString();
        bool animate(opacityTimer_
                     && !btnDragInProgress()
                     && widget->isEnabled() && animatedWidget_ == widget
                     && !prevState.isEmpty());
        if (animate && prevState == status)
        {
          if (opacityTimer_->isActive())
            opacityTimer_->stop();
          animationOpacity_ = 0;
          animate = false;
        }
        if (tb->autoRaise())
          autoraise = true;
        if (!autoraise || !status.startsWith(QLatin1String("normal")) || drawRaised)
        {
          if (animate)
          {
            if (!opacityTimer_->isActive())
            {
              animationOpacity_ = 0;
              opacityTimer_->start(ANIMATION_FRAME);
            }
            if (animationOpacity_ < 100
                && (!autoraise || !prevState.startsWith(QLatin1String("normal")) || drawRaised))
            {
              renderFrame(painter,r,fspec,fspec.element+"-"+prevState,0,0,0,0,0,drawRaised);
              if (!fillWidgetInterior)
                renderInterior(painter,r,fspec,ispec,ispec.element+"-"+prevState,drawRaised);
            }
            painter->save();
            painter->setOpacity(static_cast<qreal>(animationOpacity_)/100.0);
          }
          renderFrame(painter,r,fspec,fspec.element+"-"+status,0,0,0,0,0,drawRaised);
          if (!fillWidgetInterior)
            renderInterior(painter,r,fspec,ispec,ispec.element+"-"+status,drawRaised);
          if (animate)
          {
            painter->restore();
            if (animationOpacity_ >= 100)
              styleObject->setProperty("_kv_state", status);
          }
          else if (styleObject)
            styleObject->setProperty("_kv_state", status);
          if (fillWidgetInterior)
            painter->fillRect(interiorRect(r,fspec),
                              tb->palette().brush(status.contains(QLatin1String("-inactive"))
                                                                    ? QPalette::Inactive
                                                                    : QPalette::Active,
                                                                  QPalette::Button));
          hasPanel = true;
        }
        // auto-raised fade out animation
        else if (animate && !prevState.startsWith(QLatin1String("normal")))
        {
          if (!opacityTimer_->isActive())
          {
            animationOpacity_ = 0;
            opacityTimer_->start(ANIMATION_FRAME);
          }
          if (animationOpacity_ < 100)
          {
            painter->save();
            painter->setOpacity(1.0 - static_cast<qreal>(animationOpacity_)/100.0);
            renderFrame(painter,r,fspec,fspec.element+"-"+prevState);
            if (!fillWidgetInterior)
              renderInterior(painter,r,fspec,ispec,ispec.element+"-"+prevState);
            painter->restore();
            if (fillWidgetInterior)
              painter->fillRect(interiorRect(r,fspec),
                                tb->palette().brush(status.contains(QLatin1String("-inactive"))
                                                                      ? QPalette::Inactive
                                                                      : QPalette::Active,
                                                                     QPalette::Button));
          }
          if (animationOpacity_ >= 100)
            styleObject->setProperty("_kv_state", status);
        }
        else if (styleObject)
          styleObject->setProperty("_kv_state", status);

        /*if (!isHorizontal && !withArrow)
          painter->restore();*/
      }
      else // there is no QToolButton
      {
        if (styleObject)
          styleObject->setProperty("_kv_state", status);
        if (!autoraise || !status.startsWith(QLatin1String("normal")))
        {
          bool libreoffice = false;
          if (isLibreoffice_ && widget == nullptr
              && (option->state & State_Enabled) && !status.startsWith(QLatin1String("toggled"))
              && enoughContrast(getFromRGBA(lspec.normalColor), standardPalette().color(QPalette::ButtonText)))
          {
            libreoffice = true;
            painter->fillRect(option->rect, standardPalette().brush(QPalette::Window));
            painter->save();
            painter->setOpacity(0.5);
          }
          if (!fillWidgetInterior)
          {
            if ((option->state & State_Enabled) // Damn! QML has a bad text color for the diabled state.
                && widget == nullptr
                && enoughContrast(option->palette.color(QPalette::ButtonText),
                                  standardPalette().color(QPalette::ButtonText)))
            { // QML colorized button
              fspec.left = qMin(fspec.left,3);
              fspec.right = qMin(fspec.right,3);
              fspec.top = qMin(fspec.top,3);
              fspec.bottom = qMin(fspec.bottom,3);
              if (hasExpandedBorder(fspec))
                fspec.expansion = 0;
              else
                fspec.expansion = qMin(fspec.expansion, LIMITED_EXPANSION);
              painter->fillRect(interiorRect(r,fspec),
                                option->palette.brush(QPalette::Active, QPalette::Button));
            }
            else
              renderInterior(painter,r,fspec,ispec,ispec.element+"-"+status);
          }
          else // widget isn't null
            painter->fillRect(interiorRect(r,fspec),
                              widget->palette().brush(status.contains(QLatin1String("-inactive"))
                                                                        ? QPalette::Inactive
                                                                        : QPalette::Active,
                                                                      QPalette::Button));
          renderFrame(painter,r,fspec,fspec.element+"-"+status);
          if (libreoffice) painter->restore();
          hasPanel = true;
        }
      }

      /* the disabled painter is already restored when drawRaised is true */
      if (!(option->state & State_Enabled) && !drawRaised)
        painter->restore();

      if (widget && hasPanel && !paneledButtons.contains(widget))
      {
        paneledButtons.insert(widget);
        connect(widget, &QObject::destroyed, this, &Style::removeFromSet, Qt::UniqueConnection);
      }

      /* force text color if the button isn't drawn in a standard way */
      if (widget && !standardButton.contains(widget)
          && (option->state & State_Enabled))
      {
        QColor col;
        if (hasPanel)
        {
          if (status.startsWith(QLatin1String("pressed")))
          {
            if (isInactive)
            {
              col = getFromRGBA(lspec.pressInactiveColor);
              if (!col.isValid())
                col = getFromRGBA(lspec.pressColor);
            }
            else
              col = getFromRGBA(lspec.pressColor);
          }
          else if (status.startsWith(QLatin1String("toggled")))
          {
            if (isInactive)
            {
              col = getFromRGBA(lspec.toggleInactiveColor);
              if (!col.isValid())
                col = getFromRGBA(lspec.toggleColor);
            }
            else
              col = getFromRGBA(lspec.toggleColor);
          }
          else if (option->state & State_MouseOver)
          {
            if (isInactive)
            {
              col = getFromRGBA(lspec.focusInactiveColor);
              if (!col.isValid())
                col = getFromRGBA(lspec.focusColor);
            }
            else
              col = getFromRGBA(lspec.focusColor);
          }
          else
          {
            if (isInactive)
            {
              col = getFromRGBA(lspec.normalInactiveColor);
              if (!col.isValid())
                col = getFromRGBA(lspec.normalColor);
            }
            else
              col = getFromRGBA(lspec.normalColor);
          }

          if (!col.isValid())
          {
            col = standardPalette().color(isInactive ? QPalette::Inactive : QPalette::Active,
                                          QPalette::ButtonText);
          }
        }
        else
          /* FIXME: in fact, the foreground color of the parent widget should be
             used here (-> CE_ToolButtonLabel) but I've encountered no problem yet */
          col = standardPalette().color(isInactive ? QPalette::Inactive : QPalette::Active,
                                        QPalette::WindowText);
        forceButtonTextColor(widget,col);
      }

      if (drawSep)
      {
        const QString inactiveStr = isInactive ? "-inactive" : QString();
        renderElement(painter,fspec.element + "-separator" + inactiveStr,
                      QRect(x+r.width()-fspec.right, y+fspec.top, fspec.right, h-fspec.top-fspec.bottom));
        renderElement(painter,fspec.element + "-separator" + inactiveStr + "-top",
                      QRect(x+r.width()-fspec.right, y, fspec.right, fspec.top));
        renderElement(painter,fspec.element + "-separator" + inactiveStr + "-bottom",
                      QRect(x+r.width()-fspec.right, y+h-fspec.bottom, fspec.right, fspec.bottom));
      }

      /* focus rect should be drawn before label and arrow (-> drawComplexControl -> CC_ToolButton) */
      if (opt && (opt->state & State_HasFocus)
          /* drawn for tabbar scroll buttons at CE_ToolButtonLabel */
          && (!qobject_cast<QTabBar*>(p)
              || ((opt->toolButtonStyle != Qt::ToolButtonIconOnly && !opt->text.isEmpty())
                  || !opt->icon.isNull()
                  || !(opt->features & QStyleOptionToolButton::Arrow)
                  || opt->arrowType == Qt::NoArrow)))
      {
        if (fspec.hasFocusFrame)
        {
          renderFrame(painter,opt->rect,fspec,fspec.element+"-focus");
          const interior_spec ispec = getInteriorSpec(group);
          if (ispec.hasFocusInterior)
            renderInterior(painter,opt->rect,fspec,ispec,ispec.element+"-focus");
        }
        else
        {
          QRect focusRect;
          if (fspec.expansion > 0)
            focusRect = labelRect(opt->rect, fspec, lspec).adjusted(-2,-2,2,2);
          else
            focusRect = interiorRect(opt->rect, fspec);
          drawFocusRect(painter, focusRect, fspec.focusRectElement);
        }
      }

      break;
    }

    /* the frame is always drawn at PE_PanelButtonTool */
    case PE_FrameButtonTool : {return;}

    case PE_IndicatorRadioButton : {
      const interior_spec ispec = getInteriorSpec(QStringLiteral("RadioButton"));

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
        bool animate(opacityTimer_);
        if (qstyleoption_cast<const QStyleOptionMenuItem*>(option))
        {
          animate = false; // no animation for menuitems
          if (widget != nullptr // not QML or Libreoffice's unstyled menu
              && elementExists("menu-"+ispec.element+suffix))
          {
            prefix = "menu-"; // make exception for menuitems
          }
        }
        if (isWidgetInactive(widget))
          suffix.append("-inactive");
        /*if (isLibreoffice_ && suffix == "-checked-focused"
            && qstyleoption_cast<const QStyleOptionMenuItem*>(option))
          painter->fillRect(option->rect, option->palette.brush(QPalette::Highlight));*/
        QObject *styleObject = option->styleObject;
        const QString animationStartState = styleObject != nullptr
                                              ? styleObject->property("_kv_state").toString()
                                              : QString();
        animate = animate && widget && animatedWidget_ == widget
                  && !animationStartState.isEmpty()
                  && animationStartState != suffix
                  && !qstyleoption_cast<const QStyleOptionMenuItem*>(option)
                  && !qobject_cast<const QAbstractScrollArea*>(widget);
        if (animate && animationStartState == suffix)
        {
          if (opacityTimer_->isActive())
            opacityTimer_->stop();
          animationOpacity_ = 0;
          animate = false;
        }
        if (animate)
        {
          if (!opacityTimer_->isActive())
          {
            animationOpacity_ = 0;
            opacityTimer_->start(ANIMATION_FRAME);
          }
          if (animationOpacity_ < 100)
            renderElement(painter, ispec.element+animationStartState, option->rect);
          painter->save();
          painter->setOpacity(static_cast<qreal>(animationOpacity_)/100.0);
        }
        renderElement(painter, prefix+ispec.element+suffix, option->rect);
        if (animate)
        {
          painter->restore();
          if (animationOpacity_ >= 100)
            styleObject->setProperty("_kv_state", suffix);
        }
        else if (styleObject)
          styleObject->setProperty("_kv_state", suffix);
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
        if (widget != nullptr // not QML
            && qstyleoption_cast<const QStyleOptionMenuItem*>(option)
            && elementExists("menu-"+ispec.element+suffix))
          prefix = "menu-";
        if (isWidgetInactive(widget))
          suffix.append("-inactive");
        renderElement(painter, prefix+ispec.element+suffix, option->rect);
        if (!(option->state & State_Enabled))
          painter->restore();
      }

      break;
    }

    case PE_IndicatorCheckBox : {
      const interior_spec ispec = getInteriorSpec(QStringLiteral("CheckBox"));

      if (option->state & State_Enabled)
      {
        QString suffix, prefix;
        if (qstyleoption_cast<const QStyleOptionViewItem*>(option))
        {
          if (option->state & State_Selected)
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
        }
        else
        {
          if (option->state & State_MouseOver) // covers menu checkboxes too
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
        }
        bool animate(opacityTimer_ != nullptr);
        if (qstyleoption_cast<const QStyleOptionMenuItem*>(option))
        {
          animate = false;
          if (widget != nullptr // not QML
              && elementExists("menu-"+ispec.element+suffix))
          {
            prefix = "menu-"; // make exception for menuitems
          }
        }
        else if (qstyleoption_cast<const QStyleOptionViewItem*>(option))
        {
          animate = false;
          if (widget != nullptr // not QML
              && elementExists("item-"+ispec.element+suffix))
          {
            prefix = "item-"; // make exception for viewitems
          }
        }
        if (isWidgetInactive(widget))
          suffix.append("-inactive");
        /*if (isLibreoffice_ && suffix == "-checked-focused"
            && qstyleoption_cast<const QStyleOptionMenuItem*>(option))
          painter->fillRect(option->rect, option->palette.brush(QPalette::Highlight));*/
        QObject *styleObject = option->styleObject;
        const QString animationStartState = styleObject != nullptr
                                              ? styleObject->property("_kv_state").toString()
                                              : QString();
        animate =  animate && widget && animatedWidget_ == widget
                   && !animationStartState.isEmpty()
                   && animationStartState != suffix
                   && !qobject_cast<const QAbstractScrollArea*>(widget);
        if (animate && animationStartState == suffix)
        {
          if (opacityTimer_->isActive())
            opacityTimer_->stop();
          animationOpacity_ = 0;
          animate = false;
        }
        if (animate)
        {
          if (!opacityTimer_->isActive())
          {
            animationOpacity_ = 0;
            opacityTimer_->start(ANIMATION_FRAME);
          }
          if (animationOpacity_ < 100)
            renderElement(painter, ispec.element+animationStartState, option->rect);
          painter->save();
          painter->setOpacity(static_cast<qreal>(animationOpacity_)/100.0);
        }
        renderElement(painter, prefix+ispec.element+suffix, option->rect);
        if (animate)
        {
          painter->restore();
          if (animationOpacity_ >= 100)
            styleObject->setProperty("_kv_state", suffix);
        }
        else if (styleObject)
          styleObject->setProperty("_kv_state", suffix);
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
        if (qstyleoption_cast<const QStyleOptionMenuItem*>(option))
        {
          if (widget != nullptr // not QML
              && elementExists("menu-"+ispec.element+suffix))
          {
            prefix = "menu-";
          }
        }
        else if (widget != nullptr // not QML
                 && qstyleoption_cast<const QStyleOptionViewItem*>(option)
                 && elementExists("item-"+ispec.element+suffix))
        {
          prefix = "item-";
        }
        if (isWidgetInactive(widget))
          suffix.append("-inactive");
        renderElement(painter, prefix+ispec.element+suffix, option->rect);
        if (!(option->state & State_Enabled))
          painter->restore();
      }

      break;
    }

    case PE_FrameFocusRect : {
      if (qstyleoption_cast<const QStyleOptionFocusRect*>(option)
          /* this would be not only useless but also ugly */
          && !(widget && widget->inherits("QComboBoxListView")))
      { // no interior
        frame_spec fspec = getFrameSpec(QStringLiteral("Focus"));
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
      const indicator_spec dspec = getIndicatorSpec(QStringLiteral("TreeExpander"));
      QRect r = option->rect;
      bool rtl(option->direction == Qt::RightToLeft);
      qreal expanderAdjust = 0.0;

      if (option->state & State_Children)
      {
        frame_spec fspec;
        default_frame_spec(fspec);

        QString status = getState(option,widget);
        QString eStatus = "normal";
        /* some apps (like Dolphin) don't set the state to enabled; moreover,
           we don't consider the disabled state in drawing branch lines either */
        /*if (!(option->state & State_Enabled))
          eStatus = "disabled";
        else */if (option->state & State_MouseOver)
          eStatus = "focused";
        else if (status.startsWith(QLatin1String("toggled")) || status.startsWith(QLatin1String("pressed")))
          eStatus = "pressed";
        if (isWidgetInactive(widget))
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
          //if(!( sizeLimit&1)) --sizeLimit; // make it odd
          expanderAdjust = static_cast<qreal>(sizeLimit)/2 + static_cast<qreal>(1);
        }
      }

      if (tspec_.tree_branch_line) // adapted from Oxygen
      {
        const QRectF rf(r); // maximum precision
        const QPointF center(rf.center());
        const qreal centerX = center.x();
        const qreal centerY = center.y();

        QColor col;
        if (qGray(option->palette.color(QPalette::Window).rgb()) <= 100)
          col = option->palette.color(QPalette::Light);
        else
          col = option->palette.color(QPalette::Dark);
        if (!col.isValid()) break;

        /* With a translucent light/dark color, small overlaps are visible. They could be
           avoided by adding or subtracting 1 in proper places, but that method would cause
           gaps on mouse-over with scale factors. As a workaround, we make the color opaque. */
        QColor baseCol = standardPalette().color(QPalette::Base);
        baseCol.setAlpha(255);
        col = overlayColor(baseCol, col);

        painter->save();
        painter->setPen(col);
        if (option->state & (State_Item | State_Children | State_Sibling))
        {
          const QLineF line(QPointF(centerX, rf.top()), QPointF(centerX, centerY - expanderAdjust));
          painter->drawLine(line);
        }
        // the right/left (depending on dir) line will be drawn if there is an item
        if (option->state & State_Item)
        {
          const QLineF line = rtl ?
                QLineF(QPointF(rf.left(), centerY), QPointF(centerX - expanderAdjust, centerY)) :
                QLineF(QPointF(centerX + expanderAdjust, centerY), QPointF(rf.right(), centerY));
          painter->drawLine(line);
        }
        // the bottom if we have a sibling
        if (option->state & State_Sibling)
        {
          const QLineF line(QPointF(centerX, centerY + expanderAdjust), QPointF(centerX, rf.bottom()));
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
      /* At least QComboBoxPrivateContainer and toolbars may also
         use this, so continue only if the widget is really a menu. */
      if ((widget // it's NULL in the case of QML menus
           && !qobject_cast<const QMenu*>(widget))
          /*|| isLibreoffice_*/) // LibreOffice's menus can be styled but not well
        break;

      const QString group = "Menu";
      frame_spec fspec = getFrameSpec(group);
      fspec.expansion = 0;
      const interior_spec ispec = getInteriorSpec(group);

      QRect r = option->rect;
      theme_spec tspec_now = settings_->getCompositeSpec();
      bool isTranslucent(!noComposite_ && widget && translucentWidgets_.contains(widget)
                         /* detached (Qt5) menus may come here because of setSurfaceFormat() */
                         && !widget->testAttribute(Qt::WA_X11NetWmWindowTypeMenu));

      if (tspec_.spread_menuitems
          && (tspec_.shadowless_popup || noComposite_ || !tspec_now.composite))
      { // PM_MenuHMargin is zero but we draw the frame (this condition was used in getMenuMargin())
        fspec.left = fspec.right = qMax(fspec.left,fspec.right);
      }
      else
      {
        fspec.left = fspec.right = pixelMetric(PM_MenuHMargin,option,widget);
      }
      fspec.top = fspec.bottom = pixelMetric(PM_MenuVMargin,option,widget);

      if (!tspec_.shadowless_popup
          && isTranslucent && tspec_now.menu_shadow_depth > 0
          && fspec.left >= tspec_now.menu_shadow_depth // otherwise shadow will have no meaning
          && menuShadow_.count() == 4)
      {
        if (tspec_.spread_menuitems)
          r.adjust(fspec.left-realMenuShadow_.at(0) , 0, -fspec.right+realMenuShadow_.at(2), 0);
        if (tspec_.reduce_menu_opacity > 0)
        {
          QRect R = r;
          R = R.marginsRemoved(QMargins(realMenuShadow_.at(0), realMenuShadow_.at(1),
                                        realMenuShadow_.at(2), realMenuShadow_.at(3)));
          painter->save();
          painter->setClipRegion(QRegion(r).subtracted(QRegion(R)));
          renderFrame(painter,r,fspec,fspec.element+"-shadow");
          painter->restore();

          painter->save();
          painter->setOpacity(1.0 - static_cast<qreal>(tspec_.reduce_menu_opacity)/100.0);

          painter->save();
          painter->setClipRegion(QRegion(R));
          renderFrame(painter,r,fspec,fspec.element+"-shadow");
          painter->restore();

          if (!renderInterior(painter,r,fspec,ispec,ispec.element+"-normal"))
            painter->fillRect(interiorRect(r,fspec), standardPalette().color(QPalette::Window));
          painter->restore();
        }
        else
        {
          renderFrame(painter,r,fspec,fspec.element+"-shadow");
          if (!renderInterior(painter,r,fspec,ispec,ispec.element+"-normal"))
            painter->fillRect(interiorRect(r,fspec), standardPalette().color(QPalette::Window));
        }
      }
      else
      {
        if (isTranslucent)
        {
          painter->save();
          painter->setOpacity(1.0 - static_cast<qreal>(tspec_.reduce_menu_opacity)/100.0);
        }
        if (!widget || !renderInterior(painter,r,fspec,ispec,ispec.element+"-normal")) // QML
          painter->fillRect(r, standardPalette().color(QPalette::Window));
        renderFrame(painter,r,fspec,fspec.element+"-normal");
        if (isTranslucent)
          painter->restore();
      }

      break;
    }

    case PE_FrameWindow : {
      /* QPainter::drawLine() isn't reliable with devicePixelRatio>1. Also, note
         that QRect::bottomRight() is one pixel short in both directions. */

      QRect r = option->rect;
      QColor shadow(0,0,0,55);

      // left
      painter->fillRect(QRect(QPoint(r.left()+1, r.top()), QPoint(r.left()+1, r.bottom()-1)),
                        QColor(255,255,255,15));
      painter->fillRect(QRect(QPoint(r.left(), r.top()), QPoint(r.left(), r.bottom()-1)),
                        shadow);
      // bottom
      painter->fillRect(QRect(QPoint(r.left()+2, r.bottom()-1), QPoint(r.right()-2, r.bottom()-1)),
                        QColor(0,0,0,25));
      painter->fillRect(QRect(QPoint(r.left(), r.bottom()), QPoint(r.right()-1, r.bottom())),
                        shadow);
      // right
      painter->fillRect(QRect(QPoint(r.right()-1, r.top()), QPoint(r.right()-1, r.bottom()-1)),
                        QColor(0,0,0,15));
      painter->fillRect(QRect(QPoint(r.right(), r.top()), QPoint(r.right(), r.bottom())),
                        shadow);

      break;
    }

    //case PE_FrameButtonBevel :
    case PE_Frame : {
      const QAbstractScrollArea *sa = qobject_cast<const QAbstractScrollArea*>(widget);
      if (!widget // it's NULL with QML
          || sa
          || widget->inherits("QWellArray") // color dialog's color rects
          || widget->inherits("QComboBoxPrivateContainer")) // frame for combo popups
      {
        bool pcmanfmInactiveView(false);
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
                  && QString(pw->metaObject()->className()).startsWith(QLatin1String("Dolphin")))
              {
                break;
              }
            }
          }
          else if (isPcmanfm_ && (hspec_.transparent_pcmanfm_view || hspec_.transparent_pcmanfm_sidepane))
          {
            if (QWidget *pw = widget->parentWidget())
            {
              if (hspec_.transparent_pcmanfm_view && pw->inherits("Fm::FolderView"))
              {
                /* fill in an inactive view frame of a split view with the base color */
                if (pw->palette().color(QPalette::Active, QPalette::Base)
                    != standardPalette().color(QPalette::Active,QPalette::Base))
                {
                   pcmanfmInactiveView = true;
                   painter->fillRect(interiorRect(option->rect,getFrameSpec(QStringLiteral("GenericFrame"))),
                                     standardPalette().color(isWidgetInactive(widget)
                                                               ? QPalette::Inactive
                                                               : QPalette::Active,
                                                             QPalette::Base));
                }
                else if (qobject_cast<QMainWindow*>(pw->window()))
                  break;
              }
              else if (hspec_.transparent_pcmanfm_sidepane && pw->inherits("Fm::SidePane"))
              {
                break;
              }
            }
          }
        }

        if (widget && widget->inherits("QComboBoxPrivateContainer")
            && tspec_.combo_menu /*&& !isLibreoffice_*/)
        { // as with PE_PanelMenu FIXME: calling it instead?
          const QString group = "Menu";
          frame_spec fspec = getFrameSpec(group);
          fspec.expansion = 0;
          const interior_spec ispec = getInteriorSpec(group);
          fspec.top = fspec.bottom = pixelMetric(PM_MenuVMargin,option,widget);
          QRect r = option->rect;
          theme_spec tspec_now = settings_->getCompositeSpec();
          bool isTranslucent(!noComposite_ && widget && translucentWidgets_.contains(widget)
                             /* detached (Qt5) menus may come here because of setSurfaceFormat() */
                             && !widget->testAttribute(Qt::WA_X11NetWmWindowTypeMenu));
          if (tspec_.spread_menuitems
              && (tspec_.shadowless_popup || noComposite_ || !tspec_now.composite))
          { // PM_MenuHMargin is zero but we draw the frame (this condition was used in getMenuMargin())
            fspec.left = fspec.right = qMax(fspec.left,fspec.right);
          }
          else
          {
            fspec.left = fspec.right = pixelMetric(PM_MenuHMargin,option,widget);
          }
          if (!tspec_.shadowless_popup
              && isTranslucent && tspec_now.menu_shadow_depth > 0
              && fspec.left >= tspec_now.menu_shadow_depth // otherwise shadow will have no meaning
              && menuShadow_.count() == 4)
          {
            if (tspec_.spread_menuitems)
              r.adjust(fspec.left-realMenuShadow_.at(0) , 0, -fspec.right+realMenuShadow_.at(2), 0);
            if (tspec_.reduce_menu_opacity > 0)
            {
              QRect R = r;
              R = R.marginsRemoved(QMargins(realMenuShadow_.at(0), realMenuShadow_.at(1),
                                            realMenuShadow_.at(2), realMenuShadow_.at(3)));
              painter->save();
              painter->setClipRegion(QRegion(r).subtracted(QRegion(R)));
              renderFrame(painter,r,fspec,fspec.element+"-shadow");
              painter->restore();

              painter->save();
              painter->setOpacity(1.0 - static_cast<qreal>(tspec_.reduce_menu_opacity)/100.0);

              painter->save();
              painter->setClipRegion(QRegion(R));
              renderFrame(painter,r,fspec,fspec.element+"-shadow");
              painter->restore();

              if (!renderInterior(painter,r,fspec,ispec,ispec.element+"-normal"))
                painter->fillRect(interiorRect(r,fspec), standardPalette().color(QPalette::Window));
              painter->restore();
            }
            else
            {
              renderFrame(painter,r,fspec,fspec.element+"-shadow");
              if (!renderInterior(painter,r,fspec,ispec,ispec.element+"-normal"))
                painter->fillRect(interiorRect(r,fspec), standardPalette().color(QPalette::Window));
            }
          }
          else
          {
            if (isTranslucent)
            {
              painter->save();
              painter->setOpacity(1.0 - static_cast<qreal>(tspec_.reduce_menu_opacity)/100.0);
            }
            else // we enforced translucency on the combo menu at polish(QWidget*)
              painter->fillRect(r, standardPalette().color(QPalette::Window));
            if (!renderInterior(painter,r,fspec,ispec,ispec.element+"-normal") && isTranslucent)
              painter->fillRect(r, standardPalette().color(QPalette::Window));
            renderFrame(painter,r,fspec,fspec.element+"-normal");
            if (isTranslucent)
              painter->restore();
          }
          break;
        }

        /* -> polish(QWidget *widget) */
        bool hasFlatBg = !(sa
                           && (sa->backgroundRole() == QPalette::Window
                               || sa->backgroundRole() == QPalette::Button)
                           && sa->viewport()
                           && (sa->viewport()->backgroundRole() == QPalette::Window
                               || sa->viewport()->backgroundRole() == QPalette::Button));
        if (!hasFlatBg && tspec_.remove_extra_frames) break;

        frame_spec fspec = getFrameSpec(QStringLiteral("GenericFrame"));
        fspec.expansion = 0;

        if (!(option->state & State_Enabled))
        {
          painter->save();
          painter->setOpacity(DISABLED_OPACITY);
        }
        QString fStatus = "normal";
        if (widget && widget->hasFocus() && hasFlatBg
            && !widget->inherits("QWellArray") // color rects always have focus!
            && elementExists(fspec.element+"-focused-top"))
        {
          fStatus = "focused";
        }
        if (isWidgetInactive(widget))
          fStatus = "normal-inactive"; // the focus state is meaningless here
        if (!widget) // QML again!
        {
          QColor baseCol = standardPalette().color(QPalette::Base);
          baseCol.setAlpha(255);
          painter->fillRect(option->rect, baseCol);
        }
        bool canAnimate(opacityTimer_ && !pcmanfmInactiveView && widget && widget->isEnabled());
        bool animate(canAnimate
                     && ((animatedWidget_ == widget
                          && opacityTimer_->isActive()
                          && !fStatus.startsWith(QLatin1String("normal")))
                         || (animatedWidgetOut_ == widget
                             && opacityTimerOut_->isActive()
                             && fStatus.startsWith(QLatin1String("normal")))));
        QString animationStartState(animationStartState_);
        int animationOpacity = animationOpacity_;
        if (animate)
        {
          if (fStatus.startsWith(QLatin1String("normal"))) // -> QEvent::FocusOut
          {
            animationStartState = animationStartStateOut_;
            animationOpacity = animationOpacityOut_;
          }
          if (animationStartState == fStatus)
          {
            animationOpacity = 100;
            if (fStatus.startsWith(QLatin1String("normal")))
              animationOpacityOut_ = 100;
            else
              animationOpacity_ = 100;
          }
          else if (animationOpacity < 100)
            renderFrame(painter,option->rect,fspec,fspec.element+"-"+animationStartState);
          painter->save();
          painter->setOpacity(static_cast<qreal>(animationOpacity)/100.0);
        }
        renderFrame(painter,option->rect,fspec,fspec.element+"-"+fStatus);
        if (animate)
        {
          painter->restore();
          if (animationOpacity >= 100)
          {
            if (fStatus.startsWith(QLatin1String("normal")))
              animationStartStateOut_ = fStatus;
            else
              animationStartState_ = fStatus;
          }
        }
        else if (canAnimate)
        {
          if (animatedWidget_ == widget)
            animationStartState_ = fStatus;
          if (animatedWidgetOut_ == widget)
            animationStartStateOut_ = fStatus;
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
      bool isInactive(isWidgetInactive(widget));
      if (const QAbstractScrollArea *sa = qobject_cast<const QAbstractScrollArea*>(widget))
      {
        if (QWidget *vp = sa->viewport())
        {
          if (!vp->autoFillBackground()
              || (vp->testAttribute(Qt::WA_StyleSheetTarget)
                  && !vp->styleSheet().isEmpty() && vp->styleSheet().contains(QLatin1String("background"))))
          {
            return;
          }
          col = vp->palette().color(isInactive
                                      ? QPalette::Inactive
                                      : QPalette::Active,
                                    vp->backgroundRole());
        }
      }
      if (!col.isValid())
        col = option->palette.color(isInactive
                                      ? QPalette::Inactive
                                      : QPalette::Active,
                                    QPalette::Window);
      painter->fillRect(option->rect, col);
      break;
    }

    case PE_FrameGroupBox : {
      const QStyleOptionFrame *frame = qstyleoption_cast<const QStyleOptionFrame*>(option);
      if (frame)
      {
        if (frame->features & QStyleOptionFrame::Flat)
          break;
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
        if (isWidgetInactive(widget))
          suffix = "-normal-inactive";
        renderFrame(painter,option->rect,fspec,fspec.element+suffix,0,0,0,0,0,true);
        if (tspec_.groupbox_top_label
            && widget) // QML anchoring
          renderInterior(painter,option->rect,fspec,ispec,ispec.element+suffix,true);
        if (!(option->state & State_Enabled))
          painter->restore();
      }

      break;
    }

    case PE_FrameTabWidget : {
      const QString group = "TabFrame";
      frame_spec fspec = getFrameSpec(group);
      const interior_spec ispec = getInteriorSpec(group);

      frame_spec fspec1 = fspec;
      int d = 0;
      int l = 0;
      int tp = 0;
      QRect attachmentRect;

      if (tspec_.attach_active_tab)
      {
        fspec1.expansion = fspec.expansion = 0;
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
          if (const QStyleOptionTabWidgetFrame *twf =
              qstyleoption_cast<const QStyleOptionTabWidgetFrame*>(option))
          {
            if (!twf->tabBarSize.isEmpty()) // it's empty in Kdenlive
              tr = twf->selectedTabRect;
          }
          // as in GoldenDict's Preferences dialog
          else if (QTabBar *tb = tw->tabBar())
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
                attachmentRect = QRect(d, option->rect.y(), l, fspec.top);
                break;
              }
              case QTabWidget::South: {
                fspec1.bottom = 0;
                d = tr.x();
                l = tr.width();
                attachmentRect = QRect(d, option->rect.y()+option->rect.height()-fspec.bottom, l, fspec.bottom);
                break;
              }
              case QTabWidget::West: {
                fspec1.left = 0;
                d = tr.y();
                l = tr.height();
                attachmentRect = QRect(option->rect.x(), d, fspec.left, l);
                break;
              }
              case QTabWidget::East: {
                fspec1.right = 0;
                d = tr.y();
                l = tr.height();
                attachmentRect = QRect(option->rect.x()+option->rect.width()-fspec.right, d, fspec.right, l);
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
      if (isWidgetInactive(widget))
        suffix = "-normal-inactive";
      if (widget) // WARNING: QML has anchoring!
      {
        /* The frame SVG elements may have translucency. So, instead of drawing the
           interior inside the whole extended rectangle, we clip the painter region. */
        QRegion reg = QRegion(interiorRect(option->rect,fspec)).united(attachmentRect);
        if (!reg.isEmpty())
        {
          painter->save();
          painter->setClipRegion(reg);
          renderInterior(painter,option->rect,fspec1,ispec,ispec.element+suffix,true);
          painter->restore();
        }
      }
      const frame_spec fspecT = getFrameSpec(QStringLiteral("Tab"));
      renderFrame(painter,
                  option->rect,
                  fspec,fspec.element+suffix,
                  d, l, fspecT.left, fspecT.right,tp,
                  true);
      if (!(option->state & State_Enabled))
        painter->restore();

      break;
    }

    /* frame is forced on lineedits at PE_PanelLineEdit */
    case PE_FrameLineEdit : {
      if (isLibreoffice_ && widget == nullptr) // Libreoffice doesn't use PE_PanelLineEdit
        drawPrimitive(PE_PanelLineEdit, option, painter, widget);
      return;
    }

    case PE_PanelLineEdit : {
      /* don't draw the interior or frame of a Plasma spinbox */
      if (isPlasma_ && widget && widget->window()->testAttribute(Qt::WA_NoSystemBackground))
        break;

      QWidget *p = getParent(widget,1);

      /* We draw the lineedit of an editable combo only in drawComplexControl() -> CC_ComboBox.
         It seems that some style plugins draw it twice. */
      if (qobject_cast<const QLineEdit*>(widget))
      {
        if (qobject_cast<QComboBox*>(p) || (p && p->property("_kv_combo").toBool()))
          break;
      }
      else if (QObject *styleObject = option->styleObject)
      {
        if (QObject *pObj = styleObject->parent())
        {
          if (pObj->property("_kv_combo").toBool())
            break;
        }
      }

      QString group;
      if ((!getFrameSpec(QStringLiteral("ToolbarLineEdit")).element.isEmpty()
           || !getInteriorSpec(QStringLiteral("ToolbarLineEdit")).element.isEmpty())
          && getStylableToolbarContainer(widget, true)
          && !enoughContrast(widget->palette().color(QPalette::Active, QPalette::Text),
                             getFromRGBA(getLabelSpec(QStringLiteral("Toolbar")).normalColor)))
      {
        group = "ToolbarLineEdit";
      }
      else
        group = "LineEdit";
      interior_spec ispec = getInteriorSpec(group);
      frame_spec fspec = getFrameSpec(group);

      /* always draw a thin frame when editing itemview texts */
      if (qobject_cast<QAbstractItemView*>(getParent(p,1)))
      {
        painter->save();
        painter->setPen(option->palette.color(QPalette::Highlight));
        painter->drawRect(x, y, w-1, h-1);
        painter->restore();
        fspec.left = fspec.right = fspec.top = fspec.bottom = 1;
        fspec.expansion = 0;
        if (ispec.hasInterior)
          renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-focused");
        else
        {
          QColor baseCol = widget->palette().color(QPalette::Base);
          baseCol.setAlpha(255);
          painter->fillRect(interiorRect(option->rect,fspec), baseCol);
        }
        return;
      }

      const label_spec lspec = getLabelSpec(group);
      const size_spec sspec = getSizeSpec(group);

      bool colored(false); // a workaround for bad codes that change line-edit base color

      QAbstractSpinBox *sb = qobject_cast<QAbstractSpinBox*>(p);
      const QStyleOptionSpinBox *sbOpt = qstyleoption_cast<const QStyleOptionSpinBox*>(option);
      bool insideSpinBox(sb || sbOpt
                         || (p && (p->inherits("KisAbstractSliderSpinBox")
                                   || p->inherits("Digikam::DAbstractSliderSpinBox")))
                         /*|| (isLibreoffice_ && sbOpt)*/);

      if (widget == nullptr && !isLibreoffice_) // WARNING: QML has anchoring!
      {
        if (hasExpandedBorder(fspec))
          fspec.expansion = 0;
        else
          fspec.expansion = qMin(fspec.expansion, LIMITED_EXPANSION);
        ispec.px = ispec.py = 0;
      }

      /*if (isLibreoffice_)
      {
        fspec.left = qMin(fspec.left,3);
        fspec.right = qMin(fspec.right,3);
        fspec.top = qMin(fspec.top,3);
        fspec.bottom = qMin(fspec.bottom,3);
      }*/
      else if (qobject_cast<const QLineEdit*>(widget))
      {
        colored = !isPcmanfm_ && !insideSpinBox && group == "LineEdit"
                  && widget->palette().color(QPalette::Base)
                     != standardPalette().color(widget->palette().currentColorGroup(), QPalette::Base)
                  && widget->palette().color(QPalette::Base).saturation() > 10;
        if ((widget->testAttribute(Qt::WA_StyleSheetTarget)
             && !widget->styleSheet().isEmpty() && widget->styleSheet().contains(QLatin1String("padding")))
            || widget->minimumWidth() == widget->maximumWidth()
            || widget->height() < sizeCalculated(widget->font(),fspec,lspec,sspec,QStringLiteral("W"),QSize()).height())
        {
          fspec.left = qMin(fspec.left,3);
          fspec.right = qMin(fspec.right,3);
          fspec.top = qMin(fspec.top,3);
          fspec.bottom = qMin(fspec.bottom,3);

          if (!hasExpandedBorder(fspec))
            fspec.expansion = qMin(fspec.expansion, LIMITED_EXPANSION);
          else
          {
            fspec.leftExpanded = qMin(fspec.leftExpanded,3);
            fspec.rightExpanded = qMin(fspec.rightExpanded,3);
            fspec.topExpanded = qMin(fspec.topExpanded,3);
            fspec.bottomExpanded = qMin(fspec.bottomExpanded,3);
          }
        }
      }
      if (insideSpinBox)
      {
        if (!sb || sb->buttonSymbols() != QAbstractSpinBox::NoButtons)
        {
          fspec.isAttached = true;
          fspec.HPos = -1;
        }

        // the measure we used for CC_SpinBox at drawComplexControl()
        if (fspec.HPos == -1 && (tspec_.vertical_spin_indicators
                                 || (!widget && sbOpt && sbOpt->frame)
                                 // Krita 5.0.0
                                 || (widget && (widget->inherits("KisIntParseSpinBox")
                                                || widget->inherits("KisDoubleParseSpinBox")))
                                 || (p && (p->inherits("KisIntParseSpinBox")
                                           || p->inherits("KisDoubleParseSpinBox")))))
        {
          fspec.left = qMin(fspec.left,3);
          fspec.right = qMin(fspec.right,3);
          fspec.top = qMin(fspec.top,3);
          fspec.bottom = qMin(fspec.bottom,3);

          if (hasExpandedBorder(fspec))
            fspec.expansion = 0;
          else
            fspec.expansion = qMin(fspec.expansion, LIMITED_EXPANSION);
        }
        else if (sb)
        {
          QString maxTxt = spinMaxText(sb);
          if (maxTxt.isEmpty()
              || option->rect.width() < textSize(sb->font(),maxTxt).width() + fspec.left
                                        + (sspec.incrementW ? sspec.minW : 0)
                                        + (sb->buttonSymbols() == QAbstractSpinBox::NoButtons ? fspec.right : 0)
              || (sb->buttonSymbols() != QAbstractSpinBox::NoButtons
                  && sb->width() < widget->width() + 2*tspec_.spin_button_width
                                                   + getFrameSpec(QStringLiteral("IndicatorSpinBox")).right)
              || sb->height() < fspec.top+fspec.bottom+QFontMetrics(widget->font()).height())
          {
            fspec.left = qMin(fspec.left,3);
            fspec.right = qMin(fspec.right,3);
            fspec.top = qMin(fspec.top,3);
            fspec.bottom = qMin(fspec.bottom,3);

            if (!hasExpandedBorder(fspec))
              fspec.expansion = qMin(fspec.expansion, LIMITED_EXPANSION);
            else
            {
              fspec.leftExpanded = qMin(fspec.leftExpanded,3);
              fspec.rightExpanded = qMin(fspec.rightExpanded,3);
              fspec.topExpanded = qMin(fspec.topExpanded,3);
              fspec.bottomExpanded = qMin(fspec.bottomExpanded,3);
            }
          }
        }
      }
      else if (qobject_cast<QComboBox*>(p)) // FIXME: without being a QLineEdit?!
      {
        fspec.isAttached = true;
        /* see if there is any icon on the left of the combo box (for LTR) */
        if (option->direction == Qt::RightToLeft)
        {
          int arrowFrameSize = tspec_.combo_as_lineedit ? fspec.left : getFrameSpec(QStringLiteral("ComboBox")).left;
          if (widget->width() < p->width()
                               - (tspec_.square_combo_button
                                  ? qMax(COMBO_ARROW_LENGTH, p->height()-arrowFrameSize)
                                  : COMBO_ARROW_LENGTH)
                                - arrowFrameSize)
        {
          fspec.HPos = 0;
        }
          else fspec.HPos = 1;
        }
        else
        {
          if (widget->x() > 0) fspec.HPos = 0;
          else fspec.HPos = -1;
        }
      }

      bool fillWidgetInterior(!ispec.hasInterior
                              && hasHighContrastWithContainer(widget,
                                                              widget ? widget->palette().color(QPalette::Text)
                                                              : standardPalette().color(QPalette::Text)));

      // lineedits only have normal and focused states in Kvantum
      QString leStatus = (option->state & State_HasFocus) ? "focused" : "normal";
      if (isWidgetInactive(widget))
        leStatus.append("-inactive");
      if (!(option->state & State_Enabled))
      {
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      bool canAnimate(/*!isLibreoffice_ &&*/ opacityTimer_ && widget && widget->isEnabled()
                      && !qobject_cast<const QAbstractScrollArea*>(widget));
      bool animateSpin(canAnimate
                       && qobject_cast<QAbstractSpinBox*>(p)
                       && ((animatedWidget_ == p
                            && opacityTimer_->isActive()
                            && !leStatus.startsWith(QLatin1String("normal")))
                           || (animatedWidgetOut_ == p
                               && opacityTimerOut_->isActive()
                               && leStatus.startsWith(QLatin1String("normal")))));
      bool animate(canAnimate
                   && ((animatedWidget_ == widget
                        && opacityTimer_->isActive()
                        && !leStatus.startsWith(QLatin1String("normal")))
                       || (animatedWidgetOut_ == widget
                           && opacityTimerOut_->isActive()
                           && leStatus.startsWith(QLatin1String("normal")))
                       || animateSpin));
      QString animationStartState(animationStartState_);
      int animationOpacity = animationOpacity_;
      if (animate)
      {
        if (leStatus.startsWith(QLatin1String("normal"))) // -> QEvent::FocusOut
        {
          animationStartState = animationStartStateOut_;
          animationOpacity = animationOpacityOut_;
        }
        if (animationStartState == leStatus)
        {
          animationOpacity = 100;
          if (leStatus.startsWith(QLatin1String("normal")))
            animationOpacityOut_ = 100;
          else
            animationOpacity_ = 100;
        }
        else if (animationOpacity < 100)
        {
          renderFrame(painter,option->rect,fspec,fspec.element+"-"+animationStartState);
          if (!fillWidgetInterior)
            renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+animationStartState);
        }
        painter->save();
        painter->setOpacity(static_cast<qreal>(animationOpacity)/100.0);
      }
      /* force frame */
      renderFrame(painter,
                  /*isLibreoffice_ && !sbOpt ?
                    option->rect.adjusted(fspec.left,fspec.top,-fspec.right,-fspec.bottom) :*/
                    option->rect,
                  fspec,
                  fspec.element+"-"+leStatus);
      if (!fillWidgetInterior)
        renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+leStatus);
      if (animate)
      {
        painter->restore();
        if (animationOpacity >= 100)
        {
          if (leStatus.startsWith(QLatin1String("normal")))
            animationStartStateOut_ = leStatus;
          else
            animationStartState_ = leStatus;
        }
      }
      else if (canAnimate)
      {
        if (animatedWidget_ == widget
            || (qobject_cast<QAbstractSpinBox*>(p) && animatedWidget_ == p))
        {
          animationStartState_ = leStatus;
        }
        if (animatedWidgetOut_ == widget
            || (qobject_cast<QAbstractSpinBox*>(p) && animatedWidget_ == p))
        {
          animationStartStateOut_ = leStatus;
        }
      }
      if (fillWidgetInterior) // widget isn't null
      {
        QColor baseCol = widget->palette().color(leStatus.contains(QLatin1String("-inactive"))
                                                   ? QPalette::Inactive
                                                   : QPalette::Active,
                                                 QPalette::Base);
        baseCol.setAlpha(255);
        painter->fillRect(interiorRect(option->rect,fspec), baseCol);
      }
      else if (colored) // widget isn't null
      {
        QColor baseCol = widget->palette().color(QPalette::Base);
        baseCol.setAlpha(255);
        colorizeRect(painter, baseCol, subElementRect(SE_LineEditContents, option, widget));
      }
      if (!(option->state & State_Enabled))
        painter->restore();

      break;
    }

    /* toolbar is drawn at CE_ToolBar */
    case PE_PanelToolBar : {
      return;
    }

    case PE_IndicatorToolBarHandle :
    case PE_IndicatorToolBarSeparator : {
      const QString group = "Toolbar";

      frame_spec fspec;
      default_frame_spec(fspec);
      interior_spec ispec;
      default_interior_spec(ispec);
      const indicator_spec dspec = getIndicatorSpec(group);

      QRect r = option->rect;
      const QString inactiveStr = isWidgetInactive(widget) ? "-inactive" : QString();
      if (element == PE_IndicatorToolBarHandle && tspec_.center_toolbar_handle)
      {
        int margin = qMax(3 - pixelMetric(PM_ToolBarItemMargin,option,widget)
                            - tspec_.toolbar_interior_spacing,
                          0); // -> PM_ToolBarHandleExtent
        if (option->state & State_Horizontal)
          renderIndicator(painter,
                          option->direction == Qt::RightToLeft ? r.adjusted(0,0,-margin,0)
                                                               : r.adjusted(margin,0,0,0),
                          fspec,dspec,dspec.element+"-handle"+inactiveStr,option->direction,
                          Qt::AlignVCenter | Qt::AlignLeft);
        else
          renderIndicator(painter,
                          r.adjusted(0,margin,0,0),
                          fspec,dspec,dspec.element+"-handle"+inactiveStr,option->direction,
                          Qt::AlignHCenter | Qt::AlignTop);
      }
      else
      {
        if (!(option->state & State_Horizontal))
        {
          r.setRect(y, x, h, w);
          painter->save();
          QTransform m;
          m.scale(1,-1);
          m.rotate(-90);
          painter->setTransform(m, true);
        }
        renderInterior(painter,r,fspec,ispec,
                       dspec.element
                         +(element == PE_IndicatorToolBarHandle ? "-handle" : "-separator")+inactiveStr);

        if (!(option->state & State_Horizontal))
          painter->restore();
      }

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
      bool verticalIndicators(tspec_.vertical_spin_indicators
                              || (!widget && opt && opt->frame)
                              // Krita 5.0.0
                              || (widget && (widget->inherits("KisIntParseSpinBox")
                                             || widget->inherits("KisDoubleParseSpinBox"))));

      frame_spec fspec;
      int vOffset = 0;
      if (!verticalIndicators)
      {
        fspec = getFrameSpec(group);
        fspec.isAttached = true;
        if (up)
          fspec.HPos = 1;
        else
          fspec.HPos = 0;
        if (tspec_.inline_spin_indicators)
        { // only when there is enough space for the line-edit (-> SE_LineEditContents)
          const label_spec lspec = getLabelSpec(QStringLiteral("LineEdit"));
          vOffset = (lspec.bottom-lspec.top)/2;
          if (vOffset != 0
              && option->rect.height() < sizeCalculated(widget ? widget->font() : painter->font(),
                                                        getFrameSpec(QStringLiteral("LineEdit")),
                                                        lspec,
                                                        getSizeSpec(QStringLiteral("LineEdit")),
                                                        QStringLiteral("W"),QSize()).height())
          {
            vOffset = 0;
          }
        }

        // -> CC_SpinBox
        if (opt)
        {
          if (up)
          {
            int m = opt->rect.width() - tspec_.spin_button_width;
            if (fspec.right > m)
            {
              m = qMax(m,2);
              fspec.right = qMin(fspec.right,m);
              if (hasExpandedBorder(fspec))
                fspec.expansion = 0;
              else
                fspec.expansion = qMin(fspec.expansion, LIMITED_EXPANSION);
            }
          }
          else if (w < tspec_.spin_button_width)
          {
            if (hasExpandedBorder(fspec))
              fspec.expansion = 0;
            else
              fspec.expansion = qMin(fspec.expansion, LIMITED_EXPANSION);
          }
          if (const QAbstractSpinBox *sb = qobject_cast<const QAbstractSpinBox*>(widget))
          {
            if (spinTextless(sb))
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
      }
      else
      {
        fspec = getFrameSpec(QStringLiteral("LineEdit"));
        fspec.left = qMin(fspec.left,3);
        fspec.right = qMin(fspec.right,3);
        fspec.top = qMin(fspec.top,3);
        fspec.bottom = qMin(fspec.bottom,3);

        if (hasExpandedBorder(fspec))
          fspec.expansion = 0;
        else
          fspec.expansion = qMin(fspec.expansion, LIMITED_EXPANSION);
      }

      /*if (isLibreoffice_)
      {
        fspec.left = qMin(fspec.left,3);
        fspec.right = qMin(fspec.right,3);
        fspec.top = qMin(fspec.top,3);
        fspec.bottom = qMin(fspec.bottom,3);
        fspec.expansion = 0;
      } */

      QString iStatus = getState(option,widget); // indicator state
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
          quint32 sc = QStyle::SC_SpinBoxUp;
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
        else if (bStatus.startsWith(QLatin1String("normal")))
          iStatus = "normal";

        if (isWidgetInactive(widget))
        {
          if (!iStatus.endsWith(QLatin1String("-inactive")))
            iStatus.append("-inactive");
          if (!bStatus.endsWith(QLatin1String("-inactive")))
            bStatus.append("-inactive");
        }
      }

      /* a workaround for LibreOffice;
         also see subControlRect() -> CC_SpinBox */
      //if (isLibreoffice_)
      //{
        //bStatus = iStatus = "normal";
        /*if (up) iString = "-plus-";
        else iString = "-minus-";*/
      //}

      QString iString; // indicator string
      if (element == PE_IndicatorSpinPlus) iString = "-plus-";
      else if (element == PE_IndicatorSpinMinus) iString = "-minus-";
      else if (element == PE_IndicatorSpinUp) iString = "-up-";
      else  iString = "-down-";

      QRect r = option->rect;
      indicator_spec dspec = getIndicatorSpec(group);

      bool inlineOnDarkToolbar = false;
      if ((verticalIndicators || tspec_.inline_spin_indicators)
          && themeRndr_ && themeRndr_->isValid())
      {
        if ((!getFrameSpec(QStringLiteral("ToolbarLineEdit")).element.isEmpty()
             || !getInteriorSpec(QStringLiteral("ToolbarLineEdit")).element.isEmpty())
            && getStylableToolbarContainer(widget, true))
        {
          QColor tCol = getFromRGBA(getLabelSpec(QStringLiteral("Toolbar")).normalColor);
          QLineEdit *le = widget->findChild<QLineEdit*>();
          if (enoughContrast(standardPalette().color(QPalette::Active,QPalette::Text), tCol)
              && !enoughContrast(le->palette().color(QPalette::Active, QPalette::Text), tCol))
          {
            inlineOnDarkToolbar = true;
          }
        }
      }

      if (!verticalIndicators && (!tspec_.inline_spin_indicators || tspec_.inline_spin_separator))
      {
        if (bStatus.startsWith(QLatin1String("disabled")))
        {
          bStatus.replace(QLatin1String("disabled"),QLatin1String("normal"));
          painter->save();
          painter->setOpacity(DISABLED_OPACITY);
        }
        if (!verticalIndicators && !tspec_.inline_spin_indicators)
        {
          interior_spec ispec = getInteriorSpec(group);
          ispec.px = ispec.py = 0;
          renderFrame(painter,r,fspec,fspec.element+"-"+bStatus,0,0,0,0,0,true);
          renderInterior(painter,r,fspec,ispec,ispec.element+"-"+bStatus,true);
        }
        if (!inlineOnDarkToolbar
            && (element == PE_IndicatorSpinDown || element == PE_IndicatorSpinMinus))
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

      if (verticalIndicators)
      {
        fspec.left = 0;
        if (up) fspec.bottom = 0;
        else fspec.top = 0;
      }
      if ((verticalIndicators || tspec_.inline_spin_indicators)
          && themeRndr_ && themeRndr_->isValid())
      {
        QColor col = getFromRGBA(getLabelSpec(group).normalColor);
        if (!col.isValid())
          col = standardPalette().color(QPalette::ButtonText);
        if (enoughContrast(col, standardPalette().color(QPalette::Text))
            && flatArrowExists(dspec.element))
          dspec.element = "flat-"+dspec.element;

        if (inlineOnDarkToolbar)
        {
          if (dspec.element.startsWith(QLatin1String("flat-")))
            dspec.element.remove(0, 5);
          else
            dspec.element = "flat-"+dspec.element;
        }
      }
      renderIndicator(painter,
                      r,
                      fspec,dspec,
                      dspec.element+iString+iStatus,
                      option->direction,
                      Qt::AlignCenter, // horizontally center both indicators
                      vOffset);

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
          fspec.left += lspec.left;
        }
        else
        {
          fspec.left = 0;
          fspec.right += lspec.right;
        }

        QString aStatus = getState(option,widget);
        /* distinguish between the toggled and pressed states
           only if a toggled down arrow element exists */
        if (aStatus.startsWith(QLatin1String("toggled")))
        {
          if (!elementExists(dspec.element+"-down-toggled"))
            aStatus.replace(QLatin1String("toggled"),QLatin1String("pressed"));
        }
        else if (aStatus.startsWith(QLatin1String("focused"))
                 && option->styleObject != nullptr
                 && option->styleObject->property("_kv_hover_bug").toBool()) // hover bug
        {
          aStatus.replace(QLatin1String("focused"),QLatin1String("normal"));
        }
        if (opt->sortIndicator == QStyleOptionHeader::SortDown)
          renderIndicator(painter,option->rect,fspec,dspec,dspec.element+"-down-"+aStatus,option->direction);
        else if (opt->sortIndicator == QStyleOptionHeader::SortUp)
          renderIndicator(painter,option->rect,fspec,dspec,dspec.element+"-up-"+aStatus,option->direction);
      }

      break;
    }

    case PE_IndicatorButtonDropDown : {
      QRect r = option->rect;
      interior_spec ispec;
      int vOffset = 0;
      QString group = "DropDownButton";
      bool fillWidgetInterior(false);

      const QToolButton *tb = qobject_cast<const QToolButton*>(widget);
      QWidget *p = getParent(widget,1);
      QWidget *stb = nullptr;
      bool autoraise = false;
      if (tb)
      {
        autoraise = tb->autoRaise();
        stb = getStylableToolbarContainer(widget);
        if (stb)
        {
          autoraise = true;
          if (!getFrameSpec(QStringLiteral("ToolbarButton")).element.isEmpty()
              || !getInteriorSpec(QStringLiteral("ToolbarButton")).element.isEmpty())
          {
            group = "ToolbarButton";
          }
          ispec = getInteriorSpec(group);
          if (!ispec.hasInterior
              && enoughContrast(getFromRGBA(getLabelSpec(group).normalColor),
                                getFromRGBA(getLabelSpec(QStringLiteral("Toolbar")).normalColor)))
          { // high contrast on toolbar
            fillWidgetInterior = true;
          }
        }
        else
        {
          ispec = getInteriorSpec(group);
          if (p && !ispec.hasInterior)
          {
            QString containerGroup;
            if (qobject_cast<QMenuBar*>(p))
            {
              if (mergedToolbarHeight(p))
                containerGroup = "Toolbar";
              else
                containerGroup = "MenuBar";
            }
            else if (QMenuBar *gp = qobject_cast<QMenuBar*>(getParent(p,1)))
            {
              if (mergedToolbarHeight(gp))
                containerGroup = "Toolbar";
              else
                containerGroup = "MenuBar";
            }
            if (!containerGroup.isEmpty()
                && enoughContrast(getFromRGBA(getLabelSpec(group).normalColor),
                                  getFromRGBA(getLabelSpec(containerGroup).normalColor)))
            { // high contrast on menubar
              fillWidgetInterior = true;
            }
          }
        }
        const label_spec lspec = getLabelSpec(QStringLiteral("PanelButtonTool"));
        vOffset = (lspec.bottom-lspec.top)/2;
      }
      else
        ispec = getInteriorSpec(group);

      frame_spec fspec = getFrameSpec(group);
      fspec.expansion = 0; // depends on the containing widget
      indicator_spec dspec = getIndicatorSpec(group);
      if (group == "ToolbarButton")
        dspec.element += "-down";

      QString status = getState(option,widget);
      bool rtl(option->direction == Qt::RightToLeft);

      const QStyleOptionComboBox *combo =
            qstyleoption_cast<const QStyleOptionComboBox*>(option);
      const QComboBox *cb = qobject_cast<const QComboBox*>(widget);

      QString cGroup;
      if (combo)
      {
        if ((!getFrameSpec(QStringLiteral("ToolbarComboBox")).element.isEmpty()
             || !getInteriorSpec(QStringLiteral("ToolbarComboBox")).element.isEmpty())
            && getStylableToolbarContainer(widget, true))
        {
          cGroup = "ToolbarComboBox";
        }
        else cGroup = "ComboBox";

        if (tspec_.combo_as_lineedit && combo->editable)
        {
          // -> drawComplexControl() -> CC_ComboBox
          QLineEdit *lineEditWidget = nullptr;
          if (cb != nullptr)
            lineEditWidget = cb->lineEdit();
          else if (widget)
          {
            lineEditWidget = widget->findChild<QLineEdit*>(QString(), Qt::FindDirectChildrenOnly);
            if (lineEditWidget != nullptr && !widget->isAncestorOf(lineEditWidget))
              lineEditWidget = nullptr;
          }
          if ((!getFrameSpec(QStringLiteral("ToolbarLineEdit")).element.isEmpty()
               || !getInteriorSpec(QStringLiteral("ToolbarLineEdit")).element.isEmpty())
              && getStylableToolbarContainer(lineEditWidget, true)
              && !enoughContrast(lineEditWidget->palette().color(QPalette::Active, QPalette::Text),
                                 getFromRGBA(getLabelSpec(QStringLiteral("Toolbar")).normalColor)))
          {
            fspec = getFrameSpec(QStringLiteral("ToolbarLineEdit"));
            ispec = getInteriorSpec(QStringLiteral("ToolbarLineEdit"));
            indicator_spec dspec1 = getIndicatorSpec(QStringLiteral("ToolbarLineEdit"));
            if (elementExists(dspec1.element+"-normal"))
              dspec = dspec1;
            else
            { // fall back to LineEdit for backward compatibility
              dspec1 = getIndicatorSpec(QStringLiteral("LineEdit"));
              if (elementExists(dspec1.element+"-normal"))
                dspec = dspec1;
            }
            if (enoughContrast(standardPalette().color(QPalette::Active,QPalette::Text),
                               getFromRGBA(getLabelSpec(QStringLiteral("Toolbar")).normalColor)))
            {
              dspec.element = "flat-"+dspec.element;
            }
          }
          else
          {
            fspec = getFrameSpec(QStringLiteral("LineEdit"));
            ispec = getInteriorSpec(QStringLiteral("LineEdit"));
            const indicator_spec dspec1 = getIndicatorSpec(QStringLiteral("LineEdit"));
            if (elementExists(dspec1.element+"-normal"))
              dspec = dspec1;
          }
          const label_spec lspec = getLabelSpec(QStringLiteral("LineEdit"));
          vOffset = (lspec.bottom-lspec.top)/2;
        }
        else
        {
          fspec = getFrameSpec(cGroup);
          ispec = getInteriorSpec(cGroup);
          const indicator_spec dspec1 = getIndicatorSpec(cGroup);
          if (themeRndr_ && themeRndr_->isValid()
              && themeRndr_->elementExists(dspec1.element+"-normal"))
          {
            dspec = dspec1;
          }
          const label_spec lspec = getLabelSpec(QStringLiteral("ComboBox"));
          vOffset = (lspec.bottom-lspec.top)/2;
        }

        if (!(combo->editable
              // someone may want transparent lineedits (as the developer of Cantata does)
              && cb && cb->lineEdit()
              && cb->lineEdit()->palette().color(cb->lineEdit()->backgroundRole()).alpha() == 0))
        {
          fspec.isAttached = true;
          if (rtl)
            fspec.HPos = -1;
          else
            fspec.HPos = 1;
        }

        status = (option->state & State_Enabled) ?
                  (option->state & State_On) ? "toggled" :
                  ((option->state & State_Sunken)
                   || (cb && cb->hasFocus())
                   || (widget && widget->hasFocus())) ? "pressed" :
                  (option->state & State_MouseOver)
                    && !(option->styleObject
                         && option->styleObject->property("_kv_hover_bug").toBool()) // hover bug
                  ? "focused" : "normal"
                : "disabled";
        if (isWidgetInactive(widget))
          status.append("-inactive");

        if (!combo->editable)
        {
          /* in this case, the state definition isn't the usual one */
          status = (option->state & State_Enabled) ?
                    (option->state & State_On) ? "toggled" :
                    (option->state & State_MouseOver)
                      && !(option->styleObject
                           && option->styleObject->property("_kv_hover_bug").toBool()) // hover bug
                    ? "focused" :
                    (option->state & State_Sunken)
                    || (option->state & State_Selected) ? "pressed" : "normal"
                   : "disabled";
          if (isWidgetInactive(widget))
            status.append("-inactive");
          /* when there isn't enough space */
          const label_spec lspec1 = getLabelSpec(QStringLiteral("ComboBox"));
          QFont F(painter->font());
          if (lspec1.boldFont) F.setWeight(lspec1.boldness);
          QSize txtSize = textSize(F,combo->currentText);
          if (/*cb->width() < fspec.left+lspec1.left+txtSize.width()+lspec1.right+COMBO_ARROW_LENGTH+fspec.right
              ||*/ cb && cb->height() < fspec.top+lspec1.top+txtSize.height()+fspec.bottom+lspec1.bottom)
          {
            if (rtl)
              r.adjust(0,0,-qMax(fspec.left-3,0),0);
            else
              r.adjust(qMax(fspec.right-3,0),0,0,0);
            fspec.left = qMin(fspec.left,3);
            fspec.right = qMin(fspec.right,3);
            fspec.top = qMin(fspec.top,3);
            fspec.bottom = qMin(fspec.bottom,3);
            vOffset = 0;
            if (rtl)
              r.adjust(0,0,-qMax(qMin(dspec.size,cb->height()-fspec.top-fspec.bottom)-9,0),0);
            else
              r.adjust(qMax(qMin(dspec.size,cb->height()-fspec.top-fspec.bottom)-9,0),0,0,0);
            dspec.size = qMin(dspec.size,9);
          }
        }
        else
        {
          if (vOffset != 0 && cb && cb->lineEdit())
          { // -> drawComboLineEdit()
            const label_spec lspec1 = getLabelSpec(QStringLiteral("LineEdit"));
            const size_spec sspec1 = getSizeSpec(QStringLiteral("LineEdit"));
            if (cb->lineEdit()->height()
                < sizeCalculated(cb->lineEdit()->font(),fspec,lspec1,sspec1,QStringLiteral("W"),QSize()).height())
            {
              vOffset = 0;
            }
          }

          if (tspec_.combo_as_lineedit)
          { // correct the state for an editabe combo that's drawn as lineedit
            if ((cb && cb->hasFocus()) || (widget && widget->hasFocus()))
            {
              if (isWidgetInactive(widget))
                status = "focused-inactive";
              else status = "focused";
            }
            else if (status.startsWith(QLatin1String("focused")))
              status.replace(QLatin1String("focused"),QLatin1String("normal"));
            else if (status.startsWith(QLatin1String("toggled")))
              status.replace(QLatin1String("toggled"),QLatin1String("normal"));
          }
        }
      }

      if (tb)
      {
        bool drawSep(false);
        if (QObject *styleObject = option->styleObject)
        { // hover bug
          if (status.startsWith(QLatin1String("focused")) && styleObject->property("_kv_hover_bug").toBool())
            status.replace(QLatin1String("focused"),QLatin1String("normal"));
        }
        const QToolBar *toolBar = qobject_cast<const QToolBar*>(tb->parentWidget());
        const frame_spec fspec1 = getFrameSpec(QStringLiteral("PanelButtonTool"));
        fspec.top = fspec1.top; fspec.bottom = fspec1.bottom;
        bool drawRaised = false;
        if (tspec_.group_toolbar_buttons
            && toolBar && toolBar->orientation() != Qt::Vertical)
        {
          drawRaised = true;

          //const QStyleOptionToolButton *opt = qstyleoption_cast<const QStyleOptionToolButton*>(option);
          int kind = whichGroupedTBtn(tb, toolBar, drawSep);
          if (kind != tbAlone)
          {
            fspec.isAttached = true;
            fspec.HPos = kind;
          }
        }

        if (!fspec.isAttached)
        {
          fspec.isAttached = true;
          fspec.HPos = rtl ? -1 : 1;
        }
        else if (fspec.HPos == -1)
          fspec.HPos = 0;
        else if (fspec.HPos == 2)
          fspec.HPos = rtl ? -1 : 1;

        /* lack of space */
        const QStyleOptionToolButton *opt = qstyleoption_cast<const QStyleOptionToolButton*>(option);
        if (opt && opt->toolButtonStyle == Qt::ToolButtonIconOnly && !opt->icon.isNull()
            && tb->popupMode() == QToolButton::MenuButtonPopup)
        {
          if (tb->width() < opt->iconSize.width()+fspec1.left
                            +(rtl ? fspec.left : fspec.right)
                            +TOOL_BUTTON_ARROW_SIZE+2*BUTTON_ARROW_MARGIN)
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

        /* distinguish popup indicators from other button indicators */
        if (!status.startsWith(QLatin1String("normal")))
        {
          if (r.isValid()
              && (option->state & State_Enabled)
              && (option->state & State_MouseOver)
              && !status.startsWith(QLatin1String("pressed")))
          {
            QString group1 = group == "ToolbarButton" ? group : "PanelButtonTool";
            QColor col;
            if (status.startsWith(QLatin1String("toggled")))
            {
              if(status.endsWith(QLatin1String("-inactive")))
              {
                col = getFromRGBA(getLabelSpec(group1).toggleInactiveColor);
                if (!col.isValid())
                  col = getFromRGBA(getLabelSpec(group1).toggleColor);
              }
              else
                col = getFromRGBA(getLabelSpec(group1).toggleColor);
            }
            else
            {
              if(status.endsWith(QLatin1String("-inactive")))
              {
                col = getFromRGBA(getLabelSpec(group1).focusInactiveColor);
                if (!col.isValid())
                  col = getFromRGBA(getLabelSpec(group1).focusColor);
              }
              else
                col = getFromRGBA(getLabelSpec(group1).focusColor);
            }
            if (!col.isValid())
              col = standardPalette().color(status.endsWith(QLatin1String("-inactive"))
                                              ? QPalette::Inactive : QPalette::Active,
                                            QPalette::ButtonText);
            col.setAlphaF(0.4);
            /* see renderIndicator() */
            QRect interior = interiorRect(r,fspec);
            int s;
            if (!interior.isValid())
              s = qMin(r.width(), r.height());
            else
              s = qMin(interior.width(), interior.height());
            s = qMin(s, dspec.size);
            if (interior.height() - s >= vOffset)
              interior.adjust(0,-vOffset,0,-vOffset);
            QPoint center(rtl ? interior.right() + 1 : interior.left(),
                          (interior.top() + interior.bottom() + 1) / 2);
            painter->save();
            painter->setPen(col);
            painter->drawLine(center - QPoint(0, s/2 + 1), center + QPoint(0, s/2 + 1));
            painter->restore();
          }
        }
        /* use the "flat" indicator with flat buttons if it exists */
        else if (autoraise && !drawRaised && themeRndr_ && themeRndr_->isValid())
        {
          QString group1 = group == "ToolbarButton" ? group : "PanelButtonTool";
          const indicator_spec dspec1 = getIndicatorSpec(group1);
          if (flatArrowExists(dspec1.element))
          {
            QColor col = getFromRGBA(getLabelSpec(group1).normalColor);
            if (!col.isValid())
              col = standardPalette().color(QPalette::ButtonText);
            QWidget *gp = getParent(widget,2);
            QWidget* menubar = nullptr;
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
            else if (stb)
            {
              if (enoughContrast(col, getFromRGBA(getLabelSpec(QStringLiteral("Toolbar")).normalColor)))
                dspec.element = "flat-"+dspec1.element+"-down";
            }
            else if (p && enoughContrast(col, p->palette().color(p->foregroundRole())))
              dspec.element = "flat-"+dspec1.element+"-down";
          }
        }
      }
      else if ((combo && combo->editable
                && (!tspec_.combo_as_lineedit // otherwise drawn at CC_ComboBox
                    || (isLibreoffice_ && widget == nullptr)))
               /*&& (!(option->state & State_AutoRaise)
                   || (!status.startsWith("normal") && (option->state & State_Enabled)))*/)
      {
        /* fillWidgetInterior wasn't checked for combos  */
        fillWidgetInterior = !ispec.hasInterior
                             && hasHighContrastWithContainer(widget, getFromRGBA(getLabelSpec(cGroup).normalColor));
        if (tspec_.square_combo_button // the rest of the combo is like a lineedit
            || (isLibreoffice_ && widget == nullptr))
        {
          status.replace(QLatin1String("focused"),QLatin1String("normal"));
        }
        if (!(option->state & State_Enabled))
        {
          status.replace(QLatin1String("disabled"),QLatin1String("normal"));
          painter->save();
          painter->setOpacity(DISABLED_OPACITY);
        }
        bool canAnimate(opacityTimer_ && cb && cb->isEnabled());
        bool mouseAnimation(canAnimate
                            && animatedWidget_ == widget
                            && opacityTimer_->isActive()
                            && (!status.startsWith(QLatin1String("normal"))
                                || animationStartState_.startsWith(QLatin1String("focused"))));
        bool animate(canAnimate
                     && (mouseAnimation
                         || (animatedWidgetOut_ == widget
                             && opacityTimerOut_->isActive()
                             && status.startsWith(QLatin1String("normal")))));
        QString animationStartState(animationStartState_);
        if (animationStartState.startsWith(QLatin1String("c-")))
          animationStartState.remove(0, 2);
        int animationOpacity = animationOpacity_;
        bool animatePanel(!(tspec_.combo_focus_rect
                            && (status.startsWith(QLatin1String("normal"))
                                || status.startsWith(QLatin1String("pressed")))
                            && (animationStartState.startsWith(QLatin1String("normal"))
                                || animationStartState.startsWith(QLatin1String("pressed")))));
        QString _status;
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
          else if (animationOpacity < 100 && animatePanel)
          {
            _status = animationStartState;
            if (tspec_.combo_focus_rect)
              _status.replace(QLatin1String("pressed"), QLatin1String("normal"));
            if (!fillWidgetInterior)
              renderInterior(painter,r,fspec,ispec,ispec.element+"-"+_status);
            renderFrame(painter,r,fspec,fspec.element+"-"+_status);
          }
          if (animatePanel)
          {
            painter->save();
            painter->setOpacity(static_cast<qreal>(animationOpacity)/100.0);
          }
        }
        _status = status;
        if (tspec_.combo_focus_rect)
          _status.replace(QLatin1String("pressed"), QLatin1String("normal"));
        if (!fillWidgetInterior)
          renderInterior(painter,r,fspec,ispec,ispec.element+"-"+_status);
        renderFrame(painter,r,fspec,fspec.element+"-"+_status);

        if (animate)
        {
          if (animatePanel)
            painter->restore();
          if (animationOpacity >= 100)
          {
            if (animatedWidget_ == widget)
            {
              animationStartState_ = status;
              // distinguish between toggled combo and toggled button
              if (animationStartState_.startsWith(QLatin1String("toggled")))
                animationStartState_ = "c-" + animationStartState_;
            }
            if (!mouseAnimation)
              animationStartStateOut_ = status;
          }
        }
        else if (canAnimate)
        {
          if (animatedWidget_ == widget)
          {
            animationStartState_ = status;
            if (animationStartState_.startsWith(QLatin1String("toggled")))
              animationStartState_ = "c-" + animationStartState_;
          }
          if (animatedWidgetOut_ == widget)
            animationStartStateOut_ = status;
        }
        if (fillWidgetInterior) // widget isn't null
        {
          QColor comboCol = widget->palette().color(status.contains(QLatin1String("-inactive"))
                                                      ? QPalette::Inactive
                                                      : QPalette::Active,
                                                    QPalette::Button);
          comboCol.setAlpha(255);
          painter->fillRect(interiorRect(r,fspec), comboCol);
        }

        /* draw combo separator if it exists */
        const QString sepName = getIndicatorSpec(cGroup).element + "-separator";
        QRect sep;
        if (rtl)
        {
          sep = QRect(x+r.width()-fspec.right, y+fspec.top, fspec.right, h-fspec.top-fspec.bottom);
          painter->save();
          QTransform m;
          m.translate(2*sep.x() + sep.width(), 0); m.scale(-1,1);
          painter->setTransform(m, true);
        }
        else
          sep = QRect(x, y+fspec.top, fspec.left, h-fspec.top-fspec.bottom);
        if (renderElement(painter, sepName+"-"+_status, sep))
        {
          sep.adjust(0, -fspec.top, 0, -h+fspec.top+fspec.bottom);
          renderElement(painter, sepName+"-top-"+_status, sep);
          sep.adjust(0, h-fspec.bottom, 0, h-fspec.top);
          renderElement(painter, sepName+"-bottom-"+_status, sep);
        }
        if (rtl)
          painter->restore();

        if (!(option->state & State_Enabled))
        {
          painter->restore();
          status = "disabled";
          if (isWidgetInactive(widget))
            status.append("-inactive");
        }
      }

      /* distinguish between the toggled and pressed states
         only if a toggled arrow element exists */
      if (status.startsWith(QLatin1String("toggled"))
          && !(themeRndr_ && themeRndr_->isValid()
               && themeRndr_->elementExists(dspec.element+"-toggled")))
      {
        status.replace(QLatin1String("toggled"),QLatin1String("pressed"));
      }
      /* Konqueror may have added an icon to the right of lineedit (for LTR),
         in which case, the arrow rectangle whould be widened at CC_ComboBox */
      if (combo && combo->editable && cb && cb->lineEdit())
      { // fspec is set above to that of line-edit when combo_as_lineedit is true
        int extra = r.width()
                    - (tspec_.square_combo_button
                         ? qMax(COMBO_ARROW_LENGTH, cb->height()-(rtl ? fspec.left : fspec.right))
                         : COMBO_ARROW_LENGTH)
                    - (rtl ? fspec.left : fspec.right);
        if (extra > 0)
        {
          if (rtl) r.adjust(0,0,-extra,0);
          else r.adjust(extra,0,0,0);
        }
      }
      renderIndicator(painter,
                      r,
                      fspec,dspec,dspec.element+"-"+status,
                      option->direction,
                      Qt::AlignCenter,
                      vOffset);

      break;
    }

    case PE_PanelMenuBar : {
      break;
    }

    case PE_IndicatorTabTearRight :
    case PE_IndicatorTabTearLeft :
    {
      indicator_spec dspec = getIndicatorSpec(QStringLiteral("Tab"));
      renderElement(painter,dspec.element+"-tear",option->rect);

      break;
    }

    case PE_IndicatorTabClose : {
      frame_spec fspec;
      default_frame_spec(fspec);
      const indicator_spec dspec = getIndicatorSpec(QStringLiteral("Tab"));

      bool pseudoState(false);
      QString status;
      if (!(option->state & State_Enabled))
        status = "disabled";
      else if ((option->state & State_Selected) && hasActiveIndicator_)
      {
        if (option->state & State_Sunken)
        {
          pseudoState = true;
          status = "toggledPressed";
        }
        else if (option->state & State_MouseOver)
        {
          pseudoState = true;
          status = "toggledFocused";
        }
        else
          status = "toggled";
      }
      else
      {
        status = option->state & State_Sunken ? "pressed" :
                   option->state & State_MouseOver ? "focused" : "normal";
      }
      if (isWidgetInactive(widget))
        status.append("-inactive");

      QObject *styleObject = option->styleObject;
      const QString animationStartState = styleObject != nullptr
                                            ? styleObject->property("_kv_state").toString()
                                            : QString();
      bool animate(opacityTimer_
                   && widget && widget->isEnabled() && animatedWidget_ == widget
                   && !animationStartState.isEmpty()
                   && qobject_cast<const QAbstractButton*>(widget));
      if (animate && animationStartState == status)
      {
        if (opacityTimer_->isActive())
          opacityTimer_->stop();
        animationOpacity_ = 0;
        animate = false;
      }

      if (animate)
      {
        if (!opacityTimer_->isActive())
        {
          animationOpacity_ = 0;
          opacityTimer_->start(ANIMATION_FRAME);
        }
        if (animationOpacity_ < 100)
        {
          renderIndicator(painter,option->rect,fspec,dspec,
                                  dspec.element+"-close-"+animationStartState,option->direction);
        }
        painter->save();
        painter->setOpacity(static_cast<qreal>(animationOpacity_)/100.0);
      }

      if (pseudoState)
      {
        if(!renderIndicator(painter,option->rect,fspec,dspec,dspec.element+"-close-"+status,option->direction))
        { // the "toggledX" pseudo-state doesn't exist
          status = "toggled" + (isWidgetInactive(widget) ? "-inactive" : QString());
          renderIndicator(painter,option->rect,fspec,dspec,dspec.element+"-close-"+status,option->direction);
        }
      }
      else
        renderIndicator(painter,option->rect,fspec,dspec,dspec.element+"-close-"+status,option->direction);

      if (animate)
      {
          painter->restore();
          if (animationOpacity_ >= 100)
            styleObject->setProperty("_kv_state", status);
      }
      else if (styleObject)
          styleObject->setProperty("_kv_state", status);

      break;
    }

    case PE_IndicatorArrowUp :
    case PE_IndicatorArrowDown :
    case PE_IndicatorArrowLeft :
    case PE_IndicatorArrowRight : {
      if (const QToolButton *tb = qobject_cast<const QToolButton*>(widget))
      {
        /* if this is a tool button, the richer function
           drawControl(CE_ToolButtonLabel,...) should be called instead */
        QStyleOptionToolButton o;
        o.initFrom(widget);
        o.rect = option->rect;
        o.toolButtonStyle = Qt::ToolButtonIconOnly;
        o.text = QString();
        o.icon = QIcon();
        o.features |= QStyleOptionToolButton::Arrow;
        if (element == PE_IndicatorArrowUp)
          o.arrowType = Qt::UpArrow;
        else if (element == PE_IndicatorArrowDown)
          o.arrowType = Qt::DownArrow;
        else if (element == PE_IndicatorArrowLeft)
          o.arrowType = Qt::LeftArrow;
        else
          o.arrowType = Qt::RightArrow;
        if (tb->isEnabled())
        {
          o.state = State_Enabled;
          if (tb->isChecked())
            o.state |= State_On;
          else if (tb->isDown())
            o.state |= State_Sunken;
          else if (tb->underMouse())
            o.state |= State_MouseOver;
        }
        else
          o.state = State_None;
        drawControl(CE_ToolButtonLabel,&o,painter,widget);
        break;
      }

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

      indicator_spec dspec = getIndicatorSpec(QStringLiteral("IndicatorArrow"));

      if (qstyleoption_cast<const QStyleOptionMenuItem*>(option))
      {
        /* menuitems may have their own right/left arrows */
        if (element == PE_IndicatorArrowLeft || element == PE_IndicatorArrowRight)
        {
          const indicator_spec dspec1 = getIndicatorSpec(QStringLiteral("MenuItem"));
          dspec.size = dspec1.size;
          /* the arrow rectangle is set at CE_MenuItem appropriately */
          if (renderElement(painter, (/*isLibreoffice_ && aStatus.startsWith("normal") ? dspec.element :*/ dspec1.element)
                                     + dir+aStatus,
                            option->rect))
          {
            break;
          }
        }
        /* -> CE_MenuScroller */
        if (themeRndr_ && themeRndr_->isValid()
            && enoughContrast(standardPalette().color(QPalette::WindowText),
                              getFromRGBA(getLabelSpec(QStringLiteral("MenuItem")).normalColor))
            && flatArrowExists(dspec.element))
        {
          dspec.element = "flat-"+dspec.element;
        }
      }
      /* only theoretically; toolbar/menubar arrows are drawn at SP_ToolBarHorizontalExtensionButton */
      else if (themeRndr_ && themeRndr_->isValid())
      {
        QColor col;
        if (isStylableToolbar(widget)
            || mergedToolbarHeight(widget) > 0
            || getStylableToolbarContainer(widget)) // like k3b path arrows
        {
          col = getFromRGBA(getLabelSpec(QStringLiteral("Toolbar")).normalColor);
        }
        else if (qobject_cast<const QMenuBar*>(widget))
          col = getFromRGBA(getLabelSpec(QStringLiteral("MenuBar")).normalColor);
        if (enoughContrast(col, standardPalette().color(QPalette::Active,QPalette::WindowText))
            && flatArrowExists(dspec.element))
        {
          dspec.element = "flat-"+dspec.element;
        }
      }

      if (isWidgetInactive(widget))
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

    case PE_PanelItemViewRow : {
      if (const QStyleOptionViewItem *opt = qstyleoption_cast<const QStyleOptionViewItem*>(option))
      {
        QPalette::ColorGroup cg = (widget ? widget->isEnabled() : (opt->state & QStyle::State_Enabled))
                                    ? QPalette::Normal
                                    : QPalette::Disabled;
        if (cg == QPalette::Normal && isWidgetInactive(widget))
          cg = QPalette::Inactive; // Qt checks QStyle::State_Active, which isn't consistent with inactive base color

        if ((opt->state & QStyle::State_Selected) && styleHint(QStyle::SH_ItemView_ShowDecorationSelected, opt, widget))
        {
          /* we never do this: */
          //painter->fillRect(opt->rect, opt->palette.brush(cg, QPalette::Highlight));
          break;
        }
        else if (opt->features & QStyleOptionViewItem::Alternate)
        {
          if ((isDolphin_ && hspec_.transparent_dolphin_view)
              || (isPcmanfm_ && hspec_.transparent_pcmanfm_view))
          {
            if (const auto *iv = qobject_cast<const QAbstractItemView*>(widget))
            {
              if (iv->viewport() && !iv->viewport()->autoFillBackground())
                break;
            }
          }
          painter->fillRect(opt->rect, opt->palette.brush(cg, QPalette::AlternateBase));
        }
      }
      break;
    }

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
                         (option->state & State_MouseOver) ? "focused"
                         : "normal" : "disabled";
      if (ivStatus == "disabled" && (option->state & State_Selected))
      {
        /* Disabled items aren't selectable but Qt Creator wrongly disables
           the headers of a cmake project and let the user select them! */
        ivStatus = "pressed";
      }
      else if (ivStatus == "focused" && isCursorOutsideWidget(widget)) // hover bug
      {
        ivStatus = "normal";
      }
      bool isInactive(isWidgetInactive(widget));

      const QStyleOptionViewItem *opt = qstyleoption_cast<const QStyleOptionViewItem*>(option);
      const QAbstractItemView *iv = qobject_cast<const QAbstractItemView*>(widget);
      if (opt)
      {
        QModelIndex indx = opt->index;
        if (indx.isValid())
        {
          QModelIndex siblingIndx;
          switch (opt->viewItemPosition) {
            case QStyleOptionViewItem::OnlyOne:
            case QStyleOptionViewItem::Invalid: break;
            case QStyleOptionViewItem::Beginning: {
              if (indx.sibling(indx.row(), indx.column()+1).isValid()) // not a vertical itemview
              {
                if (opt->direction == Qt::RightToLeft)
                {
                  /* NOTE: Unfortunately, some apps have wrong item positions and some have wrong
                           visual rectangles for indexes. Conditions like this cover all cases. */
                  if (!iv || ((siblingIndx = iv->indexAt(option->rect.topLeft() - QPoint(2,0))).isValid()
                              && siblingIndx.column() != indx.column() && siblingIndx.row() == indx.row()))
                  {
                    fspec.isAttached = true;
                    fspec.HPos = 1;
                  }
                }
                else if (!iv || ((siblingIndx = iv->indexAt(option->rect.topRight() + QPoint(2,0))).isValid()
                                 && siblingIndx.column() != indx.column() && siblingIndx.row() == indx.row()))
                {
                  fspec.isAttached = true;
                  fspec.HPos = -1;
                }
              }
              break;
            }
            case QStyleOptionViewItem::End: {
              if (indx.sibling(indx.row(), indx.column()-1).isValid())
              {
                if (opt->direction == Qt::RightToLeft)
                {
                  if (!iv || ((siblingIndx = iv->indexAt(option->rect.topRight() + QPoint(2,0))).isValid()
                              && siblingIndx.column() != indx.column() && siblingIndx.row() == indx.row()))
                  {
                    fspec.isAttached = true;
                    fspec.HPos = -1;
                  }
                }
                else if (!iv || ((siblingIndx = iv->indexAt(option->rect.topLeft() - QPoint(2,0))).isValid()
                                 && siblingIndx.column() != indx.column() && siblingIndx.row() == indx.row()))
                {
                  fspec.isAttached = true;
                  fspec.HPos = 1;
                }
              }
              break;
            }
            case QStyleOptionViewItem::Middle: {
              if (indx.sibling(indx.row(), indx.column()+1).isValid()) // there is an item after this
              {
                if (opt->direction == Qt::RightToLeft)
                {
                  if (!iv || ((siblingIndx = iv->indexAt(option->rect.topLeft() - QPoint(2,0))).isValid()
                              && siblingIndx.column() != indx.column() && siblingIndx.row() == indx.row()))
                  {
                    fspec.isAttached = true;
                    fspec.HPos = 1;
                  }
                }
                else if (!iv || ((siblingIndx = iv->indexAt(option->rect.topRight() + QPoint(2,0))).isValid()
                                 && siblingIndx.column() != indx.column() && siblingIndx.row() == indx.row()))
                { // there is an item on the right
                  fspec.isAttached = true;
                  fspec.HPos = -1;
                }
              }
              if (indx.sibling(indx.row(), indx.column()-1).isValid()) // there is an item before this
              {
                if (opt->direction == Qt::RightToLeft)
                {
                  if (!iv || ((siblingIndx = iv->indexAt(option->rect.topRight() + QPoint(2,0))).isValid()
                              && siblingIndx.column() != indx.column() && siblingIndx.row() == indx.row()))
                  {
                    if (fspec.isAttached)
                      fspec.HPos = 0;
                    else
                    {
                      fspec.isAttached = true;
                      fspec.HPos = -1;
                    }
                  }
                }
                else if (!iv || ((siblingIndx = iv->indexAt(option->rect.topLeft() - QPoint(2,0))).isValid()
                                 && siblingIndx.column() != indx.column() && siblingIndx.row() == indx.row()))
                { // there is an item on the left
                  if (fspec.isAttached)
                    fspec.HPos = 0;
                  else
                  {
                    fspec.isAttached = true;
                    fspec.HPos = 1;
                  }
                }
              }
              break;
            }
            default: break;
          }
        }
        if (opt->backgroundBrush.style() != Qt::NoBrush
            && ivStatus != "pressed" && ivStatus != "toggled")
        {
          /* In this case, the item is colored intentionally.
             However, since QCommonStyle excludes the selected states, we did so too. */
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
          painter->fillRect(opt->rect, brush);
          painter->setBrushOrigin(oldBO);

          if (ivStatus == "focused")
          {
            if (isWidgetInactive(widget))
              ivStatus.append("-inactive");
            fspec.expansion = 0;
            renderFrame(painter,option->rect,fspec,fspec.element+"-"+ivStatus,0,0,0,0,0,fspec.isAttached,true);
          }
          break;
        }
        else if (indx.isValid() && !(indx.flags() & Qt::ItemIsEditable)
                 && iv && !iv->isPersistentEditorOpen(indx) // Qt is inconsistent here
                 && (option->state & State_Enabled))
        {
          /* force colors when text isn't drawn at CE_ItemViewItem (as in VLC) */
          if (QWidget *iw = iv->indexWidget(indx))
          {
            const label_spec lspec = getLabelSpec(group);
            QColor col;
            if (ivStatus == "normal")
            {
              QColor tmpCol;
              if (isInactive)
              {
                tmpCol = getFromRGBA(lspec.normalInactiveColor);
                if (!tmpCol.isValid())
                  tmpCol = getFromRGBA(lspec.normalColor);
              }
              else
                tmpCol = getFromRGBA(lspec.normalColor);
              QColor baseCol = opt->palette.color(QPalette::Base);
              if (baseCol.alpha() == 0)
                baseCol = standardPalette().color(QPalette::Base);
              if (enoughContrast(baseCol, tmpCol))
                col = tmpCol;
            }
            else if (ivStatus == "focused")
            {
              QColor tmpCol;
              if (isInactive)
              {
                tmpCol = getFromRGBA(lspec.focusInactiveColor);
                if (!tmpCol.isValid())
                  tmpCol = getFromRGBA(lspec.focusColor);
              }
              else
                tmpCol = getFromRGBA(lspec.focusColor);
              QColor baseCol = opt->palette.color(QPalette::Base);
              if (baseCol.alpha() == 0)
                baseCol = standardPalette().color(QPalette::Base);
              if (enoughContrast(standardPalette().color(QPalette::Text), tmpCol)
                  // supposing that the focus interior is translucent, take care of contrast
                  || enoughContrast(baseCol, tmpCol))
              {
                col = tmpCol;
              }
            }
            else if (ivStatus == "pressed")
            {
              if (isInactive)
              {
                col = getFromRGBA(lspec.pressInactiveColor);
                if (!col.isValid())
                  col = getFromRGBA(lspec.pressColor);
              }
              else
                col = getFromRGBA(lspec.pressColor);
            }
            else if (ivStatus == "toggled")
            {
              if (isInactive)
              {
                col = getFromRGBA(lspec.toggleInactiveColor);
                if (!col.isValid())
                  col = getFromRGBA(lspec.toggleColor);
              }
              else
                col = getFromRGBA(lspec.toggleColor);
            }
            if (!col.isValid())
            {
              col = standardPalette().color(isInactive ? QPalette::Inactive : QPalette::Active,
                                            QPalette::Text);
            }
            if (col.isValid())
            {
              QPalette palette = iw->palette();
              palette.setColor(QPalette::Active,QPalette::Text,col);
              palette.setColor(QPalette::Inactive,QPalette::Text,col);
              forcePalette(iw, palette);
            }
          }
        }
      }

      if (ivStatus == "normal" || ivStatus == "disabled")
        break; // for the sake of consistency, we don't draw any background here

      if (isInactive)
        ivStatus.append("-inactive");

      /* this is needed for elegance */
      if (option->rect.height() < 2)
        fspec.expansion = 0;
      else
        fspec.expansion = qMin(fspec.expansion,option->rect.height()/2);
      /* since Dolphin's view-items have problem with QSvgRenderer, we set usePixmap to true */
      renderFrame(painter,option->rect,fspec,fspec.element+"-"+ivStatus,0,0,0,0,0,fspec.isAttached,true);
      renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+ivStatus,fspec.isAttached,true);

      break;
    }

    case PE_PanelTipLabel : {
      const QString group = "ToolTip";

      frame_spec fspec = getFrameSpec(group);
      fspec.expansion = 0;
      const interior_spec ispec = getInteriorSpec(group);
      fspec.left = fspec.right = fspec.top = fspec.bottom = pixelMetric(PM_ToolTipLabelFrameWidth,option,widget);

      theme_spec tspec_now = settings_->getCompositeSpec();
      if (!tspec_.shadowless_popup && !noComposite_ && tspec_now.tooltip_shadow_depth > 0
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

Style::KvIconMode Style::getIconMode(int state, bool isInactive, const label_spec &lspec) const
{
  if (state == -1) // a special case used by disabled toggled buttons
    return DisabledSelected;
  KvIconMode icnMode = state == 0 ? Disabled : Normal;
  QColor txtCol;
  if (state == 1 || state == 0)
  {
    if (isInactive)
      txtCol = getFromRGBA(lspec.normalInactiveColor);
    if (!txtCol.isValid())
      txtCol = getFromRGBA(lspec.normalColor);
  }
  else if (state == 2)
  {
    if (isInactive)
      txtCol = getFromRGBA(lspec.focusInactiveColor);
    if (!txtCol.isValid())
      txtCol = getFromRGBA(lspec.focusColor);
  }
  else if (state == 3)
  {
    if (isInactive)
      txtCol = getFromRGBA(lspec.pressInactiveColor);
    if (!txtCol.isValid())
      txtCol = getFromRGBA(lspec.pressColor);
  }
  else if (state == 4)
  {
    if (isInactive)
      txtCol = getFromRGBA(lspec.toggleInactiveColor);
    if (!txtCol.isValid())
      txtCol = getFromRGBA(lspec.toggleColor);
  }

  if (txtCol.isValid() // because the lack of contrast should be checked
      && !enoughContrast(txtCol, standardPalette().color(QPalette::Window)))
  {
    icnMode = state == 0 ? DisabledSelected : Selected;
  }

  return icnMode;
}

/* NOTE: This is a workaround for a regression in Qt 5.15.0, which results
         in QStyleOptionTab reporting an incorrect tab position when there
         are two tabs and a new active tab is created after the first one. */
static inline QStyleOptionTab::TabPosition tabPosition(const QStyleOptionTab *opt, const QWidget *widget)
{
  const QTabBar *tb = qobject_cast<const QTabBar*>(widget);
  if (tb == nullptr
      /* making an exception in the case of a dragged tab */
      || (opt->position == QStyleOptionTab::OnlyOneTab
          && opt->selectedPosition == QStyleOptionTab::NotAdjacent))
  {
    return opt->position;
  }

  if(!opt->rect.isValid())
    return QStyleOptionTab::OnlyOneTab;

  QPoint c = opt->rect.center();
  bool start(true);
  bool end(true);

  for (int i = 0; i < tb->count(); ++i)
  {
    if (tb->isTabVisible(i))
    {
      if (!tb->tabRect(i).contains(c))
        start = false;
      break;
    }
  }
  for (int i = tb->count() - 1; i >= 0; --i)
  {
    if (tb->isTabVisible(i))
    {
      if (!tb->tabRect(i).contains(c))
        end = false;
      break;
    }
  }

  if (start)
  {
    if (end)
      return QStyleOptionTab::OnlyOneTab;
    return QStyleOptionTab::Beginning;
  }
  if (end)
    return QStyleOptionTab::End;
  return QStyleOptionTab::Middle;
}

static inline void fitToolButtonText(const QStyleOptionToolButton *opt, QString &txt,
                                     const QSize &txtSize, const QFontMetrics &fm,
                                     const Qt::ToolButtonStyle tialign,
                                     frame_spec &fspec, label_spec &lspec,
                                     const int indicatorSize,
                                     bool hasPopupArrow,
                                     bool isLXQtPanel)
{
  QSize availableSize = lspec.left > 0 && lspec.right > 0 && hasPopupArrow
                          ? QSize(2, 0) // we removed 2px in CT_ToolButton
                          : QSize(0, 0);

  if (tialign == Qt::ToolButtonTextBesideIcon || tialign == Qt::ToolButtonTextUnderIcon)
  {
    availableSize += opt->rect.size()
                     - (tialign == Qt::ToolButtonTextUnderIcon
                          ? QSize(0, opt->iconSize.height())
                          : QSize(opt->iconSize.width(), 0))
                     - QSize(fspec.left+fspec.right+lspec.left+lspec.right,
                             fspec.top+fspec.bottom+lspec.top+lspec.bottom);
    if (tialign == Qt::ToolButtonTextBesideIcon)
      availableSize -= QSize(lspec.tispace, 0);
    else
      availableSize -= QSize(0, lspec.tispace);
    if ((opt->features & QStyleOptionToolButton::Arrow)
        && opt->arrowType != Qt::NoArrow)
    {
      availableSize -= QSize(indicatorSize+lspec.tispace+INDICATOR_MARGIN, 0);
    }

    if (txtSize.height() > availableSize.height())
    {
      lspec.top = lspec.bottom = qMin(lspec.top,2);
      fspec.top = qMin(fspec.top,3);
      fspec.bottom = qMin(fspec.bottom,3);
      lspec.boldFont = false;
      if (tialign == Qt::ToolButtonTextUnderIcon)
        lspec.tispace = qMin(lspec.tispace,2);
    }
    if (txtSize.width() > availableSize.width())
    {
      lspec.boldFont = false;
      if (tialign == Qt::ToolButtonTextUnderIcon)
      {
        lspec.left = lspec.right = qMin(lspec.left,2);
        fspec.left = qMin(fspec.left,3);
        fspec.right = qMin(fspec.right,3);
      }
      else
      {
        int availableWidth = availableSize.width();
        if (!isLXQtPanel) // not a task button
        {
          lspec.left = lspec.right = qMin(lspec.left,2);
          lspec.tispace = qMin(lspec.tispace,2);
          fspec.left = qMin(fspec.left,3);
          fspec.right = qMin(fspec.right,3);
          availableWidth = opt->rect.width() - opt->iconSize.width()
                           - fspec.left-fspec.right
                           - lspec.left-lspec.right-lspec.tispace;
          if ((opt->features & QStyleOptionToolButton::Arrow)
              && opt->arrowType != Qt::NoArrow)
          {
            availableWidth -= (indicatorSize + lspec.tispace + INDICATOR_MARGIN);
          }
        }
        /* if the text is beside the icon but doesn't fit in, elide it */
        txt = makeTextElided(fm, txt, availableWidth);
      }
    }
  }
  else if (tialign == Qt::ToolButtonTextOnly)
  {
    availableSize += opt->rect.size()
                     - QSize(fspec.left+fspec.right+lspec.left+lspec.right,
                             fspec.top+fspec.bottom+lspec.top+lspec.bottom);

    if (txtSize.height() > availableSize.height())
    {
      lspec.boldFont = false;
      lspec.top = lspec.bottom = qMin(lspec.top,2);
      fspec.top = qMin(fspec.top,3);
      fspec.bottom = qMin(fspec.bottom,3);
    }
    if (txtSize.width() > availableSize.width())
    {
      lspec.boldFont = false;
      int availableWidth = availableSize.width();
      if (!isLXQtPanel) // not a task button
      {
        lspec.left = lspec.right = qMin(lspec.left,2);
        lspec.tispace = qMin(lspec.tispace,2);
        fspec.left = qMin(fspec.left,3);
        fspec.right = qMin(fspec.right,3);
        availableWidth = opt->rect.width()
                         - fspec.left-fspec.right
                         - lspec.left-lspec.right;
      }
      /* again, elide the text if it doesn't fit in */
      txt = makeTextElided(fm, txt, availableWidth);
    }
  }
}

void Style::drawControl(QStyle::ControlElement element,
                        const QStyleOption *option,
                        QPainter *painter,
                        const QWidget *widget) const
{
  int x,y,h,w;
  option->rect.getRect(&x,&y,&w,&h);

  const QIcon::State iconstate =
      (option->state & State_On) ? QIcon::On : QIcon::Off;

  switch (static_cast<unsigned>(element)) { // unsigned because of CE_Kv_KCapacityBar
    case CE_MenuTearoff : {
      QString status = (option->state & State_Selected) ? "focused" : "normal";
      /* see PM_MenuTearoffHeight and also PE_PanelMenu
         (PM_MenuHMargin is already taken into account in option->rect) */
      QRect r(x, y+h-8, w, 8);
      const indicator_spec dspec = getIndicatorSpec(QStringLiteral("MenuItem"));
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
        label_spec lspec = getLabelSpec(group);

        const QComboBox *combo = qobject_cast<const QComboBox*>(widget);

        /* we should limit text-icon spacing for combo menu
           because we can't know whether it has icon */
        if (combo != nullptr)
          lspec.tispace = qMin(lspec.tispace, 6);

        if (opt->menuItemType == QStyleOptionMenuItem::Separator)
          renderElement(painter,dspec.element+"-separator",option->rect);
        //else if (opt->menuItemType == QStyleOptionMenuItem::TearOff)
          //renderElement(painter,dspec.element+"-tearoff",option->rect,20,0);
        else
        {
          /* don't draw panel for normal and disabled states */
          if (!status.startsWith(QLatin1String("normal")) && (option->state & State_Enabled))
          {
            if (isLibreoffice_ && widget == nullptr)
            {
              painter->save();
              painter->setOpacity(0.6);
              painter->fillRect(option->rect, standardPalette().brush(QPalette::Highlight));
              painter->restore();
              lspec.pressColor = lspec.toggleColor
                               = getName(standardPalette().color(QPalette::HighlightedText));
            }
            else
            {
              renderFrame(painter,option->rect,fspec,fspec.element+"-"+status);
              renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+status);
            }
          }

          QString txt = opt->text;
          txt.replace('\n', ' '); // may be needed with combos
          QString shortcutTxt;
          if (combo == nullptr)
          {
            int tabIndex = txt.indexOf(QLatin1Char('\t'));
            if (tabIndex > 0)
            {
              shortcutTxt = txt.mid(tabIndex + 1);
              txt = txt.left(tabIndex);
            }
          }

          int smallIconSize = pixelMetric(PM_SmallIconSize);
          int talign = Qt::AlignVCenter | Qt::TextSingleLine;
          if (!styleHint(SH_UnderlineShortcut, opt, widget))
            talign |= Qt::TextHideMnemonic;
          else
            talign |= Qt::TextShowMnemonic;

          int state = 1;
          if (!(option->state & State_Enabled))
            state = 0;
          else if (status.startsWith(QLatin1String("pressed")))
          { // only if it exists (focus color seems more natural)
            if (lspec.pressColor.isEmpty())
              state = 2;
            else
              state = 3;
          }
          else if (status.startsWith(QLatin1String("toggled")))
          { // only if it exists (focus color seems more natural)
            if (lspec.toggleColor.isEmpty())
              state = 2;
            else
              state = 4;
          }

          /* some apps (like Qt Creator) may force a bad text color */
          if (state == 1 || state == 0)
          {
            if (widget == nullptr // QML; see PE_PanelMenu
                || lspec.normalColor.isEmpty())
            {
              lspec.normalColor = cspec_.windowTextColor;
              lspec.normalInactiveColor = cspec_.inactiveWindowTextColor;
            }
          }
          else if (state == 2)
          {
            if (lspec.normalColor.isEmpty())
            {
              lspec.focusColor = cspec_.windowTextColor;
              lspec.focusInactiveColor = cspec_.inactiveWindowTextColor;
            }
          }

          bool rtl(option->direction == Qt::RightToLeft);
          bool hideCheckBoxes(tspec_.combo_menu //&& !isLibreoffice_
                              && tspec_.hide_combo_checkboxes
                              && combo != nullptr); // as in CT_MenuItem

          int iw = qMin(smallIconSize, pixelMetric(PM_IndicatorWidth,option,widget)); // qMin as a precaution
          int ih = qMin(smallIconSize, pixelMetric(PM_IndicatorHeight,option,widget));

          bool isInactive(status.contains(QLatin1String("-inactive")));

          /* label */
          painter->save();
          painter->setFont(opt->font); // some apps (like TeXstudio) use special fonts
          int checkSpace = 0;
          if (!hideCheckBoxes
              && ((widget && opt->menuHasCheckableItems)
                  || opt->checkType != QStyleOptionMenuItem::NotCheckable)) // as in CT_MenuItem
          {
            checkSpace = iw + pixelMetric(PM_CheckBoxLabelSpacing);
          }
          int iconSpace = 0;
          if (opt->icon.isNull() || hspec_.iconless_menu)
          {
            if (!txt.isEmpty())
            {
              if (((opt->maxIconWidth > 0
                    /* combobox always announces the existence of an icon,
                       so we don't care about aligning its menu texts */
                    && combo == nullptr)
                   || widget == nullptr) // QML menus set maxIconWidth to 0, although they have icon
                  && !hspec_.iconless_menu)
              {
                iconSpace = smallIconSize + lspec.tispace;
              }
              renderLabel(option,painter,
                          option->rect.adjusted(rtl ? 0 : iconSpace+checkSpace,
                                                0,
                                                rtl ? -iconSpace-checkSpace : 0,
                                                0),
                          fspec,lspec,
                          Qt::AlignLeft | talign,
                          txt,QPalette::Text,
                          state,
                          isInactive);
            }
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
            QSize iconSize = combo != nullptr ? combo->iconSize() : QSize(smallIconSize, smallIconSize);
            QPixmap px = getPixmapFromIcon(opt->icon, getIconMode(state,isInactive,lspec), iconstate, iconSize);
            if (px.isNull()) // with a non-null icon
              iconSpace = iconSize.width() + lspec.tispace;
            QRect r = option->rect.adjusted(rtl ? 0 : iconSpace+checkSpace,
                                            0,
                                            rtl ? -iconSpace-checkSpace : 0,
                                            0);
            if (txt.isEmpty()) // textless menuitem, as in Kdenlive's play button menu
              r = alignedRect(option->direction,Qt::AlignVCenter | Qt::AlignLeft,
                              iconSize,labelRect(r,fspec,lspec));
            renderLabel(option,painter,r,
                        fspec,lspec,
                        Qt::AlignLeft | talign,
                        txt,QPalette::Text,
                        state,
                        isInactive,
                        px,
                        iconSize);
          }
          painter->restore();

          /* shortcut, with a little paler color */
          if (!shortcutTxt.isEmpty())
          {
            if (state == 1)
            {
              QColor col;
              if (isInactive)
                col = getFromRGBA(lspec.normalInactiveColor);
              if (!col.isValid())
                col = getFromRGBA(lspec.normalColor);
              col.setAlphaF(col.alphaF() * 0.8);
              if (isInactive)
                lspec.normalInactiveColor = getName(col);
              else
                lspec.normalColor = getName(col);
            }
            else if (state == 2)
            {
              QColor col;
              if (isInactive)
                col = getFromRGBA(lspec.focusInactiveColor);
              if (!col.isValid())
                col = getFromRGBA(lspec.focusColor);
              col.setAlphaF(col.alphaF() * 0.8);
              lspec.focusColor = getName(col);
            }
            else if (state == 3)
            {
              QColor col;
              if (isInactive)
                col = getFromRGBA(lspec.pressInactiveColor);
              if (!col.isValid())
                col = getFromRGBA(lspec.pressColor);
              col.setAlphaF(col.alphaF() * 0.8);
              lspec.pressColor = getName(col);
            }
            else if (state == 4)
            {
              QColor col;
              if (isInactive)
                col = getFromRGBA(lspec.toggleInactiveColor);
              if (!col.isValid())
                col = getFromRGBA(lspec.toggleColor);
              col.setAlphaF(col.alphaF() * 0.8);
              lspec.toggleColor = getName(col);
            }
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
                        shortcutTxt,QPalette::Text,
                        state,
                        isInactive);
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
          if (opt->checkType != QStyleOptionMenuItem::NotCheckable && !hideCheckBoxes)
          {
            o.rect = alignedRect(option->direction,
                                 Qt::AlignLeft | Qt::AlignVCenter,
                                 QSize(iw,ih),
                                 isLibreoffice_ && widget == nullptr ? // LibreOffice's unstyled menus
                                   opt->rect.adjusted(qMax(-opt->rect.x(),0),0,0,0)
                                   : interiorRect(opt->rect,fspec).adjusted(rtl ? 0 : lspec.left,
                                                                            0,
                                                                            rtl ? -lspec.right : 0,
                                                                            0));
            if (opt->checkType == QStyleOptionMenuItem::Exclusive
                // combo menu checkboxes are exclusive
                || combo != nullptr)
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
      if (const QStyleOptionViewItem *opt = qstyleoption_cast<const QStyleOptionViewItem*>(option))
      {
        bool hasIcon(opt->features & QStyleOptionViewItem::HasDecoration
                     && !opt->decorationSize.isEmpty());
        QPalette palette(opt->palette);
        if ((!opt->text.isEmpty() || hasIcon)
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
          if (state == 0 && (option->state & State_Selected))
            state = 3; // see the workaround for Qt Creator in PE_PanelItemViewItem
          else if (state == 2
                   && isCursorOutsideWidget(widget)) // hover bug
          {
            state = 1;
          }
          if (state != 0)
          {
            const label_spec lspec = getLabelSpec(QStringLiteral("ItemView"));
            QColor normalColor = getFromRGBA(lspec.normalColor);
            QColor focusColor = getFromRGBA(lspec.focusColor);
            QColor pressColor = getFromRGBA(lspec.pressColor);
            QColor toggleColor = getFromRGBA(lspec.toggleColor);
            bool isInactive;
            QColor col;
            if (opt->backgroundBrush.style() != Qt::NoBrush && state <= 2) //-> PE_PanelItemViewItem
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
              normalColor = focusColor = /*pressColor = toggleColor =*/ col;
              isInactive = false;
            }
            else
              isInactive = isWidgetInactive(widget);
            if (state == 1)
            {
              QColor normalInactiveColor = getFromRGBA(lspec.normalInactiveColor);
              QColor baseCol = palette.color(QPalette::Base);
              if (baseCol.alpha() == 0)
                baseCol = standardPalette().color(QPalette::Base);
              if ((!isInactive || !normalInactiveColor.isValid())
                  && normalColor.isValid()
                  /* since we don't draw the normal interior,
                     a minimum amount of contrast is needed */
                  && (col.isValid() || enoughContrast(baseCol, normalColor)))
              {
                col = normalColor;
              }
              else if (isInactive
                       && normalInactiveColor.isValid()
                       && (col.isValid() || enoughContrast(baseCol, normalInactiveColor)))
              {
                col = normalInactiveColor;
              }
              else
                col = QColor();
              if (col.isValid())
              {
                QStyleOptionViewItem o(*opt);
                palette.setColor(QPalette::Text, col);
                o.palette = palette;
                /* getting the pixmap here and with other states is especially needed
                   by HDPI-enabled apps but is always done for the sake of certainty */
                if (hasIcon)
                {
                  QPixmap px = getPixmapFromIcon(opt->icon, getIconMode(state,isInactive,lspec),
                                                 iconstate, opt->decorationSize);
                  o.icon = QIcon(px);
                }
                drawViewItem(&o,painter,widget);
                return;
              }
            }
            else if (state == 2)
            {
              QColor focusInactiveColor = getFromRGBA(lspec.focusInactiveColor);
              QColor baseCol = palette.color(QPalette::Base);
              if (baseCol.alpha() == 0)
                baseCol = standardPalette().color(QPalette::Base);
              if ((!isInactive || !focusInactiveColor.isValid())
                  && focusColor.isValid()
                  && (col.isValid()
                      || enoughContrast(standardPalette().color(QPalette::Text), focusColor)
                      // supposing that the focus interior is translucent, take care of contrast
                      || enoughContrast(baseCol, focusColor)))
              {
                col = focusColor;
              }
              else if (isInactive
                       && focusInactiveColor.isValid()
                       && (col.isValid()
                           || enoughContrast(standardPalette().color(QPalette::Text), focusInactiveColor)
                           || enoughContrast(baseCol, focusInactiveColor)))
              {
                col = focusInactiveColor;
              }
              else
                col = QColor();
              if (col.isValid())
              {
                QStyleOptionViewItem o(*opt);
                palette.setColor(QPalette::Text, col);
                palette.setColor(QPalette::HighlightedText, col);
                o.palette = palette;
                if (hasIcon)
                {
                  qreal tintPercentage = static_cast<qreal>(hspec_.tint_on_mouseover);
                  if (tintPercentage > 0)
                  {
                    QPixmap px = tintedPixmap(option,
                                              getPixmapFromIcon(opt->icon,
                                                                getIconMode(state,isInactive,lspec),
                                                                iconstate,
                                                                opt->decorationSize),
                                              tintPercentage);
                    o.icon = QIcon(px);
                  }
                  else
                  {
                    QPixmap px = getPixmapFromIcon(opt->icon, getIconMode(state,isInactive,lspec),
                                                   iconstate, opt->decorationSize);
                    o.icon = QIcon(px);
                  }
                }
                drawViewItem(&o,painter,widget);
                return;
              }
            }
            else if (state == 3)
            {
              QColor pressInactiveColor = getFromRGBA(lspec.pressInactiveColor);
              col = (!isInactive || !pressInactiveColor.isValid())
                      ? pressColor
                      : pressInactiveColor;
              if (col.isValid())
              {
                QStyleOptionViewItem o(*opt);
                palette.setColor(QPalette::HighlightedText, col);
                o.palette = palette;
                if (hasIcon)
                {
                  QPixmap px = getPixmapFromIcon(opt->icon, getIconMode(state,isInactive,lspec),
                                                 iconstate, opt->decorationSize);
                  o.icon = QIcon(px);
                }
                drawViewItem(&o,painter,widget);
                return;
              }
            }
            else if (state == 4)
            {
              QColor toggleInactiveColor = getFromRGBA(lspec.toggleInactiveColor);
              col = (!isInactive || !toggleInactiveColor.isValid())
                      ? toggleColor
                      : toggleInactiveColor;
              if (col.isValid())
              {
                QStyleOptionViewItem o(*opt);
                palette.setColor(QPalette::HighlightedText, col);
                o.palette = palette;
                /* A use case: because the inactive toggled bg may have contrast with the active one
                   and since this can be a symbolic SVG icon, we use the pixmap with inactiveness. */
                if (hasIcon)
                {
                  QPixmap px = getPixmapFromIcon(opt->icon, getIconMode(state,isInactive,lspec),
                                                 iconstate, opt->decorationSize);
                  o.icon = QIcon(px);
                }
                drawViewItem(&o,painter,widget);
                return;
              }
            }
          }
          else if (hasIcon) // disabled
          {
            qreal opacityPercentage = static_cast<qreal>(hspec_.disabled_icon_opacity);
            if (opacityPercentage < 100.0)
            {
              QStyleOptionViewItem o(*opt);
              const label_spec lspec = getLabelSpec(QStringLiteral("ItemView"));
              QPixmap px = translucentPixmap(getPixmapFromIcon(opt->icon,
                                                               getIconMode(state,
                                                                           isWidgetInactive(widget),
                                                                           lspec),
                                                               iconstate,
                                                               opt->decorationSize),
                                             opacityPercentage);
              o.icon = QIcon(px);
              drawViewItem(&o,painter,widget);
              return;
            }
          }
        }
        drawViewItem(opt,painter,widget);
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
          qstyleoption_cast<const QStyleOptionMenuItem*>(option);
      if (opt) {
        QString status = getState(option,widget);
        if (!styleHint(SH_MenuBar_MouseTracking, opt, widget))
        {
          if (status.startsWith(QLatin1String("toggled")))
            status.replace(QLatin1String("toggled"),QLatin1String("normal"));
          if (status.startsWith(QLatin1String("focused")))
            status.replace(QLatin1String("focused"),QLatin1String("normal"));
        }

        QString group = "MenuBarItem";
        label_spec lspec = getLabelSpec(group);

        group = "MenuBar";
        QRect r = opt->menuRect; // menubar svg element may not be simple
        if (r.isEmpty()) r = option->rect;
        if (int th = mergedToolbarHeight(widget))
        {
          group = "Toolbar";
          r.adjust(0,0,0,th);
        }

        frame_spec fspec = getFrameSpec(group);
        if (tspec_.merge_menubar_with_toolbar && group != "Toolbar")
        {
          const frame_spec fspec1 = getFrameSpec(QStringLiteral("Toolbar"));
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
          if (hasExpandedBorder(fspec))
            fspec.expansion = 0;
          else
            fspec.expansion = qMin(fspec.expansion, LIMITED_EXPANSION);
          ispec.px = ispec.py = 0;
        }
        QString inactive;
        if (isWidgetInactive(widget))
          inactive = "-inactive";
        renderFrame(painter,r,fspec,fspec.element+"-normal"+inactive);
        renderInterior(painter,r,fspec,ispec,ispec.element+"-normal"+inactive);

        fspec = getFrameSpec(QStringLiteral("MenuBarItem"));
        ispec = getInteriorSpec(QStringLiteral("MenuBarItem"));

        if (isPlasma_ && widget && widget->window()->testAttribute(Qt::WA_NoSystemBackground))
        {
          lspec.left = lspec.right = lspec.top = lspec.bottom = 0;
          fspec.left = qMin(fspec.left,3);
          fspec.right = qMin(fspec.right,3);
          fspec.top = qMin(fspec.top,3);
          fspec.bottom = qMin(fspec.bottom,3);
          if (hasExpandedBorder(fspec))
            fspec.expansion = 0;
          else
            fspec.expansion = qMin(fspec.expansion, LIMITED_EXPANSION);
        }

        /* topFrame and bottomFrame were added at CT_MenuBarItem
           (also, see PM_MenuBarItemSpacing for the reason) */
        r = option->rect.adjusted(0,topFrame,0,-bottomFrame);

        /* draw a panel for the menubar-item only if it's focused or pressed */
        if (!status.startsWith(QLatin1String("normal")) && (option->state & State_Enabled))
        {
          if (isLibreoffice_ && widget == nullptr)
          {
            painter->fillRect(option->rect, standardPalette().brush(QPalette::Window));
            painter->save();
            painter->setOpacity(0.6);
            painter->fillRect(option->rect, standardPalette().brush(QPalette::Highlight));
            painter->restore();
            lspec.pressColor = lspec.toggleColor
                             = getName(standardPalette().color(QPalette::HighlightedText));
          }
          else
          {
            renderFrame(painter,r,fspec,fspec.element+"-"+status);
            renderInterior(painter,r,fspec,ispec,ispec.element+"-"+status);
          }
        }
        else // always get normal color from menubar (or toolbar if they're merged)
        {
          const label_spec lspec1 = getLabelSpec(group);
          lspec.normalColor = lspec1.normalColor;
          lspec.normalInactiveColor = lspec1.normalInactiveColor;
        }

        int talign = Qt::AlignCenter | Qt::TextSingleLine;
        if (!styleHint(SH_UnderlineShortcut, opt, widget))
          talign |= Qt::TextHideMnemonic;
        else
          talign |= Qt::TextShowMnemonic;
        int state = 1;
        if (!(option->state & State_Enabled))
          state = 0;
        /* press and toggle text colors are used only if
           they exist (focus color seems more natural) */
        else
        {
          if (styleHint(SH_MenuBar_MouseTracking, opt, widget))
          {
            if (status.startsWith(QLatin1String("pressed")))
            {
              if (lspec.pressColor.isEmpty())
                state = 2;
              else
                state = 3;
            }
            else if (status.startsWith(QLatin1String("toggled")))
            {
              if (lspec.toggleColor.isEmpty())
                state = 2;
              else
                state = 4;
            }
          }
          else if (status.startsWith(QLatin1String("pressed")))
          {
            if (lspec.pressColor.isEmpty())
              state = 2;
            else
              state = 3;
          }
        }
        bool isInactive(status.contains(QLatin1String("-inactive")));
        if (!opt->icon.isNull())
        {
          int icnSize = pixelMetric(PM_SmallIconSize);
          QSize iconSize(icnSize,icnSize);
          QPixmap px = getPixmapFromIcon(opt->icon, getIconMode(state,isInactive,lspec),iconstate,iconSize);
          renderLabel(option,painter,r,
                      fspec,lspec,
                      talign,QString(),QPalette::WindowText,
                      state,
                      isInactive,
                      px,
                      iconSize);
        }
        else
        {
          renderLabel(option,painter,r,
                      fspec,lspec,
                      talign,opt->text,QPalette::WindowText,
                      state,
                      isInactive);
        }
      }

      break;
    }

    case CE_MenuBarEmptyArea : {
      /*if (isLibreoffice_ // shouldn't be used with the Qt5 skin
          && enoughContrast(getFromRGBA(getLabelSpec(QStringLiteral("MenuBarItem")).normalColor),
                            QApplication::palette().color(QPalette::WindowText)))
      {
        break;
      }*/
      QString group = "MenuBar";
      QRect r = option->rect;
      if (int th = mergedToolbarHeight(widget))
      {
        group = "Toolbar";
        r.adjust(0,0,0,th);
      }

      /* set the palettes of corner widgets if needed */
      if (const QMenuBar *mb = qobject_cast<const QMenuBar*>(widget))
      {
        QList<QWidget*> cornerWidgets;
        if (QWidget *r = mb->cornerWidget())
        {
          if (!qobject_cast<QToolButton*>(r)) // flat toolbuttons are dealt with at CE_ToolButtonLabel
            cornerWidgets << r;
        }
        if (QWidget *l = mb->cornerWidget(Qt::TopLeftCorner))
        {
          if (!qobject_cast<QToolButton*>(l))
            cornerWidgets << l;
        }
        if (!cornerWidgets.isEmpty())
        {
          const label_spec lspec = getLabelSpec(group);
          QColor txtCol = getFromRGBA(lspec.normalColor);
          if (txtCol.isValid()
              && txtCol != standardPalette().color(QPalette::Active,QPalette::WindowText))
          {
            QColor inactiveTxtCol, disabledTxtCol, pTxtCol;
            for (QWidget *cw : cornerWidgets)
            {
              QPalette palette = cw->palette();
              if (palette.color(QPalette::Active, QPalette::WindowText) != txtCol)
              {
                if (!inactiveTxtCol.isValid())
                {
                  if (tspec_.no_inactiveness)
                    inactiveTxtCol = txtCol;
                  else
                  {
                    inactiveTxtCol = getFromRGBA(lspec.normalInactiveColor);
                    if (!inactiveTxtCol.isValid()) inactiveTxtCol = txtCol;
                  }
                  disabledTxtCol = txtCol;
                  disabledTxtCol.setAlpha(102); // 0.4 * disabledTxtCol.alpha()
                  pTxtCol = txtCol; // placeholder
                  pTxtCol.setAlpha(128);
                }
                palette.setColor(QPalette::Active, QPalette::WindowText, txtCol);
                palette.setColor(QPalette::Inactive, QPalette::WindowText, inactiveTxtCol);
                palette.setColor(QPalette::Disabled, QPalette::WindowText, disabledTxtCol);
                palette.setColor(QPalette::Active, QPalette::ButtonText, txtCol);
                palette.setColor(QPalette::Inactive, QPalette::ButtonText, inactiveTxtCol);
                palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledTxtCol);
                if (qobject_cast<QLabel*>(cw))
                {
                  palette.setColor(QPalette::Active, QPalette::Text, txtCol);
                  palette.setColor(QPalette::Inactive, QPalette::Text, inactiveTxtCol);
                  palette.setColor(QPalette::Disabled, QPalette::Text, disabledTxtCol);
                  palette.setColor(QPalette::PlaceholderText, pTxtCol);
                }
                forcePalette(cw, palette);
              }
            }
          }
        }
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
        const frame_spec fspec1 = getFrameSpec(QStringLiteral("Toolbar"));
        fspec.left = fspec1.left;
        fspec.top = fspec1.top;
        fspec.right = fspec1.right;
        fspec.bottom = fspec1.bottom;
      }

      QString inactive;
      if (isWidgetInactive(widget))
        inactive = "-inactive";
      renderFrame(painter,r,fspec,fspec.element+"-normal"+inactive);
      renderInterior(painter,r,fspec,ispec,ispec.element+"-normal"+inactive);

      break;
    }

    case CE_MenuScroller : {
      /*if (enoughContrast(standardPalette().color(QPalette::WindowText),
          getFromRGBA(getLabelSpec(QStringLiteral("MenuItem")).normalColor)))
      {
        painter->fillRect(option->rect, standardPalette().brush(QPalette::Window));
      }*/
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

        if (opt->state & State_HasFocus)
        {
          QString group;
          QRect fRect;
          if (isRadio)
          {
            group = "RadioButton";
            fRect = subElementRect(SE_RadioButtonFocusRect, opt, widget);
          }
          else
          {
            group = "CheckBox";
            fRect = subElementRect(SE_CheckBoxFocusRect, opt, widget);
          }
          const frame_spec fspec = getFrameSpec(group);
          if (fspec.hasFocusFrame)
          {
            renderFrame(painter,fRect,fspec,fspec.element+"-focus");
            const interior_spec ispec = getInteriorSpec(group);
            if (ispec.hasFocusInterior)
              renderInterior(painter,fRect,fspec,ispec,ispec.element+"-focus");
          }
          else
          {
            QRect focusRect = subElementRect(isRadio ? SE_RadioButtonFocusRect
                                                     : SE_CheckBoxFocusRect, opt, widget);
            drawFocusRect(painter, focusRect, fspec.focusRectElement);
          }
        }

        QStyleOptionButton subopt = *opt;
        subopt.rect = subElementRect(isRadio ? SE_RadioButtonIndicator
                                             : SE_CheckBoxIndicator, opt, widget);
        drawPrimitive(isRadio ? PE_IndicatorRadioButton : PE_IndicatorCheckBox,
                      &subopt, painter, widget);
        subopt.rect = subElementRect(isRadio ? SE_RadioButtonContents
                                             : SE_CheckBoxContents, opt, widget);
        drawControl(isRadio ? CE_RadioButtonLabel : CE_CheckBoxLabel, &subopt, painter, widget);
      }

      break;
    }

    case CE_RadioButtonLabel : {
      const QStyleOptionButton *opt =
          qstyleoption_cast<const QStyleOptionButton*>(option);

      if (opt) {
        frame_spec fspec;
        default_frame_spec(fspec);
        label_spec lspec = getLabelSpec(QStringLiteral("RadioButton"));
        /* vertically centered */
        lspec.top = qMin(lspec.top,2);
        lspec.bottom = qMin(lspec.bottom,2);
        if (widget && qobject_cast<QMenu*>(widget->window()))
        {
          /* in Style::polish(QWidget*), we set the window text color
             of menus to the normal menuitem text color because some
             apps (like QtAv) might do weird things with menus */
          lspec.normalColor = lspec.focusColor
          = getName(widget->palette().color(QPalette::Active,QPalette::WindowText));
          lspec.normalInactiveColor = lspec.focusInactiveColor
          = getName(widget->palette().color(QPalette::Inactive,QPalette::WindowText));
        }

        int talign = Qt::AlignLeft | Qt::AlignVCenter;
        if (!styleHint(SH_UnderlineShortcut, opt, widget))
          talign |= Qt::TextHideMnemonic;
        else
          talign |= Qt::TextShowMnemonic;
        int state = option->state & State_Enabled ? option->state & State_MouseOver ? 2 : 1 : 0;
        bool isInactive(isWidgetInactive(widget));
        renderLabel(option,painter,
                    option->rect,
                    fspec,lspec,
                    talign,opt->text,QPalette::WindowText,
                    state,
                    isInactive,
                    getPixmapFromIcon(opt->icon, getIconMode(state,isInactive,lspec), iconstate, opt->iconSize),
                    opt->iconSize, Qt::ToolButtonTextBesideIcon, false);
      }

      break;
    }

    case CE_CheckBoxLabel : {
      const QStyleOptionButton *opt =
          qstyleoption_cast<const QStyleOptionButton*>(option);

      if (opt) {
        frame_spec fspec;
        default_frame_spec(fspec);
        label_spec lspec = getLabelSpec(QStringLiteral("CheckBox"));
        /* vertically centered */
        lspec.top = qMin(lspec.top,2);
        lspec.bottom = qMin(lspec.bottom,2);
        if (widget && qobject_cast<QMenu*>(widget->window()))
        { // see the explanation at CE_RadioButtonLabel (above)
          lspec.normalColor = lspec.focusColor
          = getName(widget->palette().color(QPalette::Active,QPalette::WindowText));
          lspec.normalInactiveColor = lspec.focusInactiveColor
          = getName(widget->palette().color(QPalette::Inactive,QPalette::WindowText));
        }

        int talign = Qt::AlignLeft | Qt::AlignVCenter;
        if (!styleHint(SH_UnderlineShortcut, opt, widget))
          talign |= Qt::TextHideMnemonic;
        else
          talign |= Qt::TextShowMnemonic;
        int state = option->state & State_Enabled ? option->state & State_MouseOver ? 2 : 1 : 0;
        bool isInactive(isWidgetInactive(widget));
        renderLabel(option,painter,
                    option->rect,
                    fspec,lspec,
                    talign,opt->text,QPalette::WindowText,
                    state,
                    isInactive,
                    getPixmapFromIcon(opt->icon, getIconMode(state,isInactive,lspec), iconstate, opt->iconSize),
                    opt->iconSize, Qt::ToolButtonTextBesideIcon, false);
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
                    && !(option->styleObject
                         && option->styleObject->property("_kv_hover_bug").toBool()) // hover bug
                  ? "focused" :
                  (option->state & State_Sunken)
                  // to know whether it has keyboard focus
                  || (option->state & State_Selected) ? "pressed" : "normal"
                 : "disabled";
        if (isWidgetInactive(widget))
          status.append("-inactive");

        QString group;
        if ((!getFrameSpec(QStringLiteral("ToolbarComboBox")).element.isEmpty()
             || !getInteriorSpec(QStringLiteral("ToolbarComboBox")).element.isEmpty())
            && getStylableToolbarContainer(widget, true))
        {
          group = "ToolbarComboBox";
        }
        else group = "ComboBox";

        frame_spec fspec = getFrameSpec(group);
        label_spec lspec = getLabelSpec(group);
        size_spec sspec = getSizeSpec(group);

        /* there's no reason for a variable distance from the arrow
            because the combobox length doesn't change with it */
        if (opt->direction == Qt::RightToLeft)
        {
          fspec.left = 0;
          lspec.left = 3;
        }
        else
        {
          fspec.right = 0;
          lspec.right = 3;
        }

        QRect r = subControlRect(CC_ComboBox,opt,SC_ComboBoxEditField,widget);
        int talign = Qt::AlignLeft | Qt::AlignVCenter;
        if (!styleHint(SH_UnderlineShortcut, opt, widget))
          talign |= Qt::TextHideMnemonic;
        else
          talign |= Qt::TextShowMnemonic;

        int state = 1;
        if (!(option->state & State_Enabled))
          state = 0;
        else if (status.startsWith(QLatin1String("pressed")))
          state = 3;
        else if (status.startsWith(QLatin1String("toggled")))
          state = 4;
        else if (status.startsWith(QLatin1String("focused")))
          state = 2;

        /* when there isn't enough space */
        int cbH = r.height();
        if (const QComboBox *cb = qobject_cast<const QComboBox*>(widget))
        {
          if (!cb->lineEdit())
            cbH = qMin(cb->height(), cbH);
        }
        QFont F(painter->font());
        if (lspec.boldFont) F.setWeight(lspec.boldness);
        QSize txtSize = textSize(F,opt->currentText);
        if (/*r.width() < fspec.left+lspec.left+txtSize.width()+lspec.right+COMBO_ARROW_LENGTH+fspec.right
                        + (sspec.incrementW ? sspec.minW : 0)
            ||*/ cbH < fspec.top+lspec.top+txtSize.height()+fspec.bottom+lspec.bottom)
        {
          fspec.left = qMin(fspec.left,3);
          fspec.right = qMin(fspec.right,3);
          fspec.top = qMin(fspec.top,3);
          fspec.bottom = qMin(fspec.bottom,3);

          lspec.left = qMin(lspec.left,2);
          lspec.right = qMin(lspec.right,2);
          lspec.top = lspec.bottom = 0;
          lspec.tispace = qMin(lspec.tispace,2);
          lspec.boldFont = false;

          sspec.incrementW = false;
        }

        if (sspec.incrementW)
        {
          if (opt->currentIcon.isNull())
          {
            if (opt->direction == Qt::RightToLeft)
              lspec.right += sspec.minW/2;
            else
              lspec.left += sspec.minW/2;
          }
          else
            lspec.tispace += sspec.minW/2;
        }
        bool isInactive(status.contains(QLatin1String("-inactive")));
        QStyleOptionComboBox o(*opt);
        if ((option->state & State_MouseOver) && !status.startsWith(QLatin1String("focused")))
          o.state = o.state & ~QStyle::State_MouseOver; // hover bug
        renderLabel(&o,painter,r,
                    fspec,lspec,
                    talign,opt->currentText,QPalette::ButtonText,
                    state,
                    isInactive,
                    getPixmapFromIcon(opt->currentIcon, getIconMode(state,isInactive,lspec), iconstate, opt->iconSize),
                    opt->iconSize);
      }

      break;
    }

    case CE_TabBarTabShape : {
      const QStyleOptionTab *opt =
        qstyleoption_cast<const QStyleOptionTab*>(option);

      if (opt)
      {
        frame_spec fspec = getFrameSpec(QStringLiteral("Tab"));
        interior_spec ispec = getInteriorSpec(QStringLiteral("Tab"));

        /* Let's forget about the pressed state. It's useless here and
           makes trouble in KDevelop. The disabled state is useless too. */
        QString status =
                (option->state & State_On) ? "toggled" :
                 (option->state & State_Selected) ? "toggled" :
                 (option->state & State_MouseOver) ?
                  (option->state & State_Enabled) ? "focused" : "normal"
                : "normal";
        int frameExpansion = fspec.expansion;
        if (status != "toggled" && tspec_.no_inactive_tab_expansion)
          fspec.expansion = 0;

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
          ispec.element = "floating-"+ispec.element;
          fspec.element = "floating-"+fspec.element;
          if (!fspec.expandedElement.isEmpty())
            fspec.expandedElement = "floating-"+fspec.expandedElement;
          joinedActiveTab = joinedActiveFloatingTab_;
          sepName = "floating-"+sepName;
        }

        QStyleOptionTab::TabPosition tabPos = tabPosition(opt, widget);
        if (isLibreoffice_ && widget == nullptr)
        {
          /* LibreOffice gives wrong tab positions, doesn't give selected positions, and
             its tabs may be drawn in multiple rows. Here, only the first issue is fixed. */
          if (tabPos == QStyleOptionTab::Beginning)
            tabPos = QStyleOptionTab::End;
          else if (tabPos == QStyleOptionTab::End)
            tabPos = QStyleOptionTab::Beginning;
        }

        if (joinedActiveTab) // only use normal and toggled states
        {
          if ((option->state & State_On) || (option->state & State_Selected)
               // use toggled separator on both sides of selected tabs if possible
              || (tabPos != QStyleOptionTab::OnlyOneTab
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
            || status == "normal" || status == "focused")
        {
          if (tspec_.joined_inactive_tabs
              && tabPos != QStyleOptionTab::OnlyOneTab)
          {
            int hPos = 2;
            if (tabPos == QStyleOptionTab::Beginning)
            {
              if ((joinedActiveTab && !noActiveTabSep) || opt->selectedPosition != QStyleOptionTab::NextIsSelected)
              {
                fspec.isAttached = true;
                hPos = -1;
              }
            }
            else if (tabPos == QStyleOptionTab::Middle)
            {
              fspec.isAttached = true;
              if ((joinedActiveTab && !noActiveTabSep))
                hPos = 0;
              else if (opt->selectedPosition == QStyleOptionTab::NextIsSelected)
                hPos = 1;
              else if (opt->selectedPosition == QStyleOptionTab::PreviousIsSelected)
                hPos = -1;
              else
                hPos = 0;
            }
            else if (tabPos == QStyleOptionTab::End)
            {
              if ((joinedActiveTab && !noActiveTabSep) || opt->selectedPosition != QStyleOptionTab::PreviousIsSelected)
              {
                fspec.isAttached = true;
                hPos = 1;
              }
            }
            /* will be flipped both vertically and horizontally */
            if (mirroredBottomTab)
              hPos = -1*hPos;
            /* I've seen this only in KDevelop */
            if (rtl)
              hPos = -1*hPos;
            fspec.HPos = hPos;
          }
        }

        bool drawSep(joinedActiveTab
                     && tabPos != QStyleOptionTab::OnlyOneTab
                     && (mirroredBottomTab
                           ? rtl ? tabPos != QStyleOptionTab::End
                                 : tabPos != QStyleOptionTab::Beginning
                           : rtl ? tabPos != QStyleOptionTab::Beginning
                                 : tabPos != QStyleOptionTab::End));
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
        if (fspec.isAttached)
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

        /* handle overlapping */
        int overlap = tspec_.active_tab_overlap;
        if (overlap > 0 && (!joinedActiveTab || noActiveTabSep)
            && tabPos != QStyleOptionTab::OnlyOneTab)
        {
          if (status != "toggled")
          {
            int exp = qMin(frameExpansion, qMin(r.width(), r.height())) / 2 + 1;
            overlap = qMin(overlap, qMax(exp, qMax(fspec.left, fspec.right)));
            if (rtl)
            {
              if (mirroredBottomTab)
              {
                if (opt->selectedPosition == QStyleOptionTab::PreviousIsSelected)
                  r.adjust(-overlap, 0, 0, 0);
                else if (opt->selectedPosition == QStyleOptionTab::NextIsSelected)
                  r.adjust(0, 0, overlap, 0);
              }
              else
              {
                if (opt->selectedPosition == QStyleOptionTab::NextIsSelected)
                  r.adjust(-overlap, 0, 0, 0);
                else if (opt->selectedPosition == QStyleOptionTab::PreviousIsSelected)
                  r.adjust(0, 0, overlap, 0);
              }
            }
            else
            {
              if (mirroredBottomTab)
              {
                if (opt->selectedPosition == QStyleOptionTab::NextIsSelected)
                  r.adjust(-overlap, 0, 0, 0);
                else if (opt->selectedPosition == QStyleOptionTab::PreviousIsSelected)
                  r.adjust(0, 0, overlap, 0);
              }
              else
              {
                if (opt->selectedPosition == QStyleOptionTab::PreviousIsSelected)
                  r.adjust(-overlap, 0, 0, 0);
                else if (opt->selectedPosition == QStyleOptionTab::NextIsSelected)
                  r.adjust(0, 0, overlap, 0);
              }
            }
          }
          else if (!widget) // with QML, we imitate an overlap
          {
            frame_spec fspec1 = fspec;
            if (tspec_.no_inactive_tab_expansion)
              fspec1.expansion = 0;
            fspec1.isAttached = true;
            fspec1.HPos = 0;
            QRect R = r;
            if (tabPos == QStyleOptionTab::Beginning)
            {
              if (rtl)
                R.adjust(0,0,-r.width()/2,0);
              else
                R.adjust(r.width()/2,0,0,0);
            }
            else if (tabPos == QStyleOptionTab::End)
            {
              if (rtl)
                R.adjust(r.width()/2,0,0,0);
              else
                R.adjust(0,0,-r.width()/2,0);
            }
            renderInterior(painter,R,fspec1,ispec,ispec.element+"-normal",true);
            renderFrame(painter,R,fspec1,fspec1.element+"-normal",0,0,0,0,0,true);
          }
        }

        bool libreoffice = false;
        if (isLibreoffice_ && widget == nullptr && (option->state & State_Enabled) && status == "toggled")
        {
            if (enoughContrast(getFromRGBA(getLabelSpec(QStringLiteral("Tab")).toggleColor),
                               standardPalette().color(QPalette::ButtonText)))
            {
              libreoffice = true;
              painter->save();
              painter->setOpacity(0.5);
            }
        }
        if (isWidgetInactive(widget))
        {
          status.append("-inactive");
          sepName.append("-inactive");
        }
        renderInterior(painter,r,fspec,ispec,ispec.element+"-"+status,true);
        renderFrame(painter,r,fspec,fspec.element+"-"+status,0,0,0,0,0,true);
        if (libreoffice) painter->restore();
        if ((opt->state & State_HasFocus)
            && fspec.hasFocusFrame) // otherwise -> CE_TabBarTabLabel
        {
          renderFrame(painter,r,fspec,fspec.element+"-focus");
          if (ispec.hasFocusInterior)
            renderInterior(painter,r,fspec,ispec,ispec.element+"-focus");
        }
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

        /* When a top tab is attached to its tab widget, for example,
           we should update a thin rectangle below it because if tabs
           are moved by scroll buttons, the top side of the tab widget
           rectangle won't be updated automatically. (A Qt design flaw?) */
        if (!docMode && tspec_.attach_active_tab)
        { // tw exists
          fspec = getFrameSpec(QStringLiteral("TabFrame"));
          if (verticalTabs)
          {
            if (opt->shape == QTabBar::RoundedWest || opt->shape == QTabBar::TriangularWest)
              tw->update(widget->x()+widget->width(), y, fspec.left, h);
            else
              tw->update(widget->x()+x-fspec.right , y, fspec.right, h);
          }
          else
          {
            if (opt->shape == QTabBar::RoundedSouth || opt->shape == QTabBar::TriangularSouth)
              tw->update(widget->x()+x, widget->y()-fspec.bottom, w, fspec.bottom);
            else
              tw->update(widget->x()+x, widget->y()+h, w, fspec.top);
          }
        }
      }

      break;
    }

    case CE_TabBarTabLabel : {
      const QStyleOptionTab *opt =
        qstyleoption_cast<const QStyleOptionTab*>(option);

      if (opt) {
        int state =
            (option->state & State_Enabled) ?
              (option->state & State_On) ? 4 :
              (option->state & State_Selected) ? 4 :
              (option->state & State_MouseOver) ? 2 : 1
            : 0;

        const QString group = "Tab";
        frame_spec fspec = getFrameSpec(group);
        label_spec lspec = getLabelSpec(group);

        if (state == 4 && tspec_.bold_active_tab)
          lspec.boldFont = true;

        int talign;
        //if (!widget) // QML
          talign = Qt::AlignCenter;
        /*else
          talign = Qt::AlignLeft | Qt::AlignVCenter;*/
        if (!styleHint(SH_UnderlineShortcut, opt, widget))
          talign |= Qt::TextHideMnemonic;
        else
          talign |= Qt::TextShowMnemonic;

        QRect r = option->rect;
        bool rtl(opt->direction == Qt::RightToLeft);
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
          painter->save();
          r.setRect(0, 0, h, w);
          QTransform m;
          if (rtl)
          { // needed for the correct text-icon order and leveling
            int t = fspec.bottom;
            fspec.bottom = fspec.top;
            fspec.top = t;
            t = lspec.bottom;
            lspec.bottom = lspec.top;
            lspec.top = t;
            m.translate(w+2*x, h+2*y); m.scale(-1,-1);
          }
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

        /* the close button area is always SE_TabBarTabRightButton,
           whose width is determined by PM_TabCloseIndicatorWidth and PM_TabBarTabHSpace */
        int ltb = 0;
        int rtb = 0;
        if (verticalTabs)
        {
          ltb = qMax(0, opt->leftButtonSize.height());
          rtb = qMax(0, opt->rightButtonSize.height());
        }
        else
        {
          ltb = qMax(0, opt->leftButtonSize.width());
          rtb = qMax(0, opt->rightButtonSize.width());
        }
        /* the tab is widened for INDICATOR_MARGIN, tab_button_extra_margin
           PM_TabBarTabHSpace and tispace at CT_TabBarTab */
        if (rtb > 0) rtb += INDICATOR_MARGIN
                            + tspec_.tab_button_extra_margin
                            + pixelMetric(PM_TabBarTabHSpace,option,widget)/2;
        if (ltb > 0) ltb += INDICATOR_MARGIN
                            + tspec_.tab_button_extra_margin
                            + pixelMetric(PM_TabBarTabHSpace,option,widget)/2;
        if (rtl)
        {
          r.adjust(rtb, 0, -ltb, 0);
          if (rtb > 0)
          {
            fspec.left = 0;
            if (!opt->icon.isNull() && lspec.left < lspec.tispace)
              lspec.left = lspec.tispace; // minimum spacing between text and button
          }
          if (ltb > 0)
          {
            fspec.right = 0;
            if (!opt->icon.isNull() && lspec.right < lspec.tispace)
              lspec.right = lspec.tispace;
          }
        }
        else
        {
          r.adjust(ltb, 0, -rtb, 0);
          if (rtb > 0)
          {
            fspec.right = 0;
            if (!opt->icon.isNull() && lspec.right < lspec.tispace)
              lspec.right = lspec.tispace;
          }
          if (ltb > 0)
          {
            fspec.left = 0;
            if (!opt->icon.isNull() && lspec.left < lspec.tispace)
              lspec.left = lspec.tispace;
          }
        }

        QSize iconSize;
        if (!opt->icon.isNull())
          iconSize = opt->iconSize;
        int icnSize = iconSize.isValid() ?
                        qMax(iconSize.width(), iconSize.height())
                        : pixelMetric(PM_TabBarIconSize,option,widget);

        /* eliding (WARNING: QML may report an empty text when there isn't
                             enough space, so nothing can be done for it.) */
        QString txt = opt->text;
        if (!txt.isEmpty())
        {
          const QTabBar *tb = qobject_cast<const QTabBar*>(widget);
          int txtWidth = r.width() - lspec.right-lspec.left-fspec.left-fspec.right
                         - (opt->icon.isNull() ? 0 : icnSize + lspec.tispace);
          QFont F(painter->font());
          if (lspec.boldFont)
          {
            if (!tb) // QML
              lspec.boldFont = false;
            else
              F.setWeight(lspec.boldness);
          }
          txt.replace('\n', ' ');
          QSize txtSize = textSize(F,txt);
          if (txtSize.width() > txtWidth)
          {
            /* Even if the text is elided because of the size hint,
               it might not fit into our available space. So, we always
               elide it but respect the elide mode when present. */
            QFontMetrics fm(F);
            txt = fm.elidedText(txt,
                                (tb && tb->elideMode() != Qt::ElideNone)
                                  ? tb->elideMode() : Qt::ElideRight,
                                txtWidth, Qt::TextShowMnemonic);
          }
          if (txtSize.height() > r.height() - lspec.top-lspec.bottom-fspec.top-fspec.bottom)
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
        bool isInactive(isWidgetInactive(widget));
        renderLabel(option,painter,
                    r,
                    fspec,lspec,
                    talign,txt,QPalette::WindowText,
                    state,
                    isInactive,
                    getPixmapFromIcon(opt->icon, getIconMode(state,isInactive,lspec), iconstate, iconSize),
                    iconSize);

        if ((opt->state & State_HasFocus)
            && !fspec.hasFocusFrame) // otherwise -> CE_TabBarTabShape
        {
          QRect focusRect;
          if (fspec.expansion > 0)
          {
            if (rtl)
              focusRect = labelRect(r, fspec, lspec).adjusted(-2, -2, 2, 2);
            else
              focusRect = labelRect(r, fspec, lspec).adjusted(-2, -2, 2, 2);
          }
          else
          {
            QRect FR = opt->rect;
            if (verticalTabs)
              FR.setRect(0, 0, h, w);
            focusRect= interiorRect(FR, getFrameSpec(group)); // fspec may have been changed
          }
          drawFocusRect(painter, focusRect, fspec.focusRectElement);
        }

        if (verticalTabs)
          painter->restore();
      }

      break;
    }

    case CE_ToolBoxTab: {
      if (const QStyleOptionToolBox *opt = qstyleoption_cast<const QStyleOptionToolBox *>(option)) {
        drawControl(CE_ToolBoxTabShape, opt, painter, widget);
        drawControl(CE_ToolBoxTabLabel, opt, painter, widget);
      }

      break;
    }

    /* as in QCommonStyle but with more precision */
    case CE_ToolBoxTabShape: {
      /* Anti-aliasing is terrible with QPainter::drawPolyline().
         So, we use QPainter::drawPolygon() instead. */
      if (const QStyleOptionToolBox *opt = qstyleoption_cast<const QStyleOptionToolBox *>(option)) {
        painter->save();

        painter->setRenderHints(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(opt->palette.mid().color().darker(150));
        QRectF R(opt->rect);
        qreal d = 20.0 + R.height() - 3.0;
        if (opt->direction != Qt::RightToLeft)
        { // the shape is -----\__
          painter->drawPolygon(QPolygonF() << QPointF(-1.0, 1.0)
                                           << QPointF(R.width() - d, 1.0)
                                           << QPointF(R.width() - 20.0, R.height() - 2.0)
                                           << QPointF(R.width() - 1.0, R.height() - 2.0)
                                           << QPointF(R.width() - 1.0, R.height() - 1.0)//
                                           << QPointF(R.width() - 20.5, R.height() - 1.0)
                                           << QPointF(R.width() - d - 0.5, 2.0)
                                           << QPointF(-1.0, 2.0));
          painter->setBrush(opt->palette.light().color());
          painter->drawPolygon(QPolygonF() << QPointF(-1.0, 2.0)
                                           << QPointF(R.width() - d - 0.5, 2.0)
                                           << QPointF(R.width() - 20.5, R.height() - 1.0)
                                           << QPointF(R.width() - 1.0, R.height() - 1.0)
                                           << QPointF(R.width() - 1.0, R.height())//
                                           << QPointF(R.width() - 21.0, R.height())
                                           << QPointF(R.width() - d - 1.0, 3.0)
                                           << QPointF(-1.0, 3.0));
        }
        else
        { // the shape is __/-----
          painter->drawPolygon(QPolygonF() << QPointF(R.width(), 1.0)
                                           << QPointF(d - 1.0, 1.0)
                                           << QPointF(19.0, R.height() - 2.0)
                                           << QPointF(0.0, R.height() - 2.0)
                                           << QPointF(0.0, R.height() - 1.0)//
                                           << QPointF(19.5, R.height() - 1.0)
                                           << QPointF(d - 0.5, 2.0)
                                           << QPointF(R.width(), 2.0));
          painter->setBrush(opt->palette.light().color());
          painter->drawPolygon(QPolygonF() << QPointF(R.width(), 2.0)
                                           << QPointF(d - 0.5, 2.0)
                                           << QPointF(19.5, R.height() - 1.0)
                                           << QPointF(0.0, R.height() - 1.0)
                                           << QPointF(0.0, R.height())//
                                           << QPointF(20.0, R.height())
                                           << QPointF(d + 0.5, 3.0)
                                           << QPointF(R.width(), 3.0));
        }

        painter->restore();
     }

      break;
    }

    case CE_ToolBoxTabLabel : {
      if (const QStyleOptionToolBox *opt = qstyleoption_cast<const QStyleOptionToolBox*>(option))
      {
        int state = 1;
        if (!(option->state & State_Enabled))
          state = 0;
        else if ((option->state & State_On) || (option->state & State_Sunken) || (option->state & State_Selected))
          state = 3;
        else if (option->state & State_MouseOver)
          state = 2;

        bool isInactive(isWidgetInactive(widget));

        const label_spec lspec = getLabelSpec(QStringLiteral("ToolboxTab"));
        int smallIconSize = pixelMetric(PM_SmallIconSize,opt,widget);
        QPixmap px = getPixmapFromIcon(opt->icon,
                                       getIconMode(state,isInactive,lspec),
                                       iconstate,
                                       QSize(smallIconSize,smallIconSize));

        /* first get the correct pixmap and palette */
        QColor col;
        if (state != 0)
        {
          const label_spec lspec = getLabelSpec(QStringLiteral("ToolboxTab"));
          if (state == 1)
          {
            if (isInactive)
            {
              col = getFromRGBA(lspec.normalInactiveColor);
              if (!col.isValid())
                col = getFromRGBA(lspec.normalColor);
            }
            else
              col = getFromRGBA(lspec.normalColor);
          }
          else if (state == 2)
          {
            if (isInactive)
            {
              col = getFromRGBA(lspec.focusInactiveColor);
              if (!col.isValid())
                col = getFromRGBA(lspec.focusColor);
            }
            else
              col = getFromRGBA(lspec.focusColor);
            qreal tintPercentage = static_cast<qreal>(hspec_.tint_on_mouseover);
            if (tintPercentage > 0 && !opt->icon.isNull())
              px = tintedPixmap(option, px,tintPercentage);
          }
          else if (state == 3)
          {
            if (isInactive)
            {
              col = getFromRGBA(lspec.pressInactiveColor);
              if (!col.isValid())
                col = getFromRGBA(lspec.pressColor);
            }
            else
              col = getFromRGBA(lspec.pressColor);
            qreal tintPercentage = static_cast<qreal>(hspec_.tint_on_mouseover);
            if (tintPercentage > 0 && (option->state & State_MouseOver) && !opt->icon.isNull())
              px = tintedPixmap(option, px,tintPercentage);
            if (styleHint(QStyle::SH_ToolBox_SelectedPageTitleBold, opt, widget))
            {
              QFont f(painter->font());
              f.setWeight(lspec.boldness);
              painter->save();
              painter->setFont(f);
            }
          }
        }
        else // disabled
        {
          col = standardPalette().color(QPalette::Disabled,QPalette::Text);
          if (!opt->icon.isNull())
          {
            qreal opacityPercentage = static_cast<qreal>(hspec_.disabled_icon_opacity);
            if (opacityPercentage < 100.0)
              px = translucentPixmap(px, opacityPercentage);
          }
        }

        /* then, draw the text and icon as in QCommonStyle::drawControl()
           but with corrections, especially for RTL */

        QRect cr = subElementRect(QStyle::SE_ToolBoxTabContents, opt, widget);
        QRect tr, ir;
        if (px.isNull())
        {
          tr = cr;
          tr.adjust(4, 0, -8, 0);
        }
        else
        {
          bool rtl(option->direction == Qt::RightToLeft);
          ir = alignedRect(option->direction,
                           Qt::AlignLeft | Qt::AlignVCenter,
                           QSize(smallIconSize,smallIconSize), cr);
          if (rtl)
          {
            ir.adjust(-4, 0, -4, 0);
            tr = QRect(cr.left()+3, cr.top(), cr.width()-smallIconSize-14, cr.height()); //  14 = 7 + 4 + 3 (spacing is 7)
          }
          else
          {
            ir.adjust(4 , 0, 4, 0);
            tr = QRect(ir.right()+8, cr.top(), cr.width()-ir.right()-11, cr.height()); // 11 = 8 + 3
          }
        }

        QString txt = QFontMetrics(painter->font()).elidedText(opt->text, Qt::ElideRight,
                                                               tr.width(), Qt::TextShowMnemonic);

        if (!px.isNull())
          drawItemPixmap(painter, ir, Qt::AlignCenter, px);

        int talign = Qt::AlignLeft | Qt::AlignVCenter;
        if (!styleHint(SH_UnderlineShortcut, opt, widget))
          talign |= Qt::TextHideMnemonic;
        else
          talign |= Qt::TextShowMnemonic;

        painter->save();
        if (col.isValid())
        {
          painter->setOpacity(col.alphaF());
          col.setAlpha(255);
          painter->setPen(col);
        }
        painter->setLayoutDirection(option->direction); // because of a bug in QToolBox?
        drawItemText(painter, tr, talign, opt->palette, state != 0, txt,
                     col.isValid() ? QPalette::NoRole : QPalette::ButtonText);
        painter->restore();

        if (state == 3 && styleHint(QStyle::SH_ToolBox_SelectedPageTitleBold, opt, widget))
          painter->restore();

        if (!txt.isEmpty() && (opt->state & State_HasFocus))
        {
          QStyleOptionFocusRect o;
          o.rect = tr;
          o.palette = opt->palette;
          o.state = QStyle::State_None;
          drawPrimitive(QStyle::PE_FrameFocusRect, &o, painter, widget);
        }
      }

      break;
    }

    case CE_ProgressBar : {
      const QStyleOptionProgressBar *opt =
          qstyleoption_cast<const QStyleOptionProgressBar*>(option);
      if (opt)
      {
        QStyleOptionProgressBar subopt = *opt;
        subopt.rect = subElementRect(SE_ProgressBarGroove, opt, widget);
        drawControl(CE_ProgressBarGroove, &subopt, painter, widget);
        subopt.rect = subElementRect(SE_ProgressBarContents, opt, widget);
        drawControl(CE_ProgressBarContents, &subopt, painter, widget);
        if (opt->textVisible)
        {
          subopt.rect = subElementRect(SE_ProgressBarLabel, opt, widget);
          drawControl(CE_ProgressBarLabel, &subopt, painter, widget);
        }
      }

      break;
    }

    case CE_ProgressBarGroove : {
      const QStyleOptionProgressBar *opt =
          qstyleoption_cast<const QStyleOptionProgressBar*>(option);
      QString group;
      if (tspec_.vertical_spin_indicators && isKisSlider_)
        group = "LineEdit";
      else group = "Progressbar";

      frame_spec fspec = getFrameSpec(group);
      fspec.left = fspec.right = qMin(fspec.left,fspec.right);
      const interior_spec ispec = getInteriorSpec(group);
      if (isKisSlider_)
      {
        fspec.isAttached = true;
        fspec.HPos = -1;
        if (tspec_.vertical_spin_indicators)
        {
          fspec.left = qMin(fspec.left,3);
          fspec.right = qMin(fspec.right,3);
          fspec.top = qMin(fspec.top,3);
          fspec.bottom = qMin(fspec.bottom,3);
          if (hasExpandedBorder(fspec))
            fspec.expansion = 0;
          else
            fspec.expansion = qMin(fspec.expansion, LIMITED_EXPANSION);
        }
        else if (fspec.expansion != 0)
        {
          fspec.expansion = qMin(tspec_.inline_spin_indicators
                                   ? getFrameSpec(QStringLiteral("LineEdit")).expansion
                                   : getFrameSpec(QStringLiteral("IndicatorSpinBox")).expansion,
                                 fspec.expansion);
        }
      }

      QRect r = option->rect;

      /* checking State_Horizontal wouldn't work with
         Krita's progress-spin boxes (KisSliderSpinBox) */
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
      /* always invert RTL horizontal bars
         (but not for KisSliders, which have spinboxes) */
      if (!isVertical && !isKisSlider_ && option->direction == Qt::RightToLeft)
        inverted = !inverted;

      QFont f(painter->font());
      const label_spec lspec = getLabelSpec(QStringLiteral("Progressbar"));
      if (lspec.boldFont) f.setWeight(lspec.boldness);

      /* This is the condition set at CT_ProgressBar for using thin progressbars.
         It is independent of the real progressbar thickness. */
      if (!isKisSlider_ && !isKrita_ && tspec_.progressbar_thickness > 0
          && QFontMetrics(f).height() > tspec_.progressbar_thickness)
      { // determine the text position relative to the bar
        bool topText;
        QSize s;
        if (isVertical)
        {
          s = QSize(qMin(tspec_.progressbar_thickness,w), h);
          topText = (tspec_.progressbar_thickness + QFontMetrics(f).height()+3 <= w);
        }
        else
        {
          s = QSize(w ,qMin(tspec_.progressbar_thickness,h));
          topText = (tspec_.progressbar_thickness + QFontMetrics(f).height()+3 <= h);
        }

        if (opt && opt->textVisible && QFontMetrics(f).height() > (isVertical ? s.width() : s.height()))
        {
          if(topText)
          { // when the space is more than enough (with an expanding vertical policy)
            r = alignedRect(option->direction, Qt::AlignCenter,
                            isVertical
                              ? QSize(tspec_.progressbar_thickness + QFontMetrics(f).height()+3, h)
                              : QSize(w, tspec_.progressbar_thickness + QFontMetrics(f).height()+3),
                            r);
          }
          r = alignedRect(option->direction,
                          topText ? isVertical ? Qt::AlignVCenter|Qt::AlignLeft : Qt::AlignHCenter|Qt::AlignBottom
                                  : Qt::AlignCenter,
                          s,r);
          x = r.x();
          y = r.y();
          h = r.height();
          w = r.width();

          if (!topText)
          {
            QString maxText = progressMaxText(pb, opt);
            if (!maxText.isEmpty())
            {
              int textWidth = QFontMetrics(f).horizontalAdvance(maxText) + 6; // 3px space + margin
              if (isVertical)
              {
                if (inverted)
                {
                  r.adjust(0, 0, 0, -textWidth);
                  y += textWidth;
                }
                else
                  r.adjust(0, textWidth, 0, 0);
                h = r.height();
              }
              else
              {
                if (inverted)
                  r.adjust(textWidth, 0, 0, 0);
                else
                  r.adjust(0, 0, -textWidth, 0);
                w = r.width();
              }
            }
          }
        }
        else // thin progressbar without text
        {
          r = alignedRect(option->direction,Qt::AlignCenter,s,r);
          x = r.x();
          y = r.y();
          h = r.height();
          w = r.width();
        }
      }
      /*else
      { // when the space is more than enough (with an expanding vertical policy)
          r = alignedRect(option->direction, Qt::AlignCenter,
                          isVertical
                            ? QSize(qMin(QFontMetrics(f).height()+fspec.top+fspec.bottom, w), h)
                            : QSize(w, qMin(QFontMetrics(f).height()+fspec.top+fspec.bottom, h)),
                          r);
          x = r.x();
          y = r.y();
          h = r.height();
          w = r.width();
      }*/

      if (isVertical)
      {
        /* we don't save and restore the painter to draw
           the contents and the label correctly below */
        r.setRect(y, x, h, w);
        QTransform m;
        m.scale(-1,1);
        m.rotate(90);
        painter->setTransform(m, true);
      }

      if (!(option->state & State_Enabled))
      {
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      QString suffix = "-normal";
      if (isWidgetInactive(widget))
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
                   //(option->state & State_Selected) ? "toggled" :
                   (option->state & State_MouseOver) ? "focused" : "normal"
                 : "disabled";
        if (isWidgetInactive(widget))
          status.append("-inactive");

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
        /* horizontal bars are inverted in CE_ProgressBarGroove (but not for KisSliders) */
        if (!isVertical && !isKisSlider_ && option->direction == Qt::RightToLeft)
          inverted = !inverted;

        frame_spec fspecPr = getFrameSpec(QStringLiteral("Progressbar"));
        if (isKisSlider_)
          fspecPr.right = 0;
        else
        {
          fspecPr.left = fspecPr.right = qMin(fspecPr.left,fspecPr.right); // -> CE_ProgressBarGroove
        }

        /* take care of bad designs (as in pavucontrol-qt) */
        bool spreadProgressbar = tspec_.spread_progressbar;
        if (!spreadProgressbar && (isVertical ? w : h) <= fspecPr.top + fspecPr.bottom)
          spreadProgressbar = true;

        QRect r = option->rect;
        if (!spreadProgressbar)
        {
          if (isVertical)
            r.adjust(fspecPr.top, fspecPr.right, -fspecPr.bottom, -fspecPr.left);
          else
            r.adjust(fspecPr.left, fspecPr.top, -fspecPr.right, -fspecPr.bottom);
        }

        QFont f(painter->font());
        const label_spec lspec = getLabelSpec(QStringLiteral("Progressbar"));
        if (lspec.boldFont) f.setWeight(lspec.boldness);

        if (!isKisSlider_ && !isKrita_ && tspec_.progressbar_thickness > 0
            && QFontMetrics(f).height() > tspec_.progressbar_thickness)
        { // determine the text position relative to the bar
          bool topText;
          QSize s;
          int thickness = tspec_.progressbar_thickness - (spreadProgressbar
                                                          ? 0
                                                          : fspecPr.top + fspecPr.bottom);
          if (thickness <= 0) return;
          if (isVertical)
          {
            s = QSize(qMin(thickness,r.width()), r.height());
            topText = (thickness + QFontMetrics(f).height()+3 <= r.width());
          }
          else
          {
            s = QSize(r.width() , qMin(thickness,r.height()));
            topText = (thickness + QFontMetrics(f).height()+3 <= r.height());
          }

          thickness = isVertical ? spreadProgressbar
                                   ? s.width() : s.width() + fspecPr.top + fspecPr.bottom
                                 : spreadProgressbar
                                   ? s.height() : s.height() + fspecPr.top + fspecPr.bottom;
          if (opt->textVisible && QFontMetrics(f).height() > thickness)
          {
            if(topText)
            { // when the space is more than enough (with an expanding vertical policy)
              if (isVertical)
              {
                r = alignedRect(option->direction, Qt::AlignCenter,
                                QSize(tspec_.progressbar_thickness + QFontMetrics(f).height()+3, r.height()),
                                option->rect);
                if (!spreadProgressbar)
                  r.adjust(fspecPr.top, 0, -fspecPr.bottom, 0);
              }
              else
              {
                r = alignedRect(option->direction, Qt::AlignCenter,
                                QSize(r.width(), tspec_.progressbar_thickness + QFontMetrics(f).height()+3),
                                option->rect);
                if (!spreadProgressbar)
                  r.adjust(0, fspecPr.top, 0, -fspecPr.bottom);
              }
            }
            r = alignedRect(option->direction,
                            topText ? isVertical ? Qt::AlignVCenter|Qt::AlignLeft : Qt::AlignHCenter|Qt::AlignBottom
                                    : Qt::AlignCenter,
                            s,r);
            x = r.x();
            y = r.y();
            h = r.height();
            w = r.width();

            if (!topText)
            {
              QString maxText = progressMaxText(pb, opt);
              if (!maxText.isEmpty())
              {
                int textWidth = QFontMetrics(f).horizontalAdvance(maxText) + 6; // 3px space + margin
                if (isVertical)
                {
                  if (inverted)
                  {
                    r.adjust(0, 0, 0, -textWidth);
                    y += textWidth;
                  }
                  else
                    r.adjust(0, textWidth, 0, 0);
                  h = r.height();
                }
                else
                {
                  if (inverted)
                    r.adjust(textWidth, 0, 0, 0);
                  else
                    r.adjust(0, 0, -textWidth, 0);
                  w = r.width();
                }
              }
            }
          }
          else // thin progressbar without text
          {
            r = alignedRect(option->direction,Qt::AlignCenter,
                            isVertical
                              ? QSize(qMin(tspec_.progressbar_thickness, option->rect.width()),
                                      option->rect.height())
                              : QSize(option->rect.width(),
                                      qMin(tspec_.progressbar_thickness, option->rect.height())),
                            option->rect);
            if (!spreadProgressbar)
            {
              if (isVertical)
                r.adjust(fspecPr.top, fspecPr.right, -fspecPr.bottom, -fspecPr.left);
              else
                r.adjust(fspecPr.left, fspecPr.top, -fspecPr.right, -fspecPr.bottom);
            }
            x = r.x();
            y = r.y();
            h = r.height();
            w = r.width();
          }
        }
        else
        {
          /* when the space is more than enough (with an expanding vertical policy) */
          /*r = alignedRect(option->direction, Qt::AlignCenter,
                          isVertical
                            ? QSize(qMin(QFontMetrics(f).height()+fspecPr.top+fspecPr.bottom, option->rect.width()),
                                    option->rect.height())
                            : QSize(option->rect.width(),
                                    qMin(QFontMetrics(f).height()+fspecPr.top+fspecPr.bottom, option->rect.height())),
                          option->rect);
          if (!spreadProgressbar)
          {
            if (isVertical)
              r.adjust(fspecPr.top, fspecPr.right, -fspecPr.bottom, -fspecPr.left);
            else
              r.adjust(fspecPr.left, fspecPr.top, -fspecPr.right, -fspecPr.bottom);
          }*/
          x = r.x();
          y = r.y();
          h = r.height();
          w = r.width();
        }

        const QString group = "ProgressbarContents";
        frame_spec fspec = getFrameSpec(group);
        if (isKisSlider_)
        {
          //fspec.right = 0;
          fspec.isAttached = true;
          fspec.HPos = -1;
        }
        else
        {
          fspec.left = fspec.right = qMin(fspec.left,fspec.right); // as with Progressbar
        }
        const interior_spec ispec = getInteriorSpec(group);

        /* if the progressbar is rounded, its contents should be so too */
        bool isRounded = false;
        if (tspec_.vertical_spin_indicators && isKisSlider_)
        {
          fspec.left = qMin(fspec.left,3);
          fspec.right = qMin(fspec.right,3);
          fspec.top = qMin(fspec.top,3);
          fspec.bottom = qMin(fspec.bottom,3);
          if (hasExpandedBorder(fspec))
            fspec.expansion = 0;
          else
            fspec.expansion = qMin(fspec.expansion, LIMITED_EXPANSION);
        }
        else
        {
          fspec.expansion = (isKisSlider_
                               ? qMin(fspecPr.expansion,
                                      tspec_.inline_spin_indicators
                                        ? getFrameSpec(QStringLiteral("LineEdit")).expansion
                                        : getFrameSpec(QStringLiteral("IndicatorSpinBox")).expansion)
                               : fspecPr.expansion)
                            - (spreadProgressbar ? 0 : fspecPr.top+fspecPr.bottom);
          if (fspec.expansion >= qMin(h,w)) isRounded = true;
        }

        // after this, we could visualize horizontally...
        if (isVertical)
          r.setRect(y, x, h, w);

        bool thin = false;
        if (opt->maximum != 0 || opt->minimum != 0)
        {
          if (QObject *styleObject = option->styleObject)
          { // could only have ProgressbarAnimation
            if (Animation *animation = animations_.take(styleObject))
            {
              animation->stop();
              delete animation;
              animation = nullptr;
            }
          }

          int length = isVertical ? h : w;
          int empty = length
                      - sliderPositionFromValue(opt->minimum,
                                                opt->maximum,
                                                qMax(opt->progress,opt->minimum),
                                                length,
                                                false);
          if (isVertical)
          {
            if (inverted)
              r.adjust(empty,0,0,0);
            else
              r.adjust(0,0,-empty,0);
          }
          else
          {
            if (inverted)
              r.adjust(empty,0,0,0);
            else
              r.adjust(0,0,-empty,0);

          }

          // take care of thin indicators
          if (r.isValid())
          {
            if (isRounded)
            {
              if ((!isVertical && r.width() < h) || (isVertical && r.width() < w))
              {
                painter->save();
                painter->setClipRegion(r);
                if (isVertical)
                {
                  if (inverted)
                    r.adjust(r.width()-w,0,0,0);
                  else
                    r.setWidth(w);
                }
                else
                {
                  if (inverted)
                    r.adjust(r.width()-h,0,0,0);
                  else
                    r.setWidth(h);

                }
                thin = true;
              }
            }
            else if (r.width() < fspec.left+fspec.right)
            {
              painter->save();
              painter->setClipRegion(r);
              if (isVertical)
              {
                if (inverted)
                  r.adjust(r.width()-fspec.left-fspec.right,0,0,0);
                else
                  r.setWidth(fspec.left+fspec.right);
              }
              else
              {
                if (inverted)
                  r.adjust(r.width()-fspec.left-fspec.right,0,0,0);
                else
                  r.setWidth(fspec.left+fspec.right);

              }
              thin = true;
            }
          }
          if (r.height() >= 0 && r.height() < fspec.top+fspec.bottom)
          {
            fspec.top = fspec.bottom = r.height()/2;
          }
          renderFrame(painter,r,fspec,fspec.element+"-"+status,0,0,0,0,0,true);
          renderInterior(painter,r,fspec,ispec,ispec.element+"-"+status,true);
          if (thin)
            painter->restore();
        }
        else if (QObject *styleObject = option->styleObject)
        { // busy progressbar
          if (!r.isValid()) return;
          ProgressbarAnimation *anim = qobject_cast<ProgressbarAnimation *>(animations_.value(styleObject));
          if (!anim)
          {
            anim = new ProgressbarAnimation(styleObject);
            startAnimation(anim);
            return;
          }
          int animPixels = anim->pixels();
          int pm = qMin(qMax(pixelMetric(PM_ProgressBarChunkWidth),fspec.left+fspec.right),r.width()/2-2);
          if (pm > h
              && h >= fspec.left+fspec.right
              && h >= QFontMetrics(f).height())
          { // make it compact with ordinary progressbars
            pm = h;
          }
          QRect R = r.adjusted(animPixels,0,0,0);
          if (isVertical)
          {
            if (inverted)
              R.setX(r.x()+r.width()-(animPixels % r.width()));
            else
              R.setX(r.x()+(animPixels % r.width()));
          }
          else
          {
            if (inverted)
              R.setX(r.x()+r.width()-(animPixels % r.width()));
            else
              R.setX(r.x()+(animPixels % r.width()));
          }
          int W = !isRounded ? pm : !isVertical ? qMax(h,pm) : qMax(w,pm);
          if (W <= 0 || W > r.width()) return;
          R.setWidth(W);
          if (R.height() >= 0 && R.height() < fspec.top+fspec.bottom)
          {
            fspec.top = fspec.bottom = r.height()/2;
          }

          if (R.x()+R.width() > r.x()+r.width())
          {
            R.setWidth(r.x() + r.width() - R.x());

            // keep external corners rounded
            thin = false;
            QRect R1(R);
            if (R1.isValid())
            {
              if (isRounded)
              {
                if (R1.width() < (!isVertical ? h : w))
                {
                  painter->save();
                  painter->setClipRegion(R);
                  if (!isVertical)
                    R1.adjust(R.width()-h,0,0,0);
                  else
                    R1.adjust(R.width()-w,0,0,0);
                  thin = true;
                }
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

            R = QRect(r.x(), r.y(), W-R.width(), r.height());

            thin = false;
            if (R.isValid())
            {
              if (isRounded)
              {
                if (R.width() < (!isVertical ? h : w))
                {
                  painter->save();
                  painter->setClipRegion(R);
                  if (!isVertical)
                    R.setWidth(h);
                  else
                    R.setWidth(w);
                  thin = true;
                }
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
      }

      break;
    }

    case CE_ProgressBarLabel : {
      const QStyleOptionProgressBar *opt =
          qstyleoption_cast<const QStyleOptionProgressBar*>(option);

      if (opt && opt->textVisible)
      {
        QString txt = opt->text;
        if (txt.isEmpty()) break;

        frame_spec fspec;
        default_frame_spec(fspec);
        label_spec lspec = getLabelSpec(QStringLiteral("Progressbar"));
        lspec.left = lspec.right = lspec.top = lspec.bottom = 0;

        frame_spec fspecPr = getFrameSpec(QStringLiteral("Progressbar"));
        if (isKisSlider_)
          fspecPr.right = 0;
        else
        {
          fspecPr.left = fspecPr.right = qMin(fspecPr.left,fspecPr.right); // -> CE_ProgressBarContents
        }

        QFont f(painter->font());
        if (lspec.boldFont) f.setWeight(lspec.boldness);
        const QProgressBar *pb = qobject_cast<const QProgressBar*>(widget);
        bool isVertical(opt->orientation == Qt::Vertical);
        bool inverted(pb && pb->invertedAppearance());
        if ((!isVertical && !isKisSlider_ && option->direction == Qt::RightToLeft) // -> CE_ProgressBarGroove
            || (isVertical && opt->bottomToTop))
        {
          inverted = !inverted;
        }

        bool sideText(false);
        bool topText(false);
        if (!isKisSlider_ // KisSliderSpinBox doesn't obey thickness setting
            && !isKrita_
            && tspec_.progressbar_thickness > 0
            && QFontMetrics(f).height() > tspec_.progressbar_thickness)
        { // see if text can be outside progressbar
          if (isVertical)
            topText = (tspec_.progressbar_thickness + QFontMetrics(f).height()+3 <= w);
          else
            topText = (tspec_.progressbar_thickness + QFontMetrics(f).height()+3 <= h);
          sideText = !topText;
          if (topText || sideText)
          {
            QPalette palette = widget ? widget->palette() : standardPalette();
            if (enoughContrast(getFromRGBA(lspec.normalColor),
                               palette.color(QPalette::WindowText)))
            {
              lspec.normalColor = lspec.focusColor =
                getName(palette.color(QPalette::Active,QPalette::WindowText));
              lspec.normalInactiveColor = lspec.focusInactiveColor =
                getName(palette.color(QPalette::Inactive,QPalette::WindowText));
            }
            if (enoughContrast(getFromRGBA(lspec.toggleColor),
                               palette.color(QPalette::HighlightedText)))
            {
              lspec.toggleColor =
                getName(palette.color(QPalette::Active,QPalette::HighlightedText));
              lspec.toggleInactiveColor =
                getName(palette.color(QPalette::Inactive,QPalette::HighlightedText));
            }
          }
        }

        int length = w;
        QRect r = option->rect;
        if (isVertical)
        {
          length = h;
          r.setRect(y, x, h, w);
          QTransform m;
          if (!opt->bottomToTop)
          {
            m.translate(0, w+2*x); m.scale(1,-1);
          }
          else
          {
            m.translate(h+2*y, 0); m.scale(-1,1);
          }
          painter->setTransform(m, true);
        }
        if (sideText)
          length -= 6; // fixed 3px space + fixed 3px margin
        else if (!topText) // ordinary progressbar
        {
          length -= fspecPr.left + fspecPr.right;
          /* with interior bars, only if there remains enough vertical space for the text,
             consider top and bottom frame widths to move the text vertically when needed */
          if (!tspec_.spread_progressbar
              /* take care of bad designs (as in pavucontrol-qt; see CE_ProgressBarContents) */
              && (isVertical ? w : h) > fspecPr.top + fspecPr.bottom
              && QFontMetrics(f).height() <= r.height() - fspecPr.top - fspecPr.bottom)
          {
            if (isVertical && !opt->bottomToTop)
              r.adjust(0,fspecPr.bottom,0,-fspecPr.top);
            else
              r.adjust(0,fspecPr.top,0,-fspecPr.bottom);
          }
        }

        txt = QFontMetrics(f).elidedText(txt, Qt::ElideRight, length, Qt::TextShowMnemonic);

        bool isInactive(isWidgetInactive(widget));
        int state = option->state & State_Enabled ?
                      (option->state & State_Selected) && (topText || sideText) ? 4
                      : option->state & State_MouseOver ? 2 : 1 : 0;

        /* Either the bar has a selected state or it's a child of an itemview;
           otherwise, there will be no way to know about the selected state.
           We don't go deeper than the third child. */
        if ((topText || sideText) && state != 4)
        {
          if (QWidget *gp = getParent(widget,2))
          {
            QAbstractItemView *iv = nullptr;
            if ((iv = qobject_cast<QAbstractItemView*>(gp))
                || (iv = qobject_cast<QAbstractItemView*>(getParent(gp,1))))
            {
              if (QItemSelectionModel *sm = iv->selectionModel())
              {
                const QModelIndexList sList = sm->selectedIndexes();
                for (const QModelIndex &sIndex : sList)
                {
                  if (sIndex.isValid())
                  {
                    if (QWidget *iw = iv->indexWidget(sIndex))
                    {
                      if (iw->isAncestorOf(widget))
                      {
                        state = 4;
                        break;
                      }
                    }
                  }
                }
              }
            }
          }
        }
        if (state == 4)
        {
          lspec.toggleColor = getName(standardPalette().color(isInactive
                                                                ? QPalette::Inactive
                                                                : QPalette::Active,
                                                              QPalette::HighlightedText));
        }

        QRect R; // the indicator part
        if (!sideText && !topText)
        {
          /* with ordinary progressbars, we put the text inside the frame horizontally,
             hoping that it'll be completely inside the frame */
          r.setRect(r.left()+fspecPr.left, r.top(), length, r.height());

          /* find the part inside the indicator */
          QColor col = getFromRGBA(cspec_.progressIndicatorTextColor);
          if (col.isValid() && state != 0)
          {
            QColor txtCol;
            if (state == 1) txtCol = getFromRGBA(lspec.normalColor);
            else if (state == 2) txtCol = getFromRGBA(lspec.focusColor);
            else if (state == 4) txtCol = getFromRGBA(lspec.toggleColor);
            /* do nothing if the colors are the same */
            if ((!txtCol.isValid() || col != txtCol)
                && (txtCol.isValid() || col != standardPalette().color(QPalette::WindowText)))
            {
              /* find the empty part exactly as in CE_ProgressBarContents */
              int L, margin;
              if (tspec_.spread_progressbar || (isVertical ? w : h) <= fspecPr.top + fspecPr.bottom)
              {
                L = isVertical ? h : w;
                margin = (!isKisSlider_ || inverted ? fspecPr.left : 0);
              }
              else
              {
                L = r.width();
                margin = 0;
              }
              int empty = L
                          - sliderPositionFromValue(opt->minimum,
                                                    opt->maximum,
                                                    qMax(opt->progress,opt->minimum),
                                                    L,
                                                    false);
              empty = qMax(0, empty-margin);

              if (isVertical)
              {
                if (inverted)
                  R = r.adjusted(empty, 0, 0, 0);
                else
                  R = r.adjusted(0, 0, -empty, 0);
              }
              else
              {
                if (inverted)
                  R = r.adjusted(empty, 0, 0, 0);
                else
                  R = r.adjusted(0, 0, -empty, 0);
              }
            }
          }
        }

        if (R.isValid())
        {
          painter->save();
          painter->setClipRegion(QRegion(r).subtracted(QRegion(R)));
        }
        int talign;
        if (topText)
        {
          /*  when the space is more than enough (with an expanding vertical policy) */
          r = alignedRect(option->direction, Qt::AlignCenter,
                          QSize(r.width(), tspec_.progressbar_thickness + QFontMetrics(f).height()+3),
                          r);
          if (isVertical && opt->bottomToTop)
          {
            if (option->direction == Qt::RightToLeft)
            {
              r.adjust(0,0,-tspec_.progressbar_thickness,0);
              talign = Qt::AlignHCenter|Qt::AlignTop;
            }
            else
            {
              r.adjust(tspec_.progressbar_thickness,0,0,0);
              talign = Qt::AlignHCenter|Qt::AlignBottom;
            }
          }
          else
          {
            if (isVertical && option->direction == Qt::RightToLeft)
            {
              r.adjust(0,tspec_.progressbar_thickness,0,0);
              talign = Qt::AlignHCenter|Qt::AlignBottom;
            }
            else
            {
              r.adjust(0,0,0,-tspec_.progressbar_thickness);
              talign = Qt::AlignHCenter|Qt::AlignTop;
            }
          }
        }
        else if (sideText)
        {
          int hAlignment = Qt::AlignRight;
          if ((inverted && option->direction != Qt::RightToLeft)
              || (!inverted && option->direction == Qt::RightToLeft))
          {
            hAlignment = Qt::AlignLeft;
          }
          talign = hAlignment | Qt::AlignVCenter;
          /* consider a 3px margin */
          if (hAlignment == Qt::AlignRight)
            r.adjust(0,0,-3,0);
          else
            r.adjust(3,0,0,0);
        }
        else
          talign = Qt::AlignCenter;
        renderLabel(option,painter,
                    r,
                    fspec,lspec,
                    talign,txt,QPalette::WindowText,
                    state,
                    isInactive);
        if (R.isValid())
        {
          painter->restore();
          painter->save();
          painter->setClipRect(R);
          renderLabel(option,painter,
                      r,
                      fspec,lspec,
                      Qt::AlignCenter,txt,QPalette::WindowText,
                      -1,
                      isInactive);
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
      if (isWidgetInactive(widget))
        status.append("-inactive");

      QRect r = option->rect;
      /* we don't check State_Horizontal because it may
         lead to wrong results (like in Qt Designer) */
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
        status.replace(QLatin1String("disabled"),QLatin1String("normal"));
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
                                QSize(iW, qMin(dspec.size, qMax(w,h))),
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
      const indicator_spec dspec = getIndicatorSpec(QStringLiteral("Scrollbar"));

      QString iStatus = getState(option,widget); // indicator state
      if (option->state & State_Enabled)
      {
        const QStyleOptionSlider *opt = qstyleoption_cast<const QStyleOptionSlider*>(option);
        if (opt)
        {
          quint32 sc = QStyle::SC_ScrollBarAddLine;
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

          if (isWidgetInactive(widget)
              && !iStatus.endsWith(QLatin1String("-inactive")))
          {
            iStatus.append("-inactive");
          }
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
      /* no toggled state (especially good with transient scrollbars) */
      QString status = (option->state & State_Enabled) ?
                       (option->state & State_Sunken) ? "pressed" :
                       (option->state & State_Selected) ? "pressed" :
                       (option->state & State_MouseOver) ? "focused" : "normal"
                       : "disabled";
      if (isWidgetInactive(widget))
        status.append("-inactive");
      /*if (status.startsWith("focused")
          && isCursorOutsideWidget(widget)) // hover bug
      {
        status.replace("focused","normal");
      }*/
      QString sStatus = status; // slider state
      if (!tspec_.animate_states // focus on entering the groove only with animation
          && (option->state & State_Enabled))
      {
        const QStyleOptionSlider *opt = qstyleoption_cast<const QStyleOptionSlider*>(option);
        if (opt && opt->activeSubControls != QStyle::SC_ScrollBarSlider)
        {
          sStatus = "normal";
          if (isWidgetInactive(widget))
            sStatus = "normal-inactive";
        }
      }

      QRect r = option->rect;
      if (option->state & State_Horizontal)
      {
        /* the painter was saved at CC_ScrollBar,
           so no transformation here */
        r.setRect(y, x, h, w);
      }
      /* WARNING: We can't rely on pixelMetric() to know the extent
         because it may not have a fixed value for transient scrollbars,
         depending on whether its argument "widget" is null or not. */
      int space = r.width() - tspec_.scroll_width; // see PM_ScrollBarExtent
      if (space > 0)
      { // don't let a transient scrollbar overlap with the view frame
        QAbstractScrollArea *sa = qobject_cast<QAbstractScrollArea*>(getParent(widget,2));
        bool hasFrame(sa && (sa->frameStyle() & QFrame::StyledPanel)); // see Qt -> qabstractscrollarea.cpp
        if (option->direction == Qt::RightToLeft)
        {
          if (option->state & State_Horizontal)
          { // 90 degrees clockwise + horizontal mirroring
            if (hasFrame)
              r.adjust(0, space, -space, -space);
            else
              r.adjust(space, 0, 0, 0);
          }
          else
          {
            if (hasFrame)
              r.adjust(space, space, 0, -space);
            else
              r.adjust(0, 0, -space, 0);
          }
        }
        else
        {
          if (hasFrame)
            r.adjust(0, space, -space, -space);
          else
            r.adjust(space, 0, 0, 0);
        }
      }

      const QString group = "ScrollbarSlider";
      frame_spec fspec;
      interior_spec ispec;
      const indicator_spec dspec = getIndicatorSpec(group);

      if (option->styleObject && styleHint(SH_ScrollBar_Transient,option,widget)) // as in CC_ScrollBar
      { // use transient elements if they exist
        fspec = getFrameSpec(QStringLiteral("ScrollbarTransientSlider"));
        ispec = getInteriorSpec(QStringLiteral("ScrollbarTransientSlider"));
        if (!fspec.hasFrame && !ispec.hasInterior)
        {
          fspec = getFrameSpec(group);
          ispec = getInteriorSpec(group);
        }
      }
      else
      {
        fspec = getFrameSpec(group);
        ispec = getInteriorSpec(group);
      }

      fspec.expansion = 0; // no need to frame expansion because the thickness is known

      if (!(option->state & State_Enabled))
      {
        sStatus.replace(QLatin1String("disabled"),QLatin1String("normal"));
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      bool canAnimate(opacityTimer_
                      && widget && widget->isEnabled() && animatedWidget_ == widget
                      && !qobject_cast<const QAbstractScrollArea*>(widget));
      bool animate(canAnimate && opacityTimer_->isActive());
      if (animate)
      {
        qreal opacity = painter->opacity();
        if (animationStartState_ == sStatus)
          animationOpacity_ = 100;
        else if (animationOpacity_ < 100)
        {
          renderFrame(painter,r,fspec,fspec.element+"-"+animationStartState_);
          renderInterior(painter,r,fspec,ispec,ispec.element+"-"+animationStartState_);
        }
        painter->save();
        painter->setOpacity(qMin(static_cast<qreal>(animationOpacity_)/100.0, opacity));
      }
      renderFrame(painter,r,fspec,fspec.element+"-"+sStatus);
      renderInterior(painter,r,fspec,ispec,ispec.element+"-"+sStatus);
      if (animate)
      {
        painter->restore();
        if (animationOpacity_ >= 100)
          animationStartState_ = sStatus;
      }
      else if (canAnimate)
        animationStartState_ = sStatus;
      QSize indicatorSize(r.width() - fspec.left-fspec.right,
                          qMin(dspec.size, r.height() - fspec.top-fspec.bottom));
      if (tspec_.center_scrollbar_indicator)
      {
        int squareW = qMin(qMin(dspec.size, indicatorSize.width()), indicatorSize.height());
        indicatorSize = QSize(squareW,squareW);
      }
      renderElement(painter,
                    dspec.element+"-"+status, // let the grip change on mouse-over for the whole scrollbar
                    alignedRect(option->direction,
                                Qt::AlignCenter,
                                indicatorSize,
                                r));
      if (!(option->state & State_Enabled))
        painter->restore();

      break;
    }

    case CE_HeaderEmptyArea:
      if (QAbstractItemView *iv = qobject_cast<QAbstractItemView*>(getParent(widget,1)))
      {
        if (iv->viewport() && iv->viewport()->autoFillBackground())
        {
          painter->fillRect(option->rect, option->palette.brush(iv->viewport()->backgroundRole()));
          break;
        }
      }
      painter->fillRect(option->rect, option->palette.window());
      break;

    case CE_HeaderSection : {
      /* WARNING: There is an issue in Qt5, which didn't exist in Qt4: The horizontal
                  position is always from left to right, so that, for example,
                  "QStyleOptionHeader::Beginning" is the leftmost section with RTL
                  too. This isn't logical but simplifies the calculation a lot. */
      const QString group = "HeaderSection";
      frame_spec fspec = getFrameSpec(group);
      const interior_spec ispec = getInteriorSpec(group);
      bool rtl(option->direction == Qt::RightToLeft);
      bool stretched(false);
      if (const QHeaderView *hv = qobject_cast<const QHeaderView*>(widget))
        stretched = hv-> stretchLastSection();

      bool horiz = true;
      QRect sep;
      if (const QStyleOptionHeader *opt = qstyleoption_cast<const QStyleOptionHeader*>(option))
      {
        if (opt->orientation != Qt::Horizontal) horiz = false;
        switch (opt->position) {
          case QStyleOptionHeader::End:
            fspec.isAttached = true;
            fspec.HPos = tspec_.spread_header && (stretched || (rtl && horiz)) ? 0 : 1;
            break;
          case QStyleOptionHeader::Beginning:
          case QStyleOptionHeader::Middle:
            fspec.isAttached = true;
            if (opt->position == QStyleOptionHeader::Beginning)
              fspec.HPos = tspec_.spread_header && (!(rtl && horiz)|| stretched) ? 0 : -1;
            else
              fspec.HPos = 0;
            if (horiz)
            {
              sep.setRect(x+w-fspec.right,
                          y+fspec.top,
                          fspec.right,
                          h-fspec.top-fspec.bottom);
            }
            else
            {
              sep.setRect(x+fspec.top, // -> CT_HeaderSection
                          y+h-fspec.right,
                          w-fspec.top-fspec.bottom,
                          fspec.right);
            }
            break;
          case QStyleOptionHeader::OnlyOneSection:
            if (tspec_.spread_header)
            {
              fspec.isAttached = true;
              fspec.HPos = stretched ? 0 : rtl ? -1 : 1;
            }
            break;
          default: break;
        }
      }

      if (tspec_.spread_header)
      {
        fspec.isAttached = true;
        if (horiz || !rtl)
          fspec.VPos = 1;
        else
          fspec.VPos = -1;
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
      if (QObject *styleObject = option->styleObject)
      {
        if (status.startsWith(QLatin1String("focused"))
            && isCursorOutsideRect(widget, option->rect))
        { // hover bug
          styleObject->setProperty("_kv_hover_bug", true);
          status.replace(QLatin1String("focused"),QLatin1String("normal"));
        }
        else
          styleObject->setProperty("_kv_hover_bug", QVariant());
      }
      if (!(option->state & State_Enabled))
      {
        status.replace(QLatin1String("disabled"),QLatin1String("normal"));
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }
      /* for elegance */
      /*if (r.height() < 2)
        fspec.expansion = 0;
      else
        fspec.expansion = qMin(fspec.expansion,r.height()/2);*/
      fspec.expansion = 0; // vertical headers have variable heights

      bool libreoffice = false;

      /* merge the background of the header with that of its parent view, if any */
      if (QAbstractItemView *iv = qobject_cast<QAbstractItemView*>(getParent(widget,1)))
      {
        if (iv->viewport() && iv->viewport()->autoFillBackground())
          painter->fillRect(r, option->palette.brush(iv->viewport()->backgroundRole()));
      }
      else if (isLibreoffice_ && widget == nullptr && (option->state & State_Enabled))
      {
        const label_spec lspec = getLabelSpec("HeaderSection");
        if ((status.startsWith(QLatin1String("normal"))
             && enoughContrast(getFromRGBA(lspec.normalColor), standardPalette().color(QPalette::ButtonText)))
            || (status.startsWith(QLatin1String("toggled"))
                && enoughContrast(getFromRGBA(lspec.toggleColor), standardPalette().color(QPalette::ButtonText))))
        {
          libreoffice = true;
          painter->fillRect(option->rect, standardPalette().brush(QPalette::Window));
          painter->save();
          painter->setOpacity(0.5);
        }
      }

      renderFrame(painter,r,fspec,fspec.element+"-"+status,0,0,0,0,0,true);
      renderInterior(painter,r,fspec,ispec,ispec.element+"-"+status,true);
      /* if there's no header separator, use the right frame */
      if (themeRndr_ && themeRndr_->isValid() && !elementExists(QStringLiteral("header-separator")))
        renderElement(painter, fspec.element + "-" + status + "-right", sep);
      else
        renderElement(painter,QStringLiteral("header-separator"),sep);
      if (!(option->state & State_Enabled))
        painter->restore();
      if (!horiz)
        painter->restore();
      if (libreoffice)
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
          //int t = fspec.left;
          fspec.left = fspec.top;
          /*fspec.top = t;
          t = fspec.right;*/
          fspec.right = fspec.bottom;
          //fspec.bottom = t;
          /* unfortunately, vertical headers don't obey CT_HeaderSection vertically */
          fspec.top = fspec.bottom = lspec.top = lspec.bottom = 0;
        }
        if (opt->position == QStyleOptionHeader::End || opt->position == QStyleOptionHeader::Middle)
        {
          if (opt->orientation == Qt::Horizontal)
            fspec.left = 0;
          else
            fspec.top = 0;
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
          if (opt->icon.isNull())
          {
            lspec.right = lspec.left = 0;
          }
          else
          {
            if (rtl)
              fspec.left = 0;
            else
              lspec.right = 0;
          }
        }
        if (opt->sortIndicator != QStyleOptionHeader::None)
        { // the frame is taken care of at SE_HeaderArrow
          if (rtl)
            fspec.left = 0;
          else
            fspec.right = 0;
        }

        int smallIconSize = pixelMetric(PM_SmallIconSize);

        /* for thin horizontal headers, like in Dolphin's details view */
        if (opt->orientation == Qt::Horizontal)
        {
          QFont f;
          if (widget) f = widget->font();
          else f = QApplication::font();
          if (lspec.boldFont) f.setWeight(lspec.boldness);
          int contentsH = QFontMetrics(f).height();
          if (!opt->icon.isNull())
            contentsH = qMax(contentsH, smallIconSize);
          if (h < contentsH + fspec.top + fspec.bottom + lspec.top + lspec.bottom)
          {
            fspec.top = fspec.bottom = lspec.top = lspec.bottom = 0;
          }
        }

        QString status = getState(option,widget);
        int state = 1;
        if (!(option->state & State_Enabled))
          state = 0;
        else if (status.startsWith(QLatin1String("pressed")))
          state = 3;
        else if (status.startsWith(QLatin1String("toggled")))
          state = 4;
        else if (status.startsWith(QLatin1String("focused")))
        {
          if (option->styleObject == nullptr
              || !option->styleObject->property("_kv_hover_bug").toBool()) // hover bug
          {
            state = 2;
          }
        }

        QSize iconSize = QSize(smallIconSize,smallIconSize);
        bool isInactive(status.contains(QLatin1String("-inactive")));
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
                    isInactive,
                    getPixmapFromIcon(opt->icon,getIconMode(state,isInactive,lspec),iconstate,iconSize),
                    iconSize);
      }

      break;
    }

    case CE_ToolBar : {
      /* QCommonStyle checks for QStyleOptionToolBar but we don't need it.
         Moreover, we can't check for it in getStylableToolbarContainer(). */

      QRect r = option->rect;

      /* update the menubar if needed */
      bool stylable(isStylableToolbar(widget));
      int hPos = 2;
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
                if (mb->width() != widget->width())
                {
                  if (mb->x() != widget->x())
                  {
                    if (mb->x() + mb->width() != widget->x() + widget->width())
                      hPos = 0;
                    else
                      hPos = 1;
                  }
                  else hPos = -1;
                }
              }
            }
          }
        }
      }
      const QString group = "Toolbar";
      if (!stylable)
      {
        if (widget && option->styleObject)
        { // restore or correct foreground colors if needed (see the case of stylable toolbar below)
          QString toolbarState = option->styleObject->property("_kv_toolbar").toString();
          if (toolbarState.isEmpty() // no child palette checked
              || toolbarState == "styled") // the toolbar was styled before
          {
            QColor txtCol = standardPalette().color(QPalette::Active, QPalette::Text);
            QColor toolbarTxtCol(getFromRGBA(getLabelSpec(group).normalColor));

            if (enoughContrast(txtCol, toolbarTxtCol))
            {
              bool toolbarComboBox(!getFrameSpec(QStringLiteral("ToolbarComboBox")).element.isEmpty()
                                   || !getInteriorSpec(QStringLiteral("ToolbarComboBox")).element.isEmpty());

              const QList<QWidget*> children = widget->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
              for (QWidget *child : children)
              {
                if (widget != child->parentWidget() // not inside another window
                    || child->isWindow()) // not another window (like a menu)
                {
                  continue;
                }
                QPalette palette = child->palette();
                if (!qobject_cast<QToolButton*>(child)
                    && (!toolbarComboBox || !qobject_cast<QComboBox*>(child))
                    && palette.color(QPalette::Active, QPalette::WindowText) == toolbarTxtCol)
                {
                  palette.setColor(QPalette::Active, QPalette::WindowText,
                                   standardPalette().color(QPalette::Active,QPalette::WindowText));
                  palette.setColor(QPalette::Inactive, QPalette::WindowText,
                                   standardPalette().color(QPalette::Inactive,QPalette::WindowText));
                  palette.setColor(QPalette::Disabled, QPalette::WindowText,
                                   standardPalette().color(QPalette::Disabled,QPalette::WindowText));
                  if (qobject_cast<QLabel*>(child))
                  {
                    palette.setColor(QPalette::Active, QPalette::Text, txtCol);
                    palette.setColor(QPalette::Inactive, QPalette::Text,
                                     standardPalette().color(QPalette::Inactive, QPalette::Text));
                    palette.setColor(QPalette::Disabled, QPalette::Text,
                                     standardPalette().color(QPalette::Disabled, QPalette::Text));
                    palette.setColor(QPalette::PlaceholderText,
                                     standardPalette().color(QPalette::PlaceholderText));
                    /* see the case of stylable toolbar for the reason */
                    palette.setColor(QPalette::Active, QPalette::ButtonText,
                                     standardPalette().color(QPalette::Active,QPalette::WindowText));
                    palette.setColor(QPalette::Inactive, QPalette::ButtonText,
                                     standardPalette().color(QPalette::Inactive,QPalette::WindowText));
                    palette.setColor(QPalette::Disabled, QPalette::ButtonText,
                                     standardPalette().color(QPalette::Disabled,QPalette::WindowText));
                  }
                  else
                  {
                    palette.setColor(QPalette::Active, QPalette::ButtonText,
                                     standardPalette().color(QPalette::Active,QPalette::ButtonText));
                    palette.setColor(QPalette::Inactive, QPalette::ButtonText,
                                     standardPalette().color(QPalette::Inactive,QPalette::ButtonText));
                    palette.setColor(QPalette::Disabled, QPalette::ButtonText,
                                     standardPalette().color(QPalette::Disabled,QPalette::ButtonText));
                  }
                  forcePalette(child, palette);
                }
              }

              if (toolbarComboBox)
              {
                QColor comboTxtCol = getFromRGBA(getLabelSpec(QStringLiteral("ComboBox")).normalColor);
                if (comboTxtCol.isValid())
                {
                  QColor comboDisabledTxtCol = comboTxtCol;
                  comboDisabledTxtCol.setAlpha(102); // 0.4 * comboDisabledTxtCol.alpha()
                  const QList<QComboBox*> combos = widget->findChildren<QComboBox*>();
                  for (QComboBox *cb : combos)
                  {
                    if (!widget->isAncestorOf(cb)) continue; // not inside another window
                    QPalette palette = cb->palette();
                    if (comboTxtCol != palette.color(QPalette::ButtonText))
                    {
                      palette.setColor(QPalette::ButtonText, comboTxtCol);
                      palette.setColor(QPalette::WindowText, comboTxtCol);
                      palette.setColor(QPalette::Disabled, QPalette::ButtonText, comboDisabledTxtCol);
                      palette.setColor(QPalette::Disabled, QPalette::WindowText, comboDisabledTxtCol);
                      QColor baseCol = palette.color(QPalette::Active, QPalette::Base);
                      if (baseCol.alpha() < 255
                          && baseCol == standardPalette().color(QPalette::Active, QPalette::Base))
                      { // it was opacified for QComboBoxPrivateContainer in "Style::polish()"
                        QColor winCol = standardPalette().color(QPalette::Active, QPalette::Window);
                        winCol.setAlpha(255);
                        baseCol = overlayColor(winCol, baseCol);
                        palette.setColor(QPalette::Base, baseCol);
                      }
                      forcePalette(cb, palette);
                    }
                    else break; // all or nothing
                  }
                }
              }

              if (!getFrameSpec(QStringLiteral("ToolbarLineEdit")).element.isEmpty()
                  || !getInteriorSpec(QStringLiteral("ToolbarLineEdit")).element.isEmpty())
              {
                const QList<QLineEdit*> lineEdits = widget->findChildren<QLineEdit*>();
                for (QLineEdit *le : lineEdits)
                {
                  if (!widget->isAncestorOf(le)) continue;
                  QPalette palette = le->palette();
                  if (txtCol != palette.color(QPalette::Active, QPalette::Text))
                  {
                    palette.setColor(QPalette::Active, QPalette::Text, txtCol);
                    palette.setColor(QPalette::Inactive, QPalette::Text,
                                     standardPalette().color(QPalette::Inactive, QPalette::Text));
                    palette.setColor(QPalette::Disabled, QPalette::Text,
                                     standardPalette().color(QPalette::Disabled, QPalette::Text));
                    palette.setColor(QPalette::PlaceholderText,
                                     standardPalette().color(QPalette::PlaceholderText));
                    forcePalette(le, palette);
                  }
                  /* if this line-edit is inside a combo, its palette is
                     already set but its icon still needs to be updated */
                  if (QAction *clearAction = le->findChild<QAction*>(QLatin1String("_q_qlineeditclearaction")))
                    clearAction->setIcon(standardIcon(QStyle::SP_LineEditClearButton, nullptr, le));
                  /* all line-edits should be checked because some may be inside combos */
                }
              }
            }
            else
            {
              const QList<QLabel*> labels = widget->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly);
              for (QLabel *label : labels)
              {
                if (widget != label->parentWidget()) continue;
                QPalette palette = label->palette();
                palette.setColor(QPalette::Active, QPalette::ButtonText,
                                 standardPalette().color(QPalette::Active,QPalette::WindowText));
                palette.setColor(QPalette::Inactive, QPalette::ButtonText,
                                 standardPalette().color(QPalette::Inactive,QPalette::WindowText));
                palette.setColor(QPalette::Disabled, QPalette::ButtonText,
                                 standardPalette().color(QPalette::Disabled,QPalette::WindowText));
                forcePalette(label, palette);
              }
            }

            option->styleObject->setProperty("_kv_toolbar", "unstyled");
          }
        }
        break;
      }

      if (!(option->state & State_Horizontal))
      { // shouldn't happen because vertical toolbars aren't stylable
        r.setRect(0, 0, h, w);
        painter->save();
        QTransform m;
        m.scale(1,-1);
        m.rotate(-90);
        painter->setTransform(m, true);
      }

      frame_spec fspec = getFrameSpec(group);
      interior_spec ispec = getInteriorSpec(group);
      if (hPos != 2)
      {
        fspec.isAttached = true;
        fspec.HPos = hPos;
      }
      if (!widget) // WARNING: QML has anchoring!
      {
        fspec.expansion = 0;
        ispec.px = ispec.py = 0;
      }
      else if (option->styleObject)
      { // correct foreground colors if needed
        QString toolbarState = option->styleObject->property("_kv_toolbar").toString();
        if (toolbarState.isEmpty() // no child palette checked
            || toolbarState == "unstyled") // the toolbar wasn't styled before
        {
          const label_spec tlspec = getLabelSpec(group);
          QColor txtCol = getFromRGBA(tlspec.normalColor);

          if (txtCol.isValid())
          {
            QColor inactiveTxtCol;
            if (tspec_.no_inactiveness)
              inactiveTxtCol = txtCol;
            else
            {
              inactiveTxtCol = getFromRGBA(tlspec.normalInactiveColor);
              if (!inactiveTxtCol.isValid()) inactiveTxtCol = txtCol;
            }
            QColor disabledTxtCol = txtCol;
            disabledTxtCol.setAlpha(102); // 0.4 * disabledTxtCol.alpha()
            QColor pTxtCol = txtCol; // placeholder
            pTxtCol.setAlpha(128);

            /* NOTE: Only the active colors are compared, which means that the active and
                     inactive toolbars shouldn't have a hight contrast with each other. */
            if (enoughContrast(standardPalette().color(QPalette::Active,QPalette::Text), txtCol))
            {
              bool toolbarComboBox(!getFrameSpec(QStringLiteral("ToolbarComboBox")).element.isEmpty()
                                   || !getInteriorSpec(QStringLiteral("ToolbarComboBox")).element.isEmpty());

              const QList<QWidget*> children = widget->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
              for (QWidget *child : children)
              {
                if (widget != child->parentWidget() || child->isWindow())
                  continue;
                /* this is only a workaround for a new bug in Audacious 4.0 but is
                   harmless in other places because the whole stylesheet is checked */
                if (child->styleSheet() == QStringLiteral("font-weight: bold"))
                {
                  child->setStyleSheet(QString());
                  QFont f = widget->font();
                  f.setWeight(QFont::Bold);
                  child->setFont(f);
                }

                QPalette palette = child->palette();
                if (!qobject_cast<QToolButton*>(child) // flat toolbuttons are dealt with at CE_ToolButtonLabel
                    && (!toolbarComboBox || !qobject_cast<QComboBox*>(child)) // handled below
                    && palette.color(QPalette::Active, QPalette::WindowText) != txtCol)
                { // like labels on a stylable toolbar (as in Audacious), or in K3b or Amarok
                  palette.setColor(QPalette::Active, QPalette::WindowText, txtCol);
                  palette.setColor(QPalette::Inactive, QPalette::WindowText, inactiveTxtCol);
                  palette.setColor(QPalette::Disabled, QPalette::WindowText, disabledTxtCol);
                  palette.setColor(QPalette::Active, QPalette::ButtonText, txtCol);
                  palette.setColor(QPalette::Inactive, QPalette::ButtonText, inactiveTxtCol);
                  palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledTxtCol);
                  if (qobject_cast<QLabel*>(child))
                  {
                    palette.setColor(QPalette::Active, QPalette::Text, txtCol);
                    palette.setColor(QPalette::Inactive, QPalette::Text, inactiveTxtCol);
                    palette.setColor(QPalette::Disabled, QPalette::Text, disabledTxtCol);
                    palette.setColor(QPalette::PlaceholderText, pTxtCol);
                  }
                  forcePalette(child, palette);
                }
              }

              if (toolbarComboBox)
              {
                QColor comboTxtCol = getFromRGBA(getLabelSpec(QStringLiteral("ToolbarComboBox")).normalColor);
                if (comboTxtCol.isValid())
                {
                  QColor comboDisabledTxtCol = comboTxtCol;
                  comboDisabledTxtCol.setAlpha(102); // 0.4 * comboDisabledTxtCol.alpha()
                  const QList<QComboBox*> combos = widget->findChildren<QComboBox*>();
                  for (QComboBox *cb : combos)
                  {
                    if (!widget->isAncestorOf(cb)) continue;
                    QPalette palette = cb->palette();
                    if (comboTxtCol != palette.color(QPalette::ButtonText))
                    {
                      palette.setColor(QPalette::ButtonText, comboTxtCol);
                      palette.setColor(QPalette::WindowText, comboTxtCol);
                      palette.setColor(QPalette::Disabled,QPalette::ButtonText, comboDisabledTxtCol);
                      palette.setColor(QPalette::Disabled,QPalette::WindowText, comboDisabledTxtCol);
                      QColor baseCol = palette.color(QPalette::Active, QPalette::Base);
                      if (baseCol.alpha() < 255
                          && baseCol == standardPalette().color(QPalette::Active, QPalette::Base))
                      {
                        QColor winCol = standardPalette().color(QPalette::Active, QPalette::Window);
                        winCol.setAlpha(255);
                        baseCol = overlayColor(winCol, baseCol);
                        palette.setColor(QPalette::Base, baseCol);
                      }
                      forcePalette(cb, palette);
                    }
                    else break;
                  }
                }
              }

              if (!getFrameSpec(QStringLiteral("ToolbarLineEdit")).element.isEmpty()
                  || !getInteriorSpec(QStringLiteral("ToolbarLineEdit")).element.isEmpty())
              {
                const QList<QLineEdit*> lineEdits = widget->findChildren<QLineEdit*>();
                for (QLineEdit *le : lineEdits)
                {
                  if (!widget->isAncestorOf(le)) continue;
                  QPalette palette = le->palette();
                  if (palette.color(QPalette::Active, QPalette::Text) != txtCol)
                  {
                    palette.setColor(QPalette::Active, QPalette::Text, txtCol);
                    palette.setColor(QPalette::Inactive, QPalette::Text, inactiveTxtCol);
                    palette.setColor(QPalette::Disabled, QPalette::Text, disabledTxtCol);
                    palette.setColor(QPalette::PlaceholderText, pTxtCol);
                    forcePalette(le, palette);
                    /* also correct the color of the symbolic clear icon */
                    if (QAction *clearAction = le->findChild<QAction*>(QLatin1String("_q_qlineeditclearaction")))
                      clearAction->setIcon(standardIcon(QStyle::SP_LineEditClearButton, nullptr, le));
                  }
                }
              }
            }
            else
            { // on toolbars, labels get the button text color; that's corrected here
              const QList<QLabel*> labels = widget->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly);
              for (QLabel *label : labels)
              {
                if (widget != label->parentWidget()) continue;
                QPalette palette = label->palette();
                palette.setColor(QPalette::Active, QPalette::ButtonText, txtCol);
                palette.setColor(QPalette::Inactive, QPalette::ButtonText, inactiveTxtCol);
                palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledTxtCol);
                forcePalette(label, palette);
              }
            }
          }

          option->styleObject->setProperty("_kv_toolbar", "styled");
        }
      }

      /*if (!(option->state & State_Enabled))
      {
        painter->save();
        painter->setOpacity(DISABLED_OPACITY);
      }*/
      QString suffix = "-normal";
      if (isWidgetInactive(widget))
        suffix = "-normal-inactive";

      renderFrame(painter,r,fspec,fspec.element+suffix);
      renderInterior(painter,r,fspec,ispec,ispec.element+suffix);
      /*if (!(option->state & State_Enabled))
        painter->restore();*/
      if (!(option->state & State_Horizontal))
        painter->restore();

      break;
    }

    case CE_SizeGrip : {
      const indicator_spec dspec = getIndicatorSpec(QStringLiteral("SizeGrip"));
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
          connect(widget, &QObject::destroyed, this, &Style::removeFromSet, Qt::UniqueConnection);
        }
        drawControl(QStyle::CE_PushButtonBevel, opt, painter, widget);
        QStyleOptionButton subopt(*opt);
        subopt.rect = subElementRect(SE_PushButtonContents, opt, widget);
        drawControl(QStyle::CE_PushButtonLabel, &subopt, painter, widget);
      }

      break;
    }

    case CE_PushButtonLabel : {
      if ((option->state & State_Enabled) && widget == nullptr && option != nullptr
          && enoughContrast(option->palette.color(QPalette::ButtonText),
                            standardPalette().color(QPalette::ButtonText)))
      { // QML colorized button
        QCommonStyle::drawControl(element,option,painter,widget);
        break;
      }

      const QStyleOptionButton *opt =
          qstyleoption_cast<const QStyleOptionButton*>(option);

      if (opt) {
        const QString group = "PanelButtonCommand";
        frame_spec fspec = getFrameSpec(group);
        const indicator_spec dspec = getIndicatorSpec(group);
        label_spec lspec = getLabelSpec(group);
        QWidget *p = getParent(widget,1);

        if (isPlasma_ && widget && widget->window()->testAttribute(Qt::WA_NoSystemBackground))
        {
          lspec.left = lspec.right = lspec.top = lspec.bottom = 0;
          fspec.left = fspec.right = fspec.top = fspec.bottom = 0;
        }
        else if (QTabWidget *tw = qobject_cast<QTabWidget*>(p))
        { // tab corner widget
          if (opt->text.isEmpty()
              && (tw->cornerWidget(Qt::TopLeftCorner) == widget
                  || tw->cornerWidget(Qt::TopRightCorner) == widget
                  || tw->cornerWidget(Qt::BottomLeftCorner) == widget
                  || tw->cornerWidget(Qt::BottomRightCorner) == widget))
          {
            fspec.left = fspec.right = fspec.top = fspec.bottom = 1;
          }
        }
        else if (qobject_cast<QAbstractItemView*>(getParent(p,1)))
        {
          lspec.left = lspec.right = lspec.top = lspec.bottom = qMin(lspec.left,2);
          fspec.left = qMin(fspec.left,3);
          fspec.right = qMin(fspec.right,3);
          fspec.top = qMin(fspec.top,3);
          fspec.bottom = qMin(fspec.bottom,3);
          lspec.tispace = qMin(lspec.tispace,3);
        }

        const QPushButton *pb = qobject_cast<const QPushButton*>(widget);
        bool isDefaultBtn(false);
        if (!hspec_.normal_default_pushbutton
            && (option->state & State_Enabled) && pb && pb->isDefault())
        {
          isDefaultBtn = true;
          QFont f(pb->font());
          f.setWeight(lspec.boldness);
          painter->save();
          painter->setFont(f);
        }

        /* take into account the possibility of the presence of an indicator */
        int ind = opt->features & QStyleOptionButton::HasMenu
                    ? dspec.size+BUTTON_ARROW_MARGIN : 0;
        QRect r = option->rect.adjusted((opt->direction == Qt::RightToLeft ? ind : 0),
                                        0,
                                        -(opt->direction == Qt::RightToLeft ? 0 : ind),
                                        0);
        QString status = getState(option,widget);
        if (status.startsWith(QLatin1String("toggled")) || status.startsWith(QLatin1String("pressed")))
        {
          int hShift = pixelMetric(PM_ButtonShiftHorizontal);
          int vShift = pixelMetric(PM_ButtonShiftVertical);
          r = r.adjusted(hShift,vShift,hShift,vShift);
        }

        QSize txtSize;
        QString txt = opt->text;
        if (!txt.isEmpty())
        {
          /* in case there isn't enough space */
          QFont fnt(painter->font());
          if ((opt->features & QStyleOptionButton::AutoDefaultButton) || lspec.boldFont)
            fnt.setWeight(lspec.boldness);
          txtSize = textSize(fnt,txt);
          QSize availableSize = r.size()
                                - QSize(opt->icon.isNull() ? 0 : opt->iconSize.width()+lspec.tispace, 0)
                                - QSize(fspec.left+fspec.right+lspec.left+lspec.right,
                                        fspec.top+fspec.bottom+lspec.top+lspec.bottom);
          if (txtSize.height() > availableSize.height())
          {
            lspec.top = lspec.bottom = 0;
            fspec.top = qMin(fspec.top,3);
            fspec.bottom = qMin(fspec.bottom,3);
            lspec.tispace = qMin(lspec.tispace,3);
            lspec.boldFont = false;
          }
          if (txtSize.width() > availableSize.width())
          {
            lspec.left = lspec.right = qMin(lspec.left,2);
            fspec.left = qMin(fspec.left,3);
            fspec.right = qMin(fspec.right,3);
            lspec.tispace = qMin(lspec.tispace,3);
            lspec.boldFont = false;
            /* elide the text if the space still isn't enough */
            int availableWidth = r.width()
                                 - (opt->icon.isNull() ? 0 : opt->iconSize.width()+lspec.tispace)
                                 - fspec.left-fspec.right-lspec.left-lspec.right;
            txt = makeTextElided(QFontMetrics(painter->font()), txt, availableWidth);
          }
        }

        int talign = Qt::AlignHCenter | Qt::AlignVCenter;
        if (!styleHint(SH_UnderlineShortcut, opt, widget))
          talign |= Qt::TextHideMnemonic;
        else
          talign |= Qt::TextShowMnemonic;

        int state = 1;
        if (!(option->state & State_Enabled))
          state = 0;
        else if (status.startsWith(QLatin1String("pressed")))
          state = 3;
        else if (status.startsWith(QLatin1String("toggled")))
          state = 4;
        else if (option->state & State_MouseOver)
        {
          if (option->styleObject == nullptr
              || !option->styleObject->property("_kv_hover_bug").toBool()) // hover bug
          {
            state = 2;
          }
        }

        if (opt->features & QStyleOptionButton::Flat) // respect the text color of the parent widget
        {
          QPalette pal = widget ? widget->palette() : standardPalette();
          lspec.normalColor = getName(pal.color(QPalette::Active,QPalette::WindowText));
          lspec.normalInactiveColor = getName(pal.color(QPalette::Inactive,QPalette::WindowText));
        }

        QStyleOptionButton o(*opt);
        if (!btnDragInProgress() && (option->state & State_MouseOver) && state != 2)
          o.state = o.state & ~QStyle::State_MouseOver; // hover bug

        bool isInactive(status.contains(QLatin1String("-inactive")));
        QColor disabledCol;
        if (state == 0
            && ((option->state & State_On)
                || (!(option->state & State_Sunken) && (option->state & State_Selected))))
        {
          if (isInactive)
            disabledCol = getFromRGBA(lspec.toggleInactiveColor);
          if (!disabledCol.isValid())
            disabledCol = getFromRGBA(lspec.toggleColor);
          if (disabledCol.isValid())
          {
            disabledCol.setAlpha(102); // 0.4 * disabledCol.alpha()
            QPalette palette(opt->palette);
            palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledCol);
            o.palette = palette;
          }
        }

        /* center text+icon */
        QRect R = r;
        if (!txtSize.isEmpty() && !opt->icon.isNull())
        {
          int margin = (r.width() - txtSize.width() - opt->iconSize.width()
                        - fspec.left - fspec.right - lspec.left - lspec.right - lspec.tispace
                        - (lspec.hasShadow ? qMax(qAbs(lspec.xshift)-1,0)+qMax(lspec.depth-1,0) : 0)) / 2;
          if (margin > 0)
            R.adjust(margin, 0, -margin, 0);
        }

        renderLabel(&o,painter,
                    R,
                    fspec,lspec,
                    talign,txt,QPalette::ButtonText,
                    state,
                    isInactive,
                    (hspec_.iconless_pushbutton && !txt.isEmpty()) ? QPixmap()
                      : getPixmapFromIcon(opt->icon,
                                          getIconMode(disabledCol.isValid()
                                                      && !enoughContrast(disabledCol,
                                                                         standardPalette().color(QPalette::Window))
                                                        ? -1 : state,
                                                      isInactive, lspec),
                                          iconstate, opt->iconSize),
                    opt->iconSize);
        if(isDefaultBtn)
          painter->restore();
      }

      break;
    }

    case CE_PushButtonBevel : { // bevel and indicator
      const QStyleOptionButton *opt = qstyleoption_cast<const QStyleOptionButton*>(option);
      const QPushButton *pb = qobject_cast<const QPushButton*>(widget);
      if (widget != nullptr)
      {
        if ((option->state & State_Sunken)
            && ((opt && (opt->features & QStyleOptionButton::HasMenu))
                || (pb && pb->menu())))
        {
          sunkenButton_ = const_cast<QWidget*>(widget);
        }
        else if (sunkenButton_.data() == widget)
          sunkenButton_.clear();
      }

      if (opt) {
        QObject *styleObject = option->styleObject;

        QString status = getState(option,widget);
        if (styleObject != nullptr)
        { // hover bug
          if (!btnDragInProgress() && status.startsWith(QLatin1String("focused"))
              && isCursorOutsideWidget(widget))
          {
            styleObject->setProperty("_kv_hover_bug", true);
            status.replace(QLatin1String("focused"),QLatin1String("normal"));
          }
          else
            styleObject->setProperty("_kv_hover_bug", QVariant());
        }
        const QString group = "PanelButtonCommand";
        frame_spec fspec = getFrameSpec(group);
        const interior_spec ispec = getInteriorSpec(group);
        indicator_spec dspec = getIndicatorSpec(group);
        label_spec lspec = getLabelSpec(group);

        /* force text color if the button isn't drawn in a standard way */
        if (widget && !standardButton.contains(widget)
            && (option->state & State_Enabled))
        {
          bool isInactive(status.contains(QLatin1String("-inactive")));
          QColor col;
          if (!(opt->features & QStyleOptionButton::Flat) || !status.startsWith(QLatin1String("normal")))
          {
            if (status.startsWith(QLatin1String("pressed")))
            {
              if (isInactive)
              {
                col = getFromRGBA(lspec.pressInactiveColor);
                if (!col.isValid())
                  col = getFromRGBA(lspec.pressColor);
              }
              else
                col = getFromRGBA(lspec.pressColor);
            }
            else if (status.startsWith(QLatin1String("toggled")))
            {
              if (isInactive)
              {
                col = getFromRGBA(lspec.toggleInactiveColor);
                if (!col.isValid())
                  col = getFromRGBA(lspec.toggleColor);
              }
              else
                col = getFromRGBA(lspec.toggleColor);
            }
            else if (option->state & State_MouseOver)
            {
              if (isInactive)
              {
                col = getFromRGBA(lspec.focusInactiveColor);
                if (!col.isValid())
                  col = getFromRGBA(lspec.focusColor);
              }
              else
                col = getFromRGBA(lspec.focusColor);
            }
            else
            {
              if (isInactive)
              {
                col = getFromRGBA(lspec.normalInactiveColor);
                if (!col.isValid())
                  col = getFromRGBA(lspec.normalColor);
              }
              else
                col = getFromRGBA(lspec.normalColor);
            }

            if (!col.isValid())
            {
              col = standardPalette().color(isInactive ? QPalette::Inactive : QPalette::Active,
                                            QPalette::ButtonText);
            }
          }
          else // FIXME: the foreground color of the parent widget should be used
            col = standardPalette().color(isInactive ? QPalette::Inactive : QPalette::Active,
                                          QPalette::WindowText);
          forceButtonTextColor(widget,col);
        }

        QWidget *p = getParent(widget,1);
        if (QTabWidget *tw = qobject_cast<QTabWidget*>(p))
        { // tab corner widget
          if (opt->text.isEmpty()
              && (tw->cornerWidget(Qt::TopLeftCorner) == widget
                  || tw->cornerWidget(Qt::TopRightCorner) == widget
                  || tw->cornerWidget(Qt::BottomLeftCorner) == widget
                  || tw->cornerWidget(Qt::BottomRightCorner) == widget))
          {
            lspec.left = lspec.right = lspec.top = lspec.bottom = qMin(lspec.left,2);
            fspec.left = qMin(fspec.left,3);
            fspec.right = qMin(fspec.right,3);
            fspec.top = qMin(fspec.top,3);
            fspec.bottom = qMin(fspec.bottom,3);
          }
        }
        else if (qobject_cast<QAbstractItemView*>(getParent(p,1)))
        {
          lspec.left = lspec.right = lspec.top = lspec.bottom = qMin(lspec.left,2);
          fspec.left = qMin(fspec.left,3);
          fspec.right = qMin(fspec.right,3);
          fspec.top = qMin(fspec.top,3);
          fspec.bottom = qMin(fspec.bottom,3);
          lspec.tispace = qMin(lspec.tispace,3);

          if (!hasExpandedBorder(fspec))
            fspec.expansion = qMin(fspec.expansion, LIMITED_EXPANSION);
          else
          {
            fspec.leftExpanded = qMin(fspec.leftExpanded,3);
            fspec.rightExpanded = qMin(fspec.rightExpanded,3);
            fspec.topExpanded = qMin(fspec.topExpanded,3);
            fspec.bottomExpanded = qMin(fspec.bottomExpanded,3);
          }
        }

        if (!opt->text.isEmpty() && qobject_cast<const QAbstractItemView*>(widget))
        { // as in Kate's preferences for its default text style
          painter->fillRect(option->rect.adjusted(1,1,-1,-1),
                            standardPalette().color(QPalette::Button));
          return;
        }

        if (!opt->text.isEmpty()) // -> CE_PushButtonLabel
        {
          if (pb)
          {
            w = pb->width();
            h = pb->height();
          }
          QFont fnt(painter->font());
          if ((opt->features & QStyleOptionButton::AutoDefaultButton) || lspec.boldFont)
            fnt.setWeight(lspec.boldness);
          QSize txtSize = textSize(fnt,opt->text);
          bool enoughSpace(true);
          if (w < txtSize.width()
                  + (opt->icon.isNull() ? 0 : opt->iconSize.width()+lspec.tispace)
                  + lspec.left+lspec.right+fspec.left+fspec.right)
          {
            lspec.left = lspec.right = qMin(lspec.left,2);
            enoughSpace = false;
          }
          if (h < txtSize.height()+lspec.top+lspec.bottom+fspec.top+fspec.bottom)
          {
            lspec.top = lspec.bottom = qMin(lspec.top,2);
            enoughSpace = false;
          }
          if (!enoughSpace)
          {
            fspec.left = qMin(fspec.left,3);
            fspec.right = qMin(fspec.right,3);
            fspec.top = qMin(fspec.top,3);
            fspec.bottom = qMin(fspec.bottom,3);

            if (!hasExpandedBorder(fspec))
              fspec.expansion = qMin(fspec.expansion, LIMITED_EXPANSION);
            else
            {
              fspec.leftExpanded = qMin(fspec.leftExpanded,3);
              fspec.rightExpanded = qMin(fspec.rightExpanded,3);
              fspec.topExpanded = qMin(fspec.topExpanded,3);
              fspec.bottomExpanded = qMin(fspec.bottomExpanded,3);
            }
          }
        }

        if (widget && !pb) // bad styling method in the app
        {
          QColor optCol = opt->palette.color(QPalette::Button);
          // KDE Partition Manager
          if (optCol.isValid() && optCol != standardPalette().color(opt->palette.currentColorGroup(), QPalette::Button))
            painter->fillRect(opt->rect, optCol);
          else // FIXME why does Qt Designer use CE_PushButtonBevel for its Widget Box headers?
            drawPrimitive(PE_Frame,option,painter,widget);
          break;
        }

        if (opt->text.size() == 0 && opt->icon.isNull())
        {
          if (qobject_cast<const QCommandLinkButton*>(widget))
          {
            if (hasExpandedBorder(fspec))
              fspec.expansion = 0;
            else
              fspec.expansion = qMin(fspec.expansion, LIMITED_EXPANSION);
          }
          else if (!isLibreoffice_)
            fspec.expansion = 0; // KColorButton (color button in general)
        }
        if (!(option->state & State_Enabled))
        {
          status = "normal";
          if (option->state & State_On)
            status = "toggled";
          if (isWidgetInactive(widget))
            status.append("-inactive");
          painter->save();
          painter->setOpacity(DISABLED_OPACITY);
        }
        if (widget && !(opt->features & QStyleOptionButton::Flat)
            && ((widget->testAttribute(Qt::WA_StyleSheetTarget)
                 && !widget->styleSheet().isEmpty() && widget->styleSheet().contains(QLatin1String("background")))
                || (opt->text.size() == 0 && opt->icon.isNull()
                    && widget->palette().color(QPalette::Button)
                       != standardPalette().color(widget->palette().currentColorGroup(), QPalette::Button))))
        { // color button!?
          fspec.expansion = 0;
          renderFrame(painter,option->rect,fspec,fspec.element+"-"+status);
          painter->fillRect(interiorRect(opt->rect,fspec), widget->palette().brush(QPalette::Button));
        }
        else
        {
          bool fillWidgetInterior(!ispec.hasInterior
                                  && hasHighContrastWithContainer(widget, getFromRGBA(getLabelSpec(group).normalColor)));
          bool libreoffice = false;
          if (isLibreoffice_ && widget == nullptr && (option->state & State_Enabled)
              && enoughContrast(getFromRGBA(lspec.normalColor), standardPalette().color(QPalette::ButtonText)))
          {
            libreoffice = true;
            painter->fillRect(option->rect, standardPalette().brush(QPalette::Window));
            painter->save();
            painter->setOpacity(0.5);
          }

          const QString prevState = styleObject != nullptr
                                      ? styleObject->property("_kv_state").toString()
                                      : QString();
          bool animate(opacityTimer_
                       && !btnDragInProgress()
                       && widget && widget->isEnabled() && animatedWidget_ == widget
                       && !prevState.isEmpty()
                       && !qobject_cast<const QAbstractScrollArea*>(widget));
          if (animate && prevState == status)
          {
            if (opacityTimer_->isActive())
              opacityTimer_->stop();
            animationOpacity_ = 0;
            animate = false;
          }
          if (!(opt->features & QStyleOptionButton::Flat) || !status.startsWith(QLatin1String("normal")))
          {
            if (animate)
            {
              if (!opacityTimer_->isActive())
              {
                animationOpacity_ = 0;
                opacityTimer_->start(ANIMATION_FRAME);
              }
              if (animationOpacity_ < 100
                  && (!(opt->features & QStyleOptionButton::Flat)
                      || !prevState.startsWith(QLatin1String("normal"))))
              {
                renderFrame(painter,option->rect,fspec,fspec.element+"-"+prevState);
                if (!fillWidgetInterior)
                  renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+prevState);
              }
              painter->save();
              painter->setOpacity(static_cast<qreal>(animationOpacity_)/100.0);
            }
            if (!fillWidgetInterior)
            {
              if ((option->state & State_Enabled)
                  && widget == nullptr
                  && enoughContrast(option->palette.color(QPalette::ButtonText),
                                    standardPalette().color(QPalette::ButtonText)))
              { // QML colorized button
                fspec.left = qMin(fspec.left,3);
                fspec.right = qMin(fspec.right,3);
                fspec.top = qMin(fspec.top,3);
                fspec.bottom = qMin(fspec.bottom,3);
                if (hasExpandedBorder(fspec))
                  fspec.expansion = 0;
                else
                  fspec.expansion = qMin(fspec.expansion, LIMITED_EXPANSION);
                painter->fillRect(interiorRect(option->rect,fspec),
                                  option->palette.brush(QPalette::Active, QPalette::Button));
              }
              else
                renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+status);
            }
            renderFrame(painter,option->rect,fspec,fspec.element+"-"+status);
            if (animate)
            {
              painter->restore();
              if (animationOpacity_ >= 100)
                styleObject->setProperty("_kv_state", status);
            }
            else if (styleObject)
              styleObject->setProperty("_kv_state", status);
            if (fillWidgetInterior) // widget isn't null
              painter->fillRect(interiorRect(option->rect,fspec),
                                widget->palette().brush(status.contains(QLatin1String("-inactive"))
                                                          ? QPalette::Inactive
                                                          : QPalette::Active,
                                                        QPalette::Button));
          }
          // fade out animation
          else if (animate && !prevState.startsWith(QLatin1String("normal")))
          {
            if (!opacityTimer_->isActive())
            {
              animationOpacity_ = 0;
              opacityTimer_->start(ANIMATION_FRAME);
            }
            if (animationOpacity_ < 100)
            {
              painter->save();
              painter->setOpacity(1.0 - static_cast<qreal>(animationOpacity_)/100.0);
              renderFrame(painter,option->rect,fspec,fspec.element+"-"+prevState);
              if (!fillWidgetInterior)
                renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-"+prevState);
              painter->restore();
              if (fillWidgetInterior)
                painter->fillRect(interiorRect(option->rect,fspec),
                                  widget->palette().brush(status.contains(QLatin1String("-inactive"))
                                                            ? QPalette::Inactive
                                                            : QPalette::Active,
                                                          QPalette::Button));
            }
            if (animationOpacity_ >= 100)
              styleObject->setProperty("_kv_state", status);
          }
          else if (styleObject)
            styleObject->setProperty("_kv_state", status);
          if (libreoffice) painter->restore();
        }
        if (!(option->state & State_Enabled))
        {
          painter->restore();
          status = "disabled";
          if (isWidgetInactive(widget))
            status.append("-inactive");
        }

        if (opt->features & QStyleOptionButton::HasMenu)
        {
          int hShift = 0;
          int vShift = 0;
          /* use the "flat" indicator with flat buttons if it exists */
          if ((opt->features & QStyleOptionButton::Flat) && status.startsWith(QLatin1String("normal")))
          {
            if (themeRndr_ && themeRndr_->isValid())
            {
              QColor ncol = getFromRGBA(lspec.normalColor);
              if (!ncol.isValid())
                ncol = standardPalette().color(QPalette::ButtonText);
              if (enoughContrast(ncol, standardPalette().color(QPalette::WindowText))
                  && flatArrowExists(dspec.element))
                dspec.element = "flat-"+dspec.element;
            }
          }
          else if (status.startsWith(QLatin1String("toggled")) || status.startsWith(QLatin1String("pressed")))
          {
            hShift = pixelMetric(PM_ButtonShiftHorizontal);
            vShift = pixelMetric(PM_ButtonShiftVertical);
          }
          renderIndicator(painter,
                          option->rect.adjusted(opt->direction == Qt::RightToLeft ? BUTTON_ARROW_MARGIN : 0,
                                                0,
                                                opt->direction == Qt::RightToLeft ? 0 : -BUTTON_ARROW_MARGIN,
                                                0)
                                       .adjusted(hShift,vShift,hShift,vShift),
                          fspec,dspec,dspec.element+"-down-"+status,
                          option->direction,
                          Qt::AlignRight | Qt::AlignVCenter,
                          (lspec.bottom-lspec.top)/2);
        }

        if (pb && pb->isDefault() && (option->state & State_Enabled))
        {
          QString di = "button-default-indicator";
          if (!((opt->features & QStyleOptionButton::Flat) && status.startsWith(QLatin1String("normal"))))
          {
            renderFrame(painter,option->rect,fspec,fspec.element+"-default");
            renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-default");
          }
          else if (elementExists(QStringLiteral("flat-button-default-indicator")))
            di = "flat-button-default-indicator";
          renderIndicator(painter,
                          option->rect,
                          fspec,dspec,di,option->direction,
                          Qt::AlignBottom | (opt->direction == Qt::RightToLeft ?
                                             Qt::AlignLeft : Qt::AlignRight));
        }

        if ((opt->state & State_HasFocus)
            && (!widget || !widget->focusProxy())) // workaround for Kate's StatusBarButton
        {
          if (fspec.hasFocusFrame)
          {
            renderFrame(painter,option->rect,fspec,fspec.element+"-focus");
            if (ispec.hasFocusInterior)
              renderInterior(painter,option->rect,fspec,ispec,ispec.element+"-focus");
          }
          else
          {
            QRect focusRect;
            if (fspec.expansion > 0)
              focusRect = labelRect(option->rect, fspec, lspec).adjusted(-2,-2,2,2);
            else
              focusRect = interiorRect(option->rect, fspec);
            drawFocusRect(painter, focusRect, fspec.focusRectElement);
          }
        }
      }

      break;
    }

    case CE_ToolButtonLabel : {
      if ((option->state & State_Enabled) && widget == nullptr && option != nullptr
          && enoughContrast(option->palette.color(QPalette::ButtonText),
                            standardPalette().color(QPalette::ButtonText)))
      { // QML colorized button
        QCommonStyle::drawControl(element,option,painter,widget);
        break;
      }

      const QStyleOptionToolButton *opt =
          qstyleoption_cast<const QStyleOptionToolButton*>(option);

      if (opt) {
        const QToolButton *tb = qobject_cast<const QToolButton*>(widget);
        QString txt = opt->text;
        QString status = getState(option,widget);
        QString group = "PanelButtonTool";
        QWidget *p = getParent(widget,1);
        QWidget *gp = getParent(p,1);
        QWidget *stb = nullptr;
        bool autoraise = false;
        bool drawRaised = false;
        if (tb)
        {
          autoraise = tb->autoRaise();
          stb = getStylableToolbarContainer(widget);
          if (stb)
          {
            autoraise = true;
            if (!getFrameSpec(QStringLiteral("ToolbarButton")).element.isEmpty()
                || !getInteriorSpec(QStringLiteral("ToolbarButton")).element.isEmpty())
            {
              group = "ToolbarButton";
            }
          }

          /* as in PE_PanelButtonTool */
          QToolBar *toolBar = qobject_cast<QToolBar*>(p);
          if ((toolBar && toolBar->orientation() != Qt::Vertical)
              || (qobject_cast<QToolBar*>(stb)
                  && qobject_cast<QToolBar*>(stb)->orientation() != Qt::Vertical
                  && tb->inherits("Fm::PathButton")))
          {
            if (tspec_.group_toolbar_buttons)
              drawRaised = true;
          }
          else if (tb->inherits("Fm::PathButton"))
            drawRaised = true;
        }

        frame_spec fspec = getFrameSpec(group);
        indicator_spec dspec = getIndicatorSpec(group);
        label_spec lspec = getLabelSpec(group);
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

        if (QObject *styleObject = option->styleObject)
        { // hover bug
          if (status.startsWith(QLatin1String("focused")) && styleObject->property("_kv_hover_bug").toBool())
            status.replace(QLatin1String("focused"),QLatin1String("normal"));
        }

        int arType = getArrowType(tb, opt);
        int dspecSize = dspec.size;

        if (tb)
        {
          /* always show menu titles in the toggled state */
          bool transMenuTitle(hspec_.transparent_menutitle);
          if (!transMenuTitle
              && tb->isDown()
              && (tb->toolButtonStyle() == Qt::ToolButtonTextBesideIcon
                  || (tb->toolButtonStyle() == Qt::ToolButtonFollowStyle
                      && styleHint(SH_ToolButtonStyle,option,widget) == Qt::ToolButtonTextBesideIcon))
              && qobject_cast<QMenu*>(p))
          {
            status.replace(QLatin1String("pressed"),QLatin1String("toggled"));
          }

          /* the right arrow is attached */
          if (arType != arrowNone)
          {
            if (opt->direction == Qt::RightToLeft)
              fspec.left = 0;
            else
              fspec.right = 0;
          }

          /* respect the text color of the parent widget */
          bool noPanel(!paneledButtons.contains(widget));
          if ((autoraise && !drawRaised) /*|| inPlasma*/ || noPanel)
          {
            bool isNormal(status.startsWith(QLatin1String("normal"))
                          /* cover the simple disabled state too */
                          || (status.startsWith(QLatin1String("disabled"))
                              && !(option->state & State_On)
                              && !(option->state & State_Sunken)
                              && !(option->state & State_Selected)));

            /* get the color used for checking the contrast (normal color with auto-raise)*/
            QColor ncol;
            if (noPanel && (option->state & State_Enabled))
            {
              if (status.startsWith(QLatin1String("normal")))
                ncol = getFromRGBA(lspec.normalColor);
              else if (status.startsWith(QLatin1String("focused")))
                ncol = getFromRGBA(lspec.focusColor);
              else if (status.startsWith(QLatin1String("pressed")))
                ncol = getFromRGBA(lspec.pressColor);
              else// if (status.startsWith("toggled"))
                ncol = getFromRGBA(lspec.toggleColor);
            }
            else // auto-raise
              ncol = getFromRGBA(lspec.normalColor);
            if (!ncol.isValid())
              ncol = standardPalette().color(QPalette::ButtonText);

            QWidget* menubar = nullptr;
            if (qobject_cast<QMenuBar*>(gp))
              menubar = gp;
            else if (qobject_cast<QMenuBar*>(p))
              menubar = p;
            if (menubar)
            {
              if (isNormal || noPanel)
              {
                QString group1("MenuBar");
                if (mergedToolbarHeight(menubar))
                  group1 = "Toolbar";
                const label_spec lspec1 = getLabelSpec(group1);
                if (themeRndr_ && themeRndr_->isValid()
                    && enoughContrast(ncol, getFromRGBA(lspec1.normalColor))
                    && flatArrowExists(dspec.element))
                {
                  dspec.element = "flat-"+dspec.element;
                }
                if (isNormal)
                {
                  lspec.normalColor = lspec1.normalColor;
                  lspec.normalInactiveColor = lspec1.normalInactiveColor;
                }
              }
            }
            else if (stb)
            {
              if (isNormal || noPanel)
              {
                const label_spec lspec1 = getLabelSpec(QStringLiteral("Toolbar"));
                if (themeRndr_ && themeRndr_->isValid()
                    && enoughContrast(ncol, getFromRGBA(lspec1.normalColor))
                    && flatArrowExists(dspec.element))
                {
                  dspec.element = "flat-"+dspec.element;
                }
                if (isNormal)
                {
                  lspec.normalColor = lspec1.normalColor;
                  lspec.normalInactiveColor = lspec1.normalInactiveColor;
                }
              }
            }
            else if (p)
            {
              QColor col;
              if (!(autoraise && !drawRaised) && noPanel) // an already styled toolbutton
                col = opt->palette.color(status.contains(QLatin1String("-inactive"))
                                           ? QPalette::Inactive
                                           : QPalette::Active,
                                         QPalette::ButtonText); // p->palette()?
              else
                col = p->palette().color(status.contains(QLatin1String("-inactive"))
                                           ? QPalette::Inactive
                                           : QPalette::Active,
                                         p->foregroundRole());
              if (!col.isValid())
                col = standardPalette().color(status.contains(QLatin1String("-inactive"))
                                               ? QPalette::Inactive
                                               : QPalette::Active,
                                              QPalette::WindowText);
              if (isNormal || noPanel)
              {
                if (themeRndr_ && themeRndr_->isValid()
                    && enoughContrast(ncol, col)
                    && flatArrowExists(dspec.element))
                {
                  dspec.element = "flat-"+dspec.element;
                }
                if (isNormal)
                {
                  lspec.normalColor = lspec.normalInactiveColor = getName(col);
                }

                if (/*inPlasma ||*/ noPanel)
                {
                  lspec.focusColor = lspec.focusInactiveColor = getName(col);
                  lspec.toggleColor = lspec.toggleInactiveColor = getName(col);
                  /* take care of Plasma menu titles */
                  if (!qobject_cast<QMenu*>(p))
                  {
                    lspec.pressColor = lspec.pressInactiveColor = getName(col);
                  }
                  else if (transMenuTitle)
                  {
                    lspec.pressColor = getLabelSpec(QStringLiteral("MenuItem")).normalColor;
                    lspec.pressInactiveColor = getLabelSpec(QStringLiteral("MenuItem")).normalInactiveColor;
                  }
                }
              }
            }
          }
          /* KDE menu titles */
          else if (qobject_cast<QMenu*>(p) && transMenuTitle)
          {
            lspec.pressColor = getLabelSpec(QStringLiteral("MenuItem")).normalColor;
            lspec.pressInactiveColor = getLabelSpec(QStringLiteral("MenuItem")).normalInactiveColor;
          }

          /* when there isn't enough space (as in Qupzilla's bookmark toolbar) */
          if (tialign != Qt::ToolButtonIconOnly)
          {
            if (!txt.isEmpty())
            {
              QFont F(painter->font());
              if (lspec.boldFont) F.setWeight(lspec.boldness);
              QSize txtSize = textSize(F, txt);
              fitToolButtonText(opt, txt,
                                txtSize, QFontMetrics(painter->font()),
                                tialign,
                                fspec, lspec,
                                dspec.size,
                                arType != arrowNone,
                                QCoreApplication::applicationName() == "lxqt-panel");
            }
          }
          /* lack of space (as in some of Krita's KisToolButtons) */
          else if (!opt->icon.isNull())
          {
            if (arType != arrowNormal)
            {
              if (arType == arrowDelayed)
              {
                if (tb->width() < opt->iconSize.width()+fspec.left+fspec.right
                                  +dspec.size+ BUTTON_ARROW_MARGIN+lspec.tispace)
                {
                  if (opt->direction == Qt::RightToLeft)
                    fspec.right = qMin(fspec.right,3);
                  else
                    fspec.left = qMin(fspec.left,3);
                  dspec.size = qMin(dspecSize ,TOOL_BUTTON_ARROW_SIZE-TOOL_BUTTON_ARROW_OVERLAP);
                  dspecSize = qMin(dspecSize, TOOL_BUTTON_ARROW_SIZE); // to be restored when needed
                  lspec.tispace = 0;
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
              const frame_spec fspec1 = getFrameSpec(QStringLiteral("DropDownButton"));
              if (tb->width() < opt->iconSize.width()+fspec.left
                                +(opt->direction == Qt::RightToLeft ? fspec1.left : fspec1.right)
                                +TOOL_BUTTON_ARROW_SIZE+2*BUTTON_ARROW_MARGIN)
              {
                fspec.left = qMin(fspec.left,3);
                fspec.right = qMin(fspec.right,3);
              }
            }
          }
        }
        else // there is no QToolButton
        {
          lspec.boldFont = false;
          if (widget != nullptr
              && opt->icon.isNull()
              && (!(opt->features & QStyleOptionToolButton::Arrow) || opt->arrowType == Qt::NoArrow))
          {
            /* KMultiTabBarTab (in kate5) is a total mess. They derive it from QPushButton
               but draw its panel with CC_ToolButton, draw its icon with drawItemPixmap(),
               set a rectangle for its text and call CE_ToolButtonLabel. */
            lspec.left = lspec.right = lspec.top = lspec.bottom = lspec.tispace = 0;
            fspec.left = fspec.right = fspec.top = fspec.bottom = 0;
          }
          else // QML
          {
            if (tialign != Qt::ToolButtonIconOnly)
            {
              if (!txt.isEmpty())
              {
                fitToolButtonText(opt, txt,
                                  textSize(painter->font(), txt), QFontMetrics(painter->font()),
                                  tialign,
                                  fspec, lspec,
                                  dspec.size,
                                  arType != arrowNone,
                                  false);
              }
            }
          }
          if (option->state & State_AutoRaise)
          {
            lspec.normalColor = getName(opt->palette.color(QPalette::Active,QPalette::ButtonText));
            lspec.normalInactiveColor = getName(opt->palette.color(QPalette::Inactive,QPalette::ButtonText));
          }
        }

        QRect r = option->rect;

        int talign = Qt::AlignCenter;
        if (!styleHint(SH_UnderlineShortcut, opt, widget))
          talign |= Qt::TextHideMnemonic;
        else
          talign |= Qt::TextShowMnemonic;

        /*
           Do NOT draw any label when all these conditions are satisfied
           because the button may have only an arrow inside it (which is
           treated as an icon, like in QtCreator's find widget):

             (1) The button style is icon-only or there's no text;
             (2) There's no icon; but
             (3) There's an arrow.
        */
        Qt::Alignment iAlignment = Qt::AlignVCenter;
        if ((tialign == Qt::ToolButtonIconOnly || opt->text.isEmpty())
            && opt->icon.isNull()
            && (opt->features & QStyleOptionToolButton::Arrow)
            && opt->arrowType != Qt::NoArrow)
        {
          if (qobject_cast<QTabBar*>(p)) // tabbar scroll button
          {
            const frame_spec fspec1 = getFrameSpec(QStringLiteral("Tab"));
            qreal rDiff = 0;
            if (lspec.top+fspec.top + lspec.bottom+fspec.bottom > 0)
            {
              rDiff = static_cast<qreal>(lspec.top+fspec.top - lspec.bottom-fspec.bottom)
                      / static_cast<qreal>(lspec.top+fspec.top + lspec.bottom+fspec.bottom);
            }
            if (opt->arrowType == Qt::LeftArrow)
            {
              iAlignment |= Qt::AlignRight;
              fspec.left = qMin(fspec.left, fspec1.left);
              fspec.right = 1;
              fspec.top = qMin(fspec.top, fspec1.top);
              fspec.bottom = qMin(fspec.bottom, fspec1.bottom);
              int vOffset = 0;
              if (lspec.top+lspec.bottom > 0 && h > fspec.top+fspec.bottom+dspec.size)
                vOffset = qRound(static_cast<qreal>(h-fspec.top-fspec.bottom-dspec.size) * rDiff / 2.0);
              fspec.top += vOffset;
              fspec.bottom -= vOffset;
            }
            else if (opt->arrowType == Qt::RightArrow)
            {
              iAlignment |= Qt::AlignLeft;
              fspec.right = qMin(fspec.right, fspec1.right);
              fspec.left = 1;
              fspec.top = qMin(fspec.top, fspec1.top);
              fspec.bottom = qMin(fspec.bottom, fspec1.bottom);
              int vOffset = 0;
              if (lspec.top+lspec.bottom > 0 && h > fspec.top+fspec.bottom+dspec.size)
                vOffset = qRound(static_cast<qreal>(h-fspec.top-fspec.bottom-dspec.size) * rDiff / 2.0);
              fspec.top += vOffset;
              fspec.bottom -= vOffset;
            }
            else if (opt->arrowType == Qt::DownArrow)
            { // panel rotated and mirrored at PE_PanelButtonTool
              iAlignment = Qt::AlignHCenter | Qt::AlignTop;
              fspec.right = qMin(fspec.bottom, fspec1.bottom);
              fspec.bottom = qMin(fspec.left, fspec1.left);
              fspec.left = qMin(fspec.top, fspec1.top);
              fspec.top = 1;
              int hOffset = 0;
              if (lspec.top+lspec.bottom > 0 && w > fspec.left+fspec.right+dspec.size)
                hOffset = qRound(static_cast<qreal>(w-fspec.left-fspec.right-dspec.size) * rDiff / 2.0);
              fspec.left += hOffset;
              fspec.right -= hOffset;
            }
            else if (opt->arrowType == Qt::UpArrow)
            { // panel rotated and mirrored at PE_PanelButtonTool
              iAlignment = Qt::AlignHCenter | Qt::AlignBottom;
              fspec.left = qMin(fspec.top, fspec1.top);
              fspec.top = qMin(fspec.right, fspec1.right);
              fspec.right = qMin(fspec.bottom, fspec1.bottom);
              fspec.bottom = 1;
              int hOffset = 0;
              if (lspec.top+lspec.bottom > 0 && w > fspec.left+fspec.right+dspec.size)
                hOffset = qRound(static_cast<qreal>(w-fspec.left-fspec.right-dspec.size) * rDiff / 2.0);
              fspec.left += hOffset;
              fspec.right -= hOffset;
            }
            /* panel has no status at PE_PanelButtonTool */
            if(option->state & State_Enabled)
            {
              status = "normal";
              if (isWidgetInactive(widget))
                status.append("-inactive");
            }
            lspec.left = lspec.right = lspec.top = lspec.bottom = 0;
            /* for apps with bad style codes (like SMPlayer) */
            if (w < fspec.left + fspec.right + dspec.size)
            {
              fspec.left = fspec.right = 0;
            }
            if (h < fspec.top + fspec.bottom + dspec.size)
            {
              fspec.top = fspec.bottom = 0;
            }
            /* draw the focus rect around the arrow */
            if (opt->state & State_HasFocus)
            {
              QRect focusRect = alignedRect(option->direction,
                                            iAlignment,
                                            QSize(dspec.size+2, dspec.size+2),
                                            interiorRect(opt->rect, fspec).adjusted(-2,-2,2,2));
              drawFocusRect(painter, focusRect, fspec.focusRectElement);
            }
          }
          else
          {
            iAlignment |= Qt::AlignHCenter;
            fspec.left = fspec.right = fspec.top = fspec.bottom = 0;
            lspec.left = lspec.right = lspec.top = lspec.bottom = 0;
          }
        }
        else
        {
          int state = 1;
          if (!(option->state & State_Enabled))
            state = 0;
          else if (status.startsWith(QLatin1String("pressed")))
            state = 3;
          else if (status.startsWith(QLatin1String("toggled")))
            state = 4;
          else if (status.startsWith(QLatin1String("focused")))
            state = 2;

          QStyleOptionToolButton o(*opt);
          if (!btnDragInProgress() && (option->state & State_MouseOver) && state != 2)
            o.state = o.state & ~QStyle::State_MouseOver; // hover bug

          bool isInactive(status.contains(QLatin1String("-inactive")));
          QColor disabledCol;
          if (state == 0
              && ((option->state & State_On)
                  || (!(option->state & State_Sunken) && (option->state & State_Selected))))
          {
            if (isInactive)
              disabledCol = getFromRGBA(lspec.toggleInactiveColor);
            if (!disabledCol.isValid())
              disabledCol = getFromRGBA(lspec.toggleColor);
            if (disabledCol.isValid())
            {
              disabledCol.setAlpha(102); // 0.4 * disabledCol.alpha()
              QPalette palette(opt->palette);
              palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledCol);
              o.palette = palette;
            }
          }

          QSize iconSize = opt->iconSize;
          if (widget && widget->inherits("QDockWidgetTitleButton"))
          {
            int s = pixelMetric(PM_TitleBarButtonIconSize, option, widget);
            iconSize = QSize(s,s);
          }
          renderLabel(&o,painter,
                      !(opt->features & QStyleOptionToolButton::Arrow)
                      || opt->arrowType == Qt::NoArrow
                      || tialign == Qt::ToolButtonTextOnly ?
                        r : // may still have arrow for a menu but that's dealt with at CC_ToolButton
                        // also add a space between indicator and text (-> CT_ToolButton)
                        r.adjusted(opt->direction == Qt::RightToLeft ?
                                     0
                                     : dspec.size+lspec.tispace+INDICATOR_MARGIN,
                                   0,
                                   opt->direction == Qt::RightToLeft ?
                                     -dspec.size-lspec.tispace-INDICATOR_MARGIN
                                     : 0,
                                   0),
                      fspec,lspec,
                      talign,txt,QPalette::ButtonText,
                      state,
                      isInactive,
                      getPixmapFromIcon(opt->icon,
                                        getIconMode(disabledCol.isValid()
                                                    && !enoughContrast(disabledCol,
                                                                      standardPalette().color(QPalette::Window))
                                                      ? -1 : state,
                                                    isInactive, lspec),
                                        iconstate, iconSize),
                      iconSize,tialign);
          iAlignment |= Qt::AlignLeft;
        }

        /* we treat arrows as icons */
        if (!(opt->features & QStyleOptionToolButton::Arrow)
            || opt->arrowType == Qt::NoArrow
            || tialign == Qt::ToolButtonTextOnly)
        {
          break;
        }

        if (status.startsWith(QLatin1String("toggled"))
            && (!themeRndr_ || !themeRndr_->isValid()
                || !themeRndr_->elementExists(dspec.element+"-down-toggled")))
        {
          /* distinguish between the toggled and pressed states
             only if a toggled down arrow element exists */
          status.replace(QLatin1String("toggled"),QLatin1String("pressed"));
        }
        if (!opt->icon.isNull()
            || (tialign != Qt::ToolButtonIconOnly
                && !txt.isEmpty())) // it's empty for QStackedWidget
        {
          r.adjust(lspec.left,lspec.top,-lspec.right,-lspec.bottom);
        }
        dspec.size = dspecSize; // may have changed due to lack of space
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

      if (opt) {
        const QString group = "DockTitle";

        frame_spec fspec = getFrameSpec(group);
        interior_spec ispec = getInteriorSpec(group);
        label_spec lspec = getLabelSpec(group);
        fspec.expansion = 0;

        QRect r = option->rect;
        QRect tRect = subElementRect(SE_DockWidgetTitleBarText, option, widget);
        bool hasVertTitle = false;
        const QDockWidget *dw = qobject_cast<const QDockWidget*>(widget);
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
          status.replace(QLatin1String("disabled"),QLatin1String("normal"));
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
        fspec.left = fspec.right = fspec.top = fspec.bottom = 0;
        lspec.left = lspec.right = lspec.top = lspec.bottom = 0;

        QString title = opt->title;
        if (!title.isEmpty())
        {
          QFont F(painter->font());
          if (lspec.boldFont) F.setWeight(lspec.boldness);
          title = QFontMetrics(F).elidedText(title, Qt::ElideRight,
                                             tRect.width(), Qt::TextShowMnemonic);
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
                    option->state & State_Enabled ? option->state & State_MouseOver ? 2 : 1 : 0,
                    status.contains(QLatin1String("-inactive")));

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
      if (const QStyleOptionFrame *f = qstyleoption_cast<const QStyleOptionFrame*>(option))
      {
        /* skip ugly frames */
        if (f->frameShape != QFrame::HLine
            && f->frameShape != QFrame::VLine
            && f->frameShape != QFrame::WinPanel
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
        drawControl(CE_ProgressBarGroove, &o, painter, widget);
        drawControl(CE_ProgressBarContents, &o, painter, widget);
        drawControl(CE_ProgressBarLabel, &o, painter, widget);
      }

      break;
    }

    default : QCommonStyle::drawControl(element,option,painter,widget);
  }
}

void Style::drawComplexControl(QStyle::ComplexControl control,
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
          connect(widget, &QObject::destroyed, this, &Style::removeFromSet, Qt::UniqueConnection);
        }
        QString group = "PanelButtonTool";
        bool autoraise(false);
        QWidget *stb = getStylableToolbarContainer(widget);
        if (tb)
        {
          autoraise = tb->autoRaise();
          if (stb)
          {
            autoraise = true;
            if (!getFrameSpec(QStringLiteral("ToolbarButton")).element.isEmpty()
                || !getInteriorSpec(QStringLiteral("ToolbarButton")).element.isEmpty())
            {
              group = "ToolbarButton";
            }
          }
        }
        frame_spec fspec = getFrameSpec(group);
        label_spec lspec = getLabelSpec(group);

        QStyleOptionToolButton o(*opt);
        o.rect = subControlRect(CC_ToolButton, option, SC_ToolButton, widget);

        /* make an exception for (KDE) menu titles */
        if (hspec_.transparent_menutitle
            && tb && tb->isDown()
            && (tb->toolButtonStyle() == Qt::ToolButtonTextBesideIcon
                || (tb->toolButtonStyle() == Qt::ToolButtonFollowStyle
                    && styleHint(SH_ToolButtonStyle,option,widget) == Qt::ToolButtonTextBesideIcon))
            && qobject_cast<QMenu*>(getParent(widget,1)))
        {
          drawControl(CE_ToolButtonLabel, &o, painter, widget);
          break;
        }

        /* when SH_DockWidget_ButtonsHaveFrame is set to true (default), dock button panels
           are also drawn at PE_PanelButtonTool with all needed states (-> qdockwidget.cpp) */
        if (!widget || !widget->inherits("QDockWidgetTitleButton"))
          drawPrimitive(PE_PanelButtonTool, option, painter, widget); // the arrow part is included
        //drawPrimitive(PE_FrameButtonTool, option, painter, widget);

        drawControl(CE_ToolButtonLabel, &o, painter, widget); // the arrow part is excluded

        o.rect = subControlRect(CC_ToolButton, option, SC_ToolButtonMenu, widget);
        int arType = getArrowType(tb, opt);
        if (arType == arrowNormal)
        { // only the indicator will be drawn at PE_IndicatorButtonDropDown
          drawPrimitive(PE_IndicatorButtonDropDown, &o, painter, widget);
        }
        else if (arType == arrowDelayed)
        {
          QWidget *p = getParent(widget,1);
          QWidget *gp = getParent(p,1);

          /* as in PE_PanelButtonTool */
          bool drawRaised = false;
          QToolBar *toolBar = qobject_cast<QToolBar*>(p);
          if ((toolBar && toolBar->orientation() != Qt::Vertical)
              || (qobject_cast<QToolBar*>(stb)
                  && qobject_cast<QToolBar*>(stb)->orientation() != Qt::Vertical
                  && tb->inherits("Fm::PathButton")))
          {
            if (tspec_.group_toolbar_buttons)
              drawRaised = true;
          }
          else if (tb && tb->inherits("Fm::PathButton"))
            drawRaised = true;

          indicator_spec dspec = getIndicatorSpec(group);
          lspec = getLabelSpec(group);

          QString aStatus = getState(option,widget);
          if (QObject *styleObject = option->styleObject)
          { // hover bug
            if (aStatus.startsWith(QLatin1String("focused")) && styleObject->property("_kv_hover_bug").toBool())
              aStatus.replace(QLatin1String("focused"),QLatin1String("normal"));
          }

          /* use the "flat" indicator with flat buttons if it exists */
          if (aStatus.startsWith(QLatin1String("normal"))
              && autoraise && !drawRaised
              && themeRndr_ && themeRndr_->isValid()
              && flatArrowExists(dspec.element))
          {
            QColor col = getFromRGBA(lspec.normalColor);
            if (!col.isValid())
              col = standardPalette().color(QPalette::ButtonText);
            QWidget* menubar = nullptr;
            if (qobject_cast<QMenuBar*>(gp))
              menubar = gp;
            else if (qobject_cast<QMenuBar*>(p))
              menubar = p;
            if (menubar)
            {
              group = "MenuBar";
              if (mergedToolbarHeight(menubar))
                group = "Toolbar";
              if (enoughContrast(col, getFromRGBA(getLabelSpec(group).normalColor)))
                dspec.element = "flat-"+dspec.element;
            }
            else if (stb)
            {
              if (enoughContrast(col, getFromRGBA(getLabelSpec(QStringLiteral("Toolbar")).normalColor)))
                dspec.element = "flat-"+dspec.element;
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
          if (tb && opt->toolButtonStyle == Qt::ToolButtonIconOnly && !opt->icon.isNull())
          {
            if (tb->width() < opt->iconSize.width()+fspec.left+fspec.right
                              +dspec.size+ BUTTON_ARROW_MARGIN+lspec.tispace)
            {
              dspec.size = qMin(dspec.size,TOOL_BUTTON_ARROW_SIZE);
              ialign = Qt::AlignRight | Qt::AlignBottom;
            }
          }
          /* distinguish between the toggled and pressed states
             only if a toggled down arrow element exists */
          if (aStatus.startsWith(QLatin1String("toggled"))
              && !(themeRndr_ && themeRndr_->isValid()
                   && themeRndr_->elementExists(dspec.element+"-down-toggled")))
          {
            aStatus.replace(QLatin1String("toggled"),QLatin1String("pressed"));
          }
          renderIndicator(painter,
                          o.rect,
                          fspec,dspec,
                          dspec.element+"-down-"+aStatus,
                          option->direction,
                          ialign,
                          (lspec.bottom-lspec.top)/2);
        }
      }

      break;
    }

    case CC_SpinBox : {
      const QStyleOptionSpinBox *opt =
        qstyleoption_cast<const QStyleOptionSpinBox*>(option);

      if (opt) {
        /* workaround for a bug in LibreOffice's Qt skin */
        if (isLibreoffice_ && widget == nullptr)
        {
          QColor winCol = option->palette.color(QPalette::Window);
          winCol.setAlpha(255);
          painter->fillRect(option->rect, winCol);
        }

        QStyleOptionSpinBox o(*opt);
        /* If a null widget is fed into this method but the spinbox
           has a frame (QML), we'll draw buttons vertically. Fortunately,
           KisSliderSpinBox never fulfills this condition. */
        bool isKisSlider1((widget && (widget->inherits("KisIntParseSpinBox")
                                      || widget->inherits("KisDoubleParseSpinBox")))); // Krita 5.0.0
        bool verticalIndicators(tspec_.vertical_spin_indicators
                                || (!widget && opt->frame)
                                || isKisSlider1);
        QRect editRect = subControlRect(CC_SpinBox,opt,SC_SpinBoxEditField,widget);
        QLineEdit *le = nullptr;

        /* The field is automatically drawn as lineedit at PE_PanelLineEdit.
           So, we don't duplicate it here but there are some exceptions. */
        if (/*isLibreoffice_
            ||*/ ((!widget || isKisSlider1) && opt->frame && (opt->subControls & SC_SpinBoxFrame)))
        {
          if (isLibreoffice_ && widget == nullptr)
          {
            editRect.adjust(-3,0,0,0); // see subControlRect() -> CC_SpinBox
            o.state = (o.state & ~QStyle::State_HasFocus); // LibreOffice doesn't allow a correct focused state
          }
          o.rect = editRect;
          drawPrimitive(PE_PanelLineEdit,&o,painter,widget);
        }
        else if (widget)
        {
          le = widget->findChild<QLineEdit*>();
          if (le && le->isVisible() && le->width() < editRect.width()
              && le->rect().left() == editRect.left())
          { // rarely happens, because of bugs in how Qt draws spin boxes
            o.rect = editRect;
            painter->save();
            painter->setClipRegion(QRegion(editRect).subtracted(QRegion(le->rect())));
            drawPrimitive(PE_PanelLineEdit,&o,painter,widget);
            painter->restore();
          }
        }

        if ((verticalIndicators || tspec_.inline_spin_indicators)
            && (opt->subControls & SC_SpinBoxUp))
        {
          QString leGroup;
          if ((!getFrameSpec(QStringLiteral("ToolbarLineEdit")).element.isEmpty()
               || !getInteriorSpec(QStringLiteral("ToolbarLineEdit")).element.isEmpty())
              && getStylableToolbarContainer(le, true)
              && !enoughContrast(le->palette().color(QPalette::Active, QPalette::Text),
                                 getFromRGBA(getLabelSpec(QStringLiteral("Toolbar")).normalColor)))
          {
            leGroup = "ToolbarLineEdit";
          }
          else
            leGroup = "LineEdit";
          const interior_spec ispec = getInteriorSpec(leGroup);
          frame_spec fspec = getFrameSpec(leGroup);
          fspec.isAttached = true;
          fspec.HPos = 1;
          if (verticalIndicators)
          {
            fspec.left = qMin(fspec.left,3);
            fspec.right = qMin(fspec.right,3);
            fspec.top = qMin(fspec.top,3);
            fspec.bottom = qMin(fspec.bottom,3);
            if (hasExpandedBorder(fspec))
              fspec.expansion = 0;
            else
              fspec.expansion = qMin(fspec.expansion, LIMITED_EXPANSION);
          }
          QRect r = subControlRect(CC_SpinBox,opt,SC_SpinBoxUp,widget);
          r.setHeight(editRect.height());
          if (!verticalIndicators) // inline
          {
            r.setLeft(subControlRect(CC_SpinBox,opt,SC_SpinBoxDown,widget).left());

            // exactly as in PE_PanelLineEdit
            /*if (isLibreoffice_)
            {
              fspec.left = qMin(fspec.left,3);
              fspec.right = qMin(fspec.right,3);
              fspec.top = qMin(fspec.top,3);
              fspec.bottom = qMin(fspec.bottom,3);
              fspec.expansion = 0;
            }
            else*/ if (le)
            {
              const label_spec lspec = getLabelSpec(leGroup);
              const size_spec sspec = getSizeSpec(leGroup);
              if ((le->testAttribute(Qt::WA_StyleSheetTarget)
                   && !le->styleSheet().isEmpty() && le->styleSheet().contains(QLatin1String("padding")))
                  || le->minimumWidth() == le->maximumWidth()
                  || le->height() < sizeCalculated(le->font(),fspec,lspec,sspec,QStringLiteral("W"),QSize()).height())
              {
                fspec.left = qMin(fspec.left,3);
                fspec.right = qMin(fspec.right,3);
                fspec.top = qMin(fspec.top,3);
                fspec.bottom = qMin(fspec.bottom,3);

                if (!hasExpandedBorder(fspec))
                  fspec.expansion = qMin(fspec.expansion, LIMITED_EXPANSION);
                else
                {
                  fspec.leftExpanded = qMin(fspec.leftExpanded,3);
                  fspec.rightExpanded = qMin(fspec.rightExpanded,3);
                  fspec.topExpanded = qMin(fspec.topExpanded,3);
                  fspec.bottomExpanded = qMin(fspec.bottomExpanded,3);
                }
              }
            }
            if (const QAbstractSpinBox *sb = qobject_cast<const QAbstractSpinBox*>(widget))
            {
              const size_spec sspec = getSizeSpec(leGroup);
              QString maxTxt = spinMaxText(sb);
              if (maxTxt.isEmpty()
                  || editRect.width() < textSize(sb->font(),maxTxt).width() + fspec.left
                                        + (sspec.incrementW ? sspec.minW : 0)
                  || (sb->buttonSymbols() != QAbstractSpinBox::NoButtons
                      && sb->width() < editRect.width() + 2*tspec_.spin_button_width
                                                        + getFrameSpec(QStringLiteral("IndicatorSpinBox")).right)
                  || sb->height() < fspec.top+fspec.bottom+QFontMetrics(widget->font()).height())
              {
                fspec.left = qMin(fspec.left,3);
                fspec.right = qMin(fspec.right,3);
                fspec.top = qMin(fspec.top,3);
                fspec.bottom = qMin(fspec.bottom,3);

                if (!hasExpandedBorder(fspec))
                  fspec.expansion = qMin(fspec.expansion, LIMITED_EXPANSION);
                else
                {
                  fspec.leftExpanded = qMin(fspec.leftExpanded,3);
                  fspec.rightExpanded = qMin(fspec.rightExpanded,3);
                  fspec.topExpanded = qMin(fspec.topExpanded,3);
                  fspec.bottomExpanded = qMin(fspec.bottomExpanded,3);
                }
              }
            }
          }
          bool fillWidgetInterior(!ispec.hasInterior
                                  && hasHighContrastWithContainer(widget,
                                                                  le ? le->palette().color(QPalette::Text)
                                                                  : standardPalette().color(QPalette::Text)));

          QString leStatus;
          if (isKisSlider_) leStatus = "normal";
          else leStatus = (option->state & State_HasFocus) ? "focused" : "normal";
          if (isWidgetInactive(widget))
            leStatus.append("-inactive");
          if (!(option->state & State_Enabled))
          {
            painter->save();
            painter->setOpacity(DISABLED_OPACITY);
          }
          bool canAnimate(opacityTimer_
                          && widget && widget->isEnabled()
                          && !qobject_cast<const QAbstractScrollArea*>(widget));
          bool animate(canAnimate
                       && ((animatedWidget_ == widget
                            && opacityTimer_->isActive()
                            && !leStatus.startsWith(QLatin1String("normal")))
                           || (animatedWidgetOut_ == widget
                               && opacityTimerOut_->isActive()
                               && leStatus.startsWith(QLatin1String("normal")))));

          QString animationStartState(animationStartState_);
          int animationOpacity = animationOpacity_;
          if (animate)
          {
            if (leStatus.startsWith(QLatin1String("normal"))) // -> QEvent::FocusOut
            {
              animationStartState = animationStartStateOut_;
              animationOpacity = animationOpacityOut_;
            }
            if (animationStartState == leStatus)
            {
              animationOpacity = 100;
              if (leStatus.startsWith(QLatin1String("normal")))
                animationOpacityOut_ = 100;
              else
                animationOpacity_ = 100;
            }
            else if (animationOpacity < 100)
            {
              renderFrame(painter,r,fspec,fspec.element+"-"+animationStartState);
              if (!fillWidgetInterior)
                renderInterior(painter,r,fspec,ispec,ispec.element+"-"+animationStartState);
            }
            painter->save();
            painter->setOpacity(static_cast<qreal>(animationOpacity)/100.0);
          }
          renderFrame(painter,r,fspec,fspec.element+"-"+leStatus);
          if (!fillWidgetInterior)
            renderInterior(painter,r,fspec,ispec,ispec.element+"-"+leStatus);
          if (animate)
          {
            painter->restore();
            if (animationOpacity >= 100)
            {
              if (leStatus.startsWith(QLatin1String("normal")))
                animationStartStateOut_ = leStatus;
              else
                animationStartState_ = leStatus;
            }
          }
          else if (canAnimate)
          {
            if (animatedWidget_ == widget)
              animationStartState_ = leStatus;
            if (animatedWidgetOut_ == widget)
              animationStartStateOut_ = leStatus;
          }
          if (fillWidgetInterior) // widget isn't null
          {
            QColor baseCol = widget->palette().color(leStatus.contains(QLatin1String("-inctive"))
                                                       ? QPalette::Inactive
                                                       : QPalette::Active,
                                                     QPalette::Base);
            baseCol.setAlpha(255);
            painter->fillRect(interiorRect(r,fspec), baseCol);
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
      /* WARNING: The existence of a lineedit isn't a sufficient condition for editability. */
      const QStyleOptionComboBox *opt =
          qstyleoption_cast<const QStyleOptionComboBox*>(option);

      if (opt) {
        QStyleOptionComboBox o(*opt);
        QRect arrowRect = subControlRect(CC_ComboBox,opt,SC_ComboBoxArrow,widget);
        const QComboBox *cb = qobject_cast<const QComboBox*>(widget);
        bool rtl(opt->direction == Qt::RightToLeft);

        QString group;
        if ((!getFrameSpec(QStringLiteral("ToolbarComboBox")).element.isEmpty()
             || !getInteriorSpec(QStringLiteral("ToolbarComboBox")).element.isEmpty())
            && getStylableToolbarContainer(cb != nullptr ? cb : widget, true))
        {
          group = "ToolbarComboBox";
        }
        else group = "ComboBox";

        QString leGroup;

        /* also cover a widget like QDateTimeEdit with calendar,
           that isn't a QComboBox but drawn as one */
        QLineEdit *lineEditWidget = nullptr;
        if (cb != nullptr)
          lineEditWidget = cb->lineEdit();
        else if (widget)
        {
          lineEditWidget = widget->findChild<QLineEdit*>(QString(), Qt::FindDirectChildrenOnly);
          if (lineEditWidget != nullptr && !widget->isAncestorOf(lineEditWidget))
            lineEditWidget = nullptr;
        }

        if ((!getFrameSpec(QStringLiteral("ToolbarLineEdit")).element.isEmpty()
             || !getInteriorSpec(QStringLiteral("ToolbarLineEdit")).element.isEmpty())
            && getStylableToolbarContainer(lineEditWidget, true)
            && !enoughContrast(lineEditWidget->palette().color(QPalette::Active, QPalette::Text),
                               getFromRGBA(getLabelSpec(QStringLiteral("Toolbar")).normalColor)))
        {
          leGroup = "ToolbarLineEdit";
        }
        else
          leGroup = "LineEdit";

        label_spec lspec = getLabelSpec(group);
        frame_spec fspec = getFrameSpec(group);
        interior_spec ispec = getInteriorSpec(group);

        if (widget == nullptr && !isLibreoffice_) // WARNING: QML has anchoring!
        {
          if (hasExpandedBorder(fspec))
            fspec.expansion = 0;
          else
            fspec.expansion = qMin(fspec.expansion, LIMITED_EXPANSION);
          ispec.px = ispec.py = 0;
        }
        int arrowFrameSize = rtl ? fspec.left : fspec.right;
        const bool drwaAsLineEdit(tspec_.combo_as_lineedit || tspec_.square_combo_button);
        if (opt->editable) // otherwise the arrow part will be integrated
        {
          /* announce that the lineedit is drawn as a combo part
             and shouldn't be redrawn in PE_PanelLineEdit */
          if (cb == nullptr)
          {
            if (QObject *styleObject = option->styleObject)
              styleObject->setProperty("_kv_combo", true);
          }

          if (drwaAsLineEdit)
          {
            fspec = getFrameSpec(leGroup);
            ispec = getInteriorSpec(leGroup);
            if (tspec_.combo_as_lineedit)
              arrowFrameSize = rtl ? fspec.left : fspec.right;
          }
          if (!tspec_.combo_as_lineedit)
          {
            fspec.isAttached = true;
            fspec.HPos = rtl ? 1 : -1;
          }
        }

        int extra = 0;
        if (opt->editable && cb)
        {
          if (QLineEdit *le = cb->lineEdit())
          {
            /* Konqueror may add an icon to the right of lineedit (for LTR) */
            int combo_arrow_length = tspec_.square_combo_button
                                      ? qMax(COMBO_ARROW_LENGTH, cb->height()-arrowFrameSize)
                                      : COMBO_ARROW_LENGTH;
            extra  = rtl ? le->x() - (combo_arrow_length+arrowFrameSize)
                         : w - (combo_arrow_length+arrowFrameSize) - (le->x()+le->width());
            if (extra > 0)
            {
              if (rtl) arrowRect.adjust(0,0,extra,0);
              else arrowRect.adjust(-extra,0,0,0);
            }
          }
        }

        if (opt->subControls & SC_ComboBoxFrame) // frame
        {
          QString status =
                   (option->state & State_Enabled) ?
                    (option->state & State_On) ? "toggled" :
                    (option->state & State_MouseOver) ? "focused" :
                    (option->state & State_Sunken)
                    // to know whether it has keyboard focus
                    || (option->state & State_Selected) ? "pressed" : "normal"
                   : "disabled";
          if (QObject *styleObject = option->styleObject)
          { // hover bug
            if (status == "focused")
            {
              if (/*styleObject->property("_kv_state").toString() == "sunken"
                  && */isCursorOutsideWidget(widget))
              {
                styleObject->setProperty("_kv_hover_bug", true);
                //styleObject->setProperty("_kv_state", QVariant());
                status = (option->state & State_Sunken)
                         || (option->state & State_Selected) ? "pressed" : "normal";
              }
              else
              {
                styleObject->setProperty("_kv_hover_bug", QVariant());
                //styleObject->setProperty("_kv_state", QVariant());
              }
            }
            else
            {
              styleObject->setProperty("_kv_hover_bug", QVariant());
              /*if (status == "normal")
                styleObject->setProperty("_kv_state", QVariant());
              else
                styleObject->setProperty("_kv_state", "sunken");*/ // includes the disabled state too
            }
          }
          if (isWidgetInactive(widget))
            status.append("-inactive");

          int margin = 0; // see CC_ComboBox at subControlRect
          if (isLibreoffice_ && widget == nullptr)
          {
            margin = rtl ? fspec.right+lspec.right : fspec.left+lspec.left;
          }
          else if (opt->editable && !opt->currentIcon.isNull())
          {
            margin = (rtl ? fspec.right+lspec.right : fspec.left+lspec.left) + lspec.tispace - 4;
            margin = qMax(margin, 0);
          }
          // SC_ComboBoxEditField includes the icon too
          o.rect = subControlRect(CC_ComboBox,opt,SC_ComboBoxEditField,widget)
                   .adjusted(rtl ? 0 : -margin,
                             0,
                             rtl ? margin : 0,
                             0);

          if (!(option->state & State_Enabled))
          {
            status.replace(QLatin1String("disabled"),QLatin1String("normal"));
            painter->save();
            painter->setOpacity(DISABLED_OPACITY);
          }
          {
            /* a workaround for a bug in LibreOffice's Qt skin */
            bool libreoffice(false);
            if (isLibreoffice_ && widget == nullptr
                && (option->state & State_Enabled)
                && !(drwaAsLineEdit && opt->editable)
                && enoughContrast(getFromRGBA(lspec.normalColor),
                                  standardPalette().color(QPalette::ButtonText)))
            {
              libreoffice = true;
              painter->save();
              painter->setOpacity(0.5);
            }

            /* don't cover the lineedit area */
            int editWidth = 0;
            if (opt->editable)
            {
              if (!drwaAsLineEdit) // otherwise, the frame and edit field are drawn together as a lineedit
              {
                if (cb != nullptr)
                {
                  if (cb->lineEdit() != nullptr)
                    editWidth = cb->lineEdit()->width();
                }
                else
                  editWidth = o.rect.width();
              }
              if (extra > 0)
                editWidth += extra;

              if (widget && widget->hasFocus())
              {
                if (drwaAsLineEdit)
                {
                  if (isWidgetInactive(widget))
                    status = "focused-inactive"; // impossible
                  else status = "focused";
                }
                else
                {
                  if (isWidgetInactive(widget))
                    status = "pressed-inactive";
                  else status = "pressed";
                }
              }
              else if (drwaAsLineEdit)
              {
                if (status.startsWith(QLatin1String("focused")))
                  status.replace(QLatin1String("focused"),QLatin1String("normal"));
                else if (status.startsWith(QLatin1String("toggled")))
                  status.replace(QLatin1String("toggled"),QLatin1String("normal"));
              }
            }

            /* a workaround for bad codes that change line-edit base color */
            bool colored(cb && cb->lineEdit()
                         && !isPcmanfm_ && opt->editable && leGroup == "LineEdit"
                         && cb->lineEdit()->palette().color(QPalette::Base)
                            != standardPalette().color(cb->lineEdit()->palette().currentColorGroup(),
                                                       QPalette::Base)
                         && cb->lineEdit()->palette().color(QPalette::Base).saturation() > 10);

            if (!opt->editable)
            { // when there isn't enough space (-> CE_ComboBoxLabel)
              int cbH = subControlRect(CC_ComboBox,opt,SC_ComboBoxEditField,widget).height();
              if (cb && !cb->lineEdit())
                cbH = qMin(cb->height(), cbH);
              QFont F(painter->font());
              if (lspec.boldFont) F.setWeight(lspec.boldness);
              QSize txtSize = textSize(F,opt->currentText);
              bool noSpace = /*cb->width() < fspec.left+lspec.left+txtSize.width()+lspec.right+COMBO_ARROW_LENGTH+fspec.right
                             ||*/ cbH < fspec.top+lspec.top+txtSize.height()+fspec.bottom+lspec.bottom;
              if (noSpace)
              {
                fspec.left = qMin(fspec.left,3);
                fspec.right = qMin(fspec.right,3);
                fspec.top = qMin(fspec.top,3);
                fspec.bottom = qMin(fspec.bottom,3);

                if (!hasExpandedBorder(fspec))
                  fspec.expansion = qMin(fspec.expansion, LIMITED_EXPANSION);
                else
                {
                  fspec.leftExpanded = qMin(fspec.leftExpanded,3);
                  fspec.rightExpanded = qMin(fspec.rightExpanded,3);
                  fspec.topExpanded = qMin(fspec.topExpanded,3);
                  fspec.bottomExpanded = qMin(fspec.bottomExpanded,3);
                }

                lspec.left = qMin(lspec.left,2);
                lspec.right = qMin(lspec.right,2);
                lspec.top = qMin(lspec.top,2);
                lspec.bottom = qMin(lspec.bottom,2);
                lspec.tispace = qMin(lspec.tispace,2);
              }
            }
            QRect r = o.rect.adjusted(rtl ? editWidth : 0, 0, rtl ? 0 : -editWidth, 0);
            /* integrate the arrow part if the combo isn't editable or is fully drawn as a lineedit */
            if (!opt->editable || (cb && !cb->lineEdit()) || tspec_.combo_as_lineedit)
              r = r.united(arrowRect);
            if (!(opt->editable
                  // nothing should be drawn here if the lineedit is transparent (as in Cantata)
                  && cb && cb->lineEdit()
                  && cb->lineEdit()->palette().color(cb->lineEdit()->backgroundRole()).alpha() == 0))
            {
              bool fillWidgetInterior(!ispec.hasInterior);
              if (fillWidgetInterior)
              {
                if (drwaAsLineEdit && opt->editable)
                {
                  if (cb == nullptr && lineEditWidget != nullptr)
                  {
                    if (leGroup == "ToolbarLineEdit")
                      fillWidgetInterior = false; // it's on a stylable toolbar
                    else
                    {
                      fillWidgetInterior =
                        hasHighContrastWithContainer(lineEditWidget, standardPalette().color(QPalette::Text));
                    }
                  }
                  else
                  {
                    fillWidgetInterior =
                      hasHighContrastWithContainer(widget,
                                                   cb && cb->lineEdit()
                                                     ? cb->lineEdit()->palette().color(QPalette::Text)
                                                     : standardPalette().color(QPalette::Text));
                  }
                }
                else
                {
                  fillWidgetInterior =
                    hasHighContrastWithContainer(widget, getFromRGBA(getLabelSpec(group).normalColor));
                }
              }

              QStyleOptionComboBox leOpt(*opt);
              if (!drwaAsLineEdit && opt->editable)
              {
                leOpt.rect = o.rect.adjusted(rtl ? 0 : o.rect.width()-editWidth, 0,
                                             rtl ? editWidth-o.rect.width() : 0, 0);
              }
              bool canAnimate(opacityTimer_ && cb && cb->isEnabled());
              bool mouseAnimation(canAnimate
                                  && animatedWidget_ == widget
                                  && opacityTimer_->isActive()
                                  && (!status.startsWith(QLatin1String("normal"))
                                      || ((!opt->editable || !drwaAsLineEdit
                                           || (cb->view() && cb->view()->isVisible()))
                                          && (animationStartState_.startsWith(QLatin1String("focused"))
                                              || animationStartState_.startsWith(QLatin1String("c-"))))));
              bool animate(canAnimate
                           && (mouseAnimation
                               || (animatedWidgetOut_ == widget
                                   && opacityTimerOut_->isActive()
                                   && status.startsWith(QLatin1String("normal")))));
              QString animationStartState(animationStartState_);
              if (animationStartState.startsWith(QLatin1String("c-")))
                animationStartState.remove(0, 2);
              int animationOpacity = animationOpacity_;
              bool animatePanel(!(tspec_.combo_focus_rect
                                  && (!drwaAsLineEdit || !opt->editable)
                                  && (status.startsWith(QLatin1String("normal"))
                                      || status.startsWith(QLatin1String("pressed")))
                                  && (animationStartState.startsWith(QLatin1String("normal"))
                                      || animationStartState.startsWith(QLatin1String("pressed")))));
              QString _status;
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
                  if (animatePanel)
                  {
                    _status = animationStartState;
                    if (tspec_.combo_focus_rect)
                      _status.replace(QLatin1String("pressed"), QLatin1String("normal"));
                    renderFrame(painter,r,fspec,fspec.element+"-"+_status);
                    if (!fillWidgetInterior)
                      renderInterior(painter,r,fspec,ispec,ispec.element+"-"+_status);
                  }
                  if (!drwaAsLineEdit && opt->editable)
                  {
                    if (!mouseAnimation)
                      leOpt.state = State_Enabled | State_Active | State_HasFocus;
                    else
                      leOpt.state = opt->state & (State_Enabled);
                    drawComboLineEdit(&leOpt, painter, cb->lineEdit(), widget,
                                      leGroup, fillWidgetInterior);
                  }
                }
                if (animatePanel)
                {
                  painter->save();
                  painter->setOpacity(static_cast<qreal>(animationOpacity)/100.0);
                }
              }
              _status = status;
              if (tspec_.combo_focus_rect)
                _status.replace(QLatin1String("pressed"), QLatin1String("normal"));
              renderFrame(painter,r,fspec,fspec.element+"-"+_status);
              if (!fillWidgetInterior)
                renderInterior(painter,r,fspec,ispec,ispec.element+"-"+_status);

              /* draw the line-edit part */
              if (!drwaAsLineEdit && opt->editable)
              {
                leOpt.state = (opt->state & (State_Enabled | State_MouseOver | State_HasFocus))
                              | State_KeyboardFocusChange;
                if (animate && !animatePanel)
                {
                  painter->save();
                  painter->setOpacity(static_cast<qreal>(animationOpacity)/100.0);
                }
                if (libreoffice)
                  painter->restore();
                drawComboLineEdit(&leOpt, painter, cb ? cb->lineEdit() : nullptr, widget,
                                  leGroup, fillWidgetInterior);
                if (libreoffice)
                {
                  painter->save();
                  painter->setOpacity(0.5);
                }
                else if (animate && !animatePanel)
                  painter->restore();
              }

              if (animate)
              {
                if (animatePanel)
                  painter->restore();
                if (animationOpacity >= 100)
                {
                  if (!mouseAnimation)
                  {
                    animationStartStateOut_ = status;
                    if (!opt->editable && animatedWidget_ == widget)
                      animationStartState_ = status;
                  }
                  else
                    animationStartState_ = status;
                  // distinguish between toggled combo and toggled button
                  if (animationStartState_.startsWith(QLatin1String("toggled")))
                    animationStartState_ = "c-" + animationStartState_;
                }
              }
              else if (canAnimate)
              {
                if (animatedWidget_ == widget)
                {
                  animationStartState_ = status;
                  if (animationStartState_.startsWith(QLatin1String("toggled")))
                    animationStartState_ = "c-" + animationStartState_;
                }
                if (animatedWidgetOut_ == widget)
                  animationStartStateOut_ = status;
              }
              if (fillWidgetInterior) // widget isn't null
              {
                QColor comboCol;
                if (colored)
                  comboCol = cb->lineEdit()->palette().color(status.contains(QLatin1String("-inactive"))
                                                              ? QPalette::Inactive
                                                              : QPalette::Active,
                                                             QPalette::Base);
                else
                  comboCol = (widget ? widget->palette() : standardPalette())
                             .color(status.contains(QLatin1String("-inactive"))
                                      ? QPalette::Inactive : QPalette::Active,
                                    drwaAsLineEdit ? QPalette::Base : QPalette::Button);
                comboCol.setAlpha(255);
                painter->fillRect(interiorRect(r,fspec), comboCol);
              }
              else if (colored) // cb isn't null
              {
                QColor baseCol = cb->lineEdit()->palette().color(QPalette::Base);
                baseCol.setAlpha(255);
                o.rect = cb->lineEdit()->rect();
                colorizeRect(painter, baseCol, subElementRect(SE_LineEditContents, &o, cb->lineEdit()));
              }

              /* draw combo icon separator if it exists */
              if (!drwaAsLineEdit && opt->editable)
              {
                const QString sepName = fspec.element + "-icon-separator";
                QRect sep;
                if (rtl)
                {
                  sep = QRect(r.x(), y+fspec.top, fspec.left, h-fspec.top-fspec.bottom);
                  painter->save();
                  QTransform m;
                  m.translate(2*sep.x() + sep.width(), 0); m.scale(-1,1);
                  painter->setTransform(m, true);
                }
                else
                  sep = QRect(x+r.width()-fspec.right, y+fspec.top, fspec.right, h-fspec.top-fspec.bottom);
                if (renderElement(painter, sepName+"-"+_status, sep))
                {
                  sep.adjust(0, -fspec.top, 0, -h+fspec.top+fspec.bottom);
                  renderElement(painter, sepName+"-top-"+_status, sep);
                  sep.adjust(0, h-fspec.bottom, 0, h-fspec.top);
                  renderElement(painter, sepName+"-bottom-"+_status, sep);
                }
                if (rtl)
                  painter->restore();
              }
            }
            if (libreoffice) painter->restore();
            /* draw focus rect */
            if (tspec_.combo_focus_rect && !opt->editable
                && (option->state & State_Enabled) && !(option->state & State_On)
                && ((option->state & State_Sunken) || (option->state & State_Selected)))
            {
              if (fspec.hasFocusFrame)
              {
                renderFrame(painter,r,fspec,fspec.element+"-focus");
                if (ispec.hasFocusInterior)
                  renderInterior(painter,r,fspec,ispec,ispec.element+"-focus");
              }
              else
              {
                QRect focusRect;
                if (fspec.expansion > 0)
                  focusRect = labelRect(r, fspec, lspec).adjusted(-2,-2,2,2);
                else
                  focusRect = interiorRect(r, fspec);
                drawFocusRect(painter, focusRect, fspec.focusRectElement);
              }
            }
            /* force label color (as in Krusader) */
            if (cb && (option->state & State_Enabled))
            {
              QList<QLabel*> llist = cb->findChildren<QLabel*>();
              if (!llist.isEmpty())
              {
                QColor col;
                col = getFromRGBA(lspec.normalColor);
                if (status.startsWith(QLatin1String("pressed")))
                  col = getFromRGBA(lspec.pressColor);
                else if (status.startsWith(QLatin1String("toggled")))
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
                      forcePalette(llist.at(i), palette);
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
            else if (status.startsWith(QLatin1String("pressed")))
              state = 3;
            else if (status.startsWith(QLatin1String("toggled")))
              state = 4;
            else if (status.startsWith(QLatin1String("focused")))
              state = 2;

            if (opt->editable && drwaAsLineEdit)
            {
              /* correct the state and colors */
              if (state > 1) state = 1;
              if (leGroup == "LineEdit")
              {
                if (widget)
                {
                  lspec.normalColor = getName(widget->palette().color(QPalette::Active,QPalette::Text));
                  lspec.normalInactiveColor = getName(widget->palette().color(QPalette::Inactive,QPalette::Text));
                }
                else
                {
                  lspec.normalColor = getName(standardPalette().color(QPalette::Active,QPalette::Text));
                  lspec.normalInactiveColor = getName(standardPalette().color(QPalette::Inactive,QPalette::Text));
                }
              }
              else
              {
                const label_spec tlspec = getLabelSpec(QStringLiteral("Toolbar"));
                lspec.normalColor = tlspec.normalColor;
                lspec.normalInactiveColor = tlspec.normalInactiveColor;
              }

              /* when there isn't enough space (-> SE_LineEditContents and drawComboLineEdit) */
              label_spec lspec1 = getLabelSpec(leGroup);
              const size_spec sspec1 = getSizeSpec(leGroup);
              if (cb && cb->lineEdit()
                  && cb->lineEdit()->height()
                     < sizeCalculated(cb->lineEdit()->font(),fspec,lspec1,sspec1,QStringLiteral("W"),QSize()).height())
              {
                lspec.top = qMin(lspec.top,2);
                lspec.bottom = qMin(lspec.bottom,2);
                if (cb->lineEdit()->height()
                    < sizeCalculated(cb->lineEdit()->font(),fspec,lspec1,sspec1,QStringLiteral("W"),QSize()).height())
                {
                  fspec.top = qMin(fspec.left,3);
                  fspec.bottom = qMin(fspec.bottom,3);
                  lspec.top = lspec.bottom = 0;
                }
              }
            }
            QPixmap icn = getPixmapFromIcon(opt->currentIcon,
                                            getIconMode(state,status.contains(QLatin1String("-inactive")),lspec),
                                            iconstate, opt->iconSize);
            QRect iconRect = alignedRect(option->direction,
                                         Qt::AlignVCenter | Qt::AlignLeft,
                                         opt->iconSize,
                                         labelRect(option->rect,fspec,lspec));
            if (!(option->state & State_Enabled))
            {
              qreal opacityPercentage = static_cast<qreal>(hspec_.disabled_icon_opacity);
              if (opacityPercentage < 100.0)
                icn = translucentPixmap(icn, opacityPercentage);
            }
            else if (option->state & State_MouseOver)
            {
              qreal tintPercentage = static_cast<qreal>(hspec_.tint_on_mouseover);
              if (tintPercentage > 0)
                icn = tintedPixmap(option, icn, tintPercentage);
            }
            drawItemPixmap(painter, iconRect, Qt::AlignCenter, icn);
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
        bool isTransient(false);
        if (option->state & State_Enabled)
        {
          QObject *styleObject = option->styleObject;
          if (styleObject && styleHint(SH_ScrollBar_Transient,option,widget))
          {
            qreal opacity = 0.0;
            bool transient(option->activeSubControls == QStyle::SC_None);

            int oldPos = styleObject->property("_q_stylepos").toInt();
            int oldMin = styleObject->property("_q_stylemin").toInt();
            int oldMax = styleObject->property("_q_stylemax").toInt();
            QRect oldRect = styleObject->property("_q_stylerect").toRect();
            //int oldState = styleObject->property("_q_stylestate").toInt();
            uint oldActiveControls = styleObject->property("_q_stylecontrols").toUInt();

            if (!transient
                || oldPos != opt->sliderPosition
                || oldMin != opt->minimum
                || oldMax != opt->maximum
                || oldRect != opt->rect
                //|| oldState != opt->state // animation on focus change
                || oldActiveControls != opt->activeSubControls)
            {
              opacity = 1.0;

              styleObject->setProperty("_q_stylepos", opt->sliderPosition);
              styleObject->setProperty("_q_stylemin", opt->minimum);
              styleObject->setProperty("_q_stylemax", opt->maximum);
              styleObject->setProperty("_q_stylerect", opt->rect);
              //styleObject->setProperty("_q_stylestate", static_cast<int>(opt->state));
              styleObject->setProperty("_q_stylecontrols", static_cast<uint>(opt->activeSubControls));

              ScrollbarAnimation *anim = qobject_cast<ScrollbarAnimation *>(animations_.value(styleObject));
              if (transient)
              {
                if (!anim)
                {
                  anim = new ScrollbarAnimation(ScrollbarAnimation::Deactivating, styleObject);
                  startAnimation(anim);
                }
                else if (anim->mode() == ScrollbarAnimation::Deactivating)
                  anim->setCurrentTime(0); /* the scrollbar was already fading out
                                              but, for example, its position changed */
              }
              else if (anim && anim->mode() == ScrollbarAnimation::Deactivating)
                stopAnimation(styleObject);
            }

            ScrollbarAnimation *anim = qobject_cast<ScrollbarAnimation *>(animations_.value(styleObject));
            if (anim && anim->mode() == ScrollbarAnimation::Deactivating)
              opacity = anim->currentValue();

            if (qFuzzyIsNull(opacity))
              return; // as in "animation.cpp" -> ScrollbarAnimation::updateCurrentTime()

            isTransient = true;
            painter->save();
            painter->setOpacity(opacity);
          }
        }

        QStyleOptionSlider o(*opt);
        o.rect = subControlRect(CC_ScrollBar,opt,SC_ScrollBarSlider,widget);

        QRect r = subControlRect(CC_ScrollBar,opt,SC_ScrollBarGroove,widget);
        bool horiz = (option->state & State_Horizontal);
        /* arrows may be forced by another style, as in Gwenview
          (-> CC_ScrollBar at drawComplexControl) */
        int extent = pixelMetric(PM_ScrollBarExtent,option,widget);
        int arrowSize = 0;
        if (!tspec_.scroll_arrows && (horiz ? r.width() == w-2*extent : r.height() == h-2*extent))
          arrowSize = extent;

        bool rtl(option->direction == Qt::RightToLeft);

        /*******************
          Groove and Slider
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

          if (isTransient)
          {
            if (tspec_.transient_groove)
            {
              QAbstractItemView *iv = qobject_cast<QAbstractItemView*>(getParent(widget,2));
              if (iv && iv->viewport() && iv->viewport()->autoFillBackground())
              {
                /* draw a translucent flat background appropriately
                   (see CE_ScrollBarSlider for the same conditions) */
                int space = r.width() - tspec_.scroll_width;
                if (space > 0) // should be equal to PM_DefaultFrameWidth
                {
                  QRect groove = r;
                  if (iv->frameStyle() & QFrame::StyledPanel)
                  {
                    if (!rtl
                        || horiz) // 90 degrees clockwise + horizontal mirroring
                    {
                      groove.adjust(0, space, -space, -space);
                    }
                    else
                      groove.adjust(space, space, 0, -space);

                    /* a 1px line is drawn below */
                    if (horiz)
                      o.rect.adjust(0, 1, 0, 0);
                    else
                    {
                      if (rtl)
                        o.rect.adjust(0, 0, -1, 0);
                      else
                        o.rect.adjust(1, 0, 0, 0);
                    }
                  }
                  else
                  { // without border, make the groove tighter
                    --space;
                    if (!rtl || horiz)
                      groove.adjust(space, 0, 0, 0);
                    else
                      groove.adjust(0, 0, -space, 0);
                  }

                  QColor col = standardPalette().color(QPalette::Base);
                  if (col.alphaF() < 1.0)
                  {
                    QColor winCol = standardPalette().color(QPalette::Window);
                    winCol.setAlphaF(1.0);
                    col = overlayColor(winCol,col);
                  }
                  col.setAlphaF(0.75);
                  painter->fillRect(groove, col);

                  painter->save();
                  col = standardPalette().color(QPalette::Text);
                  col.setAlphaF(0.2);
                  painter->setPen(col);
                  if (!rtl || horiz)
                    painter->drawLine(groove.topLeft(), groove.bottomLeft());
                  else
                    painter->drawLine(groove.topRight(), groove.bottomRight());
                  painter->restore();
                }
              }
            }
          }
          else if (r.width() <= tspec_.scroll_width) // no disabled groove with transience
          {
            if (!(option->state & State_Enabled))
            {
              painter->save();
              painter->setOpacity(DISABLED_OPACITY);
            }

            QString suffix = "-normal";
            if (isWidgetInactive(widget))
              suffix = "-normal-inactive";
            renderFrame(painter,r,fspec,fspec.element+suffix);
            renderInterior(painter,r,fspec,ispec,ispec.element+suffix);
            if (!(option->state & State_Enabled))
              painter->restore();

            /* to not need any transformation for the horizontal state later,
               we draw the slider here, before restoring the painter, but we
               we draw the glow first because the slider may be rounded */
            if (option->state & State_Enabled)
            {
              const frame_spec sFspec = getFrameSpec(QStringLiteral("ScrollbarSlider"));
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
          }

          drawControl(CE_ScrollBarSlider,&o,painter,widget);

          if ((opt->state & State_HasFocus) && !isTransient)
            drawFocusRect(painter, r, fspec.focusRectElement);

          if (horiz)
            painter->restore();
        }
        /***********
          Add-Line
        ************/
        if (opt->subControls & SC_ScrollBarAddLine)
        {
          if (arrowSize > 0)
          {
            if (horiz)
              o.rect = QRect(rtl ? x : x+w-arrowSize, y,
                             arrowSize, arrowSize);
            else
              o.rect = QRect(x, y+h-arrowSize, arrowSize, arrowSize);
          }
          else
            o.rect = subControlRect(CC_ScrollBar,opt,SC_ScrollBarAddLine,widget);
          drawControl(CE_ScrollBarAddLine,&o,painter,widget);
        }
        /***********
          Sub-Line
        ************/
        if (opt->subControls & SC_ScrollBarSubLine)
        {
          if (arrowSize > 0)
          {
            if (horiz)
              o.rect = QRect(rtl ? x+w-arrowSize : x, y,
                             arrowSize, arrowSize);
            else
              o.rect = QRect(x, y, arrowSize, arrowSize);
          }
          else
            o.rect = subControlRect(CC_ScrollBar,opt,SC_ScrollBarSubLine,widget);
          drawControl(CE_ScrollBarSubLine,&o,painter,widget);
        }

        if (isTransient)
          painter->restore();
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
        int len = pixelMetric(PM_SliderLength,option,widget);
        int thick = pixelMetric(PM_SliderControlThickness,option,widget);
        if (len == thick)
        { // also, work around bad codes
          thick = len = qMin(len, horiz ? h : w);
        }

       /************
        ** Groove **
        ************/
        if (opt->subControls & SC_SliderGroove) // QtColorPicker doesn't need the groove
        {
          /* find the groove rect, taking into account slider_width */
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
            if (!opt->upsideDown)
            {
              full.setWidth(sliderCenter.x());
              empty.adjust(sliderCenter.x(),0,0,0);
            }
            else
            {
              empty.setWidth(sliderCenter.x());
              full.adjust(sliderCenter.x(),0,0,0);
            }
          }
          else
          {
            if (!opt->upsideDown)
            {
              full.setHeight(sliderCenter.y());
              empty.adjust(0,sliderCenter.y(),0,0);
            }
            else
            {
              empty.setHeight(sliderCenter.y());
              full.adjust(0,sliderCenter.y(),0,0);
            }
          }

          fspec.isAttached = true;

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
          if (isWidgetInactive(widget))
            suffix = "-normal-inactive";
          if (option->state & State_Enabled)
          {
            if (!opt->upsideDown)
              fspec.VPos = 1;
            else
              fspec.VPos = -1;
            renderFrame(painter,empty,fspec,fspec.element+suffix);
            renderInterior(painter,empty,fspec,ispec,ispec.element+suffix);
            if (!opt->upsideDown)
              fspec.VPos = -1;
            else
              fspec.VPos = 1;
            suffix.replace(QLatin1String("normal"),QLatin1String("toggled"));
            renderFrame(painter,full,fspec,fspec.element+suffix);
            renderInterior(painter,full,fspec,ispec,ispec.element+suffix);
          }
          else
          {
            painter->save();
            painter->setOpacity(DISABLED_OPACITY);

            fspec.isAttached = false;
            renderFrame(painter,grooveRect,fspec,fspec.element+suffix);
            renderInterior(painter,grooveRect,fspec,ispec,ispec.element+suffix);

            painter->restore();
          }

          if (opt && (opt->state & State_HasFocus))
          {
            const frame_spec fspec1 = getFrameSpec(group);
            if (fspec1.hasFocusFrame)
            {
              renderFrame(painter,grooveRect,fspec1,fspec1.element+"-focus");
              const interior_spec ispec1 = getInteriorSpec(group);
              if (ispec1.hasFocusInterior)
                renderInterior(painter,grooveRect,fspec1,ispec1,ispec1.element+"-focus");
            }
            else
            {
              QRect focusRect = opt->rect;
              if (horiz)
                focusRect.setRect(0, 0, h, w);
              drawFocusRect(painter, focusRect, fspec1.focusRectElement);
            }
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
          if (isWidgetInactive(widget))
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
          else if (ticks == QSlider::NoTicks
                   && ticklessSliderHandleSize_ > 0)
          { // see pixelMetric() -> PM_SliderLength
            ispec.element += "-tickless";
            fspec.element += "-tickless";
          }

          QString status = getState(option,widget);
          bool canAnimate(opacityTimer_
                          && widget && widget->isEnabled() && animatedWidget_ == widget
                          && !qobject_cast<const QAbstractScrollArea*>(widget));
          bool animate(canAnimate && opacityTimer_->isActive());
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
            painter->setOpacity(static_cast<qreal>(animationOpacity_)/100.0);
          }
          renderFrame(painter,r,fspec,fspec.element+"-"+status);
          renderInterior(painter,r,fspec,ispec,ispec.element+"-"+status);
          if (animate)
          {
            painter->restore();
            if (animationOpacity_ >= 100)
              animationStartState_ = status;
          }
          else if (canAnimate)
            animationStartState_ = status;

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
        if (isWidgetInactive(widget))
          suffix = "-inactive";

        if (!(option->state & State_Enabled))
        {
          painter->save();
          painter->setOpacity(DISABLED_OPACITY);
        }

        renderElement(painter,"dial"+suffix,dial);
        if (opt->state & State_HasFocus)
          renderElement(painter,QStringLiteral("dial-focus"),dial);
        renderElement(painter,"dial-handle"+suffix,handle);

        if (const QDial *d = qobject_cast<const QDial*>(widget))
        {
          if (d->notchesVisible())
            renderElement(painter,"dial-notches"+suffix,dial);
        }

        if (!(option->state & State_Enabled))
          painter->restore();

        /* don't draw any focus indicator because
           even a focus circle may be ugly with some themes */
        /*if (opt->state & State_HasFocus)
        {
          QStyleOptionFocusRect fropt;
          fropt.QStyleOption::operator=(*opt);
          fropt.rect = dial;
          drawPrimitive(PE_FrameFocusRect, &fropt, painter, widget);
        }*/
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

          /* do what's done in PE_FrameWindow */
          QColor shadow(0,0,0,55);
          // left
          painter->fillRect(QRect(QPoint(o.rect.left()+1, o.rect.top()), QPoint(o.rect.left()+1, o.rect.bottom())),
                            QColor(255,255,255,15));
          painter->fillRect(QRect(QPoint(o.rect.left(), o.rect.top()), QPoint(o.rect.left(), o.rect.bottom())),
                            shadow);
          // right
          painter->fillRect(QRect(QPoint(o.rect.right()-1, o.rect.top()), QPoint(o.rect.right()-1, o.rect.bottom())),
                            QColor(0,0,0,15));
          painter->fillRect(QRect(QPoint(o.rect.right(), o.rect.top()), QPoint(o.rect.right(), o.rect.bottom())),
                            shadow);

          o.rect = subControlRect(CC_TitleBar,opt,SC_TitleBarLabel,widget);
          QString title = o.text;
          if (!title.isEmpty())
          {
            QFont F(painter->font());
            if (lspec.boldFont) F.setWeight(lspec.boldness);
            QFontMetrics fm(F);
            title = fm.elidedText(title, Qt::ElideRight,
                                  o.rect.width()-(pixelMetric(PM_TitleBarHeight)-4+lspec.tispace)
                                                // titlebars have no frame
                                                -lspec.right-lspec.left,
                                  Qt::TextShowMnemonic);
          }
          int icnSize = pixelMetric(PM_TitleBarHeight) - 4; // 2-px margins for the icon
          QSize iconSize = QSize(icnSize,icnSize);
          renderLabel(option,painter,
                      o.rect,
                      fspec,lspec,
                      Qt::AlignCenter,title,QPalette::WindowText,
                      tbStatus == "normal" ? 1 : 2,
                      false,
                      getPixmapFromIcon(o.icon,Normal,QIcon::Off,iconSize),
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
        /* NOTE: Unfortunately, SP_TitleBarMenuButton is used directly, without a QStyleOption,
                 in qmdisubwindow.cpp -> ControlLabel::updateWindowIcon() */
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
      const KvIconMode iconmode =
        (option->state & State_Enabled) ?
        (option->state & State_Sunken) ? Active :
        (option->state & State_MouseOver) ? Active : Normal
        : Disabled;
      const QIcon::State iconstate =
        (option->state & State_On) ? QIcon::On : QIcon::Off;
      const int btnSize = pixelMetric(PM_TitleBarButtonIconSize, &btnOpt, widget);
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
                                       iconmode,iconstate,QSize(btnSize,btnSize));
        drawItemPixmap(painter, btnOpt.rect, Qt::AlignCenter, pm);
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
                                       iconmode,iconstate,QSize(btnSize,btnSize));
        drawItemPixmap(painter, btnOpt.rect, Qt::AlignCenter, pm);
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
                                                    iconmode,iconstate,QSize(btnSize,btnSize));
        drawItemPixmap(painter, btnOpt.rect, Qt::AlignCenter, pm);
      }

      break;
    }

    case CC_GroupBox: { // added only for correcting RTL text alignment
      const QStyleOptionGroupBox *opt =
        qstyleoption_cast<const QStyleOptionGroupBox*>(option);
      if (opt) {
        // Draw frame
        QRect textRect = subControlRect(CC_GroupBox, opt, SC_GroupBoxLabel, widget);
        QRect checkBoxRect = subControlRect(CC_GroupBox, opt, SC_GroupBoxCheckBox, widget);
        if (!(opt->features & QStyleOptionFrame::Flat) && (opt->subControls & QStyle::SC_GroupBoxFrame))
        {
          QStyleOptionFrame frame;
          frame.QStyleOption::operator=(*opt);
          frame.features = opt->features;
          frame.lineWidth = opt->lineWidth;
          frame.midLineWidth = opt->midLineWidth;
          frame.rect = subControlRect(CC_GroupBox, opt, SC_GroupBoxFrame, widget);
          QRegion region(opt->rect);

          bool ltr = (opt->direction == Qt::LeftToRight);
          QRect finalRect;
          if (opt->subControls & QStyle::SC_GroupBoxCheckBox)
          {
            finalRect = checkBoxRect.united(textRect);
            if (!opt->text.isEmpty())
              finalRect.adjust(ltr ? -3 : 0, 0, ltr ? 0 : 3, 0);
            else
              finalRect.adjust(-3, 0, 3, 0);
          }
          else
            finalRect = textRect;

          region -= finalRect;
          if (!region.isEmpty())
          {
            painter->save();
            painter->setClipRegion(region);
            drawPrimitive(PE_FrameGroupBox, &frame, painter, widget);
            painter->restore();
          }
        }

        // Draw title
        if ((opt->subControls & QStyle::SC_GroupBoxLabel) && !opt->text.isEmpty())
        {
          if (opt->state & State_HasFocus)
          {
            const frame_spec fspec = getFrameSpec(QStringLiteral("GroupBox"));
            int spacing = tspec_.groupbox_top_label ? pixelMetric(PM_CheckBoxLabelSpacing)/2 : 0;
            if (fspec.hasFocusFrame)
            {
              renderFrame(painter,textRect.adjusted(-spacing,0,spacing,0),fspec,fspec.element+"-focus");
              const interior_spec ispec = getInteriorSpec(QStringLiteral("GroupBox"));
              if (ispec.hasFocusInterior)
                renderInterior(painter,textRect.adjusted(-spacing,0,spacing,0),fspec,ispec,ispec.element+"-focus");
            }
            else
            {
              QRect focusRect = textRect.adjusted(-spacing,0,spacing,0);
              drawFocusRect(painter, focusRect, fspec.focusRectElement);
            }
          }

          bool isInactive(isWidgetInactive(widget));
          const label_spec lspec = getLabelSpec(QStringLiteral("GroupBox"));
          QColor col;
          if (!(option->state & State_Enabled))
            col = standardPalette().color(QPalette::Disabled,QPalette::Text);
          else if (option->state & State_MouseOver)
          {
            if (isInactive)
            {
              col = getFromRGBA(lspec.focusInactiveColor);
              if (!col.isValid())
               col = getFromRGBA(lspec.focusColor);
            }
            else
              col = getFromRGBA(lspec.focusColor);
          }
          else
          {
            if (isInactive)
            {
              col = getFromRGBA(lspec.normalInactiveColor);
              if (!col.isValid())
               col = getFromRGBA(lspec.normalColor);
            }
            else
              col = getFromRGBA(lspec.normalColor);
          }

          if (!col.isValid()) // make it always valid
            col = standardPalette().color(QPalette::WindowText);

          int talign = Qt::AlignHCenter | Qt::AlignVCenter;
          if (!styleHint(SH_UnderlineShortcut, opt, widget))
            talign |= Qt::TextHideMnemonic;
          else
            talign |= Qt::TextShowMnemonic;

          if (lspec.boldFont || lspec.italicFont)
          {
            QFont font(painter->font());
            if (lspec.boldFont)
              font.setWeight(lspec.boldness);
            if (lspec.italicFont)
              font.setItalic(true);
            painter->save();
            painter->setFont(font);
          }

          if (lspec.hasShadow)
          {
            QColor shadowColor;
            if (isInactive)
            {
              shadowColor = getFromRGBA(lspec.inactiveShadowColor);
              if (!shadowColor.isValid())
                shadowColor = getFromRGBA(lspec.shadowColor);
            }
            else
              shadowColor = getFromRGBA(lspec.shadowColor);

            /* the shadow should have enough contrast with the text */
            if (enoughContrast(col, shadowColor))
            {
              painter->save();
              if (lspec.a < 255)
                shadowColor.setAlpha(lspec.a);
              painter->setOpacity(shadowColor.alphaF());
              shadowColor.setAlpha(255);
              painter->setPen(shadowColor);
              for (int i=0; i<lspec.depth; i++)
              {
                int xShift = lspec.xshift + i * (lspec.xshift < 0 ? -1 : 1);
                int yShift = lspec.yshift + i * (lspec.yshift < 0 ? -1 : 1);
                painter->drawText(textRect.adjusted(xShift,yShift, xShift,yShift),
                                  talign,opt->text);
              }
              painter->restore();
            }
          }

          painter->save();
          painter->setOpacity(col.alphaF());
          col.setAlpha(255);
          painter->setPen(col);

          drawItemText(painter, textRect, talign,
                       opt->palette, opt->state & State_Enabled, opt->text,
                       QPalette::NoRole);

          painter->restore();
          if (lspec.boldFont || lspec.italicFont)
            painter->restore();
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

int Style::pixelMetric(QStyle::PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
  switch (metric) {
    case PM_ButtonDefaultIndicator : return 0;

    /* we don't need this (in CT_PushButton) but set it because wrong style codes
       may use it instead of consulting CT_PushButton (like in Kate) */
    case PM_ButtonMargin : return 6;

    case PM_ButtonShiftHorizontal :
    case PM_ButtonShiftVertical : return /*tspec_.button_contents_shift ? 1 :*/ 0;

    case PM_DefaultFrameWidth : {
      if (qstyleoption_cast<const QStyleOptionButton*>(option))
        return 0; // not needed but logical (-> CT_PushButton)
      else if (widget && widget->inherits("QComboBoxPrivateContainer")
               && tspec_.combo_menu /*&& !isLibreoffice_*/)
      {
          int hMargin = pixelMetric(PM_MenuHMargin,option,widget);
          if (hMargin == 0) return 0; /* either menu items are spread and there's no shadow or compossiting
                                         or, improbably, shadow and horizontal menu frames are both zero */
          return qMax(hMargin, pixelMetric(PM_MenuVMargin,option,widget));
      }
      const frame_spec fspec = getFrameSpec(QStringLiteral("GenericFrame"));
      /* NOTE: There is an old RTL bug in Qt, due to which, some frames -- especially
               those inside splitters -- may be cut at the right with RTL. Unfortunately,
               here the layout direction may not be reported correctly. As a workaround,
               we could add a 1-px invisible frame and draw the real frame inside it at
               PE_Frame. However, the RTL left frame would still be positioned with a
               1-px offset. So, the width isn't increased by 1 px here. */
      return qMax(qMax(fspec.top,fspec.bottom),qMax(fspec.left,fspec.right));
    }

    /* this isn't used by Kvantum but is set here for a rare case, where
       an intruding stylesheet gives a wrong size hint to a spinbox */
    case PM_SpinBoxFrameWidth : return 4;

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
    case PM_MenuBarHMargin : return 0;

    case PM_MenuBarItemSpacing : {
      /* This is needed for putting menubar-items inside menubar frame.
         FIXME: Actually, PM_MenuBarHMargin and PM_MenuBarVMargin should
                be used but Qt has a bug about menubar corner widgets,
                that would show up badly if they were used. */
      if (tspec_.merge_menubar_with_toolbar)
        return getFrameSpec(QStringLiteral("Toolbar")).left;
      else
        return getFrameSpec(QStringLiteral("MenuBar")).left;
    }

    case PM_MenuPanelWidth :
    case PM_MenuDesktopFrameWidth : return 0;

    case PM_SubMenuOverlap : {
      if (widget && widget->testAttribute(Qt::WA_StyleSheetTarget)) // not drawn by Kvantum
        return 0;
      //if (isLibreoffice_) return QCommonStyle::pixelMetric(metric,option,widget);
      if (QApplication::layoutDirection() == Qt::RightToLeft)
        return 0; // RTL submenu positioning is a mess in Qt5
      int so = tspec_.submenu_overlap;
      /* Even when PM_SubMenuOverlap is set to zero, there's an overlap
         equal to PM_MenuHMargin. So, we make the overlap accurate here. */
      so -= getMenuMargin(true);
      return -so;
    }

    case PM_MenuHMargin :
    case PM_MenuVMargin:
    case PM_MenuTearoffHeight : {
      /* WARNING: It's a nasty Qt issue that, also when a menu is drawn by a stylesheet,
                  this block is called. Luckily, this workaround exists with Qt >= 5.12. */
      if (widget && widget->testAttribute(Qt::WA_StyleSheetTarget))
        return QCommonStyle::pixelMetric(metric,option,widget);

      /* In Qt -> qcombobox.cpp, PM_MenuVMargin is used to add a
         vertical margin to the combo menu but we don't want it. */
      if (qobject_cast<const QComboBox*>(widget)) return 0;

      /* return the stored value if it exists */
      if (widget && drawnMenus_.contains(widget))
      {
        QList<int> l = drawnMenus_.value(widget);
        if (l.size() >= 2)
        {
          if (metric == PM_MenuTearoffHeight)
            return l.at(1) + 8;
          else if (metric == PM_MenuHMargin)
            return l.at(0);
          else
            return l.at(1);
        }
      }

      //if (isLibreoffice_) return QCommonStyle::pixelMetric(metric,option,widget);
      const frame_spec fspec = getFrameSpec(QStringLiteral("Menu"));
      int v = qMax(fspec.top,fspec.bottom);
      int h = 0;
      theme_spec tspec_now = settings_->getCompositeSpec();
      bool shadowDecided(tspec_.shadowless_popup || noComposite_ || !tspec_now.composite);
      if (!(tspec_.spread_menuitems && shadowDecided))
      { // this condition was used in getMenuMargin()
        h = qMax(fspec.left,fspec.right);
        if (!shadowDecided
            && widget
            && translucentWidgets_.contains(widget) // combo menus are included
            /* detached menus may come here because of setSurfaceFormat() */
            && !widget->testAttribute(Qt::WA_X11NetWmWindowTypeMenu))
        {
          v += tspec_now.menu_shadow_depth;
          h += tspec_now.menu_shadow_depth;
          shadowDecided = true;
        }
      }
      /* a margin > 2px could create ugly corners without compositing */
      if (/*!tspec_now.composite || isLibreoffice_
          ||*/ (!widget && option) // QML menus (see PE_PanelMenu)
          /*|| (qobject_cast<const QMenu*>(widget) && !translucentWidgets_.contains(widget))*/)
      {
        v = qMin(2,v);
        if (tspec_.spread_menuitems)
          h = 0;
        else
          h = qMin(2,h);
      }

      /* remember the margins if it's decided whether the menu has shadow or not */
      if (shadowDecided && widget && !drawnMenus_.contains(widget))
      {
        drawnMenus_.insert(widget, QList<int>() << h << v);
        connect(widget, &QObject::destroyed, this, [this, widget]() {
          drawnMenus_.remove(widget);
        });
      }

      if (metric == PM_MenuTearoffHeight)
        /* we set the height of tearoff indicator to be 8px */
        return v + 8;
      else if (metric == PM_MenuHMargin)
        return h;
      else return v;
    }

    case PM_MenuScrollerHeight : {
      if (widget && widget->testAttribute(Qt::WA_StyleSheetTarget)) // not drawn by Kvantum
        return QCommonStyle::pixelMetric(metric,option,widget);
      const indicator_spec dspec = getIndicatorSpec(QStringLiteral("MenuItem"));
      return qMax(pixelMetric(PM_MenuVMargin,option,widget), dspec.size);
    }

    case PM_ToolBarFrameWidth : return tspec_.toolbar_interior_spacing;
    case PM_ToolBarItemSpacing : return tspec_.toolbar_item_spacing;
    case PM_ToolBarHandleExtent : {
      if (tspec_.center_toolbar_handle)
      {
        const indicator_spec dspec = getIndicatorSpec(QStringLiteral("Toolbar"));
        return dspec.size + 3
               /* a minimum margin of 3 px */
               + qMax(3 - pixelMetric(PM_ToolBarItemMargin,option,widget)
                        - tspec_.toolbar_interior_spacing,
                      0);
      }
      return 8;
    }
    case PM_ToolBarSeparatorExtent : {
      int thickness = tspec_.toolbar_separator_thickness;
      if (thickness > 0) return qMax(thickness,2); // see whichGroupedTBtn() for the reason
      const indicator_spec dspec = getIndicatorSpec(QStringLiteral("Toolbar"));
      return qMax(dspec.size,4);
    }
    case PM_ToolBarIconSize : return tspec_.toolbar_icon_size;
    /* we don't use this in standardIcon() -> SP_ToolBarHorizontalExtensionButton
       but it's used in Qt -> qtoolbarextension.cpp, qtoolbarlayout.cpp and qmenubar.cpp */
    case PM_ToolBarExtensionExtent : return 16;
    case PM_ToolBarItemMargin : {
      const frame_spec fspec = getFrameSpec(QStringLiteral("Toolbar"));
      int v = qMax(fspec.top,fspec.bottom);
      int h = qMax(fspec.left,fspec.right);
      return qMax(v,h);
    }

    /* PM_TabBarTabHSpace can provide an appropriate horizontal space
       around the close button but PM_TabBarTabVSpace isn't needed.
       QCommonStyle uses it on the right and left tab sides equally
       but we do so only when the right and left tab buttons exist. */
    case PM_TabBarTabHSpace : {
      const frame_spec fspec = getFrameSpec(QStringLiteral("Tab"));
      int hSpace = qMax(fspec.left,fspec.right)*2;
      if (!widget) // QML
      {
        const label_spec lspec = getLabelSpec(QStringLiteral("Tab"));
        int common = QCommonStyle::pixelMetric(metric,option,widget);
        hSpace += qMax(lspec.left,lspec.right)*2;
        hSpace = qMax(hSpace, common);
      }
      return qMax(hSpace,12);
    }
    case PM_TabBarTabVSpace : {
      if (!widget) // QML
      {
        const frame_spec fspec = getFrameSpec(QStringLiteral("Tab"));
        const label_spec lspec = getLabelSpec(QStringLiteral("Tab"));
        int common = QCommonStyle::pixelMetric(metric,option,widget);
        return qMax(fspec.top+fspec.bottom + lspec.top+lspec.bottom
                      + 1, // WARNING: Why QML tabs are cut by 1px from below?
                    common);
      }
      else return 0;
    }

    case PM_TabBarBaseOverlap :{
      if (tspec_.embedded_tabs)
      {
        const QTabWidget *tw = qobject_cast<const QTabWidget*>(widget);
        if (tw && !tw->documentMode()) // no doc mode and no QML
        {
          if (const QStyleOptionTabWidgetFrame *twf = qstyleoption_cast<const QStyleOptionTabWidgetFrame*>(option))
          {
            switch (twf->shape) {
            case QTabBar::RoundedNorth:
            case QTabBar::TriangularNorth:
            case QTabBar::RoundedSouth:
            case QTabBar::TriangularSouth:
              return twf->tabBarSize.height()/2;
              break;
            case QTabBar::RoundedEast:
            case QTabBar::TriangularEast:
            case QTabBar::RoundedWest:
            case QTabBar::TriangularWest:
              return twf->tabBarSize.width()/2;
              break;
            }
          }
        }
      }
      return 0;
    }

    case PM_TabBarTabOverlap :
    case PM_TabBarBaseHeight :
    case PM_TabBarTabShiftHorizontal :
    case PM_TabBarTabShiftVertical :
    case PM_TabBar_ScrollButtonOverlap : return 0;

    /* With transient scrollbars, we've removed arrows and put
       their frame around their contents. Here, we put transient
       scrollbars on top of scroll area by using a negative
       spacing and a zero overlap (-> qabstractscrollarea.cpp). */
    case PM_ScrollView_ScrollBarOverlap : return 0;
    case PM_ScrollView_ScrollBarSpacing : {
      if (styleHint(SH_ScrollBar_Transient,option,widget))
        return -pixelMetric(PM_ScrollBarExtent,option,widget);
      return 0;
    }

    case PM_TabBarScrollButtonWidth : {
      const frame_spec fspec1 = getFrameSpec(QStringLiteral("PanelButtonTool"));
      const frame_spec fspec2 = getFrameSpec(QStringLiteral("Tab"));
      return qMax(qMin(getIndicatorSpec(QStringLiteral("PanelButtonTool")).size, 12)
                    + qMin(qMax(fspec1.left, fspec1.right),
                           qMax(fspec2.left, fspec2.right))
                    + 2,
                  12);
    }

    case PM_TabCloseIndicatorWidth :
    case PM_TabCloseIndicatorHeight : {
       return getIndicatorSpec(QStringLiteral("Tab")).size;
    }

    case PM_TabBarIconSize :
    case PM_ListViewIconSize :
    case PM_ButtonIconSize : return tspec_.button_icon_size;

    case PM_SmallIconSize : return tspec_.small_icon_size;

    case PM_IconViewIconSize:
    case PM_LargeIconSize : return tspec_.large_icon_size;

    case PM_FocusFrameVMargin :
    case PM_FocusFrameHMargin : {
      int margin = 0;
      /* This is for putting the viewitem's text and icon
         inside its (forced) frame. Apart from viewitems,
         it's only used in CT_ComboBox. */
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
    case PM_RadioButtonLabelSpacing : return 6;

    case PM_SplitterWidth :
      return tspec_.splitter_width;

    /* don't let a transient scrollbar overlap with the view frame */
    case PM_ScrollBarExtent :
    {
      if (styleHint(SH_ScrollBar_Transient,option,widget)
          /* compbo menus always give space to transient scrollbars */
          && (!widget || !widget->window()->inherits("QComboBoxPrivateContainer")))
      {
        return tspec_.scroll_width
               + pixelMetric(PM_DefaultFrameWidth,option,widget);
      }
      return tspec_.scroll_width;
    }
    case PM_ScrollBarSliderMin :
    {
      if (styleHint(SH_ScrollBar_Transient,option,widget)
          && (!widget || !widget->window()->inherits("QComboBoxPrivateContainer")))
      {
        return tspec_.scroll_min_extent
                + 2*pixelMetric(PM_DefaultFrameWidth,option,widget);
      }
      return tspec_.scroll_min_extent;
    }

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
    case PM_SliderControlThickness : {
      int res;
      if (metric == PM_SliderLength)
        res = tspec_.slider_handle_length;
      else
        res = tspec_.slider_handle_width;
      if (themeRndr_ && themeRndr_->isValid())
      {
        const QStyleOptionSlider *opt =
            qstyleoption_cast<const QStyleOptionSlider*>(option);
        if (opt && opt->tickPosition == QSlider::NoTicks)
        {
          if (ticklessSliderHandleSize_ == -1)
          {
            const interior_spec ispec = getInteriorSpec(QStringLiteral("SliderCursor"));
            if (elementExists(ispec.element+"-tickless-normal"))
            {
              if (tspec_.tickless_slider_handle_size > 0)
              {
                res = ticklessSliderHandleSize_ = tspec_.tickless_slider_handle_size;
              }
              else if (metric == PM_SliderLength)
              { // set it to the width if there is no tick and a "-tickless" interior element exists
                res = ticklessSliderHandleSize_ = tspec_.slider_handle_width;
              }
            }
            else
              ticklessSliderHandleSize_ = 0;
          }
          else if (ticklessSliderHandleSize_ > 0)
            res = ticklessSliderHandleSize_;
        }
      }
      return res;
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

    case PM_DockWidgetSeparatorExtent :
      return tspec_.splitter_width; // the same as PM_SplitterWidth

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

    case PM_TitleBarButtonSize : return 16; // sets the space on menubar
    case PM_TitleBarButtonIconSize : return 12; // sets the real button size on menubar

    case PM_TitleBarHeight : {
      // respect the text margins
      const label_spec lspec = getLabelSpec(QStringLiteral("TitleBar"));
      int v = lspec.top + lspec.bottom;
      int b = 0;
      if (widget && lspec.boldFont)
      {
        QFont f = widget->font();
        QSize s = textSize(f, QStringLiteral("W"));
        f.setWeight(lspec.boldness);
        b = (textSize(f, QStringLiteral("W")) - s).height();
      }
      return qMax(widget ? widget->fontMetrics().lineSpacing()+v+b
                           : option ? option->fontMetrics.lineSpacing()+v : 0,
                  24);
    }

    case PM_TextCursorWidth : return 1;

    case PM_HeaderMargin : return 2;

    case PM_ToolTipLabelFrameWidth : {
      const frame_spec fspec = getFrameSpec(QStringLiteral("ToolTip"));

      int v = qMax(fspec.top,fspec.bottom);
      int h = qMax(fspec.left,fspec.right);
      if (!tspec_.shadowless_popup && !noComposite_)
      {
        theme_spec tspec_now = settings_->getCompositeSpec();
        if (tspec_now.composite
            && (!widget || translucentWidgets_.contains(widget)))
        {
          v += tspec_now.tooltip_shadow_depth;
          h += tspec_now.tooltip_shadow_depth;
        }
      }
      /* a margin > 2px could create ugly
         corners without compositing */
      if (/*!tspec_now.composite ||*/ (isLibreoffice_ && widget == nullptr)
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
      if (/*isLibreoffice_
          ||*/ qobject_cast<QAbstractItemView*>(getParent(widget,2)))
      {
        return qMin(QCommonStyle::pixelMetric(PM_IndicatorWidth,option,widget),
                    tspec_.check_size);
      }
      if (qstyleoption_cast<const QStyleOptionMenuItem*>(option)
          || qstyleoption_cast<const QStyleOptionViewItem*>(option))
      {
        return qMin(pixelMetric(PM_SmallIconSize), tspec_.check_size);
      }
      return tspec_.check_size;
    }

    case PM_TreeViewIndentation : return 20;

    default : return QCommonStyle::pixelMetric(metric,option,widget);
  }
}

/*
  To make Qt windows translucent, we should set the surface format of
  their native handles BEFORE they are created, but Qt5 windows are
  often polished AFTER they are created, such that setting the attribute
  "WA_TranslucentBackground" in "Style::polish()" would have no effect.

  Early creation of native handles could have unpredictable side effects,
  especially for menus. However, it seems that setting the attribute
  "WA_TranslucentBackground" in an early stage -- before the widget is
  created -- sets the alpha buffer size to 8 safely and automatically.
*/
void Style::setSurfaceFormat(QWidget *widget) const
{
  if (!widget || noComposite_ || subApp_
      /*|| (isLibreoffice_
          && !widget->inherits("QTipLabel")
          && !qobject_cast<QMenu*>(widget))*/)
  {
    return;
  }

  /* The widget style may change while the app style is still Kvantum
     (as in Qt Designer), in which case, we should remove our forced
     translucency. It will be up to the new style to restore
     translucency if it supports translucent windows. */
  QStyle *ws = widget->style();
  if (qobject_cast<QProxyStyle *>(ws)) // qt5ct uses QProxyStyle
    ws = qobject_cast<QProxyStyle*>(ws)->baseStyle();
  bool otherStyle(ws && ws != this && !ws->objectName().isEmpty());
  if (otherStyle
      && !(isPlasma_ || isOpaque_)
      && widget->testAttribute(Qt::WA_TranslucentBackground)
      && forcedTranslucency_.contains(widget)
      && !(widget->inherits("QTipLabel") || qobject_cast<QMenu*>(widget)))
  {
    widget->setAttribute(Qt::WA_NoSystemBackground, false);
    widget->setAutoFillBackground(true); // Fusion needs this
    return;
  }

  if (widget->testAttribute(Qt::WA_WState_Created)
      || widget->testAttribute(Qt::WA_TranslucentBackground)
      || widget->testAttribute(Qt::WA_NoSystemBackground)
      || widget->autoFillBackground() // video players like kaffeine
      || forcedTranslucency_.contains(widget))
    return;
  bool realWindow(true);
  if (widget->inherits("QTipLabel") || qobject_cast<QMenu*>(widget))
  {
    /* we want translucency and/or shadow for menus and
       tooltips even if the main window is not translucent */
    realWindow = false;
  }
  else
  {
    if (otherStyle || isPlasma_ || isOpaque_ || !widget->isWindow())
      return;
    switch (widget->windowFlags() & Qt::WindowType_Mask) {
      case Qt::Window:
      case Qt::Dialog:
      case Qt::Popup:
      case Qt::Sheet: break;
      default: return;
    }
    if (widget->windowHandle() // too late
        || widget->windowFlags().testFlag(Qt::FramelessWindowHint)
        || widget->windowFlags().testFlag(Qt::X11BypassWindowManagerHint)
        || qobject_cast<QFrame*>(widget) // a floating frame, as in Filelight
        || widget->windowType() == Qt::Desktop
        || widget->testAttribute(Qt::WA_PaintOnScreen)
        || widget->testAttribute(Qt::WA_X11NetWmWindowTypeDesktop)
        || widget->inherits("KScreenSaver")
        || widget->inherits("QSplashScreen"))
     return;

    QWidget *p = widget->parentWidget();
    if (p && (/*!p->testAttribute(Qt::WA_WState_Created) // FIXME: too soon?
              ||*/ qobject_cast<QMdiSubWindow*>(p))) // as in linguist
    {
      return;
    }
    if (QMainWindow *mw = qobject_cast<QMainWindow*>(widget))
    {
      /* it is possible that a main window is inside another one
         (like FormPreviewView in linguist), in which case,
         translucency could cause weird effects */
      if (p) return;
      /* stylesheets with background can cause total transparency */
      QString ss = mw->styleSheet();
      if (!ss.isEmpty() && ss.contains(QLatin1String("background")))
        return;
      if (QWidget *cw = mw->centralWidget())
      {
        if (cw->autoFillBackground())
          return;
        ss = cw->styleSheet();
        if (!ss.isEmpty() && ss.contains(QLatin1String("background")))
          return; // as in smplayer
      }
    }
  }

  theme_spec tspec_now = settings_->getCompositeSpec();
  if (!tspec_now.composite
      || (realWindow && !tspec_now.translucent_windows))
  {
    return;
  }

  /* this will set the alpha buffer size to 8 in a safe way */
  widget->setAttribute(Qt::WA_TranslucentBackground);
  /* distinguish forced translucency from hard-coded translucency */
  forcedTranslucency_.insert(widget);
  connect(widget, &QObject::destroyed, this, &Style::noTranslucency);
}

int Style::styleHint(QStyle::StyleHint hint,
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
    case SH_MenuBar_AltKeyNavigation : return false;


    case SH_ItemView_ShowDecorationSelected : {
      /* give all available space to the text when editing a view-item */
      return (qobject_cast<const QLineEdit*>(widget) != nullptr);
    }

    /* if this is set to true, the left key might not hide child items */
    case SH_ItemView_ArrowKeysNavigateIntoChildren : return false;

    case SH_ItemView_ActivateItemOnSingleClick : {
      switch (tspec_.click_behavior) {
        case 0 : return QCommonStyle::styleHint(hint,option,widget,returnData);
        case 1 : return true;
        case 2 : return false;
        default :return QCommonStyle::styleHint(hint,option,widget,returnData);
      }
    }

    case SH_ToolBar_Movable : return false;

    case SH_ToolButton_PopupDelay : return 250;
    case SH_Menu_SubMenuPopupDelay : return tspec_.submenu_delay;
    case SH_Menu_Scrollable : {
      if (widget && widget->testAttribute(Qt::WA_StyleSheetTarget)) // not drawn by Kvantum
        return QCommonStyle::styleHint(hint,option,widget,returnData);
      return tspec_.scrollable_menu;
    }
    case SH_Menu_SloppySubMenus : return true;
    /* QMenu has some bugs regarding this timeout. It's also
       used when reentering a menuitem with submenu and even
       when SH_Menu_SubMenuPopupDelay is negative. Here, we
       set it to 20s for negative delays as a workaround.
       NOTE: This has no effect with later versions of Qt5. */
    case SH_Menu_SubMenuSloppyCloseTimeout : {
      return tspec_.submenu_delay < 0 ? 20000 : 1000;
    }

    case SH_Menu_SubMenuUniDirection : return false;
    case SH_Menu_SubMenuUniDirectionFailCount : return 1;

    case SH_Menu_SubMenuResetWhenReenteringParent : return false;

    /* a false value for this closes submenus when the cursor
       leaves all menus, which is an annoying behavior */
    case SH_Menu_SubMenuDontStartSloppyOnLeave : return true;

    case SH_Menu_SubMenuSloppySelectOtherActions : return true;
    /* when set to true, only the last submenu is
       hidden on clicking anywhere outside the menu */
    case SH_Menu_FadeOutOnHide : return false;

    case SH_ComboBox_ListMouseTracking :
    case SH_Menu_MouseTracking : return true;

    case SH_ComboBox_PopupFrameStyle: return QFrame::StyledPanel | QFrame::Plain;
    case SH_ComboBox_Popup : return tspec_.combo_menu /*&& !isLibreoffice_*/;

    case SH_MenuBar_MouseTracking :
      return tspec_.menubar_mouse_tracking;

    case SH_TabBar_Alignment : {
      if (tspec_.left_tabs)
      {
        const QTabWidget *tw = qobject_cast<const QTabWidget*>(widget);
        if (!tw || tw->documentMode())
        {
          if (tspec_.center_doc_tabs)
            return Qt::AlignCenter;
        }
        else if (tspec_.center_normal_tabs)
          return Qt::AlignCenter;
        return Qt::AlignLeft;
      }
      else
        return Qt::AlignCenter;
    }

    case SH_TabBar_ElideMode : return Qt::ElideNone; // don't interfere with CE_TabBarTabLabel

    //case SH_ScrollBar_BackgroundMode : return Qt::OpaqueMode;

    case SH_ScrollBar_ContextMenu : return !tspec_.transient_scrollbar; // because of the hover bug
    case SH_ScrollBar_LeftClickAbsolutePosition : return !hspec_.middle_click_scroll;
    case SH_ScrollBar_MiddleClickAbsolutePosition : return hspec_.middle_click_scroll;

    case SH_ScrollBar_Transient : {
      if (qobject_cast<const QGraphicsView*>(widget) && widget->hasMouseTracking())
        return false; // prevent artifacts in QGraphicsView
      return tspec_.transient_scrollbar;
    }

    case SH_Slider_StopMouseOverSlider : return true;
    case SH_Slider_AbsoluteSetButtons : return Qt::LeftButton;
    case SH_Slider_PageSetButtons : return Qt::MiddleButton;

    /* scrollbar_in_view is always false when transient_scrollbar is true (see "ThemeConfig.cpp") */
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
      const label_spec lspec = getLabelSpec(QStringLiteral("GroupBox"));
      QColor col;
      if (option && !(option->state & State_Enabled))
      {
        col = standardPalette().color(QPalette::Disabled,QPalette::Text);
        return col.rgba();
      }
      else if (option && (option->state & State_MouseOver))
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
      int res = QCommonStyle::styleHint(hint,option,widget,returnData);
      if (res < 0 || res >= Qt::ToolButtonFollowStyle) // a bug in qt5ct
        res = Qt::ToolButtonIconOnly;
      switch (tspec_.toolbutton_style) {
        case 0 : return res;
        case 1 : return Qt::ToolButtonIconOnly;
        case 2 : return Qt::ToolButtonTextOnly;
        case 3 : return Qt::ToolButtonTextBesideIcon;
        case 4 : return Qt::ToolButtonTextUnderIcon;
        default :return res;
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

    case SH_DialogButtonLayout: {
      switch (tspec_.dialog_button_layout) {
        case 0 : return QCommonStyle::styleHint(hint,option,widget,returnData);
        case 1 : return QDialogButtonBox::KdeLayout;
        case 2 : return QDialogButtonBox::GnomeLayout;
        case 3 : return QDialogButtonBox::MacLayout;
        case 4 : return QDialogButtonBox::WinLayout;
        case 5 : return QDialogButtonBox::AndroidLayout;
        default :return QCommonStyle::styleHint(hint,option,widget,returnData);
      }
    }

    case SH_ToolTip_WakeUpDelay : {
      if (tspec_.tooltip_delay >= 0)
        return tspec_.tooltip_delay;
      return QCommonStyle::styleHint(hint,option,widget,returnData);
    }
    case SH_ToolTip_FallAsleepDelay : {
      if (tspec_.tooltip_delay >= 0)
        return 0;
      return QCommonStyle::styleHint(hint,option,widget,returnData);
    }

    case SH_ToolBox_SelectedPageTitleBold : return true;

    case SH_SpinControls_DisableOnBounds: return true;
    case SH_SpinBox_ButtonsInsideFrame : return true;

    /* forms */
    case SH_FormLayoutFieldGrowthPolicy : {
      if (hspec_.centered_forms)
        return QFormLayout::ExpandingFieldsGrow;
      return QFormLayout::AllNonFixedFieldsGrow;
    }
    case SH_FormLayoutFormAlignment : return Qt::AlignLeft | Qt::AlignTop;
    case SH_FormLayoutLabelAlignment : {
      if (hspec_.centered_forms)
        return Qt::AlignRight;
      return Qt::AlignLeft;
    }

    default : {
      if (hint >= SH_CustomBase
          && widget && widget->objectName() == "CE_CapacityBar")
      {
        return CE_Kv_KCapacityBar;
      }
      return QCommonStyle::styleHint(hint,option,widget,returnData);
    }
  }
}

QStyle::SubControl Style::hitTestComplexControl(QStyle::ComplexControl control,
                                                const QStyleOptionComplex *option,
                                                const QPoint &position,
                                                const QWidget *widget) const
{
  return QCommonStyle::hitTestComplexControl(control,option,position,widget);
}

int Style::extraComboWidth(const QStyleOptionComboBox *opt, bool hasIcon) const
{
  if (opt == nullptr) return 0;
  int res = 0;

  const frame_spec fspec = getFrameSpec(QStringLiteral("ComboBox"));
  const size_spec sspec = getSizeSpec(QStringLiteral("ComboBox"));
  const label_spec lspec = getLabelSpec(QStringLiteral("ComboBox"));
  const frame_spec fspec1 = getFrameSpec(QStringLiteral("LineEdit"));
  const label_spec lspec1 = getLabelSpec(QStringLiteral("LineEdit"));
  const size_spec sspec1 = getSizeSpec(QStringLiteral("LineEdit"));
  bool rtl(opt->direction == Qt::RightToLeft);

  /* We don't add COMBO_ARROW_LENGTH (=20) to the width because
     we've already added qMax(23,X) to it (as in qcommonstyle.cpp)
     but will consider square arrows below.

     We want that the left icon respect frame width,
     text margin and text-icon spacing in the editable mode too. */
  res = fspec.left+fspec.right
        + (opt->editable
             ? lspec1.left+lspec1.right
               + (rtl ? fspec1.right + fspec.right + (hasIcon ? lspec.right : 0)
                      : fspec1.left + fspec.left + (hasIcon ? lspec.left : 0))
             : lspec.left+lspec.right)
        + (hasIcon ? lspec.tispace : 0);

  if (opt->editable)
  {
    if (tspec_.combo_as_lineedit || tspec_.square_combo_button)
    {
      res += rtl ? (fspec1.right > fspec.right ? fspec1.right-fspec.right : 0)
                 : (fspec1.left > fspec.left ? fspec1.left-fspec.left : 0);
    }
    res += sspec.incrementW ? qMax(sspec.minW, sspec1.incrementW ? sspec1.minW : 0)
                            : (sspec1.incrementW ? sspec1.minW : 0);
  }
  else if (sspec.incrementW)
    res += sspec.minW;

  return res;
}

QSize Style::sizeFromContents(QStyle::ContentsType type,
                              const QStyleOption *option,
                              const QSize &contentsSize,
                              const QWidget *widget) const
{
  QSize s = QSize(0,0);

  switch (type) {
    case CT_LineEdit : {
      if (qobject_cast<QAbstractItemView*>(getParent(widget,2))) // when editing itemview texts
        return contentsSize + QSize(2,2);

      QFont f;
      if (widget) f = widget->font();
      else f = QApplication::font();

      const QString group = "LineEdit";
      const frame_spec fspec = getFrameSpec(group);
      size_spec sspec = getSizeSpec(group);
      /* the label spec is only used for vertical spacing */
      const label_spec lspec = getLabelSpec(group);

      int clearBtnSize = 0;
      const QLineEdit *le = qobject_cast<const QLineEdit*>(widget);
      if (le && le->isClearButtonEnabled())
        clearBtnSize = pixelMetric(PM_SmallIconSize);

      QSize defaultSize = QCommonStyle::sizeFromContents(type,option,contentsSize,widget);
      int minW = sspec.minW;
      sspec.minW = 0;
      s = sizeCalculated(f,fspec,lspec,sspec,QStringLiteral("W"),QSize(clearBtnSize,clearBtnSize));
      s.rwidth() = qMax(defaultSize.width() + lspec.left+lspec.right + qMax(fspec.left+fspec.right-2, 0),
                        s.width());
      s.rwidth() = qMax(minW + (sspec.incrementW ? s.width() : 0),
                        s.width());
      if (le)
      {
        QMargins m = le->textMargins();
        s.rheight() += qMax(m.top(), 0) + qMax(m.bottom(), 0);
      }
      /* defaultSize may be a bit thicker because of frame, which doesn't matter
         to us. However, we'll make an exception for widgets like KCalcDisplay. */
      if (s.height() < defaultSize.height() && le == nullptr)
        s.rheight() = defaultSize.height();

      return s;
    }

    case CT_SpinBox : {
      /* Here we don't use defaultSize because, for Qt4, it was based on spinbox size hint,
         which in turn was based on SC_SpinBoxEditField (Qt4 -> qabstractspinbox.cpp).
         That's corrected in Qt5 but the following method is always reliable. */
      const QStyleOptionSpinBox *opt = qstyleoption_cast<const QStyleOptionSpinBox*>(option);
      const QAbstractSpinBox *sb = qobject_cast<const QAbstractSpinBox*>(widget);
      bool hasButtons = true;
      if (sb)
      {
        if (sb->buttonSymbols() == QAbstractSpinBox::NoButtons)
          hasButtons = false;
      }
      else if (opt && opt->buttonSymbols == QAbstractSpinBox::NoButtons)
        hasButtons = false;
      frame_spec fspec = getFrameSpec(QStringLiteral("LineEdit"));
      bool isKisSlider1((widget && (widget->inherits("KisIntParseSpinBox")
                                    || widget->inherits("KisDoubleParseSpinBox")))); // Krita 5.0.0
      if ((tspec_.vertical_spin_indicators || isKisSlider1) && hasButtons)
      {
        fspec.left = qMin(fspec.left,3);
        fspec.right = qMin(fspec.right,3);
        fspec.top = qMin(fspec.top,3);
        fspec.bottom = qMin(fspec.bottom,3);
      }
      const label_spec lspec = getLabelSpec(QStringLiteral("LineEdit"));
      const size_spec sspecLE = getSizeSpec(QStringLiteral("LineEdit"));
      const frame_spec fspec1 = getFrameSpec(QStringLiteral("IndicatorSpinBox"));
      const size_spec sspec = getSizeSpec(QStringLiteral("IndicatorSpinBox"));

      if (sb)
      {
        int extraWidth = 0;
        if (!hasButtons) // as in qpdfview
        {
          extraWidth = fspec.left + lspec.left + fspec.right + lspec.right
                       + (sspecLE.incrementW ? sspecLE.minW : 0);
        }
        else if (tspec_.vertical_spin_indicators || isKisSlider1)
          extraWidth = fspec.left + tspec_.spin_button_width + fspec.right;
        else
        {
          extraWidth = fspec.left + lspec.left + 2*tspec_.spin_button_width
                       + fspec1.right
                       + (sspecLE.incrementW ? sspecLE.minW : 0);
        }
        extraWidth += 2; // cursor padding
        QString maxTxt = spinMaxText(sb);
        if (!maxTxt.isEmpty())
        {
          maxTxt += QLatin1Char(' '); // QAbstractSpinBox::sizeHint() adds a space
          QSize txtSize = textSize(sb->font(),maxTxt);
          txtSize.rheight() += txtSize.height() % 2; // for vertical centering
          s = txtSize
              + QSize(extraWidth,
                      lspec.top + lspec.bottom
                      + (tspec_.vertical_spin_indicators || isKisSlider1 || !hasButtons
                         ? fspec.top + fspec.bottom
                         : (qMax(fspec1.top,fspec.top) + qMax(fspec1.bottom,fspec.bottom))));
        }
        else
        {
          /* This is for some apps (like Kdenlive with its
             TimecodeDisplay) that subclass only QAbstractSpinBox. */
          if (tspec_.vertical_spin_indicators || isKisSlider1 || !hasButtons)
            s.rwidth() = sb->minimumWidth();
          else
            s.rwidth() = sb->minimumWidth() + tspec_.spin_button_width;
        }

        s = s.expandedTo(QSize(0, // minW doesn't have meaning here
                               sspec.minH + (sspec.incrementH ? s.height() : 0)));
      }

      QSize defaultSize(0, 0); // is calculated as in qcommonstyle.cpp
      if (opt)
      {
        int buttonWidth;
        if (!hasButtons)
          buttonWidth = 0;
        else
        {
          /* the measure we used in CC_SpinBox at drawComplexControl() (for QML) */
          if (tspec_.vertical_spin_indicators || (!widget && opt->frame) || isKisSlider1)
            buttonWidth = tspec_.spin_button_width + qMin(fspec.right,3);
          else
            buttonWidth = 2*tspec_.spin_button_width + fspec1.right;
        }
        defaultSize = contentsSize + QSize(buttonWidth, 0);
        if (isLibreoffice_ && widget == nullptr)
        { // LibreOffice's (vertical) margins are too small
          defaultSize += QSize(lspec.left+lspec.right + fspec.left,
                               lspec.top+lspec.bottom + fspec.top+fspec.bottom);
        }
      }

      return s.expandedTo(defaultSize);
    }

    case CT_ComboBox : {
      const QStyleOptionComboBox *opt =
          qstyleoption_cast<const QStyleOptionComboBox*>(option);

      if (opt) {
        const QString group = "ComboBox";
        const frame_spec fspec = getFrameSpec(group);
        const size_spec sspec = getSizeSpec(group);
        label_spec lspec = getLabelSpec(group);
        const frame_spec fspec1 = getFrameSpec(QStringLiteral("LineEdit"));

        QFont f;
        if (widget) f = widget->font();
        else f = QApplication::font();
        if (lspec.boldFont) f.setWeight(lspec.boldness);

        int clearBtnSize = 0;
        bool hasIcon = false;
        if (const QComboBox *cb = qobject_cast<const QComboBox*>(widget))
        {
          const QLineEdit *le = cb->lineEdit();
          if (le && le->isClearButtonEnabled())
            clearBtnSize = pixelMetric(PM_SmallIconSize);
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

        /* almost as in QCommonStyle (needed in subControlRect -> CC_ComboBox) */
        int comboWidth = contentsSize.width()
                         + qMax(23, 2*(pixelMetric(PM_FocusFrameHMargin) + 1)
                                    + pixelMetric(PM_ScrollBarExtent,option,widget))
                         + (opt->frame ? pixelMetric(PM_ComboBoxFrameWidth, opt, widget)*2 : 0);

        s = QSize(comboWidth,
                  sizeCalculated(f,fspec,lspec,sspec,QStringLiteral("W"),
                                 (hasIcon ? opt->iconSize : QSize())
                                   .expandedTo(QSize(clearBtnSize, clearBtnSize))).height());
        if (opt->editable)
        { // consider the top and bottom frames of lineedits inside editable combos
          s.rheight() += (fspec1.top > fspec.top ? fspec1.top-fspec.top : 0)
                         + (fspec1.bottom > fspec.bottom ? fspec1.bottom-fspec.bottom : 0);
        }

        s.rwidth() += extraComboWidth(opt, hasIcon);

        if (!sspec.incrementW && s.width() < sspec.minW)
          s.setWidth(sspec.minW);

        int arrowFrameSize = option->direction == Qt::RightToLeft
                               ? tspec_.combo_as_lineedit ? fspec1.left : fspec.left
                               : tspec_.combo_as_lineedit ? fspec1.right : fspec.right;
        if (tspec_.square_combo_button && COMBO_ARROW_LENGTH + arrowFrameSize < s.height())
          s.rwidth() += s.height() - COMBO_ARROW_LENGTH - arrowFrameSize;

        return s;
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
        size_spec sspec = getSizeSpec(group);
        label_spec lspec = getLabelSpec(group);
        const QString txt = opt->text;

        if (qobject_cast<QAbstractItemView*>(getParent(widget,2)))
        {
          lspec.left = lspec.right = lspec.top = lspec.bottom = qMin(lspec.left,2);
          fspec.left = qMin(fspec.left,3);
          fspec.right = qMin(fspec.right,3);
          fspec.top = qMin(fspec.top,3);
          fspec.bottom = qMin(fspec.bottom,3);
          lspec.tispace = qMin(lspec.tispace,3);
        }

        if (!txt.isEmpty() && qobject_cast<const QAbstractItemView*>(widget))
        { // as in Kate's preferences for its default text style
          fspec.left = qMin(fspec.left,3);
          fspec.right = qMin(fspec.right,3);
          fspec.top = qMin(fspec.top,3);
          fspec.bottom = qMin(fspec.bottom,3);
        }

       /*
          Like with CT_ToolButton, don't use sizeCalculated()!
       */

        /* Qt adds PM_ButtonMargin plus twice PM_DefaultFrameWidth to the width and
           also the height but doesn't add anything (PM_ButtonDefaultIndicator = 0)
           for auto-default (-> qcommonstyle.cpp). We ignore PM_ButtonMargin, set
           PM_DefaultFrameWidth to zero for pushbuttons, add our frames and spacings
           instead, and take into account the indicator and auto-defaultness. */
        s = contentsSize + QSize (0, contentsSize.height() % 2)
            + QSize(fspec.left+fspec.right+lspec.left+lspec.right,
                    fspec.top+fspec.bottom+lspec.top+lspec.bottom)
            + QSize(opt->features & QStyleOptionButton::HasMenu
                      ? dspec.size+BUTTON_ARROW_MARGIN : 0,
                    0);
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
          if (lspec.hasShadow)
            s = s + QSize(qMax(qAbs(lspec.xshift)-1,0)+qMax(lspec.depth-1,0),
                          qMax(qAbs(lspec.yshift)-1,0)+qMax(lspec.depth-1,0));
          if (!opt->icon.isNull())
            s = s + QSize(lspec.tispace, 0);
          /* take into account the boldness of default button text
             and also the possibility of boldness in general */
          if ((opt->features & QStyleOptionButton::AutoDefaultButton) || lspec.boldFont)
          {
            QFont f;
            if (widget) f = widget->font();
            else f = QApplication::font();
            QSize sNormal = textSize(f, txt);
            f.setWeight(lspec.boldness);
            QSize sBold = textSize(f, txt);
            QSize delta = sBold - (opt->icon.isNull() || sNormal.height() >= opt->iconSize.height()
                                     ? sNormal : QSize(sNormal.width(), opt->iconSize.height()));
            s = s + delta.expandedTo(QSize(0,0));
          }
          // consider a global min. width for push buttons
          s.rwidth() = qMax(s.width(),
                            2*qMax(qMax(fspec.top,fspec.bottom),qMax(fspec.left,fspec.right))
                              + opt->fontMetrics.horizontalAdvance("W") * 6);
        }

        s = s.expandedTo(QSize(sspec.minW + (sspec.incrementW ? s.width() : 0),
                               sspec.minH + (sspec.incrementH ? s.height() : 0)));

        if (!txt.isEmpty())
          s.rwidth() = qMax(s.width(), 80); // not smaller than 80px with text
        else
          s.rwidth() = qMax(s.width(), s.height()); // don't let width < height

        return s;
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
        label_spec lspec = getLabelSpec(group);
        size_spec sspec;
        if (/*isLibreoffice_
            ||*/ !widget // QML
            || qobject_cast<QAbstractItemView*>(getParent(widget,2)))
        { // see PM_IndicatorWidth (just for focus rectangle)
          lspec.top = qMin(lspec.top,2);
          lspec.bottom = qMin(lspec.bottom,2);
          default_size_spec(sspec);
        }
        else
        {
          sspec = getSizeSpec(group);
          sspec.minW = 0; // minW doesn't have meaning here
        }

        QFont f;
        if (widget) f = widget->font();
        else f = QApplication::font();
        if (lspec.boldFont) f.setWeight(lspec.boldness);

        int ih = pixelMetric(PM_ExclusiveIndicatorHeight,option,widget);
        if (!opt->text.isEmpty() || !opt->icon.isNull())
        s = sizeCalculated(f,fspec,lspec,sspec,opt->text, opt->icon.isNull() ? QSize() : opt->iconSize)
            + QSize(pixelMetric(PM_RadioButtonLabelSpacing), 0);
        s = s + QSize(pixelMetric(PM_ExclusiveIndicatorWidth,option,widget), (s.height() < ih ? ih : 0));
      }

      return s.expandedTo(QCommonStyle::sizeFromContents(type,option,contentsSize,widget));
    }

    case CT_CheckBox : {
      const QStyleOptionButton *opt =
        qstyleoption_cast<const QStyleOptionButton*>(option);

      if (opt) {
        const QString group = "CheckBox";
        frame_spec fspec;
        default_frame_spec(fspec);
        label_spec lspec = getLabelSpec(group);
        size_spec sspec;
        if (/*isLibreoffice_
            ||*/ !widget // QML
            || qobject_cast<QAbstractItemView*>(getParent(widget,2)))
        { // see PM_IndicatorWidth (just for focus rectangle)
          lspec.top = qMin(lspec.top,2);
          lspec.bottom = qMin(lspec.bottom,2);
          default_size_spec(sspec);
        }
        else
        {
          sspec = getSizeSpec(group);
          sspec.minW = 0; // minW doesn't have meaning here
        }

        QFont f;
        if (widget) f = widget->font();
        else f = QApplication::font();
        if (lspec.boldFont) f.setWeight(lspec.boldness);

        int ih = pixelMetric(PM_IndicatorHeight,option,widget);
        if (!opt->text.isEmpty() || !opt->icon.isNull())
          s = sizeCalculated(f,fspec,lspec,sspec,opt->text, opt->icon.isNull() ? QSize() : opt->iconSize)
              + QSize(pixelMetric(PM_CheckBoxLabelSpacing), 0);
        s = s + QSize(pixelMetric(PM_IndicatorWidth,option,widget), (s.height() < ih ? ih : 0));
      }

      return s.expandedTo(QCommonStyle::sizeFromContents(type,option,contentsSize,widget));
    }

    case CT_MenuItem : {
      const QStyleOptionMenuItem *opt =
        qstyleoption_cast<const QStyleOptionMenuItem*>(option);

      if (opt) {
        const QString group = "MenuItem";
        const frame_spec fspec = getFrameSpec(group);
        const label_spec lspec = getLabelSpec(group);
        const size_spec sspec = getSizeSpec(group);

        const QComboBox *combo = qobject_cast<const QComboBox*>(widget);
        int smallIconSize = pixelMetric(PM_SmallIconSize);

        QSize iconSize;
        if (combo != nullptr)
          iconSize = combo->iconSize();
        else
        {
          int extra = opt->maxIconWidth > smallIconSize ? 2 : 0; // just for backward compatibility
          iconSize = QSize(smallIconSize+extra, smallIconSize+extra);
        }

        int lxqtMenuIconSize = hspec_.lxqtmainmenu_iconsize;
        if (lxqtMenuIconSize >= 16
            && lxqtMenuIconSize != smallIconSize
            && qobject_cast<const QMenu*>(widget))
        {
          if (widget->objectName() == "TopLevelMainMenu")
            iconSize = QSize(lxqtMenuIconSize, lxqtMenuIconSize);
          else if (QMenu *menu = qobject_cast<QMenu*>(getParent(widget, 1)))
          {
            if (menu->objectName() == "TopLevelMainMenu")
              iconSize = QSize(lxqtMenuIconSize, lxqtMenuIconSize);
            else
            {
              while (qobject_cast<QMenu*>(getParent(menu, 1)))
              {
                menu = qobject_cast<QMenu*>(getParent(menu, 1));
                if (menu->objectName() == "TopLevelMainMenu")
                {
                  iconSize = QSize(lxqtMenuIconSize, lxqtMenuIconSize);
                  break;
                }
              }
            }
          }
        }

        if (opt->menuItemType == QStyleOptionMenuItem::Separator)
          s = QSize(contentsSize.width(), tspec_.menu_separator_height);
        else
        {
          QFont f = opt->font; // some apps (like TeXstudio) use special fonts (see CE_MenuItem)
          QString txt = opt->text;
          txt.replace('\n', ' '); // may be needed with combos
          int shortcutSpace = 0;
          if (combo == nullptr)
          {
            int tabIndex = txt.indexOf(QLatin1Char('\t'));
            if (tabIndex > 0)
            {
              txt = txt.left(tabIndex);
              shortcutSpace = QFontMetrics(f).height() * 3;
            }
          }
          s = sizeCalculated(f,fspec,lspec,sspec,txt,
                             (opt->icon.isNull() || hspec_.iconless_menu) ? QSize() : iconSize);
          if (txt.isEmpty() && combo == nullptr)
          { // workaround for bad app codes that don't give the required info
            s.setWidth(qMax(s.width(), contentsSize.width()
                                       + fspec.left+fspec.right+lspec.left+lspec.right
                                       + (!opt->icon.isNull() && !hspec_.iconless_menu && iconSize.width() > 0
                                            ? iconSize.width() + lspec.tispace : 0)));
            s.setHeight(qMax(s.height(), contentsSize.height()
                                         + fspec.top+fspec.bottom+lspec.top+lspec.bottom));
            if (!opt->icon.isNull() && !hspec_.iconless_menu)
              s.setHeight(qMax(s.height(), iconSize.height()));
          }
          else
            s.setWidth(qMax(s.width(), contentsSize.width()));
          s.setHeight(qMax(s.height(), opt->fontMetrics.height() + 8)); // as in qcommonstyle.cpp
          s.rwidth() += shortcutSpace;
        }

        /* even when there's no icon, another menuitem may have icon
           and that isn't taken into account with sizeCalculated() */
        if(opt->icon.isNull() && !hspec_.iconless_menu
           /* QML menus set maxIconWidth to 0, although they have icon */
           && (opt->maxIconWidth > 0 || widget == nullptr))
        {
          s.rwidth() += iconSize.width() + lspec.tispace;
        }

        if (opt->menuItemType == QStyleOptionMenuItem::SubMenu)
        {
          const indicator_spec dspec = getIndicatorSpec(group);
          /* we also add 2px for the right margin. */
          s.rwidth() += dspec.size + lspec.tispace + 2;
          s.setHeight(qMax(s.height(), dspec.size));
        }

        if (!(tspec_.combo_menu
              && tspec_.hide_combo_checkboxes
              && combo != nullptr) // see Qt -> qcombobox_p.h -> QComboMenuDelegate
            && ((widget && opt->menuHasCheckableItems)
                /* QML menus only use checkType, while
                   the default value of menuHasCheckableItems is true. */
                || opt->checkType != QStyleOptionMenuItem::NotCheckable))
        {
          int iw = qMin(smallIconSize, pixelMetric(PM_IndicatorWidth,option,widget)); // qMin as a precaution
          s.rwidth() += iw + pixelMetric(PM_CheckBoxLabelSpacing);
          /* for the height, see if there's really a check/radio button */
          if (opt->menuItemType != QStyleOptionMenuItem::Separator // exclude combo menu separators
              && (opt->checkType == QStyleOptionMenuItem::Exclusive
                  || opt->checkType == QStyleOptionMenuItem::NonExclusive))
          {
            s.setHeight(qMax(s.height(),
                             qMin(smallIconSize, pixelMetric(PM_IndicatorHeight,option,widget))));
          }
        }

        return s;
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
          fspec1 = getFrameSpec(QStringLiteral("Toolbar"));
        else
          fspec1 = getFrameSpec(QStringLiteral("MenuBar"));
        /* needed for putting menubar-items inside menubar frame
           (see PM_MenuBarItemSpacing for the reason) */
        fspec.top += fspec1.top+fspec1.bottom;

        if (!opt->icon.isNull())
        {
          int iconSize = pixelMetric(PM_SmallIconSize);
          s = sizeCalculated(QApplication::font(),fspec,lspec,sspec,
                             QString(),
                             QSize(iconSize,iconSize));
        }
        else
        {
          QFont f;
          if (widget) f = widget->font();
          else f = QApplication::font();
          if (lspec.boldFont) f.setWeight(lspec.boldness);

          s = sizeCalculated(f,fspec,lspec,sspec,opt->text,QSize());
        }
      }

      return s.expandedTo(contentsSize);
    }

    case CT_ToolButton : {
      const QStyleOptionToolButton *opt =
        qstyleoption_cast<const QStyleOptionToolButton*>(option);

      if (opt) {
        const QString group = "PanelButtonTool";
        frame_spec fspec = getFrameSpec(group);
        const indicator_spec dspec = getIndicatorSpec(group);
        size_spec sspec = getSizeSpec(group);
        label_spec lspec = getLabelSpec(group);

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

        /*
           We don't use "sizeCalculated()" to calculate the size
           because the button may be vertical, like in digiKam.
           Unfortunately, there's no standard way to determine
           how margins and frames are changed in such cases.
           Another drawback is that, if there's an arrow but no icon
           (in fact, if the style isn't "Qt::ToolButtonTextOnly"),
           Qt will add the icon size before making "contentsSize".
        */

        s = contentsSize + QSize(0, contentsSize.height() % 2)
            + QSize(fspec.left+fspec.right+lspec.left+lspec.right,
                    fspec.top+fspec.bottom+lspec.top+lspec.bottom)
            + QSize(!(opt->features & QStyleOptionToolButton::Arrow)
                    || opt->arrowType == Qt::NoArrow
                    || tialign == Qt::ToolButtonTextOnly
                    || ((opt->text.isEmpty() || tialign == Qt::ToolButtonIconOnly)
                        && opt->icon.isNull()) // nothing or only arrows
                      ? 0
                      // also add a space between indicator and text (-> CE_ToolButtonLabel)
                      : dspec.size+lspec.tispace+INDICATOR_MARGIN,
                    0);

        int arType = getArrowType(qobject_cast<const QToolButton*>(widget), opt);
        if (arType == arrowNormal)
        {
          const QString group1 = "DropDownButton";
          const frame_spec fspec1 = getFrameSpec(group1);
          indicator_spec dspec1 = getIndicatorSpec(group1);
          QSize defaultSize(contentsSize.width() + 6, contentsSize.height() + 5); // as in qcommonstyle.cpp
          dspec1.size = qMin(dspec1.size,qMin(defaultSize.height(),defaultSize.width()));
          s.rwidth() += (opt->direction == Qt::RightToLeft ?
                           fspec1.left-fspec.left
                           : fspec1.right-fspec.right) // there's an attachment
                        + dspec1.size+2*BUTTON_ARROW_MARGIN
                        - pixelMetric(PM_MenuButtonIndicator) // added in QToolButton::sizeHint()
                        /* If the style is not icon-only, 2 spaces are added to the width
                           in QToolButton::sizeHint(). Here, we cautiously remove only 2px. */
                        - (lspec.left < 1 || lspec.right < 1 || tialign == Qt::ToolButtonIconOnly ? 0 : 2);
        }
        else if (arType == arrowDelayed)
        {
            s.rwidth() += lspec.tispace+dspec.size + BUTTON_ARROW_MARGIN
                          - (lspec.left < 1 || lspec.right < 1 || tialign == Qt::ToolButtonIconOnly ? 0 : 2);
        }

        /* consider text-icon spacing, shadow and bold text */
        if (!opt->text.isEmpty()
            && !(tialign == Qt::ToolButtonIconOnly && !opt->icon.isNull()))
        {
          if(!opt->icon.isNull())
          {
            if (tialign == Qt::ToolButtonTextBesideIcon)
              s = s + QSize(lspec.tispace, 0);
            else if (tialign == Qt::ToolButtonTextUnderIcon)
              s = s + QSize(0, lspec.tispace);
          }

          if (lspec.hasShadow)
            s = s + QSize(qMax(qAbs(lspec.xshift)-1,0)+qMax(lspec.depth-1,0),
                          qMax(qAbs(lspec.yshift)-1,0)+qMax(lspec.depth-1,0));
          if (lspec.boldFont)
          {
            QFont f;
            if (widget) f = widget->font();
            else f = QApplication::font();
            QSize sNormal = textSize(f, opt->text);
            f.setWeight(lspec.boldness);
            QSize sBold = textSize(f, opt->text);
            QSize delta;
            if (opt->icon.isNull() || tialign == Qt::ToolButtonTextOnly)
              delta = sBold - sNormal;
            else if (tialign == Qt::ToolButtonTextBesideIcon)
            {
              delta = sBold - (sNormal.height() >= opt->iconSize.height()
                                 ? sNormal : QSize(sNormal.width(), opt->iconSize.height()));
            }
            else if (tialign == Qt::ToolButtonTextUnderIcon)
            {
              delta = sBold - (sNormal.width() >= opt->iconSize.width()
                                 ? sNormal : QSize(opt->iconSize.width(), sNormal.height()));
            }
            s = s + delta.expandedTo(QSize(0,0));
          }
        }
        else if(opt->icon.isNull()) // nothing or only an arrow
        {
          s = s.expandedTo(QSize(s.height(),0));
          return s;
        }

        if (!qobject_cast<QAbstractItemView*>(getParent(widget,2))) // -> CE_ToolButtonLabel
        {
          s = s.expandedTo(QSize(sspec.minW + (sspec.incrementW ? s.width() : 0),
                                 sspec.minH + (sspec.incrementH ? s.height() : 0)));
        }

        if (tialign == Qt::ToolButtonIconOnly || opt->text.isEmpty())
          s = s.expandedTo(QSize(s.height(),0)); // don't let width < height

        return s;
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
        size_spec sspec = getSizeSpec(group);

        bool verticalTabs = false;
        if (opt->shape == QTabBar::RoundedEast
            || opt->shape == QTabBar::RoundedWest
            || opt->shape == QTabBar::TriangularEast
            || opt->shape == QTabBar::TriangularWest)
        {
          verticalTabs = true;
        }

        QString txt = opt->text;
        if ((isLibreoffice_ && widget == nullptr) // LibreOffice only sets the contents size
            || (txt.isEmpty() && opt->icon.isNull())) // another app may do it too
        {
          if (!verticalTabs)
          {
            s = contentsSize + QSize(fspec.left+fspec.right+lspec.left+lspec.right,
                                     fspec.top+fspec.bottom+lspec.top+lspec.bottom);
            s = s.expandedTo(QSize(sspec.minW + (sspec.incrementW ? s.width() : 0),
                                   sspec.minH + (sspec.incrementH ? s.height() : 0)));
          }
          else
          {
            s = contentsSize + QSize(fspec.top+fspec.bottom+lspec.top+lspec.bottom,
                                     fspec.left+fspec.right+lspec.left+lspec.right);
            s = s.expandedTo(QSize(sspec.minH + (sspec.incrementH ? s.width() : 0),
                                   sspec.minW + (sspec.incrementW ? s.height() : 0)));
          }
        }
        else
        {
          QFont f;
          if (widget) f = widget->font();
          else f = QApplication::font();
          if (lspec.boldFont || tspec_.bold_active_tab)
            f.setWeight(lspec.boldness);

          QSize iconSize;
          if (!opt->icon.isNull())
            iconSize = opt->iconSize;
          int icnSize = iconSize.isValid() ?
                          qMax(iconSize.width(), iconSize.height())
                          : pixelMetric(PM_TabBarIconSize,option,widget); // as in CE_TabBarTabLabel

          txt.replace('\n', ' '); // always draw the tab text in a single line
          int minH = sspec.minH; // will be considered later
          sspec.minH = 0;
          s = sizeCalculated(f,fspec,lspec,sspec,txt,
                             opt->icon.isNull() ? QSize() : QSize(icnSize,icnSize),
                             Qt::ToolButtonTextBesideIcon);
          /* set the minimum contents height to 16px */
          s.rheight() = qMax(s.height(), 16 + fspec.top+fspec.bottom+lspec.top+lspec.bottom);
          /* now consider minH */
          if (sspec.incrementH)
            s.rheight() += minH;
          else
            s.rheight() = qMax(s.height(), minH);

          if (verticalTabs)
            s.transpose();
        }

        bool rtl(opt->direction == Qt::RightToLeft);
        if (opt->rightButtonSize.isValid()) // QSize(-1, -1) by default
        {
          /* Right/left frame and label spaces will be changed in CE_TabBarTabLabel if
             the icon exists. Also, INDICATOR_MARGIN and tab_button_extra_margin
             are added for SE_TabBarTabLeftButton and SE_TabBarTabRightButton, and
             PM_TabBarTabHSpace is added once, when a right or left button exists. */
          if (verticalTabs)
          {
            s.rheight() += opt->rightButtonSize.height() + pixelMetric(PM_TabBarTabHSpace,option,widget)
                                                         + INDICATOR_MARGIN
                                                         + tspec_.tab_button_extra_margin
                                                         - fspec.right
                                                         + (opt->icon.isNull() ? 0 : qMax(lspec.tispace-lspec.right, 0));
            s.rwidth() = qMax(s.width(), opt->rightButtonSize.width());
          }
          else
          {
            s.rwidth() += opt->rightButtonSize.width()
                          + pixelMetric(PM_TabBarTabHSpace,option,widget)
                          + INDICATOR_MARGIN
                          + tspec_.tab_button_extra_margin
                          + (rtl ? - fspec.left  + (opt->icon.isNull() ? 0 : qMax(lspec.tispace-lspec.left, 0))
                                 : - fspec.right + (opt->icon.isNull() ? 0 : qMax(lspec.tispace-lspec.right, 0)));
            s.rheight() = qMax(s.height(), opt->rightButtonSize.height());
          }
        }
        if (opt->leftButtonSize.isValid())
        {
          if (verticalTabs)
          {
            s.rheight() += opt->leftButtonSize.height() + INDICATOR_MARGIN
                                                        + tspec_.tab_button_extra_margin
                                                        - fspec.left
                                                        + (opt->icon.isNull() ? 0 : qMax(lspec.tispace-lspec.left, 0));
            if (!opt->rightButtonSize.isValid())
              s.rheight() += pixelMetric(PM_TabBarTabHSpace,option,widget);
            s.rwidth() = qMax(s.width(), opt->leftButtonSize.width());
          }
          else
          {
            s.rwidth() += opt->leftButtonSize.width()
                          + INDICATOR_MARGIN
                          + tspec_.tab_button_extra_margin
                          + (rtl ? - fspec.right + (opt->icon.isNull() ? 0 : qMax(lspec.tispace-lspec.right, 0))
                                 : - fspec.left + (opt->icon.isNull() ? 0 : qMax(lspec.tispace-lspec.left, 0)));
            if (!opt->rightButtonSize.isValid())
              s.rwidth() += pixelMetric(PM_TabBarTabHSpace,option,widget);
            s.rheight() = qMax(s.height(), opt->leftButtonSize.height());
          }
        }

        // for Calligra Words
        int dw = contentsSize.width() - s.width();
        int dh = contentsSize.height() - s.height();
        if (!verticalTabs)
          s += QSize(dw > 0 ? dw + fspec.left+fspec.right+lspec.left+lspec.right : 0,
                     dh > 0 ? dh + fspec.top+fspec.bottom+lspec.top+lspec.bottom : 0);
        else
          s += QSize(dw > 0 ? dw + fspec.top+fspec.bottom+lspec.top+lspec.bottom : 0,
                     dh > 0 ? dh + fspec.left+fspec.right+lspec.left+lspec.right : 0);
      }

      return s.expandedTo(contentsSize);
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

        QFont f;
        if (widget) f = widget->font();
        else f = QApplication::font();
        if (lspec.boldFont) f.setWeight(lspec.boldness);

        int iconSize = pixelMetric(PM_SmallIconSize);
        QString txt = opt->text;
        if (txt.isEmpty()) txt = "W"; // QML doesn't give a good contentsSize for empty sections
        s = sizeCalculated(f,fspec,lspec,sspec,txt,
                           opt->icon.isNull() ? QSize() : QSize(iconSize,iconSize));
        if (opt->sortIndicator != QStyleOptionHeader::None)
          s.rwidth() += dspec.size + pixelMetric(PM_HeaderMargin);
      }

      return s.expandedTo(QCommonStyle::sizeFromContents(type,option,contentsSize,widget));
    }

    case CT_ItemViewItem : {
      const QStyleOptionViewItem *opt =
          qstyleoption_cast<const QStyleOptionViewItem*>(option);
      if (opt)
      {
        QRect decorationRect, displayRect, checkRect;
        viewItemLayout(opt, &checkRect, &decorationRect, &displayRect, true);
        s = (decorationRect|displayRect|checkRect).size();
        const size_spec sspec = getSizeSpec("ItemView");
        s = s.expandedTo(QSize(0, // minW doesn't have meaning here
                               sspec.minH + (sspec.incrementH ? s.height() : 0)));
        return s;
      }
      return contentsSize;
    }

    case CT_TabWidget : {
      const frame_spec fspec = getFrameSpec(QStringLiteral("TabFrame"));
      return contentsSize + QSize(4, 4) // as in qcommonstyle.cpp
             + QSize(fspec.left+fspec.right,
                     fspec.top+fspec.bottom);
    }

    case CT_GroupBox : {
      frame_spec fspec;
      default_frame_spec(fspec);

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

      QSize textSize(0, 0);
      if (opt && !opt->text.isEmpty())
      {
        size_spec sspec;
        default_size_spec(sspec);
        label_spec lspec = getLabelSpec(QStringLiteral("GroupBox"));
        lspec.left = lspec.right = lspec.top = lspec.bottom = 0; // text margins aren't used
        QFont f;
        if (widget) f = widget->font();
        else f = QApplication::font();
        if (lspec.boldFont) f.setWeight(lspec.boldness);
        textSize = sizeCalculated(f,fspec,lspec,sspec,opt->text,QSize());
      }

      /* see CC_GroupBox in subControlRect() for why the following condition can't be used */
      //if (opt && !(opt->features & QStyleOptionFrame::Flat))
        fspec = getFrameSpec(QStringLiteral("GroupBox"));
      int checkWidth = (checkable ? pixelMetric(PM_IndicatorWidth)+pixelMetric(PM_CheckBoxLabelSpacing) : 0);
      int spacing = (tspec_.groupbox_top_label ? 0 : 6 + 10); /* 3px between text and frame and
                                                                 text starts at 10px after the left frame */
      QSize defaultSize = contentsSize // as in qcommonstyle.cpp
                          + QSize(opt && opt->features.testFlag(QStyleOptionFrame::Flat) ? 0 : 16, 0);
      s = QSize(qMax(defaultSize.width(), textSize.width() + checkWidth + spacing)
                  + fspec.left + fspec.right,
                defaultSize.height() + fspec.top + fspec.bottom
                  + (tspec_.groupbox_top_label
                     ? 3 // 3 px spacing between text and top frame (-> CC_GroupBox in subControlRect)
                     : qMax(checkable ? pixelMetric(PM_IndicatorHeight) : 0, textSize.height())/2));

      return s;
    }

    case CT_ProgressBar : {
      if (isKrita_)
        return contentsSize; // Krita's code has become too buggy since 5.1.0

      const QProgressBar *pb = qobject_cast<const QProgressBar*>(widget);
      bool isVertical(pb && pb->orientation() == Qt::Vertical);

      QFont f;
      if (widget) f = widget->font();
      else f = QApplication::font();
      const label_spec lspec = getLabelSpec(QStringLiteral("Progressbar"));
      if (lspec.boldFont) f.setWeight(lspec.boldness);

      if (!isKisSlider_ && tspec_.progressbar_thickness > 0
          && QFontMetrics(f).height() > tspec_.progressbar_thickness)
      {
        /* Set the size so that the text could be above the bar only if
           there isn't enough space inside the bar. The are safeguards
           in other places when a progressbar doesn't consult this function. */
        s = contentsSize;
        if (isVertical)
          s.rwidth() = tspec_.progressbar_thickness + QFontMetrics(f).height()+3;
        else
          s.rheight() = tspec_.progressbar_thickness + QFontMetrics(f).height()+3;
        return s;
      }
      // the label of an ordianry progressbar should fit in its interior
      const frame_spec fspec = getFrameSpec(QStringLiteral("Progressbar"));
      if (isVertical)
        s.rwidth() = QFontMetrics(f).height() + fspec.top + fspec.bottom;
      else
        s.rheight() = QFontMetrics(f).height() + fspec.top + fspec.bottom;

      return s.expandedTo(contentsSize);
    }

    case CT_MdiControls : {
      /* make the size larger to put buttons inside menubar frame (-> CC_MdiControls) */
      frame_spec fspec;
      if (tspec_.merge_menubar_with_toolbar)
        fspec = getFrameSpec(QStringLiteral("Toolbar"));
      else
        fspec = getFrameSpec(QStringLiteral("MenuBar"));
      bool rtl(option ? option->direction == Qt::RightToLeft
                      : QApplication::layoutDirection() == Qt::RightToLeft);
      const int frameWidth = (rtl ? fspec.left : fspec.right);
      if (const QStyleOptionComplex *styleOpt = qstyleoption_cast<const QStyleOptionComplex *>(option))
      {
        const int buttonSize = pixelMetric(PM_TitleBarButtonSize, styleOpt, widget);
        int width = frameWidth + 1;
        if (styleOpt->subControls & SC_MdiMinButton)
          width += buttonSize + 1;
        if (styleOpt->subControls & SC_MdiNormalButton)
          width += buttonSize + 1;
        if (styleOpt->subControls & SC_MdiCloseButton)
          width += buttonSize + 1;
        return QSize(width, buttonSize+fspec.top);
      }
      else
      {
        const int buttonSize = pixelMetric(PM_TitleBarButtonSize, option, widget);
        return QSize(frameWidth + 1 + 3*(buttonSize + 1), buttonSize+fspec.top);
      }
    }

    default : break;
  }

  return QCommonStyle::sizeFromContents(type,option,contentsSize,widget);
}

QSize Style::sizeCalculated(const QFont &font,
                            const frame_spec &fspec, // frame spec
                            const label_spec &lspec, // label spec
                            const size_spec &sspec, // size spec
                            const QString &text,
                            const QSize iconSize,
                            const Qt::ToolButtonStyle tialign // text-icon alignment
                           ) const
{
  /* text margins are taken into account without text too */
  QSize s;
  s.setWidth(fspec.left+fspec.right+lspec.left+lspec.right);
  s.setHeight(fspec.top+fspec.bottom+lspec.top+lspec.bottom);
  if (!text.isEmpty() && lspec.hasShadow)
  {
    s.rwidth() += qMax(qAbs(lspec.xshift)-1,0)+qMax(lspec.depth-1,0);
    s.rheight() += qMax(qAbs(lspec.yshift)-1,0)+qMax(lspec.depth-1,0);
  }

  QSize ts = textSize(font, text);
  int tw = ts.width();
  int th = ts.height();
  th += th%2; // for vertical centering

  if (tialign == Qt::ToolButtonIconOnly)
  {
    if (!iconSize.isEmpty())
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
    if (!iconSize.isEmpty())
    {
      s.rwidth() += iconSize.width() + (text.isEmpty() ? 0 : lspec.tispace) + tw;
      s.rheight() += qMax(iconSize.height() + iconSize.height() % 2, th);
    }
    else
    {
      s.rwidth() +=  tw;
      s.rheight() += th;
    }
  }
  else if (tialign == Qt::ToolButtonTextUnderIcon)
  {
    if (!iconSize.isEmpty())
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

  s = s.expandedTo(QSize(sspec.minW + (sspec.incrementW ? s.width() : 0),
                         sspec.minH + (sspec.incrementH ? s.height() : 0)));

  return s;
}

QRect Style::subElementRect(QStyle::SubElement element, const QStyleOption *option, const QWidget *widget) const
{
  switch (element) {
    case SE_CheckBoxFocusRect :
    case SE_RadioButtonFocusRect : {
      const QStyleOptionButton *opt =
          qstyleoption_cast<const QStyleOptionButton*>(option);
      if (opt)
      { // adapted from Qt -> qcommonstyle.cpp to draw focus rect only around contents
        bool isRadio = (element == SE_RadioButtonFocusRect);
        QRect r;
        if (opt->icon.isNull() && opt->text.isEmpty())
        {
          return subElementRect(isRadio ? SE_RadioButtonIndicator : SE_CheckBoxIndicator,
                                opt, widget).adjusted(1, 1, -1, -1);
        }
        QRect cr = visualRect(opt->direction,
                              opt->rect,
                              subElementRect(isRadio ? SE_RadioButtonContents : SE_CheckBoxContents,
                                             opt, widget));
        const label_spec lspec = getLabelSpec(isRadio ? QStringLiteral("RadioButton") : QStringLiteral("CheckBox"));
        if (opt->direction == Qt::RightToLeft)
          cr.adjust(lspec.right, 0 , -lspec.left, 0);
        else
          cr.adjust(lspec.left, 0 , -lspec.right, 0);
        if (!opt->text.isEmpty())
        {
          QFont f;
          if (widget) f = widget->font();
          else f = QApplication::font();
          if (lspec.boldFont) f.setWeight(lspec.boldness);
          r = itemTextRect(QFontMetrics(f),
                           cr,
                           Qt::AlignAbsolute | Qt::AlignLeft | Qt::AlignVCenter
                             | (styleHint(SH_UnderlineShortcut, opt, widget) ?
                                    Qt::TextShowMnemonic : Qt::TextHideMnemonic),
                           opt->state & State_Enabled,
                           opt->text);
        }
        if (!opt->icon.isNull())
        { // because of visualRect() below, here we suppose the direction is LTR
          QRect iconRect = alignedRect(Qt::LeftToRight,
                                       Qt::AlignVCenter | Qt::AlignLeft,
                                       opt->iconSize,
                                       cr);
          if (!opt->text.isEmpty())
            r = r.adjusted(0, 0 , opt->iconSize.width() + lspec.tispace, 0) | iconRect;
          else
            r = iconRect;
        }
        r.adjust(-2, -2, 2, 2);
        r = r.intersected(opt->rect);
        r = visualRect(opt->direction, opt->rect, r);
        return r;
      }
      else
        return QCommonStyle::subElementRect(element,option,widget);
    }

    case SE_PushButtonFocusRect : { // this isn't used anywhere
      QRect r;
      if (const QStyleOptionButton *opt = qstyleoption_cast<const QStyleOptionButton*>(option))
        r = opt->rect;
      return r;
    }

    case SE_ComboBoxFocusRect :
    case SE_SliderFocusRect : return QRect();

    case SE_ToolBoxTabContents : {
      if (option->direction == Qt::RightToLeft)
        return option->rect.adjusted(30,0,0,0);
      else
        return option->rect.adjusted(0,0,-30,0);
    }

    case SE_HeaderLabel : return option->rect;

    case SE_HeaderArrow : {
      const QString group = "HeaderSection";
      const frame_spec fspec = getFrameSpec(group);
      const indicator_spec dspec = getIndicatorSpec(group);
      const label_spec lspec = getLabelSpec(group);
      if (const QStyleOptionHeader *opt = qstyleoption_cast<const QStyleOptionHeader*>(option))
      {
        if (opt->orientation != Qt::Horizontal)
          return QRect();
      }

      return alignedRect(option->direction,
                         Qt::AlignRight,
                         QSize(option->direction == Qt::RightToLeft ?
                                 fspec.left+lspec.left+dspec.size
                                 : fspec.right+lspec.right+dspec.size,
                               option->rect.height()),
                         option->rect);
    }

    case SE_ProgressBarLabel :
    case SE_ProgressBarContents :
    case SE_ProgressBarGroove : return option->rect;

    case SE_LineEditContents : {
      frame_spec fspec = getFrameSpec(QStringLiteral("LineEdit"));
      label_spec lspec = getLabelSpec(QStringLiteral("LineEdit"));
      size_spec sspec = getSizeSpec(QStringLiteral("LineEdit"));
      /* when editing itemview texts, a thin frame is always drawn
         (see PE_PanelLineEdit) */
      if (qobject_cast<QAbstractItemView*>(getParent(widget,2)))
      {
        fspec.left = fspec.right = fspec.top = fspec.bottom = 1;
        lspec.left = lspec.right = lspec.top = lspec.bottom = 0;
        sspec.incrementW = false;
      }
      else if (widget)
      {
        if (qobject_cast<const QLineEdit*>(widget)
            && widget->testAttribute(Qt::WA_StyleSheetTarget)
            && !widget->styleSheet().isEmpty() && widget->styleSheet().contains(QLatin1String("padding")))
        {
          fspec.left = qMin(fspec.left,3);
          fspec.right = qMin(fspec.right,3);
          fspec.top = qMin(fspec.top,3);
          fspec.bottom = qMin(fspec.bottom,3);
          lspec.left = lspec.right = lspec.top = lspec.bottom = qMin(lspec.left,2);
          sspec.incrementW = false;
        }
        else
        {
          if (widget->minimumWidth() == widget->maximumWidth())
          {
            fspec.left = qMin(fspec.left,3);
            fspec.right = qMin(fspec.right,3);
            lspec.left = lspec.right = lspec.top = lspec.bottom = qMin(lspec.left,2);
            sspec.incrementW = false;
          }
          if (qobject_cast<QComboBox*>(widget->parentWidget())
              && widget->height() < sizeCalculated(widget->font(),fspec,lspec,sspec,QStringLiteral("W"),QSize()).height())
          { // the label spacing isn't added at CT_ComboBox
            lspec.top = qMin(lspec.top,2);
            lspec.bottom = qMin(lspec.bottom,2);
            if (widget->height() < sizeCalculated(widget->font(),fspec,lspec,sspec,QStringLiteral("W"),QSize()).height())
            {
              fspec.top = qMin(fspec.left,3);
              fspec.bottom = qMin(fspec.bottom,3);
              lspec.top = lspec.bottom = 0;
            }
          }
          else if (widget->height() < sizeCalculated(widget->font(),fspec,lspec,sspec,QStringLiteral("W"),QSize()).height())
          {
            fspec.top = qMin(fspec.left,3);
            fspec.bottom = qMin(fspec.bottom,3);
            lspec.top = lspec.bottom = 0;
          }
        }
      }
      else
      {
        const QStyleOptionSpinBox *sbOpt = qstyleoption_cast<const QStyleOptionSpinBox*>(option);
        // the measure we used in CC_SpinBox at drawComplexControl() (for QML)
        if (sbOpt && (tspec_.vertical_spin_indicators || sbOpt->frame))
        {
          fspec.left = qMin(fspec.left,3);
          fspec.right = qMin(fspec.right,3);
          fspec.top = qMin(fspec.top,3);
          fspec.bottom = qMin(fspec.bottom,3);
          lspec.left = lspec.right = lspec.top = lspec.bottom = 0;
          sspec.incrementW = false;
        }
      }

      bool rtl(option->direction == Qt::RightToLeft);
      QAbstractSpinBox *sb = qobject_cast<QAbstractSpinBox*>(getParent(widget,1));
      if (sb)
      {
        lspec.right = 0;

        /* widgets like QDateTimeEdit with calendar are drawn as editable combos */
        bool insideCombo(sb->property("_kv_combo").toBool());
        int comboArrowWidth = 0;
        if (insideCombo)
        { // -> subControlRect() -> CC_ComboBox
          frame_spec fspec = getFrameSpec(tspec_.combo_as_lineedit ? QStringLiteral("LineEdit")
                                                                   : QStringLiteral("ComboBox"));
          int combo_arrow_length = tspec_.square_combo_button
                                     ? qMax(COMBO_ARROW_LENGTH, option->rect.height()
                                                                - (rtl ? fspec.left : fspec.right))
                                     : COMBO_ARROW_LENGTH;
          if (rtl)
            comboArrowWidth = combo_arrow_length + fspec.left;
          else
            comboArrowWidth = combo_arrow_length + fspec.right;
        }

        if (((!tspec_.vertical_spin_indicators || insideCombo)
             // Krita 5.0.0
             && !sb->inherits("KisIntParseSpinBox")
             && !sb->inherits("KisDoubleParseSpinBox"))
            || sb->buttonSymbols() == QAbstractSpinBox::NoButtons)
        {
          QString maxTxt = spinMaxText(sb);
          if (maxTxt.isEmpty()
              || option->rect.width() < textSize(sb->font(),maxTxt).width() + fspec.left
                                        + (sspec.incrementW ? sspec.minW : 0)
                                        + (sb->buttonSymbols() == QAbstractSpinBox::NoButtons
                                             ? fspec.right : 0)
              || (sb->buttonSymbols() != QAbstractSpinBox::NoButtons
                  && sb->width() < option->rect.width()
                                   + (insideCombo
                                        ? comboArrowWidth
                                        : 2*tspec_.spin_button_width
                                          + getFrameSpec(QStringLiteral("IndicatorSpinBox")).right)))
          {
            fspec.left = qMin(fspec.left,3);
            fspec.right = qMin(fspec.right,3);
            lspec.left = 0;
            sspec.incrementW = false;
          }
          if (sb->height() < fspec.top+fspec.bottom+QFontMetrics(widget->font()).height())
          {
            fspec.top = qMin(fspec.top,3);
            fspec.bottom = qMin(fspec.bottom,3);
            lspec.top = lspec.bottom = 0;
          }
        }
        else
        {
          fspec.left = qMin(fspec.left,3);
          fspec.right = qMin(fspec.right,3);
          fspec.top = qMin(fspec.top,3);
          fspec.bottom = qMin(fspec.bottom,3);
          lspec.left = lspec.top = lspec.bottom = 0;
          sspec.incrementW = false;
        }
      }
      QRect rect = labelRect(option->rect, fspec, lspec);
      if (sspec.incrementW)
      {
        if (rtl && (sb == nullptr || sb->buttonSymbols() == QAbstractSpinBox::NoButtons))
          rect.adjust(0,0,-sspec.minW/2,0);
        else
          rect.adjust(sspec.minW/2,0,0,0);
      }

      /* in these cases there are attachments */
      if (widget)
      {
        if (QComboBox *cb = qobject_cast<QComboBox*>(widget->parentWidget()))
        {
          if (rtl)
          {
            rect.adjust(-fspec.left-lspec.left+3, 0, 0, 0);
            int arrowFrameSize = tspec_.combo_as_lineedit ? fspec.left
                                                          : getFrameSpec(QStringLiteral("ComboBox")).left;
            if (widget->width() < cb->width()
                                  - (tspec_.square_combo_button
                                       ? qMax(COMBO_ARROW_LENGTH, cb->height()-arrowFrameSize)
                                       : COMBO_ARROW_LENGTH)
                                  - arrowFrameSize)
            {
              rect.adjust(0, 0,
                          fspec.right+lspec.right-(tspec_.combo_as_lineedit || tspec_.square_combo_button
                                                     ? 0 : 3),
                          0);
            }
          }
          else
          {
            rect.adjust(0, 0, fspec.right+lspec.right-3, 0); // spacing between the text and arrow (button)
            /* although the left margin is removed here, this editable text can't be
               aligned exactly with the text of a similar combobox that is uneditable */
            if (widget->x() > 0)
              rect.adjust(-fspec.left-lspec.left+(tspec_.combo_as_lineedit || tspec_.square_combo_button
                                                    ? 0 : 3),
                          0, 0, 0);
          }
        }
        else if (sb && sb->buttonSymbols() != QAbstractSpinBox::NoButtons)
          rect.adjust(0,0,fspec.right,0);
      }

      /* final check */
      if (rect.height() < (widget ? widget->fontMetrics() : option->fontMetrics).height())
      {
        QRect commonRect = QCommonStyle::subElementRect(element,option,widget);
        rect.setTop(qMin(rect.top(), commonRect.top()));
        rect.setBottom(qMax(rect.bottom(), commonRect.bottom()));
      }

      return rect;
    }

    case SE_ItemViewItemCheckIndicator:
      if (!qstyleoption_cast<const QStyleOptionViewItem*>(option))
        return subElementRect(SE_CheckBoxIndicator, option, widget);
      /* Falls through. */
    case SE_ItemViewItemDecoration :
    case SE_ItemViewItemText : {
      QRect r;
      const QStyleOptionViewItem *opt =
          qstyleoption_cast<const QStyleOptionViewItem*>(option);
      if (opt)
      {
        if (!isViewItemCached(*opt))
        {
          viewItemLayout(opt, &checkRect_, &decorationRect_, &displayRect_, false);
          if (cachedOption_) {
              delete cachedOption_;
              cachedOption_ = nullptr;
          }
          cachedOption_ = new QStyleOptionViewItem(*opt);
        }
        if (element == SE_ItemViewItemCheckIndicator)
          r = checkRect_;
        else if (element == SE_ItemViewItemDecoration)
          r = decorationRect_;
        else if (element == SE_ItemViewItemText)
          r = displayRect_;
      }
      return r;
    }

    case SE_ItemViewItemFocusRect : {
      QRect r;
      if (qstyleoption_cast<const QStyleOptionViewItem*>(option))
        r = option->rect.adjusted(1,1,-1,-1);
      return r;
    }

    case SE_PushButtonBevel : {
      QRect r = option->rect;
      r = visualRect(option->direction, option->rect, r);
      return r;
    }

    case SE_PushButtonContents : {
      QRect r = QCommonStyle::subElementRect(element,option,widget);
      const QStyleOptionButton *opt =
          qstyleoption_cast<const QStyleOptionButton*>(option);
      if (opt && !opt->text.isEmpty() && qobject_cast<const QAbstractItemView*>(widget))
      { // as in Kate's preferences for its default text style
        const frame_spec fspec = getFrameSpec(QStringLiteral("PanelButtonCommand"));
        const label_spec lspec = getLabelSpec(QStringLiteral("PanelButtonCommand"));
        r.adjust(-fspec.left-lspec.left,
                 -fspec.top-lspec.top,
                 fspec.right+lspec.right,
                 fspec.bottom+lspec.bottom);
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

    case SE_TabWidgetTabPane :
    case SE_TabWidgetTabContents : {
      QRect r;
      if (const QStyleOptionTabWidgetFrame *twf = qstyleoption_cast<const QStyleOptionTabWidgetFrame*>(option))
      {
        int space = 0;
        int overlap = 0;
        /* a 2px space between tab page and tab bar
           but no spece between tab pane and tab bar */
        if (element == SE_TabWidgetTabContents && !twf->tabBarSize.isEmpty())
          space = 2;
        else
          overlap = pixelMetric(PM_TabBarBaseOverlap, twf, widget);
        int left = 0; int top = 0; int right = 0; int bottom = 0;
        bool docMode(true);
        if (element == SE_TabWidgetTabContents)
        { // space for the frame
          const QTabWidget *tw = qobject_cast<const QTabWidget*>(widget);
          if (!tw || !tw->documentMode())
          {
            docMode = false;
            const frame_spec fspec = getFrameSpec(QStringLiteral("TabFrame"));
            left = fspec.left + 1;
            top = fspec.top + 1;
            right = fspec.right + 1;
            bottom = fspec.bottom + 1;
          }
        }
        switch (twf->shape) {
          case QTabBar::RoundedNorth:
          case QTabBar::TriangularNorth:
            r = QRect(QPoint(0, qMax(twf->tabBarSize.height() + space - overlap, 0)),
                      QSize(twf->rect.width(),
                            qMin(twf->rect.height() - twf->tabBarSize.height() - space + overlap,
                                 twf->rect.height())));
            if (top >= 2 && !twf->tabBarSize.isEmpty())
              top -= 2;
            break;
          case QTabBar::RoundedSouth:
          case QTabBar::TriangularSouth:
            r = QRect(QPoint(0, 0),
                      QSize(twf->rect.width(),
                            qMin(twf->rect.height() - twf->tabBarSize.height() - space + overlap,
                                 twf->rect.height())));
            if (bottom >= 2 && !twf->tabBarSize.isEmpty())
              bottom -= 2;
            break;
          case QTabBar::RoundedEast:
          case QTabBar::TriangularEast:
            r = QRect(QPoint(0, 0),
                      QSize(qMin(twf->rect.width() - twf->tabBarSize.width() - space + overlap,
                                 twf->rect.width()),
                            twf->rect.height()));
            if (right >= 2 && !twf->tabBarSize.isEmpty())
              right -= 2;
            break;
          case QTabBar::RoundedWest:
          case QTabBar::TriangularWest:
            r = QRect(QPoint(twf->tabBarSize.width() + space - overlap, 0),
                      QSize(qMin(twf->rect.width() - twf->tabBarSize.width() - space + overlap,
                                 twf->rect.width()),
                            twf->rect.height()));
            if (left >= 2 && !twf->tabBarSize.isEmpty())
              left -= 2;
            break;
        }
        if (!docMode)
          r.adjust(left, top, -right, -bottom);
      }
      if (r.isValid()) return r;
      else return QCommonStyle::subElementRect(element,option,widget);
    }

    case SE_TabWidgetLeftCorner : {
      QRect r;
      if (const QStyleOptionTabWidgetFrame *twf =
          qstyleoption_cast<const QStyleOptionTabWidgetFrame*>(option))
      {
        int extra = pixelMetric(PM_TabBarBaseOverlap, twf, widget); // leveled with tabs
        if (extra > 0)
          extra = qMin(extra, twf->leftCornerWidgetSize.height()/2);
        QRect paneRect = subElementRect(SE_TabWidgetTabPane, twf, widget);
        switch (twf->shape) {
          case QTabBar::RoundedNorth:
          case QTabBar::TriangularNorth:
            r = QRect(QPoint(paneRect.x(), paneRect.y()-twf->leftCornerWidgetSize.height() + extra),
                      twf->leftCornerWidgetSize);
            break;
          case QTabBar::RoundedSouth:
          case QTabBar::TriangularSouth:
            r = QRect(QPoint(paneRect.x(), paneRect.y()+paneRect.height() - extra),
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
        int extra = pixelMetric(PM_TabBarBaseOverlap, twf, widget); // leveled with tabs
        if (extra > 0)
          extra = qMin(extra, twf->rightCornerWidgetSize.height()/2);
        QRect paneRect = subElementRect(SE_TabWidgetTabPane, twf, widget);
        switch (twf->shape) {
          case QTabBar::RoundedNorth:
          case QTabBar::TriangularNorth:
            r = QRect(QPoint(paneRect.x()+paneRect.width()-twf->rightCornerWidgetSize.width(),
                             paneRect.y()-twf->rightCornerWidgetSize.height() + extra),
                      twf->rightCornerWidgetSize);
            break;
          case QTabBar::RoundedSouth:
          case QTabBar::TriangularSouth:
            r = QRect(QPoint(paneRect.x()+paneRect.width()-twf->rightCornerWidgetSize.width(),
                             paneRect.y()+paneRect.height() - extra),
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

    case SE_TabBarTearIndicatorRight :
    case SE_TabBarTearIndicatorLeft :
    {
      QRect r;
      if (const QStyleOptionTab *tab = qstyleoption_cast<const QStyleOptionTab*>(option))
      {
        switch (tab->shape) {
          case QTabBar::RoundedNorth:
          case QTabBar::TriangularNorth:
          case QTabBar::RoundedSouth:
          case QTabBar::TriangularSouth:
            if (option->direction == Qt::RightToLeft)
            {
              if (element == SE_TabBarTearIndicatorLeft)
                r.setRect(tab->rect.right() - 1, tab->rect.top(), 2, option->rect.height());
              else
                r.setRect(tab->rect.left(), tab->rect.top(), 2, option->rect.height());
            }
            else
            {
              if (element == SE_TabBarTearIndicatorRight)
                r.setRect(tab->rect.right() - 1, tab->rect.top(), 2, option->rect.height());
              else
                r.setRect(tab->rect.left(), tab->rect.top(), 2, option->rect.height());
            }
            break;
          case QTabBar::RoundedWest:
          case QTabBar::TriangularWest:
          case QTabBar::RoundedEast:
          case QTabBar::TriangularEast:
            if (element == SE_TabBarTearIndicatorRight)
              r.setRect(tab->rect.left(), tab->rect.bottom() - 1, option->rect.width(), 2);
            else
              r.setRect(tab->rect.left(), tab->rect.top(), option->rect.width(), 2);
            break;
          default: break;
        }
      }
      return r;
    }

    /* Logically, SE_TabBarTabText should return a vertical rectangle with vertical tabs as
       follows (exactly like in CE_TabBarTabLabel, except for the transformations). However,
       the rectangle is used in QTabBar::initStyleOption() as if it's always horizontal. Also,
       QCommonStyle always gives a horizontal rectangle. So, we don't do the right thing here. */
    /*case SE_TabBarTabText: {
      QRect r;
      if (const QStyleOptionTab *opt = qstyleoption_cast<const QStyleOptionTab *>(option))
      {
        r = option->rect;
        bool verticalTabs (opt->shape == QTabBar::RoundedEast || opt->shape == QTabBar::TriangularEast
                           || opt->shape == QTabBar::RoundedWest || opt->shape == QTabBar::TriangularWest);
        int ltb = 0, rtb = 0;
        if (verticalTabs)
        {
          ltb = qMax(0, opt->leftButtonSize.height());
          rtb = qMax(0, opt->rightButtonSize.height());
          if (rtb > 0) rtb += INDICATOR_MARGIN
                              + tspec_.tab_button_extra_margin
                              + pixelMetric(PM_TabBarTabHSpace,option,widget)/2;
          if (ltb > 0) ltb += INDICATOR_MARGIN
                              + tspec_.tab_button_extra_margin
                              + pixelMetric(PM_TabBarTabHSpace,option,widget)/2;
          if (opt->shape == QTabBar::RoundedWest || opt->shape == QTabBar::TriangularWest)
            r.adjust(0, rtb, 0, -ltb);
          else
            r.adjust(0, ltb, 0, -rtb);
        }
        else
        {
          ltb = qMax(0, opt->leftButtonSize.width());
          rtb = qMax(0, opt->rightButtonSize.width());
          if (rtb > 0) rtb += INDICATOR_MARGIN
                              + tspec_.tab_button_extra_margin
                              + pixelMetric(PM_TabBarTabHSpace,option,widget)/2;
          if (ltb > 0) ltb += INDICATOR_MARGIN
                              + tspec_.tab_button_extra_margin
                              + pixelMetric(PM_TabBarTabHSpace,option,widget)/2;
          if (opt->direction == Qt::RightToLeft)
            r.adjust(rtb, 0, -ltb, 0);
          else
            r.adjust(ltb, 0, -rtb, 0);
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
          else
          {
            if (tspec_.mirror_doc_tabs
                && (opt->shape == QTabBar::RoundedEast || opt->shape == QTabBar::TriangularEast))
              r.adjust(0,0,0,-1);
            else
              r.adjust(0,1,0,0);
          }
        }
      }
      return r;
    }*/

    case SE_TabBarTabLeftButton:
    case SE_TabBarTabRightButton: {
      QRect r;
      if (const QStyleOptionTab *tab = qstyleoption_cast<const QStyleOptionTab *>(option))
      {
        bool selected = tab->state & State_Selected;
        int verticalShift = pixelMetric(QStyle::PM_TabBarTabShiftVertical, tab, widget);
        int horizontalShift = pixelMetric(QStyle::PM_TabBarTabShiftHorizontal, tab, widget);
        int hpadding = pixelMetric(QStyle::PM_TabBarTabHSpace, option, widget) / 2
                       + INDICATOR_MARGIN + tspec_.tab_button_extra_margin;

        bool verticalTabs(tab->shape == QTabBar::RoundedEast
                          || tab->shape == QTabBar::RoundedWest
                          || tab->shape == QTabBar::TriangularEast
                          || tab->shape == QTabBar::TriangularWest);

        QRect tr = tab->rect;
        if (tab->shape == QTabBar::RoundedSouth || tab->shape == QTabBar::TriangularSouth)
          verticalShift = -verticalShift;
        if (verticalTabs) {
          qSwap(horizontalShift, verticalShift);
          horizontalShift *= -1;
          verticalShift *= -1;
        }
        if (tab->shape == QTabBar::RoundedWest || tab->shape == QTabBar::TriangularWest)
          horizontalShift = -horizontalShift;

        tr.adjust(0, 0, horizontalShift, verticalShift);
        if (selected)
        {
          tr.setBottom(tr.bottom() - verticalShift);
          tr.setRight(tr.right() - horizontalShift);
        }

        QSize size = (element == SE_TabBarTabLeftButton) ? tab->leftButtonSize : tab->rightButtonSize;
        int w = qMax(0, size.width());
        int h = qMax(0, size.height());
        int midHeight = static_cast<int>(qCeil(float(tr.height() - h)/2));
        int midWidth = (tr.width() - w)/2;

        bool atBottom = true;
        int offset = 0;
        const frame_spec fspec = getFrameSpec(QStringLiteral("Tab"));
        const label_spec lspec = getLabelSpec(QStringLiteral("Tab"));
        switch (tab->shape) {
          case QTabBar::RoundedWest:
          case QTabBar::TriangularWest:
            atBottom = (element == SE_TabBarTabLeftButton);
            if (tab->rect.width() > w
                && lspec.top+fspec.top + lspec.bottom+fspec.bottom > 0)
            {
              qreal rDiff = static_cast<qreal>(lspec.top+fspec.top - lspec.bottom-fspec.bottom)
                            / static_cast<qreal>(lspec.top+fspec.top + lspec.bottom+fspec.bottom);
              offset = qRound(static_cast<qreal>(tab->rect.width() - w) * rDiff / 2.0);
              if (tspec_.mirror_doc_tabs
                  && (tab->shape == QTabBar::RoundedSouth
                      || tab->shape == QTabBar::TriangularSouth))
              {
                offset *= -1;
              }
            }
            break;
          case QTabBar::RoundedEast:
          case QTabBar::TriangularEast:
            atBottom = (element == SE_TabBarTabRightButton);
            if (tab->rect.width() > w
                && lspec.top+fspec.top + lspec.bottom+fspec.bottom > 0)
            {
              qreal rDiff = static_cast<qreal>(lspec.top+fspec.top - lspec.bottom-fspec.bottom)
                            / static_cast<qreal>(lspec.top+fspec.top + lspec.bottom+fspec.bottom);
              offset = qRound(static_cast<qreal>(tab->rect.width() - w) * rDiff / 2.0);
              if (tspec_.mirror_doc_tabs)
                offset *= -1;
            }
            break;
          default:
            if (tab->rect.height() > h
                && lspec.top+fspec.top + lspec.bottom+fspec.bottom > 0)
            {
              qreal rDiff = static_cast<qreal>(lspec.top+fspec.top - lspec.bottom-fspec.bottom)
                            / static_cast<qreal>(lspec.top+fspec.top + lspec.bottom+fspec.bottom);
              offset = qRound(static_cast<qreal>(tab->rect.height() - h) * rDiff / 2.0);
              if (tspec_.mirror_doc_tabs
                  && (tab->shape == QTabBar::RoundedSouth
                      || tab->shape == QTabBar::TriangularSouth))
              {
                offset *= -1;
              }
            }
            if (element == SE_TabBarTabLeftButton)
              r = QRect(tab->rect.x() + hpadding, midHeight + offset, w, h);
            else
              r = QRect(tab->rect.right() - w - hpadding, midHeight + offset, w, h);
            r = visualRect(tab->direction, tab->rect, r);
        }
        if (verticalTabs)
        {
          if (atBottom)
            r = QRect(midWidth + offset,
                      tr.y() + tab->rect.height() - hpadding - h,
                      w, h);
          else
            r = QRect(midWidth + offset, tr.y() + hpadding, w, h);
        }
      }
      return r;
    }

    default : return QCommonStyle::subElementRect(element,option,widget);
  }
}

QRect Style::subControlRect(QStyle::ComplexControl control,
                            const QStyleOptionComplex *option,
                            QStyle::SubControl subControl,
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
          const label_spec lspec = getLabelSpec(QStringLiteral("TitleBar"));
          int v = (lspec.top - lspec.bottom)/2;
          return QCommonStyle::subControlRect(control,option,subControl,widget).adjusted(0, v, 0, v);
        }

        default : return QCommonStyle::subControlRect(control,option,subControl,widget);
      }
      break;

    case CC_SpinBox : {
      int sw = tspec_.spin_button_width;
      const QAbstractSpinBox *sb = qobject_cast<const QAbstractSpinBox*>(widget);
      frame_spec fspec = getFrameSpec(QStringLiteral("IndicatorSpinBox"));
      frame_spec fspecLE = getFrameSpec(QStringLiteral("LineEdit"));
      const size_spec sspecLE = getSizeSpec(QStringLiteral("LineEdit"));
      const QStyleOptionSpinBox *opt = qstyleoption_cast<const QStyleOptionSpinBox*>(option);
      // the measure we used in CC_SpinBox at drawComplexControl() (for QML)
      bool verticalIndicators(tspec_.vertical_spin_indicators
                              || (!widget && opt && opt->frame)
                              // Krita 5.0.0
                              || (widget && (widget->inherits("KisIntParseSpinBox")
                                             || widget->inherits("KisDoubleParseSpinBox"))));

      bool hasButtons = true;
      if (sb)
      {
        if (sb->buttonSymbols() == QAbstractSpinBox::NoButtons)
          hasButtons = false;
      }
      else if (opt && opt->buttonSymbols == QAbstractSpinBox::NoButtons)
        hasButtons = false;
      if (!hasButtons) sw = 0;

      // a workaround for LibreOffice
      /*if (isLibreoffice_)
      {
        sw = 12;
        fspec.right = qMin(fspec.right,3);
      }
      else*/ if (sb && hasButtons && !verticalIndicators)
      { // when there isn't enough horizontal space (as in VLC and Pencil)
        QString maxTxt = spinMaxText(sb);
        if (!maxTxt.isEmpty()
            /* some codes may wrongly add a special text
               only when the value is minimum */
            && maxTxt != sb->specialValueText())
        {
          maxTxt += QLatin1Char(' ');
          int txtWidth = textSize(sb->font(),maxTxt).width();
          int rightFrame = w - txtWidth - 2*sw
                           - fspecLE.left - (sspecLE.incrementW ? sspecLE.minW : 0)
                           - 2; // for padding
          if (rightFrame < 0) rightFrame = 1;
          if (fspec.right > rightFrame)
          {
            sw = 16;
            // in this case, lineedit frame width is set to 3 at PE_PanelLineEdit
            rightFrame = w - txtWidth - 2*sw - 3 - 2;
            if (fspec.right > rightFrame)
            {
              rightFrame = qMax(rightFrame,2);
              if (rightFrame > 2 || w >= txtWidth + 2*8 + 2) // otherwise wouldn't help
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

      if (sw != 0 && verticalIndicators)
      {
        fspecLE.right = qMin(fspecLE.right,3);
        fspec = fspecLE;
        sw = 16 - fspecLE.right; // give 16px to the total width
      }

      // take into account the right frame width
      switch (subControl) {
        case SC_SpinBoxFrame :
          return option->rect;
        case SC_SpinBoxEditField : {
          if (sw == 0) return option->rect; // no button
          int margin = 0;
          /* LibreOffice sets a too small left margin for the text and doesn't
             consult SE_LineEditContents. As a workaround, we move the field to
             right here but draw it correctly in drawComplexControl() -> CC_SpinBox. */
          if (isLibreoffice_ && widget == nullptr)
            margin = 3;
          return QRect(x + margin,
                       y,
                       w - (sw + fspec.right) - (verticalIndicators ? 0 : sw),
                       h);
        }
        case SC_SpinBoxUp :
          if (sw == 0) return QRect();
          return QRect(x + w - (sw + fspec.right),
                       y,
                       sw + fspec.right,
                       verticalIndicators ? h/2 + (h%2 ? 1 : 0) : h);
        case SC_SpinBoxDown :
          if (sw == 0) return QRect();
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
        /* SC_ComboBoxEditField should include the icon, if any
           (-> qcombobox.cpp -> QComboBoxPrivate::updateLineEditGeometry) */
        case SC_ComboBoxEditField : {
          bool rtl(option->direction == Qt::RightToLeft);
          const QStyleOptionComboBox *opt =
              qstyleoption_cast<const QStyleOptionComboBox*>(option);
          int margin = 0;
          int arrowFrameSize = 0;
          frame_spec fspec;
          if (opt && opt->editable && (tspec_.combo_as_lineedit || tspec_.square_combo_button))
          {
            fspec = getFrameSpec(QStringLiteral("LineEdit"));
            arrowFrameSize = tspec_.combo_as_lineedit
                               ? rtl ? fspec.left : fspec.right
                               : rtl ? getFrameSpec(QStringLiteral("ComboBox")).left
                                     : getFrameSpec(QStringLiteral("ComboBox")).right;
          }
          else
          {
            fspec = getFrameSpec(QStringLiteral("ComboBox"));
            arrowFrameSize = rtl ? fspec.left : fspec.right;
          }
          const label_spec combolspec =  getLabelSpec(QStringLiteral("ComboBox"));
          if (isLibreoffice_ && widget == nullptr)
          { // neither CE_ComboBoxLabel nor SE_LineEditContents is consulted
            margin = rtl ? fspec.right+combolspec.right : fspec.left+combolspec.left;
          }
          else if (opt && opt->editable && !opt->currentIcon.isNull())
          {
            /* The left icon should respect frame width, text margin
               and text-icon spacing in the editable mode too. */
            margin = (rtl ? fspec.right+combolspec.right : fspec.left+combolspec.left)
                     + combolspec.tispace
                     - 4; // it's 4px in qcombobox.cpp -> QComboBoxPrivate::updateLineEditGeometry()
            margin = qMax(margin, 0);
          }
          int combo_arrow_length = tspec_.square_combo_button
                                    ? qMax(COMBO_ARROW_LENGTH, h-arrowFrameSize)
                                    : COMBO_ARROW_LENGTH;
          return QRect(rtl
                         ? x+combo_arrow_length+arrowFrameSize
                         : x+margin,
                       y,
                       w-(combo_arrow_length+arrowFrameSize)-margin,
                       h);
        }
        case SC_ComboBoxArrow : {
          bool rtl(option->direction == Qt::RightToLeft);
          const QStyleOptionComboBox *opt =
              qstyleoption_cast<const QStyleOptionComboBox*>(option);
          frame_spec fspec;
          if (opt && opt->editable && tspec_.combo_as_lineedit)
            fspec = getFrameSpec(QStringLiteral("LineEdit"));
          else
            fspec = getFrameSpec(QStringLiteral("ComboBox"));
          int combo_arrow_length = tspec_.square_combo_button
                                    ? qMax(COMBO_ARROW_LENGTH, h-(rtl ? fspec.left : fspec.right))
                                    : COMBO_ARROW_LENGTH;
          return QRect(rtl
                         ? x
                         : x+w-(combo_arrow_length+fspec.right),
                       y,
                       rtl
                         ? combo_arrow_length+fspec.left
                         : combo_arrow_length+fspec.right,
                       h);
        }
        case SC_ComboBoxListBoxPopup : {
          if (!tspec_.combo_menu/* || isLibreoffice_*/)
            return option->rect;

          /* take into account the needed space */
          QRect r = option->rect;
          const QStyleOptionComboBox *opt =
              qstyleoption_cast<const QStyleOptionComboBox*>(option);
          frame_spec fspec = getFrameSpec(QStringLiteral("MenuItem"));
          const label_spec lspec = getLabelSpec(QStringLiteral("MenuItem"));
          int space = fspec.left+lspec.left + fspec.right+lspec.right
                      + (tspec_.hide_combo_checkboxes
                           ? 0 // assuming a maximum value forced by Qt
                           : qMin(pixelMetric(PM_SmallIconSize), pixelMetric(PM_IndicatorWidth,option,widget))
                             + pixelMetric(PM_CheckBoxLabelSpacing))
                      /* NOTE: We added this to combobox width (as QCommonStyle does):
                                qMax(23, 2*(pixelMetric(PM_FocusFrameHMargin) + 1)
                                         + pixelMetric(PM_ScrollBarExtent))
                                         + (opt->frame ? pixelMetric(PM_ComboBoxFrameWidth)*2 : 0)
                               But We don't need it here. As for reserving space for scrollbar,
                               it shouldn't be needed with a menu and special cases can be ignored. */
                      - qMax(23, 2*(pixelMetric(PM_FocusFrameHMargin) + 1)
                                 + pixelMetric(PM_ScrollBarExtent,option,widget))
                      - (opt && opt->frame ? pixelMetric(PM_ComboBoxFrameWidth, opt, widget)*2 : 0);

          bool hasIcon = false;
          if (opt)
          {
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
            else hasIcon = true; // QML
          }

          fspec = getFrameSpec(QStringLiteral("Menu"));
          space += 2*qMax(qMax(fspec.top,fspec.bottom), qMax(fspec.left,fspec.right))
                   + (!tspec_.shadowless_popup && !noComposite_
                        ? 2*settings_->getCompositeSpec().menu_shadow_depth
                        : 0)
                   - extraComboWidth(opt, hasIcon);

          /* The width might be increased by Qt -> qcombobox.cpp -> QComboBox::showPopup()
             but the left border won't be moved. So, we align the left border.*/
          r.adjust(0, 0, qMax(space,0), 0);

          /* compensate for the offset created by the shadow */
          if (!tspec_.shadowless_popup && !noComposite_ && menuShadow_.count() == 4)
          {
            /* menu width shouldn't be less than combo width */
            r.adjust(0, 0, qMax(w - (r.width() - qRound(menuShadow_.at(0) + menuShadow_.at(2))), 0), 0);
            r.translate(-menuShadow_.at(0), -menuShadow_.at(1));
          }
          else
            r.adjust(0, 0, qMax(w-r.width(), 0), 0);
          return r;
        }

        default : return QCommonStyle::subControlRect(control,option,subControl,widget);
      }
      break;

    case CC_MdiControls : {
      int numSubControls = 0;
      if (option->subControls & SC_MdiCloseButton)
        ++numSubControls;
      if (option->subControls & SC_MdiMinButton)
        ++numSubControls;
      if (option->subControls & SC_MdiNormalButton)
        ++numSubControls;
      if (numSubControls == 0)
        break;

      // -> CT_MdiControls
      frame_spec fspec;
      if (tspec_.merge_menubar_with_toolbar)
        fspec = getFrameSpec(QStringLiteral("Toolbar"));
      else
        fspec = getFrameSpec(QStringLiteral("MenuBar"));

      int buttonWidth = (option->rect.width()
                         - (option->direction == Qt::RightToLeft ? fspec.left : fspec.right) - 1)
                        / numSubControls
                        - 1;
      int offset = option->direction == Qt::RightToLeft ? fspec.left : 0;
      switch (subControl) {
        case SC_MdiCloseButton:
          if (numSubControls == 1)
            break;
          offset += buttonWidth + 1;
        /* Falls through. */
        case SC_MdiNormalButton:
          if (numSubControls == 1 || (numSubControls == 2 && !(option->subControls & SC_MdiMinButton)))
            break;
          if (option->subControls & SC_MdiNormalButton)
            offset += buttonWidth + 1;
          break;
        default:
          break;
      }

      return QRect(offset, fspec.top, buttonWidth, option->rect.height()-fspec.top);
    }

    case CC_ScrollBar : {
      const QStyleOptionSlider *opt =
          qstyleoption_cast<const QStyleOptionSlider*>(option);
      if (!opt) break;

      int extent = pixelMetric(PM_ScrollBarExtent,option,widget);
      int arrowSize = 0;
      if (tspec_.scroll_arrows
          /* when arrows are present in a different style, as in Gwenview */
          /* WARNING: There's no way to know how app styles work. So, unfortunately, this
                      workaround can't be used and such cases should be left as they are. */
          /*|| (widget && widget->style() && widget->style() != this
              && subControl != SC_ScrollBarSubLine && subControl != SC_ScrollBarAddLine // no infinite loop
              && widget->style()->subControlRect(CC_ScrollBar,option,SC_ScrollBarSubLine,widget).isValid())*/)
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
            int handleThickness = pixelMetric(PM_SliderControlThickness,option,widget);
            if (handleThickness == pixelMetric(PM_SliderLength,option,widget))
              handleThickness = qMin(handleThickness, horiz ? h : w); // see drawComplexControl() -> CC_Slider
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
                           2*(handleThickness/2)); // it may be an odd number
            }
            else
            {
              if (ticks == QSlider::TicksAbove) // left
                x += SLIDER_TICK_SIZE/2;
              else if (ticks == QSlider::TicksBelow) // right
                x -= SLIDER_TICK_SIZE/2;
              return QRect(x+(w-handleThickness)/2,
                           y,
                           2*(handleThickness/2),
                           h);
            }
          }
        }
        /* Falls through. */

        case SC_SliderHandle : {
          if (opt)
          {
            bool horiz = opt->orientation == Qt::Horizontal;
            subControlRect(CC_Slider,option,SC_SliderGroove,widget).getRect(&x,&y,&w,&h);
            int len = pixelMetric(PM_SliderLength,option,widget);
            if (len == pixelMetric(PM_SliderControlThickness,option,widget))
              len = qMin(len, horiz ? h : w); // see drawComplexControl() -> CC_Slider
            const int sliderPos = sliderPositionFromValue(opt->minimum,
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
        /* Falls through. */

        default : return QCommonStyle::subControlRect(control,option,subControl,widget);
      }

      break;
    }

    case CC_Dial : {
      switch (subControl) {
        case SC_DialGroove : {
          int min = qMin(w,h);
          return alignedRect(option->direction,
                             Qt::AlignHCenter | Qt::AlignVCenter,
                             QSize(min,min),
                             option->rect);
        }
        case SC_DialHandle : {
          if (const QStyleOptionSlider *opt =
              qstyleoption_cast<const QStyleOptionSlider*>(option))
          { // adapted from Qtcurve
            qreal angle(0);
            if (opt->maximum == opt->minimum)
              angle = M_PI/2;
            else
            {
              const qreal fraction(static_cast<qreal>(opt->sliderValue - opt->minimum)
                                   / static_cast<qreal>(opt->maximum - opt->minimum));
              if(opt->dialWrapping)
                angle = M_PI*4/3 - fraction*2*M_PI; // angle = 1.5*M_PI - fraction*2*M_PI;
              else
                angle = (M_PI*8 - fraction*10*M_PI)/6;
            }
            QRect r(option->rect);
            /* containing rectangle */
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
            int handleSize = r.width()/5;
            //const qreal radius = 0.5*(r.width() - handleSize);
            /* give 1/10 of the width to notches */
            const qreal radius = 0.5*static_cast<qreal>(r.width() - 2*handleSize);
            center += QPoint(qRound(radius*qCos(angle)), -qRound(radius*qSin(angle)));
            r = QRect(r.x(), r.y(), handleSize, handleSize);
            r.moveCenter(center);
            return r;
          }
        }
        /* Falls through. */

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
            bool rtl(opt->direction == Qt::RightToLeft);
            const auto tb = qobject_cast<const QToolButton*>(widget);
            int arType = getArrowType(tb, opt);
            if (arType == arrowNormal)
            {
              const QString group = "DropDownButton";
              frame_spec fspec = getFrameSpec(group);
              indicator_spec dspec = getIndicatorSpec(group);
              /* limit the arrow size */
              dspec.size = qMin(dspec.size, h);
              /* lack of space */
              if (tb && opt && opt->toolButtonStyle == Qt::ToolButtonIconOnly && !opt->icon.isNull())
              {
                const frame_spec fspec1 = getFrameSpec(QStringLiteral("PanelButtonTool"));
                if (w < opt->iconSize.width()+fspec1.left
                        +(rtl ? fspec.left : fspec.right)+dspec.size+2*BUTTON_ARROW_MARGIN)
                {
                  if (rtl)
                    fspec.left = qMin(fspec.left,3);
                  else
                    fspec.right = qMin(fspec.right,3);
                  dspec.size = qMin(dspec.size,TOOL_BUTTON_ARROW_SIZE);
                }
              }
              return option->rect.adjusted(rtl ? fspec.left+dspec.size+2*BUTTON_ARROW_MARGIN : 0,
                                           0,
                                           rtl ? 0 : -fspec.right-dspec.size-2*BUTTON_ARROW_MARGIN,
                                           0);
            }
            else if (arType == arrowDelayed)
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
              if (tb && opt->toolButtonStyle == Qt::ToolButtonIconOnly && !opt->icon.isNull())
              {
                if (w < opt->iconSize.width()+fspec.left+fspec.right
                        +dspec.size+ BUTTON_ARROW_MARGIN+lspec.tispace)
                {
                  if (rtl)
                    fspec.left = qMin(fspec.left,3);
                  else
                    fspec.right = qMin(fspec.right,3);
                  dspec.size = qMin(dspec.size,TOOL_BUTTON_ARROW_SIZE-TOOL_BUTTON_ARROW_OVERLAP);
                  lspec.tispace = 0;
                }
              }
              return option->rect.adjusted(rtl ?
                                             lspec.tispace+dspec.size
                                               // -> CE_ToolButtonLabel
                                               + fspec.left
                                               + BUTTON_ARROW_MARGIN
                                             : 0,
                                           0,
                                           rtl ?
                                             0
                                             : - lspec.tispace-dspec.size
                                                 - fspec.right
                                                 - BUTTON_ARROW_MARGIN,
                                           0);
            }
          }

          return option->rect;
        }

        case SC_ToolButtonMenu : {
          const QStyleOptionToolButton *opt =
          qstyleoption_cast<const QStyleOptionToolButton*>(option);

          if (opt)
          {
            bool rtl(opt->direction == Qt::RightToLeft);
            const auto tb = qobject_cast<const QToolButton*>(widget);
            int arType = getArrowType(tb, opt);
            if (arType == arrowNormal)
            {
              const QString group = "DropDownButton";
              frame_spec fspec = getFrameSpec(group);
              indicator_spec dspec = getIndicatorSpec(group);
              /* limit the arrow size */
              dspec.size = qMin(dspec.size, h);
              /* lack of space */
              if (tb && opt && opt->toolButtonStyle == Qt::ToolButtonIconOnly && !opt->icon.isNull())
              {
                const frame_spec fspec1 = getFrameSpec(QStringLiteral("PanelButtonTool"));
                if (w < opt->iconSize.width()+fspec1.left
                        +(rtl ? fspec.left : fspec.right)+dspec.size+2*BUTTON_ARROW_MARGIN)
                {
                  if (rtl)
                    fspec.left = qMin(fspec.left,3);
                  else
                    fspec.right = qMin(fspec.right,3);
                  dspec.size = qMin(dspec.size,TOOL_BUTTON_ARROW_SIZE);
                }
              }
              int l = (rtl ? fspec.left : fspec.right)+dspec.size+2*BUTTON_ARROW_MARGIN;
              return QRect(rtl ? x : x+w-l,
                           y,l,h);
            }
            else if (arType == arrowDelayed)
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
              if (tb && opt->toolButtonStyle == Qt::ToolButtonIconOnly && !opt->icon.isNull())
              {
                const label_spec lspec = getLabelSpec(group);
                if (w < opt->iconSize.width()+fspec.left+fspec.right
                        +dspec.size+ BUTTON_ARROW_MARGIN+lspec.tispace)
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
                      + (rtl ? fspec.left : fspec.right)
                      + BUTTON_ARROW_MARGIN;
              return QRect(rtl ? x : x+w-l,
                           y,l,h);
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

        bool rtl(option->direction == Qt::RightToLeft);
        bool checkable = false;
        if (const QGroupBox *gb = qobject_cast<const QGroupBox*>(widget))
        {
          if (gb->isCheckable())
            checkable = true;
        }
        if (!checkable && (opt->subControls & QStyle::SC_GroupBoxCheckBox)) // QML
          checkable = true;

        QSize textSize(0, 0);
        if (!opt->text.isEmpty())
        { // -> CT_GroupBox
          size_spec sspec;
          default_size_spec(sspec);
          label_spec lspec = getLabelSpec(QStringLiteral("GroupBox"));
          lspec.left = lspec.right = lspec.top = lspec.bottom = 0;
          QFont f;
          if (widget) f = widget->font();
          else f = QApplication::font();
          if (lspec.boldFont) f.setWeight(lspec.boldness);
          textSize = sizeCalculated(f,fspec,lspec,sspec,opt->text,QSize());
        }

        int checkWidth = (checkable ? pixelMetric(PM_IndicatorWidth)+pixelMetric(PM_CheckBoxLabelSpacing) : 0);
        int checkHeight = (checkable ? pixelMetric(PM_IndicatorHeight) : 0);
        /* because of a bug in Qt, the flat feature may not be set here when a flat groupbox is shown */
        //if (!(opt->features & QStyleOptionFrame::Flat))
          fspec = getFrameSpec(QStringLiteral("GroupBox"));
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
            if (opt->text.isEmpty()) return QRect();
            int delta = 0;
            if (checkHeight > textSize.height())
              delta = (checkHeight - textSize.height())/2;
            int spacing = (tspec_.groupbox_top_label ? 0 : 6); // 3px between text and cut frame
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
            return interiorRect(subControlRect(control,option,SC_GroupBoxFrame,widget), fspec)
                   .adjusted(0,top,0,0);
          }
          case SC_GroupBoxFrame : {
            int top = qMax(checkHeight,textSize.height());
            if (!tspec_.groupbox_top_label)
            {
              if (fspec.top < top)
                top = (top - fspec.top)/2;
            }
            else
              top += 3; // 3 px spacing between text and top frame (-> CT_GroupBox)
            return QRect(x,
                         y + top,
                         w,
                         h - top);
          }

          default : return QCommonStyle::subControlRect(control,option,subControl,widget);
        }
      }
    }
    /* Falls through. */

    default : return QCommonStyle::subControlRect(control,option,subControl,widget);
  }

  return QCommonStyle::subControlRect(control,option,subControl,widget);
}

static inline uint qt_intensity(uint r, uint g, uint b)
{
  // 30% red, 59% green, 11% blue
  return (77 * r + 150 * g + 28 * b) / 255;
}

QPixmap Style::generatedIconPixmap(QIcon::Mode iconMode,
                                   const QPixmap &pixmap,
                                   const QStyleOption *option) const
{
  switch (iconMode) {
    case QIcon::Disabled: {
      QImage im = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);

      // Create a colortable based on the background (black -> bg -> white)
      QColor bg = option->palette.color(QPalette::Disabled, QPalette::Window);
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

      // High intensity colors need dark shifting in the color table, while
      // low intensity colors need light shifting. This is to increase the
      // percieved contrast.
      if ((red - factor > green && red - factor > blue)
          || (green - factor > red && green - factor > blue)
          || (blue - factor > red && blue - factor > green))
        intensity = qMin(255, intensity + 91);
      else if (intensity <= 128)
        intensity -= 51;

      for (int y=0; y<im.height(); ++y)
      {
        QRgb *scanLine = reinterpret_cast<QRgb*>(im.scanLine(y));
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
      QColor color = option->palette.color(QPalette::Active, QPalette::Highlight);
      color.setAlphaF(0.2); // Qt sets it to 0.3
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

void Style::drawItemText(QPainter *painter, const QRect &rect, int flags,
                         const QPalette &pal, bool enabled, const QString &text,
                         QPalette::ColorRole textRole) const
{
  /* WARNING: Applying static_cast and then qobject_cast to painter->device()
              causes a crash  with QML (= QQC2 -- used by KDE Plasma) because,
              in that case, the paint device isn't a QWidget. There may also be
              other valid uses of this function without QWidget as paint device. */

  /* Ensure a centered vertical alignment if vertical alignment
     isn't defined when this function isn't directly called by us.
     (A bad vertical alignment started to happen with tooltips of Qt 5.12.) */
  if (!(flags & Qt::AlignVertical_Mask))
    flags |= Qt::AlignVCenter;

  QCommonStyle::drawItemText(painter, rect, flags, pal, enabled, text, textRole);
}

void Style::drawItemPixmap(QPainter *painter, const QRect &rect,
                           int alignment, const QPixmap &pixmap) const
{
  qreal scale = 1;
  if (qApp->testAttribute(Qt::AA_UseHighDpiPixmaps) && !pixmap.isNull())
    scale = pixmap.devicePixelRatio();
  scale = qMax(scale, static_cast<qreal>(1));

  auto scaledSize = QSizeF(pixmap.size()/scale).toSize();
  QRect pixRect = alignedRect(QApplication::layoutDirection(), QFlag(alignment),
                              scaledSize, rect);
  QRect inter = pixRect.intersected(rect);
  painter->drawPixmap(inter.x(), inter.y(),
                      pixmap,
                      inter.x()-pixRect.x(), inter.y()-pixRect.y(),
                      rect.width() >= scaledSize.width() ? pixmap.width() // exact width
                                                         : qRound(inter.width()*scale),
                      rect.height() >= scaledSize.height() ? pixmap.height() // exact height
                                                           : qRound(inter.height()*scale));
}

QPixmap Style::getPixmapFromIcon(const QIcon &icon,
                                 const KvIconMode iconmode,
                                 const QIcon::State iconstate,
                                 QSize iconSize) const
{
  if (icon.isNull()) return QPixmap();
  QIcon::Mode icnMode;
  /* since some icon engines (like that of KDE) don't consult generatedIconPixmap(),
     we enforce it after ignoring the disabled state temporarily */
  if (iconmode == Normal || iconmode == Disabled)
    icnMode = QIcon::Normal;
  else if (iconmode == Active)
    icnMode = QIcon::Active;
  else// if (iconmode == Selected || iconmode == DisabledSelected)
    icnMode = QIcon::Selected;

  QPixmap px = icon.pixmap(iconSize,icnMode,iconstate);

  if (iconmode == Disabled || iconmode == DisabledSelected)
  {
    QStyleOption opt;
    opt.palette = standardPalette();
    px = generatedIconPixmap(QIcon::Disabled, px, &opt); // graying out
    px = translucentPixmap(px, 50); // graying out is never enough
  }
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
  tintColor.setAlphaF(tintPercentage/100.0);
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
  p.setOpacity(opacityPercentage/100.0);
  p.drawPixmap(0, 0, px);
  p.end();
  return QPixmap::fromImage(img);
}

QRect Style::interiorRect(const QRect &bounds, const frame_spec &fspec) const
{
  if (!fspec.isAttached || (fspec.HPos == 2 && fspec.VPos == 2))
    return bounds.adjusted(fspec.left,fspec.top,-fspec.right,-fspec.bottom);

  int left = 0, right = 0, top = 0, bottom = 0;
  if (fspec.HPos == -1)
    left = fspec.left;
  else if (fspec.HPos == 1)
    right = fspec.right;
  else if (fspec.HPos == 2)
  {
    left = fspec.left;
    right = fspec.right;
  }
  if (fspec.VPos == -1)
    top = fspec.top;
  else if (fspec.VPos == 1)
    bottom = fspec.bottom;
  else if (fspec.VPos == 2)
  {
    top = fspec.top;
    bottom = fspec.bottom;
  }
  return bounds.adjusted(left,top,-right,-bottom);
}

bool Style::hasExpandedBorder(const frame_spec &fspec) const
{
  if (fspec.expansion > 0
      && themeRndr_ && themeRndr_->isValid())
  {
    QString el = fspec.expandedElement;
    if (el.isEmpty())
      el = fspec.element;
    if (expandedBorders_.contains(el))
      return expandedBorders_.value(el);
    if (themeRndr_->elementExists("border-" + el + "-normal-top"))
    {
      expandedBorders_.insert(el, true);
      return true;
    }
    expandedBorders_.insert(el, false);
  }
  return false;
}

bool Style::flatArrowExists(const QString &indicatorElement) const
{
  if (flatArrows_.contains(indicatorElement))
    return flatArrows_.value(indicatorElement);
  if (themeRndr_->elementExists("flat-"+indicatorElement+"-down-normal"))
  {
    flatArrows_.insert(indicatorElement, true);
    return true;
  }
  flatArrows_.insert(indicatorElement, false);
  return false;
}

bool Style::elementExists(const QString &elementName) const
{
  if (themeRndr_ && themeRndr_->isValid())
  {
    if (elements_.contains(elementName))
      return elements_.value(elementName);
    if (themeRndr_->elementExists(elementName))
    {
      elements_.insert(elementName, true);
      return true;
    }
    elements_.insert(elementName, false);
  }
  return false;
}

label_spec Style::getLabelSpec(const QString &widgetName) const
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

}
