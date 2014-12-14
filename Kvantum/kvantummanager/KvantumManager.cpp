#include "KvantumManager.h"
#include <QFileDialog>
#include <QSettings>
#include <QMessageBox>
#include <QStyleFactory>
#if QT_VERSION >= 0x050000
#include <QFileDevice>
#endif
//#include <QDebug>

KvantumManager::KvantumManager (QWidget *parent) : QMainWindow (parent), ui (new Ui::KvantumManager)
{
    ui->setupUi (this);

    setWindowTitle ("Kvantum Manager");
    
    lastPath = QDir::home().path();
    process = new QProcess (this);

    char * _xdg_config_home = getenv ("XDG_CONFIG_HOME");
    if (!_xdg_config_home)
        xdg_config_home = QString ("%1/.config").arg (QDir::homePath());
    else
        xdg_config_home = QString (_xdg_config_home);

    ui->comboToolButton->insertItems (0, QStringList() << "Follow Style"
                                                       << "Icon Only"
                                                       << "Text Only"
                                                       << "Text Beside Icon"
                                                       << "Text Under Icon");

    QLabel *statusLabel = new QLabel();
    statusLabel->setTextInteractionFlags (Qt::TextSelectableByMouse);
    ui->statusBar->addWidget (statusLabel);

    /* set kvconfigTheme */
    updateThemeList();

    effect = new QGraphicsOpacityEffect();
    animation = new QPropertyAnimation (effect, "opacity");

    setAttribute (Qt::WA_AlwaysShowToolTips);
    showAnimated (ui->installLabel, 2000);

    connect (ui->quit, SIGNAL (clicked()), this, SLOT (close()));
    connect (ui->openTheme, SIGNAL (clicked()), this, SLOT (openTheme()));
    connect (ui->installTheme, SIGNAL (clicked()), this, SLOT (installTheme()));
    connect (ui->deleteTheme, SIGNAL (clicked()), this, SLOT (deleteTheme()));
    connect (ui->useTheme, SIGNAL (clicked()), this, SLOT (useTheme()));
    connect (ui->saveButton, SIGNAL (clicked()), this, SLOT (writeConfig()));
    connect (ui->restoreButton, SIGNAL (clicked()), this, SLOT (restoreDefault()));
    connect (ui->checkBox9, SIGNAL (clicked (bool)), this, SLOT (transparency (bool)));
    connect (ui->lineEdit, SIGNAL (textChanged (const QString &)), this, SLOT (txtChanged (const QString &)));
    connect (ui->toolBox, SIGNAL (currentChanged (int)), this, SLOT (tabChanged (int)));
    connect (ui->comboBox, SIGNAL (currentIndexChanged (const QString &)), this, SLOT (selectionChanged (const QString &)));
    connect (ui->preview, SIGNAL (clicked()), this, SLOT (preview()));
    connect (ui->aboutButton, SIGNAL (clicked()), this, SLOT (aboutDialog()));
}
/*************************/
KvantumManager::~KvantumManager()
{
    delete ui;
    delete animation;
    delete effect;
}
/*************************/
void KvantumManager::closeEvent (QCloseEvent *event)
{
    process->terminate();
    process->waitForFinished();
    event->accept();
}
/*************************/
void KvantumManager::openTheme()
{
    ui->statusBar->clearMessage();
    QString filePath = QFileDialog::getExistingDirectory (this,
                                                          tr ("Open Kvantum Theme Folder..."),
                                                          lastPath,
                                                          QFileDialog::ShowDirsOnly
                                                          | QFileDialog::ReadOnly
                                                          | QFileDialog::DontUseNativeDialog);
    ui->lineEdit->setText (filePath);
    lastPath = filePath;
}
/*************************/
bool KvantumManager::isThemeDir (const QString &folder)
{
    QDir dir = QDir (folder);
    if (!dir.exists())
        return false;

    /* QSettings doesn't accept spaces in the name */
    QString s = (dir.dirName()).simplified();
    if (s.contains (" "))
        return false;

    QStringList files = dir.entryList (QDir::Files, QDir::Name);
    foreach (const QString &file, files)
    {
        if (file == QString ("%1.kvconfig").arg (dir.dirName())
            || file == QString ("%1.svg").arg (dir.dirName()))
        {
            return true;
        }
    }

    return false;
}
/*************************/
void KvantumManager::notWritable()
{
    QMessageBox msgBox (QMessageBox::Warning,
                        tr ("Kvantum"),
                        tr ("<center><b>You have no permission to write!</b></center>"\
                            "<center>Please fix that first!</center>"),
                        QMessageBox::Close,
                        this);
    msgBox.exec();
}
/*************************/
void KvantumManager::installTheme()
{
    QString theme = ui->lineEdit->text();
    if (!isThemeDir (theme))
    {
        QMessageBox msgBox (QMessageBox::Warning,
                            tr ("Kvantum"),
                            tr ("<center><b>This is not a Kvantum theme!</b></center>"\
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
            QString themeName = themeDir.dirName();
            QDir subDir_ = QDir (QString ("%1/Kvantum/%2#").arg (xdg_config_home).arg (themeName));
            if (subDir_.exists())
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
                    notWritable();
                    return;
                }
            }
            subDir_ = QDir (QString (DATADIR) + QString ("/Kvantum/") + themeName);
            if (subDir_.exists())
            {
                QMessageBox msgBox (QMessageBox::Information,
                                    tr ("Kvantum"),
                                    tr ("<center><b>This theme is also installed as root.</b></center>"\
                                        "<center>The user installation will take priority.</center>"),
                                    QMessageBox::Close,
                                    this);
                msgBox.exec();
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
            }

            ui->statusBar->showMessage (tr ("%1 installed.").arg (themeName), 10000);
        }
        else
            notWritable();
    }
}
/*************************/
static inline bool removeDir(const QString &dirName)
{
    bool res = true;
    QDir dir(dirName);
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

    if (!removeDir (QString ("%1/Kvantum/%2").arg (xdg_config_home).arg (theme)))
    {
        QMessageBox msgBox (QMessageBox::Warning,
                            tr ("Kvantum"),
                            tr ("<center><b>Not all objects could be removed!</b></center>"\
                                "<center>You might want to investigate the cause.</center>"),
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
                kvconfigTheme = "Default#";
            }
            else
            {
                settings.remove ("theme");
                kvconfigTheme = QString();
            }
            QApplication::setStyle (QStyleFactory::create ("kvantum"));
            if (process->state() == QProcess::Running)
              preview();
        }
    }
    updateThemeList();
    ui->statusBar->showMessage (tr ("%1 deleted.").arg (theme_), 10000);
}
/*************************/
void KvantumManager::showAnimated (QWidget *w, int duration)
{
    w->show();
    w->setGraphicsEffect (effect);
    animation->setDuration (duration);
    animation->setStartValue (0.0);
    animation->setEndValue (1.0);
    animation->start();
}
/*************************/
// Activates the theme and sets kvconfigTheme.
void KvantumManager::useTheme()
{
    kvconfigTheme = ui->comboBox->currentText();
    if (kvconfigTheme.isEmpty()) return;

    QString theme = kvconfigTheme;
    if (kvconfigTheme == "Kvantum (modified)")
        kvconfigTheme = "Default#";
    else if (kvconfigTheme.endsWith (" (modified)"))
         kvconfigTheme.replace (QString (" (modified)"), QString ("#"));
    else if (kvconfigTheme == "Kvantum (default)")
        kvconfigTheme = QString();

    QString configFile = QString ("%1/Kvantum/kvantum.kvconfig").arg (xdg_config_home);
    QSettings settings (configFile, QSettings::NativeFormat);
    if (!settings.isWritable()) return;

    if (kvconfigTheme.isEmpty())
        settings.remove ("theme");
    else if (settings.value ("theme").toString() != kvconfigTheme)
        settings.setValue ("theme", kvconfigTheme);

    QLabel *statusLabel = ui->statusBar->findChild<QLabel *>();
    statusLabel->setText (tr ("<b>Active theme:</b> %1").arg (theme));
    ui->statusBar->showMessage (tr ("Theme changed to %1.").arg (theme), 10000);
    showAnimated (ui->usageLabel, 1000);

    /* this is needed if the config file is created by this method */
    QCoreApplication::processEvents();
    QApplication::setStyle (QStyleFactory::create ("kvantum"));
    int extra = QApplication::style()->pixelMetric (QStyle::PM_ScrollBarExtent) * 2;
    resize (size().expandedTo (sizeHint() + QSize (extra, extra)));
    if (process->state() == QProcess::Running)
      preview();
}
/*************************/
void KvantumManager::txtChanged (const QString &txt)
{
    if (txt.isEmpty())
        ui->installTheme->setEnabled (false);
    else if (!ui->installTheme->isEnabled())
        ui->installTheme->setEnabled (true);
}
/*************************/
void KvantumManager::defaultThemeButtons()
{
    ui->restoreButton->hide();
    ui->checkBox1->setChecked (false);
    ui->konsoleCheckBox->setChecked (false);
    ui->checkBox2->setChecked (false);
    ui->checkBox3->setChecked (true);
    ui->checkBox4->setChecked (false);
    ui->checkBox5->setChecked (false);
    ui->checkBox6->setChecked (true);
    ui->checkBox7->setChecked (false);
    ui->checkBoxProgress->setChecked (false);
    ui->checkBoxMenubar->setChecked (true);
    ui->checkBoxToolbar->setChecked (false);
    ui->checkBox8->setChecked (true);
    ui->checkBox9->setChecked (false);
    ui->checkBox10->setChecked (false);
    ui->opaqueLabel->setEnabled (false);
    ui->opaqueEdit->setEnabled (false);
    ui->comboToolButton->setCurrentIndex (0);
}
/*************************/
void KvantumManager::tabChanged (int index)
{
    ui->statusBar->clearMessage();
    if (index == 0)
        showAnimated (ui->installLabel, 1000);
    if (index == 1)
    {
        updateThemeList();
        ui->usageLabel->show();
        QString comment;
        if (kvconfigTheme.isEmpty())
            ui->deleteTheme->setEnabled (false);
        else
        {
            QString themeConfig;
            QDir themeDir = QDir (xdg_config_home + QString ("/Kvantum/") + kvconfigTheme);
            if (!themeDir.exists())
            {
                ui->deleteTheme->setEnabled (false);
                themeConfig = QString (DATADIR) + QString ("/Kvantum/%1/%1.kvconfig").arg (kvconfigTheme);
            }
            else
            {
                ui->deleteTheme->setEnabled (true);
                themeConfig = QString ("%1/Kvantum/%2/%2.kvconfig").arg (xdg_config_home).arg (kvconfigTheme);
            }
            if (QFile::exists (themeConfig))
            {
              QSettings themeSettings (themeConfig, QSettings::NativeFormat);
              themeSettings.beginGroup ("General");
              comment = themeSettings.value ("comment").toString();
              themeSettings.endGroup();
            }
        }
        if (comment.isEmpty())
          comment = "Kvantum's default theme";
        ui->comboBox->setToolTip (comment);
    }
    else if (index == 2)
    {
        ui->opaqueEdit->clear();
        if (kvconfigTheme.isEmpty())
        {
            ui->configLabel->setText (tr ("These are the most important keys.<br>For the others, click <i>Save</i> and then edit this file:<br><i>~/.config/Kvantum/Default#/<b>Default#.kvconfig</b></i>"));
            showAnimated (ui->configLabel, 1000);

            defaultThemeButtons();
        }
        else
        {
            ui->configLabel->setText (tr ("These are the most important keys.<br>For the others, edit this file:<br><i>~/.config/Kvantum/%1/<b>%1.kvconfig</b></i>").arg (kvconfigTheme));
            QString themeConfig = QString ("%1/Kvantum/%2/%2.kvconfig").arg (xdg_config_home).arg (kvconfigTheme);
            QString userSvg = QString ("%1/Kvantum/%2/%2.svg").arg (xdg_config_home).arg (kvconfigTheme);
            QString rootSvg = QString (DATADIR) + QString ("/Kvantum/%1/%1.svg").arg (kvconfigTheme);
            if (!QFile::exists (themeConfig))
            {
                themeConfig = QString (DATADIR) + QString ("/Kvantum/%1/%1.kvconfig").arg (kvconfigTheme);
                if (QFile::exists (themeConfig)
                    /* a root theme with just an SVG image */
                    || (!QFile::exists (userSvg) && QFile::exists (rootSvg)))
                {
                    ui->configLabel->setText (tr ("These are the most important keys.<br>For the others, click <i>Save</i> and then edit this file:<br><i>~/.config/Kvantum/%1#/<b>%1#.kvconfig</b></i>").arg (kvconfigTheme));
                }
            }
            showAnimated (ui->configLabel, 1000);

            if (kvconfigTheme.endsWith ("#"))
                ui->restoreButton->show();
            else
                ui->restoreButton->hide();

            /* the theme may be just an SVG image,
               so add Kvantum's default config... */
            if (!QFile::exists (themeConfig))
            {
                /* ... only if it isn't a root theme */
                if (!QFile::exists (userSvg) && QFile::exists (rootSvg))
                {
                    defaultThemeButtons();
                    int extra = QApplication::style()->pixelMetric (QStyle::PM_ScrollBarExtent) * 2;
                    resize (size().expandedTo (sizeHint() + QSize (extra, extra)));
                    return;
                }
                copyDefaultTheme (QString(), kvconfigTheme);
                themeConfig = QString ("%1/Kvantum/%2/%2.kvconfig").arg (xdg_config_home).arg (kvconfigTheme);
            }

            if (QFile::exists (themeConfig)) // FIXME not needed
            {
                QSettings themeSettings (themeConfig, QSettings::NativeFormat);
                /* consult the default config file if a key doesn't exist */
                themeSettings.beginGroup ("General");
                if (themeSettings.contains ("composite"))
                    ui->checkBox4->setChecked (!themeSettings.value ("composite").toBool());
                else
                    ui->checkBox4->setChecked (true);
                if (themeSettings.contains ("left_tabs"))
                    ui->checkBox5->setChecked (themeSettings.value ("left_tabs").toBool());
                else
                    ui->checkBox5->setChecked (false);
                if (themeSettings.contains ("joined_tabs"))
                    ui->checkBox6->setChecked (themeSettings.value ("joined_tabs").toBool());
                else
                    ui->checkBox6->setChecked (true);
                if (themeSettings.contains ("attach_active_tab"))
                    ui->checkBox7->setChecked (themeSettings.value ("attach_active_tab").toBool());
                else
                    ui->checkBox7->setChecked (false);
                if (themeSettings.contains ("textless_progressbar"))
                    ui->checkBoxProgress->setChecked (themeSettings.value ("textless_progressbar").toBool());
                else
                    ui->checkBoxProgress->setChecked (false);
                if (themeSettings.contains ("menubar_mouse_tracking"))
                    ui->checkBoxMenubar->setChecked (themeSettings.value ("menubar_mouse_tracking").toBool());
                else
                    ui->checkBoxMenubar->setChecked (true);
                if (themeSettings.contains ("slim_toolbars"))
                    ui->checkBoxToolbar->setChecked (themeSettings.value ("slim_toolbars").toBool());
                else
                    ui->checkBoxToolbar->setChecked (false);
                if (themeSettings.contains ("toolbutton_style"))
                {
                    int index = themeSettings.value ("toolbutton_style").toInt();
                    if (index > 4 || index < 0) index = 0;
                    ui->comboToolButton->setCurrentIndex (index);
                }
                else
                    ui->comboToolButton->setCurrentIndex (0);
                if (themeSettings.contains ("x11drag"))
                    ui->checkBox8->setChecked (themeSettings.value ("x11drag").toBool());
                else
                    ui->checkBox8->setChecked (true);
                bool translucency = false;
                if (themeSettings.contains ("translucent_windows"))
                    translucency = themeSettings.value ("translucent_windows").toBool();
                QStringList lst = themeSettings.value ("opaque").toStringList();
                if (!lst.isEmpty())
                    ui->opaqueEdit->setText (lst.join (","));
                ui->checkBox9->setChecked (translucency);
                ui->opaqueLabel->setEnabled (translucency);
                ui->opaqueEdit->setEnabled (translucency);
                ui->checkBox10->setEnabled (translucency);
                if (themeSettings.contains ("blurring"))
                    ui->checkBox10->setChecked (themeSettings.value ("blurring").toBool());
                else
                    ui->checkBox10->setChecked (false);
                themeSettings.endGroup();

                themeSettings.beginGroup ("Hacks");
                ui->checkBox1->setChecked (themeSettings.value ("transparent_dolphin_view").toBool());
                ui->konsoleCheckBox->setChecked (themeSettings.value ("blur_konsole").toBool());
                ui->checkBox2->setChecked (themeSettings.value ("transparent_ktitle_label").toBool());
                ui->checkBox3->setChecked (themeSettings.value ("respect_darkness").toBool());
                themeSettings.endGroup();
            }
        }
    }
    int extra = QApplication::style()->pixelMetric (QStyle::PM_ScrollBarExtent);
    resize (size().expandedTo (sizeHint()
                               + ui->comboToolButton->minimumSizeHint()
                               + QSize (extra + ui->opaqueEdit->sizeHint().width(), extra)));
}
/*************************/
void KvantumManager::selectionChanged (const QString &txt)
{
    ui->statusBar->clearMessage();

    QString theme;
    if (kvconfigTheme.isEmpty())
        theme = "Kvantum (default)";
    else if (kvconfigTheme == "Default#")
        theme = "Kvantum (modified)";
    else if (kvconfigTheme.endsWith ("#"))
        theme = QString ("%1 (modified)").arg (kvconfigTheme.left (kvconfigTheme.length() - 1));
    else
        theme = kvconfigTheme;

    if (txt == theme)
        showAnimated (ui->usageLabel, 1000);
    else
        ui->usageLabel->hide();

    if (txt == "Kvantum (default)"
        || (!txt.endsWith (" (modified)")
            && !QDir (xdg_config_home + QString ("/Kvantum/") + txt).exists())) // root
    {
        ui->deleteTheme->setEnabled (false);
    }
    else if (!ui->deleteTheme->isEnabled())
        ui->deleteTheme->setEnabled (true);

    QString comment;
    QString text = txt;
    if (text == "Kvantum (modified)")
        text = "Default#";
    else if (text.endsWith (" (modified)"))
        text.replace (QString (" (modified)"), QString ("#"));
    if (text != "Kvantum (default)")
    {
        QString themeConfig = QString ("%1/Kvantum/%2/%2.kvconfig").arg (xdg_config_home).arg (text);
        if (QFile::exists (themeConfig))
        {
          QSettings themeSettings (themeConfig, QSettings::NativeFormat);
          themeSettings.beginGroup ("General");
          comment = themeSettings.value ("comment").toString();
          themeSettings.endGroup();
        }
    }
    if (comment.isEmpty())
      comment = "Kvantum's default theme";
    ui->comboBox->setToolTip (comment);
}
/*************************/
void KvantumManager::updateThemeList()
{
    ui->comboBox->clear();

    QStringList list;

    /* add all themes */
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
                        && isThemeDir (QString (DATADIR) + QString ("/Kvantum/%1").arg (folder_)))
                    {
                        list.append (QString ("%1 (modified)").arg (folder_));
                    }
                }
                else
                    list.append (folder);
            }
        }
    }
    int rootSep = 0;
    if (!list.isEmpty())
        rootSep = list.length();
    kv = QDir (QString (DATADIR) + QString ("/Kvantum"));
    if (kv.exists())
    {
        QStringList folders = kv.entryList (QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        foreach (const QString &folder, folders)
        {
            if (!folder.contains ("#")
                && isThemeDir (QString (DATADIR) + QString ("/Kvantum/%1").arg (folder)))
            {
                if (!list.contains (folder) && !list.contains (QString ("%1 (modified)").arg (folder)))
                    list.append (folder);
            }
        }
    }
    if (list.isEmpty() || list.first() != "Kvantum (modified)")
    {
        list.prepend ("Kvantum (default)");
        rootSep++;
    }
    ui->comboBox->insertItems (0, list);
    ui->comboBox->insertSeparator (1);
    if (rootSep)
        ui->comboBox->insertSeparator (rootSep + 1);

    /* select the active theme and set kvconfigTheme */
    QString theme;
    QString configFile = QString ("%1/Kvantum/kvantum.kvconfig").arg (xdg_config_home);
    bool noConfig = false;
    if (QFile::exists (configFile))
    {
        QSettings settings (configFile, QSettings::NativeFormat);
        if (settings.contains ("theme"))
        {
            kvconfigTheme = settings.value ("theme").toString();
            if (kvconfigTheme.isEmpty())
                theme = "Kvantum (default)";
            else if (kvconfigTheme == "Default#")
                theme = "Kvantum (modified)";
            else if (kvconfigTheme.endsWith ("#"))
                theme = QString ("%1 (modified)").arg (kvconfigTheme.left (kvconfigTheme.length() - 1));
            else
                theme = kvconfigTheme;
            int index = ui->comboBox->findText (theme);
            if (index > -1)
                ui->comboBox->setCurrentIndex (index);
            else // remove from settings if its folder is deleted
            {
                if (list.contains ("Kvantum (modified)"))
                {
                    settings.setValue ("theme", "Default#");
                    kvconfigTheme = "Default#";
                    theme = "Kvantum (modified)";
                }
                else
                {
                    settings.remove ("theme");
                    kvconfigTheme = QString();
                    theme = "Kvantum (default)";
                }
            }
        }
        else noConfig = true;
    }
    else noConfig = true;

    if (noConfig)
    {
        kvconfigTheme = QString();
        theme = "Kvantum (default)";
        /* remove Default# because there's no config */
        QString theCopy = QString ("%1/Kvantum/Default#/Default#.kvconfig").arg (xdg_config_home);
        QFile::remove (theCopy);
    }

    QLabel *statusLabel = ui->statusBar->findChild<QLabel *>();
    statusLabel->setText (tr ("<b>Active theme:</b> %1").arg (theme));
}
/*************************/
void KvantumManager::preview()
{
    QString binDir = QApplication::applicationDirPath();
    QString previewExe = binDir + "/kvantumpreview";
    process->terminate();
    process->waitForFinished();
    process->start (previewExe);
}
/*************************/
void KvantumManager::copyDefaultTheme (QString source, QString target)
{
    QDir cf = QDir (xdg_config_home);
    cf.mkdir ("Kvantum");
    QDir kv = QDir (xdg_config_home + QString ("/Kvantum"));
    if (kv.exists())
    {
        kv.mkdir (target);
        QString theCopy = QString ("%1/Kvantum/%2/%2.kvconfig").arg (xdg_config_home).arg (target);
        if (!QFile::exists (theCopy))
        {
            QString themeConfig = ":default.kvconfig";
            if (!source.isEmpty())
                themeConfig = QString (DATADIR) + QString ("/Kvantum/%1/%1.kvconfig").arg (source);
            if (QFile::copy (themeConfig, theCopy))
            {
#if QT_VERSION < 0x050000
                QFile::setPermissions (theCopy, QFile::permissions (theCopy) | QFile::WriteOwner);
#else
                QFile::setPermissions (theCopy, QFile::permissions (theCopy) | QFileDevice::WriteOwner);
#endif
                ui->statusBar->showMessage (tr ("A copy of the default config is created."), 10000);
            }
            else
                notWritable();
        }
        else
        {
            ui->statusBar->clearMessage();
            ui->statusBar->showMessage (tr ("A copy was already created."), 10000);
        }
    }
    else
        notWritable();
}
/*************************/
void KvantumManager::writeConfig()
{
    bool wasDefault = false;
    if (kvconfigTheme.isEmpty()) // default theme
    {
        wasDefault = true;
        QString theCopy = QString ("%1/Kvantum/Default#/Default#.kvconfig").arg (xdg_config_home);
        QFile::remove (theCopy);
        kvconfigTheme = "Default#";
        copyDefaultTheme (QString(), kvconfigTheme);
    }

    QString themeConfig = QString ("%1/Kvantum/%2/%2.kvconfig").arg (xdg_config_home).arg (kvconfigTheme);
    if (!QFile::exists (themeConfig)) // root theme
    {
        wasDefault = true;
        QString themeConfig_ = QString (DATADIR) + QString ("/Kvantum/%1/%1.kvconfig").arg (kvconfigTheme);
        if (QFile::exists (themeConfig_))
        {
            QString theCopy = QString ("%1/Kvantum/%2#/%2#.kvconfig").arg (xdg_config_home).arg (kvconfigTheme);
            QFile::remove (theCopy);
            copyDefaultTheme (kvconfigTheme, kvconfigTheme + "#");
            kvconfigTheme = kvconfigTheme + "#";
        }
        else // root theme is just an SVG image
        {
            kvconfigTheme = kvconfigTheme + "#";
            copyDefaultTheme (QString(), kvconfigTheme);
        }
    }

    QString configFile = QString ("%1/Kvantum/kvantum.kvconfig").arg (xdg_config_home);
    QSettings settings (configFile, QSettings::NativeFormat);
    if (!settings.isWritable()) return;
    settings.setValue ("theme", kvconfigTheme);

    themeConfig = QString ("%1/Kvantum/%2/%2.kvconfig").arg (xdg_config_home).arg (kvconfigTheme);
    if (QFile::exists (themeConfig)) // user theme
    {
        QSettings themeSettings (themeConfig, QSettings::NativeFormat);
        if (!themeSettings.isWritable())
        {
            notWritable();
            return;
        }
        themeSettings.beginGroup ("Hacks");
        themeSettings.setValue ("transparent_dolphin_view", ui->checkBox1->isChecked());
        themeSettings.setValue ("blur_konsole", ui->konsoleCheckBox->isChecked());
        themeSettings.setValue ("transparent_ktitle_label", ui->checkBox2->isChecked());
        themeSettings.setValue ("respect_darkness", ui->checkBox3->isChecked());
        themeSettings.endGroup();

        themeSettings.beginGroup ("General");
        bool translucenceChanged = false;
        if (themeSettings.value ("composite").toBool() == ui->checkBox4->isChecked()
            || themeSettings.value ("translucent_windows").toBool() != ui->checkBox9->isChecked())
        {
            translucenceChanged = true;
        }
        themeSettings.setValue ("composite", !ui->checkBox4->isChecked());
        themeSettings.setValue ("left_tabs", ui->checkBox5->isChecked());
        themeSettings.setValue ("joined_tabs", ui->checkBox6->isChecked());
        themeSettings.setValue ("attach_active_tab", ui->checkBox7->isChecked());
        themeSettings.setValue ("textless_progressbar", ui->checkBoxProgress->isChecked());
        themeSettings.setValue ("menubar_mouse_tracking", ui->checkBoxMenubar->isChecked());
        themeSettings.setValue ("slim_toolbars", ui->checkBoxToolbar->isChecked());
        themeSettings.setValue ("toolbutton_style", ui->comboToolButton->currentIndex());
        themeSettings.setValue ("x11drag", ui->checkBox8->isChecked());
        themeSettings.setValue ("translucent_windows", ui->checkBox9->isChecked());
        themeSettings.setValue ("blurring", ui->checkBox10->isChecked());
        QString opaque = ui->opaqueEdit->text();
        opaque = opaque.simplified();
        opaque.remove (" ");
        QStringList opaqueList = opaque.split (",");
        themeSettings.setValue ("opaque", opaqueList);
        themeSettings.endGroup();

        ui->statusBar->showMessage (tr ("Configuration saved."), 10000);
        QString theme;
        if (kvconfigTheme == "Default#")
            theme = "Kvantum (modified)";
        else if (kvconfigTheme.endsWith ("#"))
            theme = QString ("%1 (modified)").arg (kvconfigTheme.left (kvconfigTheme.length() - 1));
        else
            theme = kvconfigTheme;
        QLabel *statusLabel = ui->statusBar->findChild<QLabel *>();
        statusLabel->setText (tr ("<b>Active theme:</b> %1").arg (theme));

        if (wasDefault && kvconfigTheme.endsWith ("#"))
        {
            ui->restoreButton->show();
            ui->configLabel->setText (tr ("These are the most important keys.<br>For the others, edit this file:<br><i>~/.config/Kvantum/%1/<b>%1.kvconfig</b></i>").arg (kvconfigTheme));
            showAnimated (ui->configLabel, 1000);
        }

        if (translucenceChanged)
            QApplication::setStyle (QStyleFactory::create ("kvantum"));
        int extra = QApplication::style()->pixelMetric (QStyle::PM_ScrollBarExtent) * 2;
        resize (size().expandedTo (sizeHint() + QSize (extra, extra)));
        if (process->state() == QProcess::Running)
          preview();
    }
}
/*************************/
void KvantumManager::restoreDefault()
{
    QString theCopy = QString ("%1/Kvantum/%2/%2.kvconfig").arg (xdg_config_home).arg (kvconfigTheme);
    QFile::remove (theCopy);
    QString configFile = QString ("%1/Kvantum/kvantum.kvconfig").arg (xdg_config_home);
    QSettings settings (configFile, QSettings::NativeFormat);
    if (!settings.isWritable()) return;
    /* reset kvconfigTheme and set the theme */
    if (kvconfigTheme == "Default#")
    {
        kvconfigTheme = QString();
        settings.remove ("theme");
    }
    else
    {
        kvconfigTheme.replace("#", "");
        settings.setValue ("theme", kvconfigTheme);
    }

    /* correct buttons and label */
    tabChanged (2);

    showAnimated (ui->configLabel, 1000);
    QLabel *statusLabel = ui->statusBar->findChild<QLabel *>();
    statusLabel->setText (tr ("<b>Active theme:</b> %1")
                             .arg (kvconfigTheme.isEmpty() ? "Kvantum (default)" : kvconfigTheme));
    ui->statusBar->showMessage (tr ("Removed the (modified) copy of %1.")
                                   .arg (kvconfigTheme.isEmpty() ? "the default theme" : kvconfigTheme),
                                10000);

    QApplication::setStyle (QStyleFactory::create ("kvantum"));
    if (process->state() == QProcess::Running)
      preview();
}
/*************************/
void KvantumManager::transparency (bool checked)
{
    ui->opaqueLabel->setEnabled (checked);
    ui->opaqueEdit->setEnabled (checked);
    ui->checkBox10->setEnabled (checked);
}
/*************************/
void KvantumManager::aboutDialog()
{
    QMessageBox::about (this, tr ("About Kvantum Manager"),
                        tr ("<center><b><big>Kvantum Manager 0.8.8</big></b></center><br>"\
                        "<center>A tool for intsalling, selecting and</center>\n"\
                        "<center>configuring <a href='https://github.com/tsujan/Kvantum'>Kvantum</a> themes</center><br>"\
                        "<center>Author: <a href='mailto:tsujan2000@gmail.com?Subject=My%20Subject'>Pedram Pourang (aka. Tsu Jan)</a></center>"));
}
