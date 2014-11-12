#include "KvantumManager.h"
#include <QFileDialog>
#include <QSettings>
#include <QMessageBox>
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

    connect (ui->quit, SIGNAL (clicked()), this, SLOT (close()));
    connect (ui->openTheme, SIGNAL (clicked()), this, SLOT (openTheme()));
    connect (ui->installTheme, SIGNAL (clicked()), this, SLOT (installTheme()));
    connect (ui->copyButton, SIGNAL (clicked()), this, SLOT (copyDefaultTheme()));
    connect (ui->useTheme, SIGNAL (clicked()), this, SLOT (useTheme()));
    connect (ui->lineEdit, SIGNAL (textChanged (const QString &)), this, SLOT (txtChanged (const QString &)));
    connect (ui->toolBox, SIGNAL (currentChanged (int)), this, SLOT (tabChanged (int)));
    connect (ui->comboBox, SIGNAL (currentIndexChanged (int)), this, SLOT (selectionChanged (int)));
    connect (ui->preview, SIGNAL (clicked()), this, SLOT (preview()));

    connect (ui->checkBox1, SIGNAL (clicked (bool)), this, SLOT (wrtieConfig (bool)));
    connect (ui->checkBox2, SIGNAL (clicked (bool)), this, SLOT (wrtieConfig (bool)));
    connect (ui->checkBox3, SIGNAL (clicked (bool)), this, SLOT (wrtieConfig (bool)));
    connect (ui->checkBox4, SIGNAL (clicked (bool)), this, SLOT (wrtieConfig (bool)));
    connect (ui->checkBox5, SIGNAL (clicked (bool)), this, SLOT (wrtieConfig (bool)));
    connect (ui->checkBox6, SIGNAL (clicked (bool)), this, SLOT (wrtieConfig (bool)));
    connect (ui->checkBox7, SIGNAL (clicked (bool)), this, SLOT (wrtieConfig (bool)));
    connect (ui->checkBox8, SIGNAL (clicked (bool)), this, SLOT (wrtieConfig (bool)));

    updateThemeList();

    setAttribute (Qt::WA_AlwaysShowToolTips);
}
/*************************/
KvantumManager::~KvantumManager()
{
    delete ui;
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
    QString s = folder.simplified();
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
    else
    {
        QString homeDir = QDir::homePath();
        QDir cf = QDir (xdg_config_home);
        cf.mkdir ("Kvantum");
        QDir kv = QDir (QString ("%1/Kvantum").arg (xdg_config_home));
        /* if the Kvantum themes directory is created or already exists... */
        if (kv.exists())
        {
            QDir themeDir = QDir (theme);
            QString themeName = themeDir.dirName();
            /* ... and contains the same theme... */
            if (!kv.mkdir (themeName))
            {
                QDir subDir = QDir (QString ("%1/Kvantum/%2").arg (xdg_config_home).arg (themeName));
                if (subDir == themeDir)
                {
                    QMessageBox msgBox (QMessageBox::Warning,
                                        tr ("Kvantum"),
                                        tr ("<center>You have selected an installed theme folder.</center>"\
                                            "<center>Please choose another directory!</center>"),
                                        QMessageBox::Close,
                                        this);
                    msgBox.exec();
                    return;
                }
                if (subDir.exists() && QFileInfo (subDir.absolutePath()).isWritable())
                {
                    QMessageBox msgBox (QMessageBox::Warning,
                                        tr ("Kvantum"),
                                        tr ("<center>The theme already exists.</center>"\
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

            ui->statusBar->showMessage (tr ("%1 installed.").arg (themeName), 20000);
        }
        else
            notWritable();
    }
}
/*************************/
void KvantumManager::useTheme()
{
    QString theme = ui->comboBox->currentText();
    if (theme.isEmpty()) return;

    QString configFile = QString ("%1/Kvantum/kvantum.kvconfig").arg (xdg_config_home);
    QSettings settings (configFile, QSettings::NativeFormat);
    if (!settings.isWritable()) return;

    if (theme == "Kvantum's default theme")
        settings.remove ("theme");
    else if (settings.value ("theme").toString() != theme)
        settings.setValue ("theme", theme);

    ui->statusBar->showMessage (tr ("Theme changed to %1.").arg (theme), 20000);
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
void KvantumManager::tabChanged (int index)
{
    ui->statusBar->clearMessage();
    if (index == 1)
        updateThemeList();
    else if (index == 2)
    {
        QString theme ("defaultTheme");
        QString configFile = QString("%1/Kvantum/kvantum.kvconfig").arg (xdg_config_home);
        if (QFile::exists (configFile))
        {
            QSettings settings (configFile, QSettings::NativeFormat);
            if (settings.contains ("theme"))
                theme = settings.value ("theme").toString();
        }

        if (theme == "defaultTheme")
        {
            if (QFile::exists (QString ("%1/Kvantum/DefaultCopy/DefaultCopy.kvconfig").arg (xdg_config_home)))
            {
                ui->configLabel->setText (tr ("You could not configure the default theme directly.<br>A configurable copy of it is created with the name <i><b>DefaultCopy</b></i>.<br>Please first select <i><b>DefaultCopy</b></i> on the previous page!"));
                ui->copyButton->hide();
            }
            else
            {
                ui->configLabel->setText (tr ("You could not configure the default theme directly.<br>To configure it, first click the button below!<br>It will create a configurable copy named <i><b>DefaultCopy</b></i>."));
                ui->copyButton->show();
            }
            ui->horizontalSpacer_5->changeSize (1, 20);
            ui->label_4->hide();
            ui->label_5->hide();
            ui->checkBox1->hide();
            ui->checkBox2->hide();
            ui->checkBox3->hide();
            ui->checkBox4->hide();
            ui->checkBox5->hide();
            ui->checkBox6->hide();
            ui->checkBox7->hide();
            ui->checkBox8->hide();
        }
        else
        {
            ui->configLabel->setText (tr ("Here you could change only the most important keys.<br>To change other keys, edit this file manually:<br><i>~/.config/Kvantum/%1/<b>%1.kvconfig</b></i>").arg (theme));
            ui->horizontalSpacer_5->changeSize (1, 20, QSizePolicy::Expanding);
            ui->copyButton->hide();
            ui->label_4->show();
            ui->label_5->show();
            ui->checkBox1->show();
            ui->checkBox2->show();
            ui->checkBox3->show();
            ui->checkBox4->show();
            ui->checkBox5->show();
            ui->checkBox6->show();
            ui->checkBox7->show();
            ui->checkBox8->show();

            QString themeConfig = QString ("%1/Kvantum/%2/%2.kvconfig").arg (xdg_config_home).arg (theme);
            if (!QFile::exists (themeConfig))
                copyDefaultTheme (theme);
            if (QFile::exists (themeConfig))
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
                if (themeSettings.contains ("x11drag"))
                    ui->checkBox8->setChecked (themeSettings.value ("x11drag").toBool());
                else
                    ui->checkBox8->setChecked (true);
                themeSettings.endGroup();

                themeSettings.beginGroup ("Hacks");
                ui->checkBox1->setChecked (themeSettings.value ("transparent_dolphin_view").toBool());
                ui->checkBox2->setChecked (themeSettings.value ("transparent_ktitle_label").toBool());
                ui->checkBox3->setChecked (themeSettings.value ("respect_darkness").toBool());
                themeSettings.endGroup();
            }
        }
    }
}
/*************************/
void KvantumManager::selectionChanged (int)
{
    ui->statusBar->clearMessage();
}
/*************************/
void KvantumManager::updateThemeList()
{
    ui->comboBox->clear();

    QStringList list;
    list << "Kvantum's default theme";

    /* add all themes */
    QDir kv = QDir (QString ("%1/Kvantum").arg (xdg_config_home));
    if (kv.exists())
    {
        QStringList folders = kv.entryList (QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        foreach (const QString &folder, folders)
        {
            if (isThemeDir (QString ("%1/Kvantum/%2").arg (xdg_config_home).arg (folder)))
                list.append (folder);
        }
    }
    ui->comboBox->insertItems (0, list);

    /* choose the active theme */
    QString configFile = QString("%1/Kvantum/kvantum.kvconfig").arg (xdg_config_home);
    if (QFile::exists (configFile))
    {
        QSettings settings (configFile, QSettings::NativeFormat);
        if (settings.contains ("theme"))
        {
            QString theme = settings.value ("theme").toString();
            int index = ui->comboBox->findText (theme);
            if (index > -1)
                ui->comboBox->setCurrentIndex (index);
        }
    }
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
void KvantumManager::copyDefaultTheme (QString name)
{
    QDir cf = QDir (xdg_config_home);
    cf.mkdir ("Kvantum");
    QDir kv = QDir (QString ("%1/Kvantum").arg (xdg_config_home));
    /* if the Kvantum themes directory is created or already exists... */
    if (kv.exists())
    {
        kv.mkdir (name);
        QString theCopy = QString ("%1/Kvantum/%2/%2.kvconfig").arg (xdg_config_home).arg (name);
        if (!QFile::exists (theCopy))
        {
            if (QFile::copy (QString (":default.kvconfig"), theCopy))
            {
#if QT_VERSION < 0x050000
                QFile::setPermissions (theCopy, QFile::permissions (theCopy) | QFile::WriteOwner);
#else
                QFile::setPermissions (theCopy, QFile::permissions (theCopy) | QFileDevice::WriteOwner);
#endif
                ui->statusBar->showMessage (tr ("A copy of the default theme is created."), 20000);
            }
            else
                notWritable();
        }
        else
        {
            ui->statusBar->clearMessage();
            ui->statusBar->showMessage (tr ("A copy of the default theme is already created."), 20000);
        }
    }
    else
        notWritable();
}
/*************************/
void KvantumManager::copyDefaultTheme()
{
    copyDefaultTheme (QString ("DefaultCopy"));
}
/*************************/
void KvantumManager::wrtieConfig (bool checked)
{
    QString theme;
    QString configFile = QString("%1/Kvantum/kvantum.kvconfig").arg (xdg_config_home);
    if (QFile::exists (configFile))
    {
        QSettings settings (configFile, QSettings::NativeFormat);
        if (settings.contains ("theme"))
            theme = settings.value ("theme").toString();
    }
    if (!theme.isEmpty())
    {
        QString themeConfig = QString ("%1/Kvantum/%2/%2.kvconfig").arg (xdg_config_home).arg (theme);
        if (QFile::exists (themeConfig))
        {
            QSettings themeSettings (themeConfig, QSettings::NativeFormat);
            if (!themeSettings.isWritable())
            {
                notWritable();
                return;
            }
            if (QObject::sender() == ui->checkBox1)
            {
                themeSettings.beginGroup ("Hacks");
                themeSettings.setValue ("transparent_dolphin_view", checked);
            }
            else if (QObject::sender() == ui->checkBox2)
            {
                themeSettings.beginGroup ("Hacks");
                themeSettings.setValue ("transparent_ktitle_label", checked);
            }
            else if (QObject::sender() == ui->checkBox3)
            {
                themeSettings.beginGroup ("Hacks");
                themeSettings.setValue ("respect_darkness", checked);
            }
            else if (QObject::sender() == ui->checkBox4)
            {
                themeSettings.beginGroup ("General");
                themeSettings.setValue ("composite", !checked);
            }
            else if (QObject::sender() == ui->checkBox5)
            {
                themeSettings.beginGroup ("General");
                themeSettings.setValue ("left_tabs", checked);
            }
            else if (QObject::sender() == ui->checkBox6)
            {
                themeSettings.beginGroup ("General");
                themeSettings.setValue ("joined_tabs", checked);
            }
            else if (QObject::sender() == ui->checkBox7)
            {
                themeSettings.beginGroup ("General");
                themeSettings.setValue ("attach_active_tab", checked);
            }
            else// if (QObject::sender() == ui->checkBox8)
            {
                themeSettings.beginGroup ("General");
                themeSettings.setValue ("x11drag", checked);
            }
            themeSettings.endGroup();
        }
    }
}
