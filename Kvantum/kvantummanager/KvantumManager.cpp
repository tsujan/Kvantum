#include "KvantumManager.h"
#include <QFileDialog>
#include <QSettings>
#include <QMessageBox>
//#include <QDebug>

KvantumManager::KvantumManager (QWidget *parent) : QMainWindow (parent), ui (new Ui::KvantumManager)
{
    ui->setupUi (this);

    setWindowTitle ("Kvantum Manager");
    
    lastPath = QDir::home().path();

    connect (ui->quit, SIGNAL (clicked()), this, SLOT (close()));
    connect (ui->openTheme, SIGNAL (clicked()), this, SLOT (openTheme()));
    connect (ui->installTheme, SIGNAL (clicked()), this, SLOT (installTheme()));
    connect (ui->useTheme, SIGNAL (clicked()), this, SLOT (useTheme()));
    connect (ui->lineEdit, SIGNAL (textChanged (const QString &)), this, SLOT (txtChanged (const QString &)));
    connect (ui->toolBox, SIGNAL (currentChanged (int)), this, SLOT (tabChanged (int)));
    connect (ui->comboBox, SIGNAL (currentIndexChanged (int)), this, SLOT (selectionChanged (int)));

    updateThemeList();
}
/*************************/
KvantumManager::~KvantumManager()
{
    delete ui;
}
/*************************/
/*void KvantumManager::closeEvent (QCloseEvent *event)
{
    bool keep = false;
    if (keep)
        event->ignore();
    else
        event->accept();
    event->accept();
}*/
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
        QString xdg_config_home;
        QString homeDir = QDir::homePath();
        char * _xdg_config_home = getenv ("XDG_CONFIG_HOME");
        if (!_xdg_config_home)
            xdg_config_home = QString ("%1/.config").arg (homeDir);
        else
            xdg_config_home = QString (_xdg_config_home);
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

            ui->statusBar->showMessage (QString ("%1 installed.").arg (themeName), 20000);
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

    QString xdg_config_home;
    QString homeDir = QDir::homePath();
    char * _xdg_config_home = getenv ("XDG_CONFIG_HOME");
    if (!_xdg_config_home)
        xdg_config_home = QString ("%1/.config").arg (homeDir);
    else
        xdg_config_home = QString (_xdg_config_home);
    QString configFile = QString ("%1/Kvantum/kvantum.kvconfig").arg (xdg_config_home);
    QSettings globalSettings (configFile, QSettings::NativeFormat);
    if (!globalSettings.isWritable()) return;

    if (theme == "Kvantum's default theme")
        globalSettings.remove ("theme");
    else if (globalSettings.value ("theme").toString() != theme)
        globalSettings.setValue ("theme", theme);

    ui->statusBar->showMessage (QString ("Theme changed to %1.").arg (theme), 20000);
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
    QString xdg_config_home;
    QString homeDir = QDir::homePath();
    char * _xdg_config_home = getenv ("XDG_CONFIG_HOME");
    if (!_xdg_config_home)
        xdg_config_home = QString ("%1/.config").arg (homeDir);
    else
        xdg_config_home = QString (_xdg_config_home);
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
        QSettings globalSettings (configFile, QSettings::NativeFormat);
        if (globalSettings.contains ("theme"))
        {
            QString theme = globalSettings.value ("theme").toString();
            int index = ui->comboBox->findText (theme);
            if (index > -1)
                ui->comboBox->setCurrentIndex (index);
        }
    }
}
