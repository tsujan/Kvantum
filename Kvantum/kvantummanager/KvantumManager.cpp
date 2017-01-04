#include "KvantumManager.h"
#include <QFileDialog>
#include <QSettings>
#include <QMessageBox>
#include <QStyleFactory>
#include <QDesktopWidget>
#include <QWhatsThis>
#if QT_VERSION >= 0x050000
#include <QFileDevice>
#include <QTextStream>
#endif
//#include <QDebug>

namespace KvManager {

KvantumManager::KvantumManager (QWidget *parent) : QMainWindow (parent), ui (new Ui::KvantumManager)
{
    ui->setupUi (this);

    confPageVisited_ = false;

    setWindowTitle ("Kvantum Manager");
    ui->toolBox->setItemIcon (0,
                              QIcon::fromTheme ("system-software-install",
                                                QIcon (":/Icons/data/system-software-install.svg")));
    ui->toolBox->setItemIcon (1,
                              QIcon::fromTheme ("preferences-desktop-theme",
                                                QIcon (":/Icons/data/preferences-desktop-theme.svg")));
    ui->toolBox->setItemIcon (2,
                              QIcon::fromTheme ("preferences-system",
                                                QIcon (":/Icons/data/preferences-system.svg")));
    ui->toolBox->setItemIcon (3,
                              QIcon::fromTheme ("unknownapp",
                                                QIcon (":/Icons/data/app.svg")));

    lastPath_ = QDir::home().path();
    process_ = new QProcess (this);

    char * _xdg_config_home = getenv ("XDG_CONFIG_HOME");
    if (!_xdg_config_home)
        xdg_config_home = QString ("%1/.config").arg (QDir::homePath());
    else
        xdg_config_home = QString (_xdg_config_home);

    desktop_ = qgetenv ("XDG_CURRENT_DESKTOP").toLower();

    ui->useTheme->setEnabled (false);

    ui->comboToolButton->insertItems (0, QStringList() << "Follow Style"
                                                       << "Icon Only"
                                                       << "Text Only"
                                                       << "Text Beside Icon"
                                                       << "Text Under Icon");

    ui->comboX11Drag->insertItems (0, QStringList() << "None"
                                                    << "Menubar"
                                                    << "Menubar and primary toolbar"
                                                    << "All");

#if QT_VERSION >= 0x050200
    ui->appsEdit->setClearButtonEnabled (true);
#endif

    QLabel *statusLabel = new QLabel();
    statusLabel->setTextInteractionFlags (Qt::TextSelectableByMouse);
    ui->statusBar->addWidget (statusLabel);

    /* set kvconfigTheme_ and connect to combobox signals */
    updateThemeList();

    effect_ = new QGraphicsOpacityEffect();
    animation_ = new QPropertyAnimation (effect_, "opacity");

    setAttribute (Qt::WA_AlwaysShowToolTips);
    showAnimated (ui->installLabel, 1500);

    connect (ui->quit, SIGNAL (clicked()), this, SLOT (close()));
    connect (ui->openTheme, SIGNAL (clicked()), this, SLOT (openTheme()));
    connect (ui->installTheme, SIGNAL (clicked()), this, SLOT (installTheme()));
    connect (ui->deleteTheme, SIGNAL (clicked()), this, SLOT (deleteTheme()));
    connect (ui->useTheme, SIGNAL (clicked()), this, SLOT (useTheme()));
    connect (ui->saveButton, SIGNAL (clicked()), this, SLOT (writeConfig()));
    connect (ui->restoreButton, SIGNAL (clicked()), this, SLOT (restoreDefault()));
    connect (ui->checkBoxNoComposite, SIGNAL (clicked (bool)), this, SLOT (notCompisited (bool)));
    connect (ui->checkBoxTrans, SIGNAL (clicked (bool)), this, SLOT (isTranslucent (bool)));
    connect (ui->checkBoxBlurWindow, SIGNAL (clicked (bool)), this, SLOT (popupBlurring (bool)));
    connect (ui->checkBoxDE, SIGNAL (clicked (bool)), this, SLOT (respectDE (bool)));
    connect (ui->lineEdit, SIGNAL (textChanged (const QString &)), this, SLOT (txtChanged (const QString &)));
    connect (ui->appsEdit, SIGNAL (textChanged (const QString &)), this, SLOT (txtChanged (const QString &)));
    connect (ui->toolBox, SIGNAL (currentChanged (int)), this, SLOT (tabChanged (int)));
    connect (ui->saveAppButton, SIGNAL (clicked()), this, SLOT (writeAppLists()));
    connect (ui->removeAppButton, SIGNAL (clicked()), this, SLOT (removeAppList()));
    connect (ui->preview, SIGNAL (clicked()), this, SLOT (preview()));
    connect (ui->aboutButton, SIGNAL (clicked()), this, SLOT (aboutDialog()));
    connect (ui->whatsthisButton, SIGNAL(clicked()), this, SLOT (showWhatsThis()));

#if QT_VERSION < 0x050000
    ui->labelTooltipDelay->setEnabled (false);
    ui->spinTooltipDelay->setEnabled (false);
#endif

    resize (sizeHint().expandedTo (QSize (600, 400)));
}
/*************************/
KvantumManager::~KvantumManager()
{
    delete ui;
    delete animation_;
    delete effect_;
}
/*************************/
void KvantumManager::closeEvent (QCloseEvent *event)
{
    process_->terminate();
    process_->waitForFinished();
    event->accept();
}
/*************************/
void KvantumManager::openTheme()
{
    ui->statusBar->clearMessage();
    QString filePath = QFileDialog::getExistingDirectory (this,
                                                          tr ("Open Kvantum Theme Folder..."),
                                                          lastPath_,
                                                          QFileDialog::ShowDirsOnly
                                                          | QFileDialog::ReadOnly
                                                          | QFileDialog::DontUseNativeDialog);
    ui->lineEdit->setText (filePath);
    lastPath_ = filePath;
}
/*************************/
/* Either folderPath is the path of a directory inside the config folder,
   in which case its name should be the theme name, or it points to a folder
   inside an alternative installation path, in which case its name should be
   "Kvantum" and the theme name should be the name of its parent directory. */
bool KvantumManager::isThemeDir (const QString &folderPath)
{
    if (folderPath.isEmpty()) return false;
    QDir dir = QDir (folderPath);
    if (!dir.exists()) return false;

    QString themeName = dir.dirName();

    QStringList parts = folderPath.split ("/");
    if (parts.last() == "Kvantum")
    {
        if (parts.size() < 3)
            return false;
        else
            themeName = parts.at (parts.size() - 2);
    }

    /* "Default" is reserved for the copied default theme,
       "Kvantum" for the alternative installation paths. */
    if (themeName == "Default" || themeName == "Kvantum")
        return false;
    /* QSettings doesn't accept spaces in the name */
    QString s = themeName.simplified();
    if (s.contains (" "))
        return false;

    QStringList files = dir.entryList (QDir::Files, QDir::Name);
    foreach (const QString &file, files)
    {
        if (file == QString ("%1.kvconfig").arg (themeName)
            || file == QString ("%1.svg").arg (themeName))
        {
            return true;
        }
    }

    return false;
}
/*************************/
// The returned path isn't checked for being a real Kvantum theme directory.
QString KvantumManager::userThemeDir (const QString &themeName)
{
    if (themeName.isEmpty())
        return xdg_config_home + QString ("/Kvantum/###"); // useless
    QString themeDir = xdg_config_home + QString ("/Kvantum/") + themeName;
    if (!themeName.endsWith ("#") && !isThemeDir (themeDir))
    {
        QString homeDir = QDir::homePath();
        themeDir = QString ("%1/.themes/%2/Kvantum").arg (homeDir).arg (themeName);
        if (!isThemeDir (themeDir))
            themeDir = QString ("%1/.local/share/themes/%2/Kvantum").arg (homeDir).arg (themeName);
    }
    return themeDir;
}
/*************************/
void KvantumManager::notWritable (const QString &path)
{
    QMessageBox msgBox (QMessageBox::Warning,
                        tr ("Kvantum"),
                        tr ("<center><b>You have no permission to write here:</b></center>"\
                            "<center>%1</center>"\
                            "<center>Please fix that first!</center>")
                           .arg (path),
                        QMessageBox::Close,
                        this);
    msgBox.exec();
}
/*************************/
void KvantumManager::installTheme()
{
    QString theme = ui->lineEdit->text();
    if (theme.isEmpty()) return;
    if (QDir (theme).dirName() == "Default")
    {
        QMessageBox msgBox (QMessageBox::Warning,
                            tr ("Kvantum"),
                            tr ("<center><b>This is not an installable Kvantum theme!</b></center>"\
                                "<center>The name of an installable themes should not be \"Default\".</center>"\
                                "<center>Please select another directory!</center>"),
                            QMessageBox::Close,
                            this);
        msgBox.exec();
    }
    else if ((QDir (theme).dirName()).contains ("#"))
    {
        QMessageBox msgBox (QMessageBox::Warning,
                            tr ("Kvantum"),
                            tr ("<center><b>This is not an installable Kvantum theme!</b></center>"\
                                "<center>Installable themes should not have # in their names.</center>"\
                                "<center>Please select another directory!</center>"),
                            QMessageBox::Close,
                            this);
        msgBox.exec();
    }
    else if (!isThemeDir (theme))
    {
            QMessageBox msgBox (QMessageBox::Warning,
                                tr ("Kvantum"),
                                tr ("<center><b>This is not a Kvantum theme folder!</b></center>"\
                                    "<center>Please select another directory!</center>"),
                                QMessageBox::Close,
                                this);
            msgBox.exec();
    }
    else
    {
        QString homeDir = QDir::homePath();
        QDir cf = QDir (xdg_config_home);
        cf.mkdir ("Kvantum");
        QDir kv = QDir (xdg_config_home + QString ("/Kvantum"));
        /* if the Kvantum themes directory is created or already exists... */
        if (kv.exists())
        {
            QDir themeDir = QDir (theme);
            QStringList parts = theme.split ("/");
            QString themeName;
            if (parts.last() == "Kvantum")
                themeName = parts.at (parts.size() - 2); // parts.size() >= 3 is guaranteed at isThemeDir()
            else
                themeName = themeDir.dirName();
            QDir _subDir = QDir (QString ("%1/Kvantum/%2#").arg (xdg_config_home).arg (themeName));
            if (_subDir.exists())
            {
                QMessageBox msgBox (QMessageBox::Warning,
                                    tr ("Kvantum"),
                                    tr ("<center><b>The theme already exists in modified form.</b></center>"\
                                        "<center>First You have to delete its modified version!</center>"),
                                    QMessageBox::Close,
                                    this);
                msgBox.exec();
                return;
            }
            /* ... and contains the same theme... */
            if (!kv.mkdir (themeName))
            {
                QDir subDir = QDir (xdg_config_home + QString ("/Kvantum/") + themeName);
                if (subDir == themeDir)
                {
                    QMessageBox msgBox (QMessageBox::Warning,
                                        tr ("Kvantum"),
                                        tr ("<center><b>You have selected an installed theme folder.</b></center>"\
                                            "<center>Please choose another directory!</center>"),
                                        QMessageBox::Close,
                                        this);
                    msgBox.exec();
                    return;
                }
                if (subDir.exists() && QFileInfo (subDir.absolutePath()).isWritable())
                {
                    QMessageBox msgBox (QMessageBox::Warning,
                                        tr ("Confirmation"),
                                        tr ("<center><b>The theme already exists.</b></center>"\
                                            "<center>Do you want to overwrite it?</center>"),
                                        QMessageBox::Yes | QMessageBox::No,
                                        this);
                    switch (msgBox.exec()) {
                    case QMessageBox::Yes:
                        /* ... then, remove the theme files first */
                        QFile::remove (QString ("%1/Kvantum/%2/%2.kvconfig").arg (xdg_config_home).arg (themeName));
                        QFile::remove (QString ("%1/Kvantum/%2/%2.svg").arg (xdg_config_home).arg (themeName));
                        break;
                    case QMessageBox::No:
                    default:
                        return;
                        break;
                    }
                }
                else
                {
                    notWritable (subDir.absolutePath());
                    return;
                }
            }
            /* inform the user about priorities */
            QString otherDir = QString (DATADIR) + QString ("/Kvantum/") + themeName;
            if (!isThemeDir (otherDir))
                otherDir = QString (DATADIR) + QString ("/themes/%1/Kvantum").arg (themeName);
            if (isThemeDir (otherDir))
            {
                QMessageBox msgBox (QMessageBox::Information,
                                    tr ("Kvantum"),
                                    tr ("<center><b>This theme is also installed as root in:</b></center>"\
                                        "<center>%1</center>"\
                                        "<center>The user installation will take priority.</center>")
                                       .arg (otherDir),
                                    QMessageBox::Close,
                                    this);
                msgBox.exec();
            }
            else
            {
                otherDir = QString ("%1/.themes/%2/Kvantum").arg (homeDir).arg (themeName);
                if (!isThemeDir (otherDir))
                    otherDir = QString ("%1/.local/share/themes/%2/Kvantum").arg (homeDir).arg (themeName);
                if (isThemeDir (otherDir))
                {
                    QMessageBox msgBox (QMessageBox::Information,
                                        tr ("Kvantum"),
                                        tr ("<center><b>This theme is also installed as user in:</b></center>"\
                                            "<center>%1</center>"\
                                            "<center>This installation will take priority.</center>")
                                           . arg (otherDir),
                                        QMessageBox::Close,
                                        this);
                    msgBox.exec();
                }
            }
            /* copy the theme files appropriately */
            QFile::copy (QString ("%1/%2.kvconfig").arg (theme).arg (themeName),
                         QString ("%1/Kvantum/%2/%2.kvconfig").arg (xdg_config_home).arg (themeName));
            QFile::copy (QString ("%1/%2.svg").arg (theme).arg (themeName),
                         QString ("%1/Kvantum/%2/%2.svg").arg (xdg_config_home).arg (themeName));
            /* also copy the color scheme file */
            QString colorFile = QString ("%1/%2.colors").arg (theme).arg (themeName);
            if (QFile::exists (colorFile))
            {
                QString kdeApps = QString ("%1/.kde/share/apps").arg (homeDir);
                QDir kdeAppsDir = QDir (kdeApps);
                if (!kdeAppsDir.exists())
                {
                    kdeApps = QString ("%1/.kde4/share/apps").arg (homeDir);
                    kdeAppsDir = QDir (kdeApps);
                }
                if (kdeAppsDir.exists())
                {
                    kdeAppsDir.mkdir ("color-schemes");
                    QString colorScheme = QString ("%1/color-schemes/%2.colors").arg (kdeApps).arg (themeName);
                    if (QFile::exists (colorScheme))
                        QFile::remove (colorScheme);
                    QFile::copy (colorFile, colorScheme);
                }
                /* repeat for kf5 */
                QString lShare = QString ("%1/.local/share").arg (homeDir);
                QDir lShareDir = QDir (lShare);
                if (lShareDir.exists())
                {
                    lShareDir.mkdir ("color-schemes");
                    QString colorScheme = QString ("%1/color-schemes/%2.colors").arg (lShare).arg (themeName);
                    if (QFile::exists (colorScheme))
                        QFile::remove (colorScheme);
                    QFile::copy (colorFile, colorScheme);
                }
            }

            ui->statusBar->showMessage (tr ("%1 installed.").arg (themeName), 10000);
        }
        else
            notWritable (kv.absolutePath());
    }

    updateThemeList();
}
/*************************/
static inline bool removeDir (const QString &dirName)
{
    bool res = true;
    QDir dir (dirName);
    if (dir.exists())
    {
        Q_FOREACH (QFileInfo info, dir.entryInfoList (QDir::Files | QDir::AllDirs
                                                      | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden,
                                                      QDir::DirsFirst))
        {
            if (info.isDir())
                res = removeDir (info.absoluteFilePath());
            else
                res = QFile::remove (info.absoluteFilePath());
            if (!res) return false;
        }
        res = dir.rmdir (dirName);
    }
    return res;
}
/*************************/
void KvantumManager::deleteTheme()
{
    QString theme = ui->comboBox->currentText();
    if (theme.isEmpty()) return;

    QString appTheme = theme; // for removing apps list later

    QMessageBox msgBox (QMessageBox::Question,
                        tr ("Confirmation"),
                        tr ("<center><b>Do you really want to delete <i>%1</i>?</b></center>").arg (theme),
                        QMessageBox::Yes | QMessageBox::No,
                        this);
    msgBox.setInformativeText (tr ("<center><i>It could not be restored unless you have a copy of it.</i></center>"));
    msgBox.setDefaultButton (QMessageBox::No);
    switch (msgBox.exec()) {
        case QMessageBox::No: return;
        case QMessageBox::Yes:
        default: break;
    }

    QString theme_ = theme;
    if (theme == "Kvantum (modified)")
        theme = "Default#";
    else if (theme.endsWith (" (modified)"))
        theme.replace (QString (" (modified)"), QString ("#"));
    else if (theme == "Kvantum (default)")
        return;

    QString homeDir = QDir::homePath();
    if (!removeDir (QString ("%1/Kvantum/%2").arg (xdg_config_home).arg (theme)))
    {
        QMessageBox msgBox (QMessageBox::Warning,
                            tr ("Kvantum"),
                            tr ("<center><b>This directory cannot be removed:</b></center>"\
                                "<center>%1/Kvantum/%2</center>"\
                                "<center>You might want to investigate the cause.</center>")
                               .arg (xdg_config_home).arg (theme),
                            QMessageBox::Close,
                            this);
        msgBox.exec();
        return;
    }
    if (!removeDir (QString ("%1/.themes/%2/Kvantum").arg (homeDir).arg (theme)))
    {
        QMessageBox msgBox (QMessageBox::Warning,
                            tr ("Kvantum"),
                            tr ("<center><b>This directory cannot be removed:</b></center>"\
                                "<center>%1/.themes/%2/Kvantum</center>"\
                                "<center>You might want to investigate the cause.</center>")
                               .arg (homeDir).arg (theme),
                            QMessageBox::Close,
                            this);
        msgBox.exec();
        return;
    }
    if (!removeDir (QString ("%1/.local/share/themes/%2/Kvantum").arg (homeDir).arg (theme)))
    {
        QMessageBox msgBox (QMessageBox::Warning,
                            tr ("Kvantum"),
                            tr ("<center><b>This directory cannot be removed:</b></center>"\
                                "<center>%1/.local/share/themes/%2/Kvantum</center>"\
                                "<center>You might want to investigate the cause.</center>")
                               .arg (homeDir).arg (theme),
                            QMessageBox::Close,
                            this);
        msgBox.exec();
        return;
    }
    QString configFile = QString ("%1/Kvantum/kvantum.kvconfig").arg (xdg_config_home);
    if (QFile::exists (configFile))
    {
        QSettings settings (configFile, QSettings::NativeFormat);
        if (settings.contains ("theme") && theme == settings.value ("theme").toString())
        {
            if (isThemeDir (QString ("%1/Kvantum/Default#").arg (xdg_config_home)))
            {
                settings.setValue ("theme", "Default#");
                kvconfigTheme_ = "Default#";
            }
            else
            {
                settings.remove ("theme");
                kvconfigTheme_ = QString();
            }
            QCoreApplication::processEvents();
            restyleWindow();
            if (process_->state() == QProcess::Running)
                preview();
        }
    }
    ui->statusBar->showMessage (tr ("%1 deleted.").arg (theme_), 10000);

    updateThemeList();
    /* remove the apps list associated with this theme */
    QStringList appList = appTheme.split (" ");
    if (appList.count() == 1) // not a modified root theme
    {
        QString appTheme = appList.first(); // for removing apps list later
        if (appTheme == "Kvantum") // impossible
            appTheme = "Default";
        appThemes_.remove (appTheme);
        origAppThemes_.remove (appTheme);
    }
    writeOrigAppLists();
}
/*************************/
void KvantumManager::showAnimated (QWidget *w, int duration)
{
    w->show();
    w->setGraphicsEffect (effect_);
    animation_->stop();
    animation_->setDuration (duration);
    animation_->setStartValue (0.0);
    animation_->setEndValue (1.0);
    animation_->start();
}
/*************************/
// Activates the theme and sets kvconfigTheme_.
void KvantumManager::useTheme()
{
    kvconfigTheme_ = ui->comboBox->currentText();
    if (kvconfigTheme_.isEmpty()) return;

    QString theme = kvconfigTheme_;
    if (kvconfigTheme_ == "Kvantum (modified)")
        kvconfigTheme_ = "Default#";
    else if (kvconfigTheme_.endsWith (" (modified)"))
         kvconfigTheme_.replace (QString (" (modified)"), QString ("#"));
    else if (kvconfigTheme_ == "Kvantum (default)")
        kvconfigTheme_ = QString();

    QString configFile = QString ("%1/Kvantum/kvantum.kvconfig").arg (xdg_config_home);
    QSettings settings (configFile, QSettings::NativeFormat);
    if (!settings.isWritable()) return;

    if (kvconfigTheme_.isEmpty())
        settings.remove ("theme");
    else if (settings.value ("theme").toString() != kvconfigTheme_)
        settings.setValue ("theme", kvconfigTheme_);

    QLabel *statusLabel = ui->statusBar->findChild<QLabel *>();
    statusLabel->setText (tr ("<b>Active theme:</b> %1").arg (theme));
    ui->statusBar->showMessage (tr ("Theme changed to %1.").arg (theme), 10000);
    showAnimated (ui->usageLabel, 1000);

    ui->useTheme->setEnabled (false);

    /* this is needed if the config file is created by this method */
    QCoreApplication::processEvents();
    restyleWindow();
    if (process_->state() == QProcess::Running)
        preview();

    confPageVisited_ = false;
}
/*************************/
void KvantumManager::txtChanged (const QString &txt)
{
    if (txt.isEmpty())
    {
        if (QObject::sender() == ui->lineEdit)
            ui->installTheme->setEnabled (false);
        else if (QObject::sender() == ui->appsEdit && appThemes_.isEmpty())
            ui->removeAppButton->setEnabled (false);
    }
    else
    {
        if (QObject::sender() == ui->lineEdit)
            ui->installTheme->setEnabled (true);
        else if (QObject::sender() == ui->appsEdit)
            ui->removeAppButton->setEnabled (true);
    }
}
/*************************/
void KvantumManager::defaultThemeButtons()
{
    ui->restoreButton->hide();
    /* Someone may have compiled Kvantum with another default theme.
       So, we get its settings directly from its kvconfig file. */
    QString defaultConfig = ":/Kvantum/default.kvconfig";
    QSettings defaultSettings (defaultConfig, QSettings::NativeFormat);

    defaultSettings.beginGroup ("Hacks");
    ui->checkBoxDolphin->setChecked (defaultSettings.value ("transparent_dolphin_view").toBool());
    ui->checkBoxPcmanfmSide->setChecked (defaultSettings.value ("transparent_pcmanfm_sidepane").toBool());
    ui->checkBoxPcmanfmView->setChecked (defaultSettings.value ("transparent_pcmanfm_view").toBool());
    ui->checkBoxBlurTranslucent->setChecked (defaultSettings.value ("blur_translucent").toBool());
    ui->checkBoxKtitle->setChecked (defaultSettings.value ("transparent_ktitle_label").toBool());
    ui->checkBoxMenuTitle->setChecked (defaultSettings.value ("transparent_menutitle").toBool());
    ui->checkBoxKCapacity->setChecked (defaultSettings.value ("kcapacitybar_as_progressbar").toBool());
    ui->checkBoxDark->setChecked (defaultSettings.value ("respect_darkness").toBool());
    ui->checkBoxGrip->setChecked (defaultSettings.value ("force_size_grip").toBool());
    ui->checkBoxNormalBtn->setChecked (defaultSettings.value ("normal_default_pushbutton").toBool());
    ui->checkBoxIconlessBtn->setChecked (defaultSettings.value ("iconless_pushbutton").toBool());
    ui->checkBoxIconlessMenu->setChecked (defaultSettings.value ("iconless_menu").toBool());
    ui->checkBoxToolbar->setChecked (defaultSettings.value ("single_top_toolbar").toBool());
    ui->checkBoxTint->setChecked (defaultSettings.value ("no_selection_tint").toBool());
    int tmp = 0;
    if (defaultSettings.contains ("tint_on_mouseover")) // it's false by default
        tmp = qMin (qMax (defaultSettings.value ("tint_on_mouseover").toInt(), 0), 100);
    ui->spinTint->setValue (tmp);
    tmp = 100;
    if (defaultSettings.contains ("disabled_icon_opacity")) // it's false by default
        tmp = qMin (qMax (defaultSettings.value ("disabled_icon_opacity").toInt(), 0), 100);
    ui->spinOpacity->setValue (tmp);
    tmp = 0;
    if (defaultSettings.contains ("lxqtmainmenu_iconsize")) // it's false by default
        tmp = qMin (qMax (defaultSettings.value ("lxqtmainmenu_iconsize").toInt(), 0), 32);
    ui->spinLxqtMenu->setValue (tmp);
    defaultSettings.endGroup();

    defaultSettings.beginGroup ("General");
    bool composited = defaultSettings.value ("composite").toBool();
    ui->checkBoxNoComposite->setChecked (!composited);
    ui->checkBoxAnimation->setChecked (defaultSettings.value ("animate_states").toBool());
    ui->checkBoxleftTab->setChecked (defaultSettings.value ("left_tabs").toBool());
    ui->checkBoxJoinTab->setChecked (defaultSettings.value ("joined_inactive_tabs").toBool());
    ui->checkBoxAttachTab->setChecked (defaultSettings.value ("attach_active_tab").toBool());
    if (defaultSettings.contains ("scroll_arrows")) // it's true by default
      ui->checkBoxNoScrollArrow->setChecked (!defaultSettings.value ("scroll_arrows").toBool());
    else
      ui->checkBoxNoScrollArrow->setChecked (false);
    ui->checkBoxScrollIn->setChecked (defaultSettings.value ("scrollbar_in_view").toBool());
    ui->checkBoxScrollableMenu->setChecked (defaultSettings.value ("scrollable_menu").toBool());
    ui->checkBoxTree->setChecked (defaultSettings.value ("tree_branch_line").toBool());
    ui->checkBoxGroupLabel->setChecked (defaultSettings.value ("groupbox_top_label").toBool());
    ui->checkBoxRubber->setChecked (defaultSettings.value ("fill_rubberband").toBool());
    if (defaultSettings.contains ("menubar_mouse_tracking")) // it's true by default
        ui->checkBoxMenubar->setChecked (defaultSettings.value ("menubar_mouse_tracking").toBool());
    else
        ui->checkBoxMenubar->setChecked (true);
    ui->checkBoxMenuToolbar->setChecked (defaultSettings.value ("merge_menubar_with_toolbar").toBool());
    if (defaultSettings.contains ("button_contents_shift")) // it's true by default
        ui->checkBoxButtonShift->setChecked (defaultSettings.value ("button_contents_shift").toBool());
    else
        ui->checkBoxButtonShift->setChecked (true);
    if (defaultSettings.contains ("alt_mnemonic")) // it's true by default
        ui->checkBoxAlt->setChecked (defaultSettings.value ("alt_mnemonic").toBool());
    else
        ui->checkBoxAlt->setChecked (true);
    int delay = -1;
#if QT_VERSION >= 0x050000
    if (defaultSettings.contains ("tooltip_delay")) // it's -1 by default
        delay = qMin (qMax (defaultSettings.value ("tooltip_delay").toInt(), -1), 9999);
    ui->spinTooltipDelay->setValue (delay);
# endif
    delay = 250;
    if (defaultSettings.contains ("submenu_delay")) // it's 250 by default
        delay = qMin (qMax (defaultSettings.value ("submenu_delay").toInt(), -1), 1000);
    ui->spinSubmenuDelay->setValue (delay);
    int index = 0;
    if (defaultSettings.contains ("toolbutton_style"))
    {
        index = defaultSettings.value ("toolbutton_style").toInt();
        if (index > 4 || index < 0) index = 0;
    }
    ui->comboToolButton->setCurrentIndex (index);
    ui->comboX11Drag->setCurrentIndex(toDrag(defaultSettings.value("x11drag").toString()));
    if (defaultSettings.contains ("respect_DE"))
        ui->checkBoxDE->setChecked (defaultSettings.value ("respect_DE").toBool());
    else
        ui->checkBoxDE->setChecked (true);
    ui->checkBoxClick->setChecked (defaultSettings.value ("double_click").toBool());
    ui->checkBoxInlineSpin->setChecked (defaultSettings.value ("inline_spin_indicators").toBool());
    ui->checkBoxVSpin->setChecked (defaultSettings.value ("vertical_spin_indicators").toBool());
    ui->checkBoxComboEdit->setChecked (defaultSettings.value ("combo_as_lineedit").toBool());
    ui->checkBoxComboMenu->setChecked (defaultSettings.value ("combo_menu").toBool());
    if (composited)
    {
        bool translucency = defaultSettings.value ("translucent_windows").toBool();
        QStringList lst = defaultSettings.value ("opaque").toStringList();
        if (!lst.isEmpty())
            ui->opaqueEdit->setText (lst.join (",")); // is cleared when the config page is shown
        ui->checkBoxTrans->setChecked (translucency);
        isTranslucent (translucency);
        if (translucency)
            ui->checkBoxBlurWindow->setChecked (defaultSettings.value ("blurring").toBool());
        popupBlurring (ui->checkBoxBlurWindow->isChecked());
        if (!ui->checkBoxBlurWindow->isChecked())
            ui->checkBoxBlurPopup->setChecked (defaultSettings.value ("popup_blurring").toBool());
    }
    int theSize = 16;
    if (defaultSettings.contains ("small_icon_size"))
        theSize = defaultSettings.value ("small_icon_size").toInt();
    theSize = qMin(qMax(theSize,16), 48);
    ui->spinSmall->setValue (theSize);
    theSize = 32;
    if (defaultSettings.contains ("large_icon_size"))
        theSize = defaultSettings.value ("large_icon_size").toInt();
    theSize = qMin(qMax(theSize,24), 128);
    ui->spinLarge->setValue (theSize);
    theSize = 16;
    if (defaultSettings.contains ("button_icon_size"))
        theSize = defaultSettings.value ("button_icon_size").toInt();
    theSize = qMin(qMax(theSize,16), 64);
    ui->spinButton->setValue (theSize);
    theSize = 24;
    if (defaultSettings.contains ("toolbar_icon_size"))
        theSize = defaultSettings.value ("toolbar_icon_size").toInt();
    else if (defaultSettings.value ("slim_toolbars").toBool())
        theSize = 16;
    theSize = qMin(qMax(theSize,16), 64);
    ui->spinToolbar->setValue (theSize);
    theSize = 2;
    if (defaultSettings.contains ("layout_spacing"))
      theSize = defaultSettings.value ("layout_spacing").toInt();
    theSize = qMin(qMax(theSize,2), 16);
    ui->spinLayout->setValue (theSize);
    theSize = 4;
    if (defaultSettings.contains ("layout_margin"))
      theSize = defaultSettings.value ("layout_margin").toInt();
    theSize = qMin(qMax(theSize,2), 16);
    ui->spinLayoutMargin->setValue (theSize);
    theSize = -1;
    if (defaultSettings.contains ("submenu_overlap"))
      theSize = defaultSettings.value ("submenu_overlap").toInt();
    theSize = qMin(qMax(theSize,-1), 16);
    ui->spinOverlap->setValue (theSize);
    theSize = 16;
    if (defaultSettings.contains ("spin_button_width"))
      theSize = defaultSettings.value ("spin_button_width").toInt();
    theSize = qMin(qMax(theSize,16), 32);
    ui->spinSpinBtnWidth->setValue (theSize);
    defaultSettings.endGroup();

    respectDE (ui->checkBoxDE->isChecked());
}
/*************************/
void KvantumManager::restyleWindow()
{
  QApplication::setStyle (QStyleFactory::create ("kvantum"));
#if QT_VERSION >= 0x050000
  // Qt5 has QEvent::ThemeChange
  Q_FOREACH(QWidget *widget, QApplication::allWidgets())
  {
    QEvent event (QEvent::ThemeChange);
    QApplication::sendEvent (widget, &event);
  }
#endif
}
/*************************/
void KvantumManager::tabChanged (int index)
{
    ui->statusBar->clearMessage();
    if (index == 0)
        showAnimated (ui->installLabel, 1000);
    if (index == 1 || index == 3)
    {
        if (index == 1)
        {
            /* put the active theme in the theme combobox */
            QString activeTheme;
            if (kvconfigTheme_.isEmpty())
                activeTheme = "Kvantum (default)";
            else if (kvconfigTheme_ == "Default#")
                activeTheme = "Kvantum (modified)";
            else if (kvconfigTheme_.endsWith ("#"))
                activeTheme = QString ("%1 (modified)").arg (kvconfigTheme_.left (kvconfigTheme_.length() - 1));
            else
                activeTheme = kvconfigTheme_;
            if (ui->comboBox->currentText() == activeTheme)
                showAnimated (ui->usageLabel, 1000);
            else
            {
#if QT_VERSION < 0x050000
                ui->comboBox->setCurrentIndex (ui->comboBox->findText (activeTheme));
#else
                ui->comboBox->setCurrentText (activeTheme); // sets tooltip, animation, etc.
#endif
            }
        }
        else
            showAnimated (ui->appLabel, 1000);
    }
    else if (index == 2)
    {
        ui->opaqueEdit->clear();
        defaultThemeButtons();

        if (kvconfigTheme_.isEmpty())
        {
            ui->configLabel->setText (tr ("These are the settings that can be safely changed.<br>For the others, click <i>Save</i> and then edit this file:<br><i>~/.config/Kvantum/Default#/<b>Default#.kvconfig</b></i>"));
            showAnimated (ui->configLabel, 1000);
        }
        else
        {
            /* a config other than the default Kvantum one */
            QString themeDir = userThemeDir (kvconfigTheme_);
            QString themeConfig = QString ("%1/%2.kvconfig").arg (themeDir).arg (kvconfigTheme_);
            QString userSvg = QString ("%1/%2.svg").arg (themeDir).arg (kvconfigTheme_);

            /* If themeConfig doesn't exist but userSvg does, themeConfig be created by copying
               the default config below and this message will be correct. If neither themeConfig
               nor userSvg exists, this message will be replaced by the one that follows it. */
            ui->configLabel->setText (tr ("These are the settings that can be safely changed.<br>For the others, edit this file:<br><i>%1</b></i>").arg (themeConfig));
            if (!QFile::exists (themeConfig) && !QFile::exists (userSvg))
            { // no user theme but a root one
                ui->configLabel->setText (tr ("These are the settings that can be safely changed.<br>For the others, click <i>Save</i> and then edit this file:<br><i>~/.config/Kvantum/%1#/<b>%1#.kvconfig</b></i>").arg (kvconfigTheme_));
            }
            showAnimated (ui->configLabel, 1000);

            if (kvconfigTheme_.endsWith ("#"))
                ui->restoreButton->show();
            else
                ui->restoreButton->hide();

            /* copy Kvantum's default theme config or find the root
               theme config if this user theme doesn't have a config */
            if (!QFile::exists (themeConfig))
            {
                if (QFile::exists (userSvg)) // a user theme without config
                    copyRootTheme (QString(), kvconfigTheme_);
                else // a root theme
                {
                    QString rootDir = QString (DATADIR) + QString ("/Kvantum/%1").arg (kvconfigTheme_);
                    if (!isThemeDir (rootDir))
                        rootDir =  QString (DATADIR) + QString ("/themes/%1/Kvantum").arg (kvconfigTheme_);
                    themeConfig = QString ("%1/%2.kvconfig").arg (rootDir).arg (kvconfigTheme_);
                }
            }

            if (QFile::exists (themeConfig)) // doesn't exist for a root theme without config
            {
                QSettings themeSettings (themeConfig, QSettings::NativeFormat);
                /* consult the default config file if a key doesn't exist */
                themeSettings.beginGroup ("General");
                bool composited = true;
                if (themeSettings.contains ("composite"))
                    composited = themeSettings.value ("composite").toBool();
                else
                    composited = false;
                ui->checkBoxNoComposite->setChecked (!composited);
                notCompisited (!composited);
                if (themeSettings.contains ("animate_states"))
                    ui->checkBoxAnimation->setChecked (themeSettings.value ("animate_states").toBool());
                if (themeSettings.contains ("left_tabs"))
                    ui->checkBoxleftTab->setChecked (themeSettings.value ("left_tabs").toBool());
                if (themeSettings.contains ("joined_inactive_tabs"))
                    ui->checkBoxJoinTab->setChecked (themeSettings.value ("joined_inactive_tabs").toBool());
                if (themeSettings.contains ("joined_tabs")) // backward compatibility
                    ui->checkBoxJoinTab->setChecked (themeSettings.value ("joined_tabs").toBool());
                if (themeSettings.contains ("attach_active_tab"))
                    ui->checkBoxAttachTab->setChecked (themeSettings.value ("attach_active_tab").toBool());
                if (themeSettings.contains ("scroll_arrows"))
                    ui->checkBoxNoScrollArrow->setChecked (!themeSettings.value ("scroll_arrows").toBool());
                if (themeSettings.contains ("scrollbar_in_view"))
                    ui->checkBoxScrollIn->setChecked (themeSettings.value ("scrollbar_in_view").toBool());
                if (themeSettings.contains ("scrollable_menu"))
                    ui->checkBoxScrollableMenu->setChecked (themeSettings.value ("scrollable_menu").toBool());
                if (themeSettings.contains ("tree_branch_line"))
                    ui->checkBoxTree->setChecked (themeSettings.value ("tree_branch_line").toBool());
                if (themeSettings.contains ("groupbox_top_label"))
                    ui->checkBoxGroupLabel->setChecked (themeSettings.value ("groupbox_top_label").toBool());
                if (themeSettings.contains ("fill_rubberband"))
                    ui->checkBoxRubber->setChecked (themeSettings.value ("fill_rubberband").toBool());
                if (themeSettings.contains ("menubar_mouse_tracking"))
                    ui->checkBoxMenubar->setChecked (themeSettings.value ("menubar_mouse_tracking").toBool());
                if (themeSettings.contains ("merge_menubar_with_toolbar"))
                    ui->checkBoxMenuToolbar->setChecked (themeSettings.value ("merge_menubar_with_toolbar").toBool());
                if (themeSettings.contains ("button_contents_shift"))
                    ui->checkBoxButtonShift->setChecked (themeSettings.value ("button_contents_shift").toBool());
                if (themeSettings.contains ("alt_mnemonic"))
                    ui->checkBoxAlt->setChecked (themeSettings.value ("alt_mnemonic").toBool());
                int delay = -1;
#if QT_VERSION >= 0x050000
                if (themeSettings.contains ("tooltip_delay"))
                    delay = qMin (qMax (themeSettings.value ("tooltip_delay").toInt(), -1), 9999);
                ui->spinTooltipDelay->setValue (delay);
#endif
                delay = 250;
                if (themeSettings.contains ("submenu_delay"))
                    delay = qMin (qMax (themeSettings.value ("submenu_delay").toInt(), -1), 1000);
                ui->spinSubmenuDelay->setValue (delay);
                if (themeSettings.contains ("toolbutton_style"))
                {
                    int index = themeSettings.value ("toolbutton_style").toInt();
                    if (index > 4 || index < 0) index = 0;
                    ui->comboToolButton->setCurrentIndex (index);
                }
                if (themeSettings.contains ("x11drag"))
                    ui->comboX11Drag->setCurrentIndex(toDrag(themeSettings.value("x11drag").toString()));
                if (themeSettings.contains ("respect_DE"))
                    ui->checkBoxDE->setChecked (themeSettings.value ("respect_DE").toBool());
                if (themeSettings.contains ("double_click"))
                    ui->checkBoxClick->setChecked (themeSettings.value ("double_click").toBool());
                if (themeSettings.contains ("inline_spin_indicators"))
                    ui->checkBoxInlineSpin->setChecked (themeSettings.value ("inline_spin_indicators").toBool());
                if (themeSettings.contains ("vertical_spin_indicators"))
                    ui->checkBoxVSpin->setChecked (themeSettings.value ("vertical_spin_indicators").toBool());
                if (themeSettings.contains ("combo_as_lineedit"))
                    ui->checkBoxComboEdit->setChecked (themeSettings.value ("combo_as_lineedit").toBool());
                if (themeSettings.contains ("combo_menu"))
                    ui->checkBoxComboMenu->setChecked (themeSettings.value ("combo_menu").toBool());
                if (composited)
                {
                    bool translucency = false;
                    if (themeSettings.contains ("translucent_windows"))
                        translucency = themeSettings.value ("translucent_windows").toBool();
                    QStringList lst = themeSettings.value ("opaque").toStringList();
                    if (!lst.isEmpty())
                        ui->opaqueEdit->setText (lst.join (","));
                    ui->checkBoxTrans->setChecked (translucency);
                    isTranslucent (translucency);
                    if (translucency && themeSettings.contains ("blurring"))
                        ui->checkBoxBlurWindow->setChecked (themeSettings.value ("blurring").toBool());
                    /* popup_blurring is required by blurring */
                    popupBlurring (ui->checkBoxBlurWindow->isChecked());
                    if (!ui->checkBoxBlurWindow->isChecked() && themeSettings.contains ("popup_blurring"))
                        ui->checkBoxBlurPopup->setChecked (themeSettings.value ("popup_blurring").toBool());
                }
                if (themeSettings.contains ("small_icon_size"))
                {
                    int icnSize = themeSettings.value ("small_icon_size").toInt();
                    icnSize = qMin(qMax(icnSize,16), 48);
                    ui->spinSmall->setValue (icnSize);
                }
                if (themeSettings.contains ("large_icon_size"))
                {
                    int icnSize = themeSettings.value ("large_icon_size").toInt();
                    icnSize = qMin(qMax(icnSize,24), 128);
                    ui->spinLarge->setValue (icnSize);
                }
                if (themeSettings.contains ("button_icon_size"))
                {
                    int icnSize = themeSettings.value ("button_icon_size").toInt();
                    icnSize = qMin(qMax(icnSize,16), 64);
                    ui->spinButton->setValue (icnSize);
                }
                if (themeSettings.contains ("toolbar_icon_size"))
                {
                    int icnSize = themeSettings.value ("toolbar_icon_size").toInt();
                    icnSize = qMin(qMax(icnSize,16), 64);
                    ui->spinToolbar->setValue (icnSize);
                }
                else if (themeSettings.contains ("slim_toolbars"))
                    ui->spinToolbar->setValue (16);
                if (themeSettings.contains ("layout_spacing"))
                {
                    int theSize = themeSettings.value ("layout_spacing").toInt();
                    theSize = qMin(qMax(theSize,2), 16);
                    ui->spinLayout->setValue (theSize);
                }
                if (themeSettings.contains ("layout_margin"))
                {
                    int theSize = themeSettings.value ("layout_margin").toInt();
                    theSize = qMin(qMax(theSize,2), 16);
                    ui->spinLayoutMargin->setValue (theSize);
                }
                if (themeSettings.contains ("submenu_overlap"))
                {
                    int theSize = themeSettings.value ("submenu_overlap").toInt();
                    theSize = qMin(qMax(theSize,-1), 16);
                    ui->spinOverlap->setValue (theSize);
                }
                if (themeSettings.contains ("spin_button_width"))
                {
                    int theSize = themeSettings.value ("spin_button_width").toInt();
                    theSize = qMin(qMax(theSize,16), 32);
                    ui->spinSpinBtnWidth->setValue (theSize);
                }
                themeSettings.endGroup();

                themeSettings.beginGroup ("Hacks");
                ui->checkBoxDolphin->setChecked (themeSettings.value ("transparent_dolphin_view").toBool());
                ui->checkBoxPcmanfmSide->setChecked (themeSettings.value ("transparent_pcmanfm_sidepane").toBool());
                ui->checkBoxPcmanfmView->setChecked (themeSettings.value ("transparent_pcmanfm_view").toBool());
                bool blurTrans ((themeSettings.contains ("blur_translucent")
                                 && themeSettings.value ("blur_translucent").toBool())
                                || themeSettings.value ("blur_konsole").toBool()); // backward compatibility
                ui->checkBoxBlurTranslucent->setChecked (blurTrans);
                ui->checkBoxKtitle->setChecked (themeSettings.value ("transparent_ktitle_label").toBool());
                ui->checkBoxMenuTitle->setChecked (themeSettings.value ("transparent_menutitle").toBool());
                ui->checkBoxKCapacity->setChecked (themeSettings.value ("kcapacitybar_as_progressbar").toBool());
                ui->checkBoxDark->setChecked (themeSettings.value ("respect_darkness").toBool());
                ui->checkBoxGrip->setChecked (themeSettings.value ("force_size_grip").toBool());
                ui->checkBoxNormalBtn->setChecked (themeSettings.value ("normal_default_pushbutton").toBool());
                ui->checkBoxIconlessBtn->setChecked (themeSettings.value ("iconless_pushbutton").toBool());
                ui->checkBoxIconlessMenu->setChecked (themeSettings.value ("iconless_menu").toBool());
                ui->checkBoxToolbar->setChecked (themeSettings.value ("single_top_toolbar").toBool());
                ui->checkBoxTint->setChecked (themeSettings.value ("no_selection_tint").toBool());
                int tmp = 0;
                if (themeSettings.contains ("tint_on_mouseover"))
                    tmp = qMin (qMax (themeSettings.value ("tint_on_mouseover").toInt(), 0), 100);
                ui->spinTint->setValue (tmp);
                tmp = 100;
                if (themeSettings.contains ("disabled_icon_opacity"))
                    tmp = qMin (qMax (themeSettings.value ("disabled_icon_opacity").toInt(), 0), 100);
                ui->spinOpacity->setValue (tmp);
                tmp = 0;
                if (themeSettings.contains ("lxqtmainmenu_iconsize"))
                    tmp = qMin (qMax (themeSettings.value ("lxqtmainmenu_iconsize").toInt(), 0), 100);
                ui->spinLxqtMenu->setValue (tmp);
                themeSettings.endGroup();

                respectDE (ui->checkBoxDE->isChecked());
            }
        }
        if (!confPageVisited_)
        { // here we try to avoid scrollbars as far as possible but there is no exact way for that
            QStyle *style = QApplication::style();
            int textIconHeight = qMax (style->pixelMetric (QStyle::PM_SmallIconSize),
                                       QFontMetrics(font()).height());
            QSize newSize = size().expandedTo (ui->groupBox->sizeHint()
                              + QSize (4*style->pixelMetric (QStyle::PM_LayoutLeftMargin)
                                           + style->pixelMetric (QStyle::PM_ScrollBarExtent),
                                       2*ui->saveButton->sizeHint().height()
                                         + ui->statusBar->sizeHint().height()
                                         + ui->configLabel->sizeHint().height()
                                         + (2+3)*style->pixelMetric (QStyle::PM_LayoutBottomMargin)
                                         + (4+3)*style->pixelMetric (QStyle::PM_LayoutVerticalSpacing)
                                         + 6*textIconHeight
                                         + (ui->restoreButton->isEnabled() ? textIconHeight : 0)));
            newSize = newSize.boundedTo (QApplication::desktop()->availableGeometry().size());
            resize (newSize);
            confPageVisited_ = true;
        }
    }
}
/*************************/
/* Gets the comment and sets the state of the "deleteTheme" button. */
QString KvantumManager::getComment (const QString &comboText, bool setState)
{
    QString comment;
    if (comboText.isEmpty()) return comment;

    QString text = comboText;
    if (text == "Kvantum (default)")
        text = QString();
    else if (text == "Kvantum (modified)")
        text = "Default#";
    else if (text.endsWith (" (modified)"))
        text.replace (QString (" (modified)"), QString ("#"));

    QString themeDir, themeConfig;
    if (!text.isEmpty())
    {
        themeDir = userThemeDir (text);
        themeConfig = QString ("%1/%2.kvconfig").arg (themeDir).arg (text);
    }

    if (setState)
    {
        if (comboText == "Kvantum (default)"
            || !isThemeDir (themeDir)) // root
        {
            ui->deleteTheme->setEnabled (false);
        }
        else
            ui->deleteTheme->setEnabled (true);
    }

    if (!text.isEmpty())
    {
        if (!isThemeDir (themeDir))
        {
            if (isThemeDir (QString (DATADIR) + QString ("/Kvantum/%1").arg (text)))
                themeConfig = QString (DATADIR) + QString ("/Kvantum/%1/%1.kvconfig").arg (text);
            else
                themeConfig = QString (DATADIR) + QString ("/themes/%1/Kvantum/%1.kvconfig").arg (text);
        }

        if (QFile::exists (themeConfig))
        {
            QSettings themeSettings (themeConfig, QSettings::NativeFormat);
            themeSettings.beginGroup ("General");
            comment = themeSettings.value ("comment").toString();
            if (comment.isEmpty()) // comma(s) in the comment
            {
                QStringList lst = themeSettings.value ("comment").toStringList();
                if (!lst.isEmpty())
                    comment = lst.join (", ");
            }
            themeSettings.endGroup();
        }
    }
    else comment = "The default Kvantum theme";
    if (comment.isEmpty())
      comment = "No description";

    return comment;
}
/*************************/
void KvantumManager::selectionChanged (const QString &txt)
{
    if (txt.isEmpty()) return; // not needed

    ui->statusBar->clearMessage();

    QString theme;
    if (kvconfigTheme_.isEmpty())
        theme = "Kvantum (default)";
    else if (kvconfigTheme_ == "Default#")
        theme = "Kvantum (modified)";
    else if (kvconfigTheme_.endsWith ("#"))
        theme = QString ("%1 (modified)").arg (kvconfigTheme_.left (kvconfigTheme_.length() - 1));
    else
        theme = kvconfigTheme_;

    if (txt == theme)
    {
        ui->useTheme->setEnabled (false);
        showAnimated (ui->usageLabel, 1000);
    }
    else
    {
        ui->useTheme->setEnabled (true);
        ui->usageLabel->hide();
    }

    QString comment = getComment (txt);
    ui->comboBox->setToolTip (comment);
    ui->comboBox->setWhatsThis (comment);
}
/*************************/
void KvantumManager::assignAppTheme (const QString &previousTheme, const QString &newTheme)
{
    if (previousTheme.isEmpty() || newTheme.isEmpty()) // not needed
        return;
    /* first assign the previous app theme... */
    QString appTheme = previousTheme;
    appTheme = appTheme.split (" ").first();
    if (appTheme == "Kvantum")
        appTheme = "Default";
    QString editTxt = ui->appsEdit->text();
    if (!editTxt.isEmpty())
    {
        editTxt = editTxt.simplified();
        editTxt.remove (" ");
        QStringList appList = editTxt.split (",", QString::SkipEmptyParts);
        appList.removeDuplicates();
        appThemes_.insert (appTheme, appList);
    }
    else
        appThemes_.remove (appTheme);
    /* ... then set the lineedit text to the apps list of the new theme */
    appTheme = newTheme;
    appTheme = appTheme.split (" ").first();
    if (appTheme == "Kvantum")
        appTheme = "Default";
    if (!appThemes_.value (appTheme).isEmpty())
        ui->appsEdit->setText (appThemes_.value (appTheme).join (","));
    else
        ui->appsEdit->setText ("");

    ui->removeAppButton->setDisabled (appThemes_.isEmpty());

    QString comment = getComment (newTheme, false);
    ui->appCombo->setToolTip (comment);
    ui->appCombo->setWhatsThis (comment);
}
/*************************/
void KvantumManager::updateThemeList (bool updateAppThemes)
{
    /* may be connected before */
    disconnect (ui->comboBox, SIGNAL (currentIndexChanged (const QString &)),
                this, SLOT (selectionChanged (const QString &)));
    ui->comboBox->clear();
    QString curAppTheme;
    if (updateAppThemes)
    {
        disconnect (ui->appCombo, SIGNAL (textChangedSignal (const QString &, const QString &)),
                    this, SLOT (assignAppTheme (const QString &, const QString &)));
        curAppTheme = ui->appCombo->currentText();
        ui->appCombo->clear();
    }

    QStringList list;

    /* first add the user themes to the list */
    QDir kv = QDir (QString ("%1/Kvantum").arg (xdg_config_home));
    if (kv.exists())
    {
        QStringList folders = kv.entryList (QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        foreach (const QString &folder, folders)
        {
            if (isThemeDir (QString ("%1/Kvantum/%2").arg (xdg_config_home).arg (folder)))
            {
                if (folder == "Default#")
                    list.prepend ("Kvantum (modified)");
                else if (folder.endsWith ("#"))
                {
                    /* see if there's a valid root installtion */
                    QString folder_ = folder.left (folder.length() - 1);
                    if (!folder_.contains ("#")
                        && (isThemeDir (QString (DATADIR) + QString ("/Kvantum/%1").arg (folder_))
                            || isThemeDir (QString (DATADIR) + QString ("/themes/%1/Kvantum").arg (folder_))))
                    {
                        list.append (QString ("%1 (modified)").arg (folder_));
                    }
                }
                else
                    list.append (folder);
            }
        }
    }
    QString homeDir = QDir::homePath();
    kv = QDir (QString ("%1/.themes").arg (homeDir));
    if (kv.exists())
    {
        QStringList folders = kv.entryList (QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        foreach (const QString &folder, folders)
        {
            if (isThemeDir (QString ("%1/.themes/%2/Kvantum").arg (homeDir).arg (folder))
                && !folder.contains ("#")
                && !list.contains (folder)) // the themes installed in the config folder have priority
            {
                list.append (folder);
            }
        }
    }
    kv = QDir (QString ("%1/.local/share/themes").arg (homeDir));
    if (kv.exists())
    {
        QStringList folders = kv.entryList (QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        foreach (const QString &folder, folders)
        {
            if (isThemeDir (QString ("%1/.local/share/themes/%2/Kvantum").arg (homeDir).arg (folder))
                && !folder.contains ("#")
                && !list.contains (folder)) // the user themes installed in the above paths have priority
            {
                list.append (folder);
            }
        }
    }

    /* now add the root themes */
    QStringList rootList;
    kv = QDir (QString (DATADIR) + QString ("/Kvantum"));
    if (kv.exists())
    {
        QStringList folders = kv.entryList (QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        foreach (const QString &folder, folders)
        {
            if (!folder.contains ("#")
                && isThemeDir (QString (DATADIR) + QString ("/Kvantum/%1").arg (folder)))
            {
                if (!list.contains (folder) // a user theme with the same name takes priority
                    && !list.contains (QString ("%1 (modified)").arg (folder)))
                {
                    rootList.append (folder);
                }
            }
        }
    }
    kv = QDir (QString (DATADIR) + QString ("/themes"));
    if (kv.exists())
    {
        QStringList folders = kv.entryList (QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        foreach (const QString &folder, folders)
        {
            if (!folder.contains ("#")
                && isThemeDir (QString (DATADIR) + QString ("/themes/%1/Kvantum").arg (folder)))
            {
                if (!list.contains (folder) // a user theme with the same name takes priority
                    && !list.contains (QString ("%1 (modified)").arg (folder))
                    // a root theme inside 'DATADIR/Kvantum/' with the same name takes priority
                    && !rootList.contains (folder))
                {
                    rootList.append (folder);
                }
            }
        }
    }
    list.append (rootList);
    list.sort();

    /* add the whole list to the combobox */
    bool hasDefaultThenme (false);
    if (list.isEmpty() || !list.contains("Kvantum (modified)"))
    {
        list.prepend ("Kvantum (default)");
        hasDefaultThenme = true;
    }
    ui->comboBox->insertItems (0, list);
    if (updateAppThemes)
        ui->appCombo->insertItems (0, list);
    if (hasDefaultThenme)
    {
        ui->comboBox->insertSeparator (1);
        ui->comboBox->insertSeparator (1);
        if (updateAppThemes)
        {
            ui->appCombo->insertSeparator (1);
            ui->appCombo->insertSeparator (1);
        }
    }
    if (updateAppThemes && !curAppTheme.isEmpty()) // restore the previous text
    {
        int indx = ui->appCombo->findText (curAppTheme);
        if (indx > -1)
            ui->appCombo->setCurrentIndex (indx);
    }

    /* select the active theme and set kvconfigTheme_ */
    QString theme;
    QString configFile = QString ("%1/Kvantum/kvantum.kvconfig").arg (xdg_config_home);
    bool noConfig = false;
    if (QFile::exists (configFile))
    {
        QSettings settings (configFile, QSettings::NativeFormat);
        if (settings.contains ("theme"))
        {
            kvconfigTheme_ = settings.value ("theme").toString();
            if (kvconfigTheme_.isEmpty())
                theme = "Kvantum (default)";
            else if (kvconfigTheme_ == "Default#")
                theme = "Kvantum (modified)";
            else if (kvconfigTheme_.endsWith ("#"))
                theme = QString ("%1 (modified)").arg (kvconfigTheme_.left (kvconfigTheme_.length() - 1));
            else
                theme = kvconfigTheme_;
            int index = ui->comboBox->findText (theme);
            if (index > -1)
                ui->comboBox->setCurrentIndex (index);
            else
            {
                /* if there's a modified version of this theme, use it instead */
                if (!kvconfigTheme_.endsWith ("#"))
                    index = ui->comboBox->findText (theme + " (modified)");
                if (index > -1)
                {
                    kvconfigTheme_ += "#";
                    theme += " (modified)";
                    ui->comboBox->setCurrentIndex (index);
                    settings.setValue ("theme", kvconfigTheme_); // correct the config file
                    QCoreApplication::processEvents();
                    restyleWindow();
                    if (process_->state() == QProcess::Running)
                        preview();
                }
                else // remove from settings if its folder is deleted
                {
                    if (list.contains ("Kvantum (modified)"))
                    {
                        settings.setValue ("theme", "Default#");
                        kvconfigTheme_ = "Default#";
                        theme = "Kvantum (modified)";
                    }
                    else
                    {
                        settings.remove ("theme");
                        kvconfigTheme_ = QString();
                        theme = "Kvantum (default)";
                    }
                }
            }
        }
        else noConfig = true;

        /* getting the app themes list is needed only the first time */
        if (updateAppThemes && appThemes_.isEmpty())
        {
            settings.beginGroup ("Applications");
            QStringList appThemes = settings.childKeys();
            QStringList appList;
            QString appTheme;
            bool nonexistent = false;
            for (int i = 0; i < appThemes.count(); ++i)
            {
                appList = settings.value (appThemes.at(i)).toStringList();
                appList.removeDuplicates();
                appTheme = appThemes.at (i);
                if (appTheme.endsWith ("#"))
                {
                    /* we remove # for simplicity and add it only at writeAppLists() */
                    appTheme.remove (appTheme.count() - 1, 1);
                    if (ui->comboBox->findText (appTheme + " (modified)") == -1)
                    {
                        nonexistent = true;
                        continue;
                    }
                }
                /* see if the theme is incorrect but its modified version exists */
                else if (ui->comboBox->findText (appTheme) == -1)
                {
                    nonexistent = true;
                    if (ui->comboBox->findText (appTheme + " (modified)") == -1)
                        continue;
                }
                appThemes_.insert (appTheme, appList);
            }
            settings.endGroup();
            origAppThemes_ = appThemes_;
            if (nonexistent) // correct the config file
                writeOrigAppLists();
        }
    }
    else noConfig = true;

    if (noConfig)
    {
        kvconfigTheme_ = QString();
        theme = "Kvantum (default)";
        /* remove Default# because there's no config */
        QString theCopy = QString ("%1/Kvantum/Default#/Default#.kvconfig").arg (xdg_config_home);
        QFile::remove (theCopy);
    }

    QString comment = getComment (ui->comboBox->currentText());
    ui->comboBox->setToolTip (comment);
    ui->comboBox->setWhatsThis (comment);

    /* connect to combobox signal */
    connect (ui->comboBox, SIGNAL (currentIndexChanged (const QString &)),
             this, SLOT (selectionChanged (const QString &)));

    /* put the app themes list in the text edit */
    if (updateAppThemes)
    {
        QString curTxt = ui->appCombo->currentText();
        if (!curTxt.isEmpty())
        {
            comment = getComment (curTxt, false);
            ui->appCombo->setToolTip (comment);
            ui->appCombo->setWhatsThis (comment);

            curTxt = curTxt.split (" ").first();
            if (curTxt == "Kvantum")
                curTxt = "Default";
            if (!appThemes_.value (curTxt).isEmpty())
                ui->appsEdit->setText (appThemes_.value (curTxt).join (","));
        }

        connect (ui->appCombo, SIGNAL (textChangedSignal (const QString &, const QString &)),
                 this, SLOT (assignAppTheme (const QString &, const QString &)));
    }

    QLabel *statusLabel = ui->statusBar->findChild<QLabel *>();
    statusLabel->setText (tr ("<b>Active theme:</b> %1").arg (theme));
}
/*************************/
void KvantumManager::preview()
{
    QString binDir = QApplication::applicationDirPath();
    QString previewExe = binDir + "/kvantumpreview";
#if QT_VERSION >= 0x050000
    previewExe += " -style kvantum";
#endif
    process_->terminate();
    process_->waitForFinished();
    process_->start (previewExe);
}
/*************************/
/* This either copies the default config to a user theme without config
   or copies a root config to a folder under the config directory. */
bool KvantumManager::copyRootTheme (QString source, QString target)
{
    if (target.isEmpty()) return false;
    QDir cf = QDir (xdg_config_home);
    cf.mkdir ("Kvantum"); // for the default theme, the config folder may not exist yet
    QString kv = xdg_config_home + QString ("/Kvantum");
    QDir kvDir = QDir (kv);
    if (!kvDir.exists())
    {
        notWritable (kv);
        return false;
    }
    QString targetFolder = QString ("%1/Kvantum/%2") .arg (xdg_config_home).arg (target);
    QString altPath;
    if (source.isEmpty()) // the default config will be copied to a user theme without config
    {
        altPath = userThemeDir (target);
        if (altPath == targetFolder || !isThemeDir (altPath))
            altPath = QString();
    }

    QString theCopy;
    if (altPath.isEmpty())
    { /* we're under the config directory (and want to copy either a non-default root
         config to it or the default config to a user theme inside the config folder) */
        kvDir.mkdir (target);
        theCopy = QString ("%1/Kvantum/%2/%2.kvconfig").arg (xdg_config_home).arg (target);
    }
    else
        /* we're under an alternative theme installation path (and want
           to copy the default config to a user theme without config) */
        theCopy = QString ("%1/%2.kvconfig").arg (altPath).arg (target);
    if (!QFile::exists (theCopy))
    {
        QString themeConfig = ":/Kvantum/default.kvconfig";
        if (!source.isEmpty())
        {
            QString sourceDir = QString (DATADIR) + QString ("/Kvantum/%1").arg (source);
            if (!isThemeDir (sourceDir))
                sourceDir = QString (DATADIR) + QString ("/themes/%1/Kvantum").arg (source);
            QString _themeConfig = QString ("%1/%2.kvconfig").arg (sourceDir).arg (source);
            if (QFile::exists (_themeConfig)) // otherwise, the root theme is just an SVG image
                themeConfig = _themeConfig;
        }
        if (QFile::copy (themeConfig, theCopy))
        {
#if QT_VERSION < 0x050000
            QFile::setPermissions (theCopy, QFile::permissions (theCopy) | QFile::WriteOwner);
#else
            QFile::setPermissions (theCopy, QFile::permissions (theCopy) | QFileDevice::WriteOwner);
#endif
            ui->statusBar->showMessage (tr ("A copy of the root config is created."), 10000);
        }
        else
        {
            notWritable (theCopy);
            return false;
        }
    }
    else
    {
        ui->statusBar->clearMessage();
        ui->statusBar->showMessage (tr ("A copy was already created."), 10000);
    }

    return true;
}
#if QT_VERSION >= 0x050000
/*************************/
static QString boolToStr (bool b)
{
    if (b) return QString ("true");
    else return QString ("false");
}
#endif
/*************************/
void KvantumManager::writeConfig()
{
    bool wasRootTheme = false;
    if (kvconfigTheme_.isEmpty()) // default theme
    {
        wasRootTheme = true;
        QFile::remove (QString ("%1/Kvantum/Default#/Default#.kvconfig").arg (xdg_config_home));
        kvconfigTheme_ = "Default#";
        if (!copyRootTheme (QString(), kvconfigTheme_))
            return;
    }

    QString themeDir = userThemeDir (kvconfigTheme_);
    QString themeConfig = QString ("%1/%2.kvconfig").arg (themeDir).arg (kvconfigTheme_);
    if (!QFile::exists (themeConfig))
    { // root theme (because Kvantum's default theme config is copied at tabChanged())
        wasRootTheme = true;
        QFile::remove (QString ("%1/Kvantum/%2#/%2#.kvconfig").arg (xdg_config_home).arg (kvconfigTheme_));
        if (!copyRootTheme (kvconfigTheme_, kvconfigTheme_ + "#"))
            return;
        kvconfigTheme_ = kvconfigTheme_ + "#";
    }

    QString configFile = QString ("%1/Kvantum/kvantum.kvconfig").arg (xdg_config_home);
    QSettings settings (configFile, QSettings::NativeFormat);
    if (!settings.isWritable())
    {
        notWritable (configFile);
        return;
    }
    settings.setValue ("theme", kvconfigTheme_);

    if (wasRootTheme)
        themeConfig = QString ("%1/Kvantum/%2/%2.kvconfig").arg (xdg_config_home).arg (kvconfigTheme_);
    if (QFile::exists (themeConfig)) // user theme (originally or after copying)
    {
        QSettings themeSettings (themeConfig, QSettings::NativeFormat);
        if (!themeSettings.isWritable())
        {
            notWritable (themeConfig);
            return;
        }
#if QT_VERSION >= 0x050000
        /*************************************************************************
          WARNING! Damn! The Qt5 QSettings changes the order of keys on writing.
                         We should write the settings directly!
        **************************************************************************/
        QMap<QString, QString> hackKeys, hackKeysMissing, generalKeys, generalKeysMissing;
        QString str;
        hackKeys.insert("transparent_dolphin_view", boolToStr (ui->checkBoxDolphin->isChecked()));
        hackKeys.insert("transparent_pcmanfm_sidepane", boolToStr (ui->checkBoxPcmanfmSide->isChecked()));
        hackKeys.insert("transparent_pcmanfm_view", boolToStr (ui->checkBoxPcmanfmView->isChecked()));
        hackKeys.insert("blur_translucent", boolToStr (ui->checkBoxBlurTranslucent->isChecked()));
        hackKeys.insert("transparent_ktitle_label", boolToStr (ui->checkBoxKtitle->isChecked()));
        hackKeys.insert("transparent_menutitle", boolToStr (ui->checkBoxMenuTitle->isChecked()));
        hackKeys.insert("kcapacitybar_as_progressbar", boolToStr (ui->checkBoxKCapacity->isChecked()));
        hackKeys.insert("respect_darkness", boolToStr (ui->checkBoxDark->isChecked()));
        hackKeys.insert("force_size_grip", boolToStr (ui->checkBoxGrip->isChecked()));
        hackKeys.insert("normal_default_pushbutton", boolToStr (ui->checkBoxNormalBtn->isChecked()));
        hackKeys.insert("iconless_pushbutton", boolToStr (ui->checkBoxIconlessBtn->isChecked()));
        hackKeys.insert("iconless_menu", boolToStr (ui->checkBoxIconlessMenu->isChecked()));
        hackKeys.insert("single_top_toolbar", boolToStr (ui->checkBoxToolbar->isChecked()));
        hackKeys.insert("no_selection_tint", boolToStr (ui->checkBoxTint->isChecked()));
        hackKeys.insert("tint_on_mouseover", str.setNum (ui->spinTint->value()));
        hackKeys.insert("disabled_icon_opacity", str.setNum (ui->spinOpacity->value()));
        hackKeys.insert("lxqtmainmenu_iconsize", str.setNum (ui->spinLxqtMenu->value()));

        generalKeys.insert("composite", boolToStr (!ui->checkBoxNoComposite->isChecked()));
        generalKeys.insert("animate_states", boolToStr (ui->checkBoxAnimation->isChecked()));
        generalKeys.insert("left_tabs", boolToStr (ui->checkBoxleftTab->isChecked()));
        generalKeys.insert("joined_inactive_tabs", boolToStr (ui->checkBoxJoinTab->isChecked()));
        generalKeys.insert("attach_active_tab", boolToStr (ui->checkBoxAttachTab->isChecked()));
        generalKeys.insert("scroll_arrows", boolToStr (!ui->checkBoxNoScrollArrow->isChecked()));
        generalKeys.insert("scrollbar_in_view", boolToStr (ui->checkBoxScrollIn->isChecked()));
        generalKeys.insert("scrollable_menu", boolToStr (ui->checkBoxScrollableMenu->isChecked()));
        generalKeys.insert("tree_branch_line", boolToStr (ui->checkBoxTree->isChecked()));
        generalKeys.insert("groupbox_top_label", boolToStr (ui->checkBoxGroupLabel->isChecked()));
        generalKeys.insert("fill_rubberband", boolToStr (ui->checkBoxRubber->isChecked()));
        generalKeys.insert("menubar_mouse_tracking",  boolToStr (ui->checkBoxMenubar->isChecked()));
        generalKeys.insert("merge_menubar_with_toolbar", boolToStr (ui->checkBoxMenuToolbar->isChecked()));
        generalKeys.insert("button_contents_shift", boolToStr (ui->checkBoxButtonShift->isChecked()));
        generalKeys.insert("alt_mnemonic", boolToStr (ui->checkBoxAlt->isChecked()));
        generalKeys.insert("tooltip_delay", str.setNum (ui->spinTooltipDelay->value()));
        generalKeys.insert("submenu_delay", str.setNum (ui->spinSubmenuDelay->value()));
        generalKeys.insert("toolbutton_style", str.setNum (ui->comboToolButton->currentIndex()));
        generalKeys.insert("x11drag", toStr((Drag)ui->comboX11Drag->currentIndex()));
        generalKeys.insert("respect_DE", boolToStr (ui->checkBoxDE->isChecked()));
        generalKeys.insert("double_click", boolToStr (ui->checkBoxClick->isChecked()));
        generalKeys.insert("inline_spin_indicators", boolToStr (ui->checkBoxInlineSpin->isChecked()));
        generalKeys.insert("vertical_spin_indicators", boolToStr (ui->checkBoxVSpin->isChecked()));
        generalKeys.insert("combo_as_lineedit", boolToStr (ui->checkBoxComboEdit->isChecked()));
        generalKeys.insert("combo_menu", boolToStr (ui->checkBoxComboMenu->isChecked()));
        generalKeys.insert("translucent_windows", boolToStr (ui->checkBoxTrans->isChecked()));
        generalKeys.insert("popup_blurring", boolToStr (ui->checkBoxBlurPopup->isChecked()));
        generalKeys.insert("blurring", boolToStr (ui->checkBoxBlurWindow->isChecked()));
        generalKeys.insert("small_icon_size", str.setNum (ui->spinSmall->value()));
        generalKeys.insert("large_icon_size", str.setNum (ui->spinLarge->value()));
        generalKeys.insert("button_icon_size", str.setNum (ui->spinButton->value()));
        generalKeys.insert("toolbar_icon_size", str.setNum (ui->spinToolbar->value()));
        generalKeys.insert("layout_spacing", str.setNum (ui->spinLayout->value()));
        generalKeys.insert("layout_margin", str.setNum (ui->spinLayoutMargin->value()));
        generalKeys.insert("submenu_overlap", str.setNum (ui->spinOverlap->value()));
        generalKeys.insert("spin_button_width", str.setNum (ui->spinSpinBtnWidth->value()));

        QString opaque = ui->opaqueEdit->text();
        opaque = opaque.simplified();
        opaque.remove (" ");
        generalKeys.insert("opaque", opaque);
#endif
        themeSettings.beginGroup ("Hacks");
        bool restyle = false;
        if (themeSettings.value ("normal_default_pushbutton").toBool() != ui->checkBoxNormalBtn->isChecked()
            || themeSettings.value ("iconless_pushbutton").toBool() != ui->checkBoxIconlessBtn->isChecked()
            || qMin(qMax(themeSettings.value ("tint_on_mouseover").toInt(),0),100) != ui->spinTint->value()
            || qMin(qMax(themeSettings.value ("disabled_icon_opacity").toInt(),0),100) != ui->spinOpacity->value())
        {
            restyle = true;
        }
#if QT_VERSION >= 0x050000
        QMap<QString, QString>::iterator it;
        it = hackKeys.begin();
        while (!hackKeys.isEmpty() && it != hackKeys.end())
        {
            if (!themeSettings.contains (it.key()))
            {
                hackKeysMissing.insert (it.key(), hackKeys.value (it.key()));
                it = hackKeys.erase (it);
            }
            else ++it;
        }
#else
        themeSettings.setValue ("transparent_dolphin_view", ui->checkBoxDolphin->isChecked());
        themeSettings.setValue ("transparent_pcmanfm_sidepane", ui->checkBoxPcmanfmSide->isChecked());
        themeSettings.setValue ("transparent_pcmanfm_view", ui->checkBoxPcmanfmView->isChecked());
        themeSettings.setValue ("blur_translucent", ui->checkBoxBlurTranslucent->isChecked());
        themeSettings.setValue ("transparent_ktitle_label", ui->checkBoxKtitle->isChecked());
        themeSettings.setValue ("transparent_menutitle", ui->checkBoxMenuTitle->isChecked());
        themeSettings.setValue ("kcapacitybar_as_progressbar", ui->checkBoxKCapacity->isChecked());
        themeSettings.setValue ("respect_darkness", ui->checkBoxDark->isChecked());
        themeSettings.setValue ("force_size_grip", ui->checkBoxGrip->isChecked());
        themeSettings.setValue ("normal_default_pushbutton", ui->checkBoxNormalBtn->isChecked());
        themeSettings.setValue ("iconless_pushbutton", ui->checkBoxIconlessBtn->isChecked());
        themeSettings.setValue ("iconless_menu", ui->checkBoxIconlessMenu->isChecked());
        themeSettings.setValue ("single_top_toolbar", ui->checkBoxToolbar->isChecked());
        themeSettings.setValue ("no_selection_tint", ui->checkBoxTint->isChecked());
        themeSettings.setValue ("tint_on_mouseover", ui->spinTint->value());
        themeSettings.setValue ("disabled_icon_opacity", ui->spinOpacity->value());
        themeSettings.setValue ("lxqtmainmenu_iconsize", ui->spinLxqtMenu->value());
#endif
        themeSettings.endGroup();

        themeSettings.beginGroup ("General");
        if (themeSettings.value ("composite").toBool() == ui->checkBoxNoComposite->isChecked()
            || themeSettings.value ("translucent_windows").toBool() != ui->checkBoxTrans->isChecked()
            || toDrag(themeSettings.value ("x11drag").toString()) != ui->comboX11Drag->currentIndex()
            || themeSettings.value ("inline_spin_indicators").toBool() != ui->checkBoxInlineSpin->isChecked()
            || themeSettings.value ("vertical_spin_indicators").toBool() != ui->checkBoxVSpin->isChecked()
            || themeSettings.value ("combo_menu").toBool() != ui->checkBoxComboMenu->isChecked()
            || themeSettings.value ("animate_states").toBool() != ui->checkBoxAnimation->isChecked()
            || themeSettings.value ("left_tabs").toBool() != ui->checkBoxleftTab->isChecked()
            || themeSettings.value ("joined_inactive_tabs").toBool() != ui->checkBoxJoinTab->isChecked()
            || themeSettings.value ("attach_active_tab").toBool() != ui->checkBoxAttachTab->isChecked()
            || themeSettings.value ("scroll_arrows").toBool() == ui->checkBoxNoScrollArrow->isChecked()
            || themeSettings.value ("scrollbar_in_view").toBool() != ui->checkBoxScrollIn->isChecked()
            || themeSettings.value ("groupbox_top_label").toBool() != ui->checkBoxGroupLabel->isChecked()
            || themeSettings.value ("button_contents_shift").toBool() != ui->checkBoxButtonShift->isChecked()
            || qMin(qMax(themeSettings.value ("button_icon_size").toInt(),16),64) != ui->spinButton->value()
            || qMin(qMax(themeSettings.value ("layout_spacing").toInt(),2),16) != ui->spinLayout->value()
            || qMin(qMax(themeSettings.value ("layout_margin").toInt(),2),16) != ui->spinLayoutMargin->value()
            || qMin(qMax(themeSettings.value ("spin_button_width").toInt(),16),32) != ui->spinSpinBtnWidth->value())
        {
            restyle = true;
        }
#if QT_VERSION >= 0x050000
        it = generalKeys.begin();
        while (!generalKeys.isEmpty() && it != generalKeys.end())
        {
            if (!themeSettings.contains (it.key()))
            {
                generalKeysMissing.insert (it.key(), generalKeys.value (it.key()));
                it = generalKeys.erase (it);
            }
            else ++it;
        }
#else
        themeSettings.setValue ("composite", !ui->checkBoxNoComposite->isChecked());
        themeSettings.setValue ("animate_states", ui->checkBoxAnimation->isChecked());
        themeSettings.setValue ("left_tabs", ui->checkBoxleftTab->isChecked());
        themeSettings.setValue ("joined_inactive_tabs", ui->checkBoxJoinTab->isChecked());
        themeSettings.setValue ("attach_active_tab", ui->checkBoxAttachTab->isChecked());
        themeSettings.setValue ("scroll_arrows", !ui->checkBoxNoScrollArrow->isChecked());
        themeSettings.setValue ("scrollbar_in_view", ui->checkBoxScrollIn->isChecked());
        themeSettings.setValue ("scrollable_menu", ui->checkBoxScrollableMenu->isChecked());
        themeSettings.setValue ("tree_branch_line", ui->checkBoxTree->isChecked());
        themeSettings.setValue ("groupbox_top_label", ui->checkBoxGroupLabel->isChecked());
        themeSettings.setValue ("fill_rubberband", ui->checkBoxRubber->isChecked());
        themeSettings.setValue ("menubar_mouse_tracking", ui->checkBoxMenubar->isChecked());
        themeSettings.setValue ("merge_menubar_with_toolbar", ui->checkBoxMenuToolbar->isChecked());
        themeSettings.setValue ("button_contents_shift", ui->checkBoxButtonShift->isChecked());
        themeSettings.setValue ("alt_mnemonic", ui->checkBoxAlt->isChecked());
        themeSettings.setValue ("submenu_delay", ui->spinSubmenuDelay->value());
        themeSettings.setValue ("toolbutton_style", ui->comboToolButton->currentIndex());
        themeSettings.setValue ("x11drag", toStr((Drag)ui->comboX11Drag->currentIndex()));
        themeSettings.setValue ("respect_DE", ui->checkBoxDE->isChecked());
        themeSettings.setValue ("double_click", ui->checkBoxClick->isChecked());
        themeSettings.setValue ("inline_spin_indicators", ui->checkBoxInlineSpin->isChecked());
        themeSettings.setValue ("vertical_spin_indicators", ui->checkBoxVSpin->isChecked());
        themeSettings.setValue ("combo_as_lineedit", ui->checkBoxComboEdit->isChecked());
        themeSettings.setValue ("combo_menu", ui->checkBoxComboMenu->isChecked());
        themeSettings.setValue ("translucent_windows", ui->checkBoxTrans->isChecked());
        themeSettings.setValue ("blurring", ui->checkBoxBlurWindow->isChecked());
        themeSettings.setValue ("popup_blurring", ui->checkBoxBlurPopup->isChecked());
        themeSettings.setValue ("small_icon_size", ui->spinSmall->value());
        themeSettings.setValue ("large_icon_size", ui->spinLarge->value());
        themeSettings.setValue ("button_icon_size", ui->spinButton->value());
        themeSettings.setValue ("toolbar_icon_size", ui->spinToolbar->value());
        themeSettings.setValue ("layout_spacing", ui->spinLayout->value());
        themeSettings.setValue ("layout_margin", ui->spinLayoutMargin->value());
        themeSettings.setValue ("submenu_overlap", ui->spinOverlap->value());
        themeSettings.setValue ("spin_button_width", ui->spinSpinBtnWidth->value());
        QString opaque = ui->opaqueEdit->text();
        opaque = opaque.simplified();
        opaque.remove (" ");
        QStringList opaqueList = opaque.split (",", QString::SkipEmptyParts);
        themeSettings.setValue ("opaque", opaqueList);
#endif
        themeSettings.endGroup();
#if QT_VERSION >= 0x050000
        QFile file (themeConfig);
        if (!file.open (QIODevice::ReadOnly | QIODevice::Text))
            return;
        QStringList lines;
        QTextStream in (&file);
        while (!in.atEnd())
        {
            bool found = false;
            QString line = in.readLine();
            if (!hackKeys.isEmpty())
            {
                for (it = hackKeys.begin(); it != hackKeys.end(); ++it)
                {   /* one "\\b" is enough because if "keyA" is in the file, "key" isn't in hackKeys */
                    if (line.contains (QRegExp ("^\\s*\\b" + it.key() + "(?=\\s*\\=)")))
                    {
                        line = QString ("%1=%2").arg (it.key()).arg (hackKeys.value (it.key()));
                        hackKeys.remove (it.key());
                        found = true;
                        break;
                    }
                }
            }
            if (!found && !generalKeys.isEmpty())
            {
                for (it = generalKeys.begin(); it != generalKeys.end(); ++it)
                {
                    if (line.contains (QRegExp ("^\\s*\\b" + it.key() + "(?=\\s*\\=)")))
                    {
                        line = QString ("%1=%2").arg (it.key()).arg (generalKeys.value (it.key()));
                        generalKeys.remove (it.key());
                        break;
                    }
                }
            }
            lines.append (line);
        }
        file.close();

        int i, j;
        if (!hackKeysMissing.isEmpty())
        {
            for (i = 0; i < lines.count(); ++i)
            {
                if (lines.at (i).contains (QRegExp ("^\\s*\\[\\s*\\bHacks\\b\\s*\\]")))
                    break;
            }
            if (i == lines.count())
            {
                lines << "" << QString ("[Hacks]");
                ++i;
            }
            for (j = i+1; j < lines.count(); ++j)
            {
                if (lines.at (j).contains (QRegExp("^\\s*\\[")))
                    break;
            }
            while (j-1 >= 0 && lines.at (j-1).isEmpty()) --j;
            for (it = hackKeysMissing.begin(); it != hackKeysMissing.end(); ++it)
            {
                lines.insert (j, QString ("%1=%2").arg (it.key()).arg (hackKeysMissing.value (it.key())));
                ++j;
            }
        }
        if (!generalKeysMissing.isEmpty())
        {
            for (i = 0; i < lines.count(); ++i)
            {
                if (lines.at (i).contains (QRegExp ("^\\s*\\[\\s*%\\bGeneral\\b\\s*\\]")))
                    break;
            }
            if (i == lines.count())
            {
                lines << "" << QString ("[%General]");
                ++i;
            }
            for (j = i+1; j < lines.count(); ++j)
            {
                if (lines.at (j).contains (QRegExp("^\\s*\\[")))
                    break;
            }
            while (j-1 >= 0 && lines.at (j-1).isEmpty()) --j;
            for (it = generalKeysMissing.begin(); it != generalKeysMissing.end(); ++it)
            {
                lines.insert (j, QString ("%1=%2").arg (it.key()).arg (generalKeysMissing.value (it.key())));
                ++j;
            }
        }

        if (!lines.isEmpty())
        {
            if (!file.open (QIODevice::WriteOnly | QIODevice::Text))
                return;
            QTextStream out (&file);
            for (int i = 0; i < lines.count(); ++i)
                out << lines.at(i) << "\n";
            file.close();
        }
#endif
        ui->statusBar->showMessage (tr ("Configuration saved."), 10000);
        QString theme;
        if (kvconfigTheme_ == "Default#")
            theme = "Kvantum (modified)";
        else if (kvconfigTheme_.endsWith ("#"))
            theme = QString ("%1 (modified)").arg (kvconfigTheme_.left (kvconfigTheme_.length() - 1));
        else
            theme = kvconfigTheme_;
        QLabel *statusLabel = ui->statusBar->findChild<QLabel *>();
        statusLabel->setText (tr ("<b>Active theme:</b> %1").arg (theme));

        if (wasRootTheme && kvconfigTheme_.endsWith ("#"))
        {
            ui->restoreButton->show();
            ui->configLabel->setText (tr ("These are the settings that can be safely changed.<br>For the others, edit this file:<br><i>~/.config/Kvantum/%1/<b>%1.kvconfig</b></i>").arg (kvconfigTheme_));
            showAnimated (ui->configLabel, 1000);
        }

        QCoreApplication::processEvents();
        if (restyle)
            restyleWindow();
        if (process_->state() == QProcess::Running)
            preview();
    }

    updateThemeList();
    if (wasRootTheme && kvconfigTheme_.endsWith ("#"))
        writeOrigAppLists();
}
/*************************/
void KvantumManager::writeAppLists()
{
    /* first update the app themes list... */
    QString appTheme = ui->appCombo->currentText();
    appTheme = appTheme.split (" ").first();
    if (appTheme == "Kvantum")
        appTheme = "Default";
    QString editTxt = ui->appsEdit->text();
    if (!editTxt.isEmpty())
    {
        if (!appTheme.isEmpty())
        {
            editTxt = editTxt.simplified();
            editTxt.remove (" ");
            QStringList appList = editTxt.split (",", QString::SkipEmptyParts);
            appList.removeDuplicates();
            appThemes_.insert (appTheme, appList);
        }
    }
    else
        appThemes_.remove (appTheme);
    origAppThemes_ = appThemes_;
    /* ... then write it to the kvconfig file */
    writeOrigAppLists();

    ui->removeAppButton->setDisabled (appThemes_.isEmpty());
}
/*************************/
void KvantumManager::writeOrigAppLists()
{
    QString configFile = QString ("%1/Kvantum/kvantum.kvconfig").arg (xdg_config_home);
    QSettings settings (configFile, QSettings::NativeFormat);
    if (!settings.isWritable())
    {
        notWritable (configFile);
        return;
    }
    settings.remove ("Applications");
    if (!origAppThemes_.isEmpty())
    {
        settings.beginGroup ("Applications");
        QHashIterator<QString, QStringList> i (origAppThemes_);
        QString appTheme;
        while (i.hasNext())
        {
            i.next();
            appTheme = i.key();
            /* add # */
            int indx = ui->comboBox->findText (appTheme + " (modified)");
            if (indx > -1)
                appTheme += "#";
            settings.setValue (appTheme, i.value());
        }
        settings.endGroup();
    }
}
/*************************/
void KvantumManager::removeAppList()
{
    appThemes_.clear();
    ui->removeAppButton->setEnabled (false);
    ui->appsEdit->setText("");
}
/*************************/
void KvantumManager::restoreDefault()
{ 
    /* The restore button is shown only when kvconfigTheme_ ends with "#" (-> tabChanged())
       but we're wise and so, cautious ;) */
    if (!kvconfigTheme_.endsWith ("#")) return;

    QMessageBox msgBox (QMessageBox::Question,
                        tr ("Confirmation"),
                        tr ("<center><b>Do you want to revert to the default (root) settings of this theme?</b></center>"),
                        QMessageBox::Yes | QMessageBox::No,
                        this);
    msgBox.setInformativeText (tr ("<center><i>You will lose the changes you may have made.</i></center>"));
    msgBox.setDefaultButton (QMessageBox::No);
    switch (msgBox.exec()) {
        case QMessageBox::No: return;
        case QMessageBox::Yes:
        default: break;
    }

    QFile::remove (QString ("%1/Kvantum/%2/%2.kvconfig").arg (xdg_config_home).arg (kvconfigTheme_));
    QString _kvconfigTheme_ (kvconfigTheme_);
    if (kvconfigTheme_ == "Default#")
    {
        if (!copyRootTheme (QString(), kvconfigTheme_))
            return;
    }
    else
    {
        _kvconfigTheme_.remove (QString("#"));
        QString rootDir = QString (DATADIR) + QString ("/Kvantum/%1").arg (_kvconfigTheme_);
        if (!isThemeDir (rootDir))
            rootDir = QString (DATADIR) + QString ("/themes/%1/Kvantum").arg (_kvconfigTheme_);
        QString _themeConfig = QString ("%1/%2.kvconfig").arg (rootDir).arg (_kvconfigTheme_);
        if (QFile::exists (_themeConfig))
        {
            if (!copyRootTheme (_kvconfigTheme_, kvconfigTheme_))
                return;
        }
        else // root theme is just an SVG image
        {
            if (!copyRootTheme (QString(), kvconfigTheme_))
                return;
        }
    }

    /* correct buttons and label */
    tabChanged (2);

    ui->statusBar->showMessage (tr ("Restored the rool default settings of %1")
                                   .arg (kvconfigTheme_ == "Default#" ? "the default theme" : _kvconfigTheme_),
                                10000);

    QCoreApplication::processEvents();
    restyleWindow();
    if (process_->state() == QProcess::Running)
        preview();

    updateThemeList (false);
}
/*************************/
void KvantumManager::notCompisited (bool checked)
{
    ui->checkBoxBlurPopup->setEnabled (!checked && !ui->checkBoxBlurWindow->isChecked());
    ui->checkBoxTrans->setEnabled (!checked);
    isTranslucent (!checked && ui->checkBoxTrans->isChecked());
    if (checked)
    {
        ui->checkBoxTrans->setChecked (false);
        ui->checkBoxBlurPopup->setChecked (false);
    }
}
/*************************/
void KvantumManager::isTranslucent (bool checked)
{
    ui->opaqueLabel->setEnabled (checked);
    ui->opaqueEdit->setEnabled (checked);
    ui->checkBoxBlurWindow->setEnabled (checked);
    if (!checked)
    {
        ui->checkBoxBlurWindow->setChecked (false);
        if (!ui->checkBoxNoComposite->isChecked())
          ui->checkBoxBlurPopup->setEnabled (true);
    }
}
/*************************/
void KvantumManager::popupBlurring (bool checked)
{
    if (checked)
      ui->checkBoxBlurPopup->setChecked (true);
    ui->checkBoxBlurPopup->setEnabled (!checked);
}
/*************************/
void KvantumManager::respectDE (bool checked)
{
    if (desktop_ == QByteArray("kde"))
    {
        ui->labelSmall->setEnabled (!checked);
        ui->spinSmall->setEnabled (!checked);
        ui->labelLarge->setEnabled (!checked);
        ui->spinLarge->setEnabled (!checked);
        ui->checkBoxClick->setEnabled (!checked);
    }
    else
    {
        QSet<QByteArray> gtkDesktops=QSet<QByteArray>() << "gnome" << "unity" << "pantheon";
        if (gtkDesktops.contains(desktop_))
        {
            ui->labelX11Drag->setEnabled (!checked);
            ui->comboX11Drag->setEnabled (!checked);
            ui->checkBoxIconlessBtn->setEnabled (!checked);
            ui->checkBoxIconlessMenu->setEnabled (!checked);
            ui->checkBoxNoComposite->setEnabled (!checked);
            ui->checkBoxBlurPopup->setEnabled (!ui->checkBoxNoComposite->isChecked()
                                               && !ui->checkBoxBlurWindow->isChecked()
                                               && !checked);
            ui->checkBoxTrans->setEnabled (!ui->checkBoxNoComposite->isChecked() && !checked);
            ui->opaqueLabel->setEnabled (!ui->checkBoxNoComposite->isChecked()
                                         && ui->checkBoxTrans->isChecked()
                                         && !checked);
            ui->opaqueEdit->setEnabled (!ui->checkBoxNoComposite->isChecked()
                                        && ui->checkBoxTrans->isChecked()
                                        && !checked);
            ui->checkBoxBlurWindow->setEnabled (!ui->checkBoxNoComposite->isChecked()
                                                && ui->checkBoxTrans->isChecked()
                                                && !checked);
        }
        else ui->checkBoxDE->setEnabled (false);
    }
}
/*************************/
void KvantumManager::showWhatsThis()
{
    QWhatsThis::enterWhatsThisMode();
}
/*************************/
void KvantumManager::aboutDialog()
{
    QString qt ("Qt5");
#if QT_VERSION < 0x050000
    qt = "Qt4";
#endif
    QMessageBox::about (this, tr ("About Kvantum Manager"),
                        tr ("<center><b><big>Kvantum Manager 0.10.3</big></b><br><br>"\
                            "A %1 tool for intsalling, selecting<br>"\
                            "and configuring <a href='https://github.com/tsujan/Kvantum'>Kvantum</a> themes<br><br>"\
                            "Author: <a href='mailto:tsujan2000@gmail.com?Subject=My%20Subject'>Pedram Pourang (aka. Tsu Jan)</a> </center><br>")
                           .arg (qt));
}

}
