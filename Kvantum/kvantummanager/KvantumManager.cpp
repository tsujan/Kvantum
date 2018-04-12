/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2018 <tsujan2000@gmail.com>
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

#include "KvantumManager.h"
#include <QFileDevice>
#include <QTextStream>
#include <QTimer>
#include <QStandardPaths>
#include <QFileDialog>
#include <QSettings>
#include <QMessageBox>
#include <QStyleFactory>
#include <QDesktopWidget>
#include <QWhatsThis>
#include <QScrollBar>
#include <QAbstractItemView>
#include <QWindow>
//#include <QDebug>

namespace KvManager {

static const QStringList windowGroups = (QStringList() << "Window" << "WindowTranslucent"
                                                       << "Dialog" << "DialogTranslucent");

KvantumManager::KvantumManager (const QString lang, QWidget *parent) : QMainWindow (parent), ui (new Ui::KvantumManager)
{
    lang_ = lang;
    ui->setupUi (this);

    confPageVisited_ = false;
    modifiedSuffix_ = " (" + tr ("modified") + ")";
    kvDefault_ = "Kvantum (" + tr ("default") + ")";

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

    /* this is just for protection against a bad sudo */
    char * _xdg_config_home = getenv ("XDG_CONFIG_HOME");
    if (!_xdg_config_home)
        xdg_config_home = QString ("%1/.config").arg (QDir::homePath());
    else
        xdg_config_home = QString (_xdg_config_home);

    desktop_ = qgetenv ("XDG_CURRENT_DESKTOP").toLower();

    ui->useTheme->setEnabled (false);

    ui->comboToolButton->insertItems (0, QStringList() << tr ("Follow Style")
                                                       << tr ("Icon Only")
                                                       << tr ("Text Only")
                                                       << tr ("Text Beside Icon")
                                                       << tr ("Text Under Icon"));

    ui->comboX11Drag->insertItems (0, QStringList() << tr ("Titlebar")
                                                    << tr ("Menubar")
                                                    << tr ("Menubar and primary toolbar")
                                                    << tr ("Anywhere possible"));

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
    /* set tooltip as "whatsthis" if the latter doesn't exist */
    QList<QWidget*> widgets = findChildren<QWidget*>();
    for (int i = 0; i < widgets.count(); ++i)
    {
        QWidget *w = widgets.at (i);
        if (w != ui->tab && w->whatsThis().isEmpty())
        {
            QString tip = w->toolTip();
            if (!tip.isEmpty())
            {
                QStringList simplified;
                QStringList paragraphs = tip.split("\n\n");
                for (int j = 0; j < paragraphs.size(); ++j)
                    simplified.append (paragraphs.at (j).simplified());
                w->setWhatsThis (simplified.join("\n\n"));
            }
        }
    }
    showAnimated (ui->installLabel, 1500);

    connect (ui->quit, &QAbstractButton::clicked, this, &KvantumManager::close);
    connect (ui->openTheme, &QAbstractButton::clicked, this, &KvantumManager::openTheme);
    connect (ui->installTheme, &QAbstractButton::clicked, this, &KvantumManager::installTheme);
    connect (ui->deleteTheme, &QAbstractButton::clicked, this, &KvantumManager::deleteTheme);
    connect (ui->useTheme, &QAbstractButton::clicked, this, &KvantumManager::useTheme);
    connect (ui->saveButton, &QAbstractButton::clicked, this, &KvantumManager::writeConfig);
    connect (ui->restoreButton, &QAbstractButton::clicked, this, &KvantumManager::restoreDefault);
    connect (ui->checkBoxNoComposite, &QAbstractButton::clicked, this, &KvantumManager::notCompisited);
    connect (ui->checkBoxTrans, &QAbstractButton::clicked, this, &KvantumManager::isTranslucent);
    connect (ui->checkBoxBlurWindow, &QAbstractButton::clicked, this, &KvantumManager::popupBlurring);
    connect (ui->checkBoxDE, &QAbstractButton::clicked, this, &KvantumManager::respectDE);
    connect (ui->checkBoxTransient, &QAbstractButton::clicked, this, &KvantumManager::trantsientScrollbarEnbled);
    connect (ui->lineEdit, &QLineEdit::textChanged, this, &KvantumManager::txtChanged);
    connect (ui->appsEdit, &QLineEdit::textChanged, this, &KvantumManager::txtChanged);
    connect (ui->toolBox, &QToolBox::currentChanged, this, &KvantumManager::tabChanged);
    connect (ui->saveAppButton, &QAbstractButton::clicked, this, &KvantumManager::writeAppLists);
    connect (ui->removeAppButton, &QAbstractButton::clicked, this, &KvantumManager::removeAppList);
    connect (ui->preview, &QAbstractButton::clicked, this, &KvantumManager::preview);
    connect (ui->aboutButton, &QAbstractButton::clicked, this, &KvantumManager::aboutDialog);
    connect (ui->whatsthisButton, &QAbstractButton::clicked, this, &KvantumManager::showWhatsThis);
    connect (ui->checkBoxComboMenu, &QAbstractButton::clicked, this, &KvantumManager::comboMenu);

    /* get ready for translucency */
    setAttribute (Qt::WA_NativeWindow, true);
    if (QWindow *window = windowHandle())
    {
        QSurfaceFormat format = window->format();
        format.setAlphaBufferSize (8);
        window->setFormat (format);
    }

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
/* Either folderPath is the path of a directory inside the Kvnatum folders,
   in which case its name should be the theme name, or it points to a folder
   inside an alternative installation path, in which case its name should be
   "Kvantum" and the theme name should be the name of its parent directory. */
bool KvantumManager::isThemeDir (const QString &folderPath) const
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

    if (QFile::exists (folderPath + QString ("/%1.kvconfig").arg (themeName))
        || QFile::exists (folderPath + QString ("/%1.svg").arg (themeName)))
    {
      return true;
    }

    return false;
}
/*************************/
// Checks if a config or SVG file with the given base (theme) name
// is inside a valid theme directory.
bool KvantumManager::fileBelongsToThemeDir (const QString &fileBaseName, const QString &folderPath) const
{
    if (!fileBaseName.isEmpty()
        /* even dark themes should be inside
           valid light theme directories */
        && isThemeDir (folderPath))
    {
        QString themeName = QDir (folderPath).dirName();
        QStringList parts = folderPath.split ("/");
        if (parts.last() == "Kvantum")
        {
            if (parts.size() < 3)
                return false;
            else
                themeName = parts.at (parts.size() - 2);
        }
        if (themeName == "Default" || themeName == "Kvantum")
            return false;
        if ((fileBaseName == themeName
             || fileBaseName == themeName + "Dark") // dark theme inside light theme folder
            && (QFile::exists (folderPath + QString("/%1.kvconfig").arg (fileBaseName))
                || QFile::exists (folderPath + QString("/%1.svg").arg (fileBaseName))))
        {
            return true;
        }
    }
    return false;
}
/*************************/
// Finds the user theme directory with the given name, considering
// the fact that a dark them can be inside a light theme directory.
QString KvantumManager::userThemeDir (const QString &themeName) const
{
    if (themeName.isEmpty())
        return QString();
    // ~/.config/Kvantum
    QString themeDir = xdg_config_home + QString ("/Kvantum/") + themeName;
    if (fileBelongsToThemeDir (themeName, themeDir))
        return themeDir;
    if (themeName.contains ("#"))
        return QString();
    QString lightFolder;
    if (themeName.size() > 4 && themeName.endsWith ("Dark"))
    {
        lightFolder = themeName.left (themeName.size() - 4);
        themeDir = xdg_config_home + QString ("/Kvantum/") + lightFolder;
        if (fileBelongsToThemeDir (themeName, themeDir))
            return themeDir;
    }
    // ~/.themes
    QString homeDir = QDir::homePath();
    themeDir = QString ("%1/.themes/%2/Kvantum").arg (homeDir).arg (themeName);
    if (fileBelongsToThemeDir (themeName, themeDir))
        return themeDir;
    if (!lightFolder.isEmpty())
    {
        themeDir = QString ("%1/.themes/%2/Kvantum").arg (homeDir).arg (lightFolder);
        if (fileBelongsToThemeDir (themeName, themeDir))
            return themeDir;
    }
    // ~/.local/share/themes
    themeDir = QString ("%1/.local/share/themes/%2/Kvantum").arg (homeDir).arg (themeName);
    if (fileBelongsToThemeDir (themeName, themeDir))
        return themeDir;
    if (!lightFolder.isEmpty())
    {
        themeDir = QString ("%1/.local/share/themes/%2/Kvantum").arg (homeDir).arg (lightFolder);
        if (fileBelongsToThemeDir (themeName, themeDir))
            return themeDir;
    }

    return QString();
}
/*************************/
// Finds the root theme directory with the given name, considering
// the fact that a dark them can be inside a light theme directory.
QString KvantumManager::rootThemeDir (const QString &themeName) const
{
    if (themeName.isEmpty()
        || themeName.contains ("#")) // # is allowed only in ~/.config/Kvantum
    {
        return QString();
    }
    // /usr/share/Kvantum
    QString themeDir = QString (DATADIR) + QString ("/Kvantum/") + themeName;
    if (fileBelongsToThemeDir (themeName, themeDir))
        return themeDir;
    QString lightFolder;
    if (themeName.size() > 4 && themeName.endsWith ("Dark"))
    {
        lightFolder = themeName.left (themeName.size() - 4);
        themeDir = QString (DATADIR) + QString ("/Kvantum/") + lightFolder;
        if (fileBelongsToThemeDir (themeName, themeDir))
            return themeDir;
    }
    // /usr/share/themes
    themeDir = QString (DATADIR) + QString ("/themes/%1/Kvantum").arg (themeName);
    if (fileBelongsToThemeDir (themeName, themeDir))
        return themeDir;
    if (!lightFolder.isEmpty())
    {
        themeDir = QString (DATADIR) + QString ("/themes/%1/Kvantum").arg (lightFolder);
        if (fileBelongsToThemeDir (themeName, themeDir))
            return themeDir;
    }

    return QString();
}
/*************************/
// isThemeDir(folderPath) should be true when this is called
bool KvantumManager::isLightWithDarkDir (const QString &folderPath) const
{
    if (folderPath.endsWith ("Dark"))
        return false;
    QDir dir = QDir (folderPath);
    QString themeName = dir.dirName();
    QStringList parts = folderPath.split ("/");
    if (parts.last() == "Kvantum")
    {
        if (parts.size() < 3)
            return false;
        else
            themeName = parts.at (parts.size() - 2);
    }
    if (themeName == "Default" || themeName == "Kvantum")
        return false;
    if (QFile::exists (folderPath + QString ("/%1.kvconfig").arg (themeName + "Dark"))
        || QFile::exists (folderPath + QString ("/%1.svg").arg (themeName + "Dark")))
    {
      return true;
    }
    return false;
}
/*************************/
void KvantumManager::notWritable (const QString &path)
{
    QMessageBox msgBox (QMessageBox::Warning,
                        tr ("Kvantum"),
                        "<center><b>" + tr ("You have no permission to write here:") + "</b></center>\n"
                        + QString ("<center>%1</center>\n").arg (path)
                        + "<center>" + tr ("Please fix that first!") + "</center>",
                        QMessageBox::Close,
                        this);
    msgBox.exec();
}
/*************************/
void KvantumManager::canNotBeRemoved (const QString &path, bool isDir)
{
    QString str;
    if (isDir)
        str = "<center><b>" + tr ("This directory cannot be removed:") + "</b></center>\n";
    else
        str = "<center><b>" + tr ("This file cannot be removed:") + "</b></center>\n";
    QMessageBox msgBox (QMessageBox::Warning,
                        tr ("Kvantum"),
                        str
                        + QString ("<center>%1</center>\n").arg (path)
                        + "<center>" + tr ("You might want to investigate the cause.") + "</center>",
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
                            "<center><b>" + tr ("This is not an installable Kvantum theme!") + "</b></center>\n"
                             + "<center>" + tr ("The name of an installable themes should not be \"Default\".") + "</center>\n"
                             + "<center>" + tr ("Please select another directory!") + "</center>",
                            QMessageBox::Close,
                            this);
        msgBox.exec();
    }
    else if ((QDir (theme).dirName()).contains ("#"))
    {
        QMessageBox msgBox (QMessageBox::Warning,
                            tr ("Kvantum"),
                            "<center><b>" + tr ("This is not an installable Kvantum theme!") + "</b></center>\n"
                            + "<center>" + tr ("Installable themes should not have # in their names.") + "</center>\n"
                            + "<center>" + tr ("Please select another directory!") + "</center>",
                            QMessageBox::Close,
                            this);
        msgBox.exec();
    }
    else if (!isThemeDir (theme))
    {
            QMessageBox msgBox (QMessageBox::Warning,
                                tr ("Kvantum"),
                                "<center><b>" + tr ("This is not a Kvantum theme folder!") + "</b></center>\n"
                                + "<center>" + tr ("Please select another directory!") + "</center>",
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
                                    "<center><b>" + tr ("The theme already exists in modified form.") + "</b></center>\n"
                                    + "<center>" + tr ("First you have to delete its modified version!") + "</center>",
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
                                        "<center><b>" + tr ("You have selected an installed theme folder.") + "</b></center>\n"
                                        + "<center>" + tr ("Please choose another directory!") + "</center>",
                                        QMessageBox::Close,
                                        this);
                    msgBox.exec();
                    return;
                }
                if (subDir.exists() && QFileInfo (subDir.absolutePath()).isWritable())
                {
                    QMessageBox msgBox (QMessageBox::Warning,
                                        tr ("Confirmation"),
                                        "<center><b>" + tr ("The theme already exists.") + "</b></center>\n"
                                        + "<center>" + tr ("Do you want to overwrite it?") + "</center>",
                                        QMessageBox::Yes | QMessageBox::No,
                                        this);
                    switch (msgBox.exec()) {
                    case QMessageBox::Yes:
                        /* ... then, remove the theme files first */
                        QFile::remove (QString ("%1/Kvantum/%2/%2.kvconfig")
                                       .arg (xdg_config_home).arg (themeName));
                        QFile::remove (QString ("%1/Kvantum/%2/%2.svg")
                                       .arg (xdg_config_home).arg (themeName));
                        QFile::remove (QString ("%1/Kvantum/%2/%2.kvconfig")
                                       .arg (xdg_config_home).arg (themeName + "Dark"));
                        QFile::remove (QString ("%1/Kvantum/%2/%2.svg")
                                       .arg (xdg_config_home).arg (themeName + "Dark"));
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
                                    "<center><b>" + tr ("This theme is also installed as root in:") + "</b></center>\n"
                                    + QString ("<center>%1</center>\n").arg (otherDir)
                                    + "<center>" + tr ("The user installation will take priority.") + "</center>",
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
                                        "<center><b>" + tr ("This theme is also installed as user in:") + "</b></center>\n"
                                        + QString ("<center>%1</center>\n").arg (otherDir)
                                        + "<center>" + tr ("This installation will take priority.") + "</center>",
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
            QStringList colorFiles;
            colorFiles.append (QString ("%1/%2.colors").arg (theme).arg (themeName));
            if (isLightWithDarkDir (theme))
            {
                QFile::copy (QString ("%1/%2.kvconfig").arg (theme).arg (themeName + "Dark"),
                             QString ("%1/Kvantum/%2/%3.kvconfig").arg (xdg_config_home)
                                                                  .arg (themeName)
                                                                  .arg (themeName + "Dark"));
                QFile::copy (QString ("%1/%2.svg").arg (theme).arg (themeName + "Dark"),
                             QString ("%1/Kvantum/%2/%3.svg").arg (xdg_config_home)
                                                             .arg (themeName)
                                                             .arg (themeName + "Dark"));
                colorFiles.append (QString ("%1/%2.colors").arg (theme).arg (themeName + "Dark"));
            }
            /* also copy the color scheme file */
            for (const QString &colorFile : static_cast<const QStringList&>(colorFiles))
            {
                if (QFile::exists (colorFile))
                {
                    QString name = colorFile.split ("/").last();
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
                        QString colorScheme = QString ("%1/color-schemes/").arg (kdeApps) + name;
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
                        QString colorScheme = QString ("%1/color-schemes/").arg (lShare) + name;
                        if (QFile::exists (colorScheme))
                            QFile::remove (colorScheme);
                        QFile::copy (colorFile, colorScheme);
                    }
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
        const QFileInfoList infoList = dir.entryInfoList (QDir::Files | QDir::AllDirs
                                                          | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden,
                                                          QDir::DirsFirst);
        for (const QFileInfo& info : infoList)
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
                        "<center><b>" + tr ("Do you really want to delete <i>%1</i>?").arg (theme) + "</b></center>",
                        QMessageBox::Yes | QMessageBox::No,
                        this);
    msgBox.setInformativeText ("<center><i>" + tr ("It could not be restored unless you have a copy of it.") + "</i></center>");
    msgBox.setDefaultButton (QMessageBox::No);
    switch (msgBox.exec()) {
        case QMessageBox::No: return;
        case QMessageBox::Yes:
        default: break;
    }

    QString theme_ = theme;
    if (theme == "Kvantum" + modifiedSuffix_)
        theme = "Default#";
    else if (theme.endsWith (modifiedSuffix_))
        theme.replace (modifiedSuffix_, "#");
    else if (theme == kvDefault_)
        return;

    QString lightTheme;
    if (theme.length() > 4 && theme.endsWith("Dark"))
    { // dark theme inside light theme folder
      lightTheme = theme.left(theme.length() - 4);
    }

    QString homeDir = QDir::homePath();
    QString file;
    QString dir = QString ("%1/Kvantum/%2").arg (xdg_config_home).arg (theme);
    if (!removeDir (dir))
    {
        canNotBeRemoved (dir, true);
        return;
    }
    if (!lightTheme.isEmpty())
    {
        file = QString ("%1/Kvantum/%2/%3.kvconfig")
               .arg (xdg_config_home).arg (lightTheme).arg (theme);
        if (QFile::exists (file) && !QFile::remove (file))
            canNotBeRemoved (file, false);
        file  = QString ("%1/Kvantum/%2/%3.svg")
                .arg (xdg_config_home).arg (lightTheme).arg (theme);
        if (QFile::exists (file) && !QFile::remove (file))
            canNotBeRemoved (file, false);
    }

    dir = QString ("%1/.themes/%2/Kvantum").arg (homeDir).arg (theme);
    if (!removeDir (dir))
    {
        canNotBeRemoved (dir, true);
        return;
    }
    if (!lightTheme.isEmpty())
    {
        file = QString ("%1/.themes/%2/Kvantum/%3.kvconfig")
               .arg (homeDir).arg (lightTheme).arg (theme);
        if (QFile::exists (file) && !QFile::remove (file))
            canNotBeRemoved (file, false);
        file = QString ("%1/.themes/%2/Kvantum/%3.svg")
               .arg (homeDir).arg (lightTheme).arg (theme);
        if (QFile::exists (file) && !QFile::remove (file))
            canNotBeRemoved (file, false);
    }

    dir = QString ("%1/.local/share/themes/%2/Kvantum").arg (homeDir).arg (theme);
    if (!removeDir (dir))
    {
        canNotBeRemoved (dir, true);
        return;
    }
    if (!lightTheme.isEmpty())
    {
        file = QString ("%1/.local/share/themes/%2/Kvantum/%3.kvconfig")
               .arg (homeDir).arg (lightTheme).arg (theme);
        if (QFile::exists (file) && !QFile::remove (file))
            canNotBeRemoved (file, false);
        file = QString ("%1/.local/share/themes/%2/Kvantum/%3.svg")
               .arg (homeDir).arg (lightTheme).arg (theme);
        if (QFile::exists (file) && !QFile::remove (file))
            canNotBeRemoved (file, false);
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
    if (kvconfigTheme_ == "Kvantum" + modifiedSuffix_)
        kvconfigTheme_ = "Default#";
    else if (kvconfigTheme_.endsWith (modifiedSuffix_))
         kvconfigTheme_.replace (modifiedSuffix_,  "#");
    else if (kvconfigTheme_ == kvDefault_)
        kvconfigTheme_ = QString();

    QString configFile = QString ("%1/Kvantum/kvantum.kvconfig").arg (xdg_config_home);
    QSettings settings (configFile, QSettings::NativeFormat);
    if (!settings.isWritable()) return;

    if (kvconfigTheme_.isEmpty())
        settings.remove ("theme");
    else if (settings.value ("theme").toString() != kvconfigTheme_)
        settings.setValue ("theme", kvconfigTheme_);

    writeOrigAppLists(); // needed only when the list is in a global config file

    QLabel *statusLabel = ui->statusBar->findChild<QLabel *>();
    statusLabel->setText ("<b>" + tr ("Active theme:") + QString ("</b> %1").arg (theme));
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
    ui->checkBoxScrollJump->setChecked (defaultSettings.value ("middle_click_scroll").toBool());
    ui->checkBoxNormalBtn->setChecked (defaultSettings.value ("normal_default_pushbutton").toBool());
    ui->checkBoxIconlessBtn->setChecked (defaultSettings.value ("iconless_pushbutton").toBool());
    ui->checkBoxIconlessMenu->setChecked (defaultSettings.value ("iconless_menu").toBool());
    ui->checkBoxToolbar->setChecked (defaultSettings.value ("single_top_toolbar").toBool());
    ui->checkBoxTint->setChecked (defaultSettings.value ("no_selection_tint").toBool());
    int tmp = 0;
    if (defaultSettings.contains ("tint_on_mouseover")) // it's 0 by default
        tmp = qMin (qMax (defaultSettings.value ("tint_on_mouseover").toInt(), 0), 100);
    ui->spinTint->setValue (tmp);
    tmp = 100;
    if (defaultSettings.contains ("disabled_icon_opacity")) // it's 100 by default
        tmp = qMin (qMax (defaultSettings.value ("disabled_icon_opacity").toInt(), 0), 100);
    ui->spinOpacity->setValue (tmp);
    tmp = 0;
    if (defaultSettings.contains ("lxqtmainmenu_iconsize")) // it's 0 by default
        tmp = qMin (qMax (defaultSettings.value ("lxqtmainmenu_iconsize").toInt(), 0), 32);
    ui->spinLxqtMenu->setValue (tmp);
    defaultSettings.endGroup();

    defaultSettings.beginGroup ("General");
    bool composited = defaultSettings.value ("composite").toBool();
    ui->checkBoxNoComposite->setChecked (!composited);
    ui->checkBoxAnimation->setChecked (defaultSettings.value ("animate_states").toBool());
    ui->checkBoxPattern->setChecked (defaultSettings.value ("no_window_pattern").toBool());
    ui->checkBoxleftTab->setChecked (defaultSettings.value ("left_tabs").toBool());
    ui->checkBoxJoinTab->setChecked (defaultSettings.value ("joined_inactive_tabs").toBool());
    if (defaultSettings.contains ("scroll_arrows")) // it's true by default
      ui->checkBoxNoScrollArrow->setChecked (!defaultSettings.value ("scroll_arrows").toBool());
    else
      ui->checkBoxNoScrollArrow->setChecked (false);
    ui->checkBoxScrollIn->setChecked (defaultSettings.value ("scrollbar_in_view").toBool());
    ui->checkBoxTransient->setChecked (defaultSettings.value ("transient_scrollbar").toBool());
    ui->checkBoxTransientGroove->setChecked (defaultSettings.value ("transient_groove").toBool());
    ui->checkBoxScrollableMenu->setChecked (defaultSettings.value ("scrollable_menu").toBool());
    ui->checkBoxTree->setChecked (defaultSettings.value ("tree_branch_line").toBool());
    ui->checkBoxGroupLabel->setChecked (defaultSettings.value ("groupbox_top_label").toBool());
    ui->checkBoxRubber->setChecked (defaultSettings.value ("fill_rubberband").toBool());
    if (defaultSettings.contains ("menubar_mouse_tracking")) // it's true by default
        ui->checkBoxMenubar->setChecked (defaultSettings.value ("menubar_mouse_tracking").toBool());
    else
        ui->checkBoxMenubar->setChecked (true);
    ui->checkBoxMenuToolbar->setChecked (defaultSettings.value ("merge_menubar_with_toolbar").toBool());
    ui->checkBoxGroupToolbar->setChecked (defaultSettings.value ("group_toolbar_buttons").toBool());
    if (defaultSettings.contains ("button_contents_shift")) // it's true by default
        ui->checkBoxButtonShift->setChecked (defaultSettings.value ("button_contents_shift").toBool());
    else
        ui->checkBoxButtonShift->setChecked (true);
    if (defaultSettings.contains ("alt_mnemonic")) // it's true by default
        ui->checkBoxAlt->setChecked (defaultSettings.value ("alt_mnemonic").toBool());
    else
        ui->checkBoxAlt->setChecked (true);
    int delay = -1;
    if (defaultSettings.contains ("tooltip_delay")) // it's -1 by default
        delay = qMin (qMax (defaultSettings.value ("tooltip_delay").toInt(), -1), 9999);
    ui->spinTooltipDelay->setValue (delay);
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
    ui->checkBoxHideComboCheckboxes->setChecked (defaultSettings.value ("hide_combo_checkboxes").toBool());
    comboMenu (ui->checkBoxComboMenu->isChecked());
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
    tmp = 0;
    if (defaultSettings.contains ("reduce_window_opacity")) // it's 0 by default
        tmp = qMin (qMax (defaultSettings.value ("reduce_window_opacity").toInt(), 0), 90);
    ui->spinReduceOpacity->setValue (tmp);

    tmp = 16;
    if (defaultSettings.contains ("small_icon_size"))
        tmp = defaultSettings.value ("small_icon_size").toInt();
    tmp = qMin(qMax(tmp,16), 48);
    ui->spinSmall->setValue (tmp);

    tmp = 32;
    if (defaultSettings.contains ("large_icon_size"))
        tmp = defaultSettings.value ("large_icon_size").toInt();
    tmp = qMin(qMax(tmp,24), 128);
    ui->spinLarge->setValue (tmp);

    tmp = 16;
    if (defaultSettings.contains ("button_icon_size"))
        tmp = defaultSettings.value ("button_icon_size").toInt();
    tmp = qMin(qMax(tmp,16), 64);
    ui->spinButton->setValue (tmp);

    tmp = 22;
    if (defaultSettings.contains ("toolbar_icon_size"))
        tmp = defaultSettings.value ("toolbar_icon_size").toInt();
    else if (defaultSettings.value ("slim_toolbars").toBool())
        tmp = 16;
    tmp = qMin(qMax(tmp,16), 64);
    ui->spinToolbar->setValue (tmp);

    tmp = 2;
    if (defaultSettings.contains ("layout_spacing"))
        tmp = defaultSettings.value ("layout_spacing").toInt();
    tmp = qMin(qMax(tmp,2), 16);
    ui->spinLayout->setValue (tmp);

    tmp = 4;
    if (defaultSettings.contains ("layout_margin"))
        tmp = defaultSettings.value ("layout_margin").toInt();
    tmp = qMin(qMax(tmp,2), 16);
    ui->spinLayoutMargin->setValue (tmp);

    tmp = 0;
    if (defaultSettings.contains ("submenu_overlap"))
        tmp = defaultSettings.value ("submenu_overlap").toInt();
    tmp = qMin(qMax(tmp,0), 16);
    ui->spinOverlap->setValue (tmp);

    tmp = 16;
    if (defaultSettings.contains ("spin_button_width"))
        tmp = defaultSettings.value ("spin_button_width").toInt();
    tmp = qMin(qMax(tmp,16), 32);
    ui->spinSpinBtnWidth->setValue (tmp);

    tmp = 36;
    if (defaultSettings.contains ("scroll_min_extent"))
        tmp = defaultSettings.value ("scroll_min_extent").toInt();
    tmp = qMin(qMax(tmp,16), 100);
    ui->spinMinScrollLength->setValue (tmp);

    defaultSettings.endGroup();

    respectDE (ui->checkBoxDE->isChecked());
}
/*************************/
void KvantumManager::restyleWindow()
{
    QApplication::setStyle (QStyleFactory::create ("kvantum"));
    // Qt5 has QEvent::ThemeChange
    const QWidgetList widgets = QApplication::allWidgets();
    for (QWidget *widget : widgets)
    {
        QEvent event (QEvent::ThemeChange);
        QApplication::sendEvent (widget, &event);
    }
    /* For some reason (a Qt problem?), the palettes related to the
       comboboxes aren't updated completely when the style changes. */
    QTimer::singleShot(0, this, SLOT (updateCombos()));
}
/*************************/
void KvantumManager::updateCombos()
{
    for (int i = 0; i < 2; ++i)
    {
        QComboBox *combo = (i == 0 ? ui->comboBox : ui->appCombo);
        if (QAbstractItemView *itemView = combo->view())
        {
            if (itemView->itemDelegate()
                && itemView->itemDelegate()->inherits ("QComboBoxDelegate"))
            {
                QPalette palette = itemView->palette();
                palette.setColor (QPalette::Text,
                                  QApplication::palette().color (QPalette::Text));
                itemView->setPalette (palette);
                if (itemView->viewport())
                {
                    palette = itemView->viewport()->palette();
                    palette.setColor (QPalette::Base,
                                     QApplication::palette().color (QPalette::Base));
                    itemView->viewport()->setPalette (palette);
                }
            }
            QList<QScrollBar*> widgets = combo->findChildren<QScrollBar*>();
            for (int j = 0; j < widgets.size(); ++j)
            {
                QPalette palette = widgets.at (j)->palette();
                palette.setColor (QPalette::Window,
                                  QApplication::palette().color (QPalette::Base));
                widgets.at (j)->setPalette (palette);
            }
            if (QAbstractItemView *cv = combo->completer()->popup())
            {
                QPalette palette = cv->palette();
                palette.setColor (QPalette::Text,
                                  QApplication::palette().color (QPalette::Text));
                palette.setColor (QPalette::Base,
                                  QApplication::palette().color (QPalette::Base));
                cv->setPalette (palette);
            }
        }
    }
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
                activeTheme = kvDefault_;
            else if (kvconfigTheme_ == "Default#")
                activeTheme = "Kvantum" + modifiedSuffix_;
            else if (kvconfigTheme_.endsWith ("#"))
                activeTheme = kvconfigTheme_.left (kvconfigTheme_.length() - 1) + modifiedSuffix_;
            else
                activeTheme = kvconfigTheme_;
            if (ui->comboBox->currentText() == activeTheme)
                showAnimated (ui->usageLabel, 1000);
            else
                ui->comboBox->setCurrentText (activeTheme); // sets tooltip, animation, etc.
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
            ui->configLabel->setText (tr ("These are the settings that can be safely changed.<br>For the others, click <i>Save</i> and then edit this file:")
                                      + "<br><i>~/.config/Kvantum/Default#/<b>Default#.kvconfig</b></i>");
            showAnimated (ui->configLabel, 1000);
            ui->checkBoxPattern->setEnabled (false);
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
            ui->configLabel->setText (tr ("These are the settings that can be safely changed.<br>For the others, edit this file:")
                                      + QString ("<br><i>%1</b></i>").arg (themeConfig));
            if (!QFile::exists (themeConfig) && !QFile::exists (userSvg))
            { // no user theme but a root one
                ui->configLabel->setText (tr ("These are the settings that can be safely changed.<br>For the others, click <i>Save</i> and then edit this file:")
                                          + QString ("<br><i>~/.config/Kvantum/%1#/<b>%1#.kvconfig</b></i>").arg (kvconfigTheme_));
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
                    QString rootDir = rootThemeDir(kvconfigTheme_);
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
                if (themeSettings.contains ("no_window_pattern"))
                    ui->checkBoxPattern->setChecked (themeSettings.value ("no_window_pattern").toBool());
                if (themeSettings.contains ("left_tabs"))
                    ui->checkBoxleftTab->setChecked (themeSettings.value ("left_tabs").toBool());
                if (themeSettings.contains ("joined_inactive_tabs"))
                    ui->checkBoxJoinTab->setChecked (themeSettings.value ("joined_inactive_tabs").toBool());
                if (themeSettings.contains ("joined_tabs")) // backward compatibility
                    ui->checkBoxJoinTab->setChecked (themeSettings.value ("joined_tabs").toBool());
                if (themeSettings.contains ("scroll_arrows"))
                    ui->checkBoxNoScrollArrow->setChecked (!themeSettings.value ("scroll_arrows").toBool());
                if (themeSettings.contains ("scrollbar_in_view"))
                    ui->checkBoxScrollIn->setChecked (themeSettings.value ("scrollbar_in_view").toBool());
                if (themeSettings.contains ("transient_scrollbar"))
                    ui->checkBoxTransient->setChecked (themeSettings.value ("transient_scrollbar").toBool());
                if (themeSettings.contains ("transient_groove"))
                    ui->checkBoxTransientGroove->setChecked (themeSettings.value ("transient_groove").toBool());
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
                if (themeSettings.contains ("group_toolbar_buttons"))
                    ui->checkBoxGroupToolbar->setChecked (themeSettings.value ("group_toolbar_buttons").toBool());
                if (themeSettings.contains ("button_contents_shift"))
                    ui->checkBoxButtonShift->setChecked (themeSettings.value ("button_contents_shift").toBool());
                if (themeSettings.contains ("alt_mnemonic"))
                    ui->checkBoxAlt->setChecked (themeSettings.value ("alt_mnemonic").toBool());
                int delay = -1;
                if (themeSettings.contains ("tooltip_delay"))
                    delay = qMin (qMax (themeSettings.value ("tooltip_delay").toInt(), -1), 9999);
                ui->spinTooltipDelay->setValue (delay);
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
                if (themeSettings.contains ("hide_combo_checkboxes"))
                    ui->checkBoxHideComboCheckboxes->setChecked (themeSettings.value ("hide_combo_checkboxes").toBool());
                comboMenu (ui->checkBoxComboMenu->isChecked());
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
                if (themeSettings.contains ("reduce_window_opacity"))
                {
                    int rwo = themeSettings.value ("reduce_window_opacity").toInt();
                    rwo = qMin (qMax (rwo, 0), 90);
                    ui->spinReduceOpacity->setValue (rwo);
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
                    theSize = qMin(qMax(theSize,0), 16);
                    ui->spinOverlap->setValue (theSize);
                }
                if (themeSettings.contains ("spin_button_width"))
                {
                    int theSize = themeSettings.value ("spin_button_width").toInt();
                    theSize = qMin(qMax(theSize,16), 32);
                    ui->spinSpinBtnWidth->setValue (theSize);
                }
                if (themeSettings.contains ("scroll_min_extent"))
                {
                    int theSize = themeSettings.value ("scroll_min_extent").toInt();
                    theSize = qMin(qMax(theSize,16), 100);
                    ui->spinMinScrollLength->setValue (theSize);
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
                ui->checkBoxScrollJump->setChecked (themeSettings.value ("middle_click_scroll").toBool());
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

                bool hasWindowPattern (false);
                for (const QString &windowGroup : windowGroups)
                {
                    themeSettings.beginGroup (windowGroup);
                    if (themeSettings.value ("interior.x.patternsize", 0).toInt() > 0
                        && themeSettings.value ("interior.y.patternsize", 0).toInt() > 0)
                    {
                        hasWindowPattern = true;
                        themeSettings.endGroup();
                        break;
                    }
                    themeSettings.endGroup();
                }
                ui->checkBoxPattern->setEnabled (hasWindowPattern);
            }
            else
                ui->checkBoxPattern->setEnabled (false);
        }

        trantsientScrollbarEnbled(ui->checkBoxTransient->isChecked());

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
    if (text == kvDefault_)
        text = QString();
    else if (text == "Kvantum" + modifiedSuffix_)
        text = "Default#";
    else if (text.endsWith (modifiedSuffix_))
        text.replace (modifiedSuffix_, "#");

    QString themeDir, themeConfig;
    if (!text.isEmpty())
    {
        themeDir = userThemeDir (text);
        themeConfig = QString ("%1/%2.kvconfig").arg (themeDir).arg (text);
    }

    if (setState)
    {
        if (comboText == kvDefault_
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
            themeDir = rootThemeDir (text);
            themeConfig = QString ("%1/%2.kvconfig").arg (themeDir).arg (text);
        }
    }
    else
        themeConfig = ":/Kvantum/default.kvconfig";

    if (text.isEmpty() || (!themeConfig.isEmpty() && QFile::exists (themeConfig)))
    {
        QSettings themeSettings (themeConfig, QSettings::NativeFormat);
        themeSettings.beginGroup ("General");
        QString commentStr ("comment");
        if (!lang_.isEmpty())
        {
            const QString lang = "[" + lang_ + "]";
            if (themeSettings.contains ("comment" + lang))
                commentStr += lang;
        }
        comment = themeSettings.value (commentStr).toString();
        if (comment.isEmpty()) // comma(s) in the comment
        {
            QStringList lst = themeSettings.value (commentStr).toStringList();
            if (!lst.isEmpty())
                comment = lst.join (", ");
        }
        themeSettings.endGroup();
    }

    if (comment.isEmpty())
      comment = tr ("No description");

    return comment;
}
/*************************/
void KvantumManager::selectionChanged (const QString &txt)
{
    if (txt.isEmpty()) return; // not needed

    ui->statusBar->clearMessage();

    QString theme;
    if (kvconfigTheme_.isEmpty())
        theme = kvDefault_;
    else if (kvconfigTheme_ == "Default#")
        theme = "Kvantum" + modifiedSuffix_;
    else if (kvconfigTheme_.endsWith ("#"))
        theme = kvconfigTheme_.left (kvconfigTheme_.length() - 1) + modifiedSuffix_;
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
#if QT_VERSION >= 0x050700
    disconnect (ui->comboBox, QOverload<const QString &>::of(&QComboBox::currentIndexChanged),
                this, &KvantumManager::selectionChanged);
#else
    disconnect (ui->comboBox, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::currentIndexChanged),
                this, &KvantumManager::selectionChanged);
#endif
    ui->comboBox->clear();
    QString curAppTheme;
    if (updateAppThemes)
    {
        disconnect (ui->appCombo, &ComboBox::textChangedSignal, this, &KvantumManager::assignAppTheme);
        curAppTheme = ui->appCombo->currentText();
        ui->appCombo->clear();
    }

    QStringList list;

    /* first add the user themes to the list */
    QDir kv = QDir (QString ("%1/Kvantum").arg (xdg_config_home));
    if (kv.exists())
    {
        const QStringList folders = kv.entryList (QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &folder : folders)
        {
            QString path = QString ("%1/Kvantum/%2").arg (xdg_config_home).arg (folder);
            if (isThemeDir (path))
            {
                if (folder == "Default#")
                    list.prepend ("Kvantum" + modifiedSuffix_);
                else if (folder.endsWith ("#"))
                {
                    /* see if there's a valid root installtion */
                    QString folder_ = folder.left (folder.length() - 1);
                    if (!rootThemeDir (folder_).isEmpty())
                        list.append (folder_ + modifiedSuffix_);
                }
                else
                {
                    list.append (folder);
                    if (isLightWithDarkDir (path))
                        list.append (folder + "Dark");
                }
            }
        }
    }
    QString homeDir = QDir::homePath();
    kv = QDir (QString ("%1/.themes").arg (homeDir));
    if (kv.exists())
    {
        const QStringList folders = kv.entryList (QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &folder : folders)
        {
            QString path = QString ("%1/.themes/%2/Kvantum").arg (homeDir).arg (folder);
            if (isThemeDir (path) && !folder.contains ("#"))
            {
                if (!list.contains (folder)) // the themes installed in the config folder have priority
                    list.append (folder);
                if (isLightWithDarkDir (path) && !list.contains (folder + "Dark"))
                    list.append (folder + "Dark");
            }
        }
    }
    kv = QDir (QString ("%1/.local/share/themes").arg (homeDir));
    if (kv.exists())
    {
        const QStringList folders = kv.entryList (QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &folder : folders)
        {
            QString path = QString ("%1/.local/share/themes/%2/Kvantum").arg (homeDir).arg (folder);
            if (isThemeDir (path) && !folder.contains ("#"))
            {
                if (!list.contains (folder)) // the user themes installed in the above paths have priority
                    list.append (folder);
                if (isLightWithDarkDir (path) && !list.contains (folder + "Dark"))
                    list.append (folder + "Dark");
            }
        }
    }

    /* now add the root themes */
    QStringList rootList;
    kv = QDir (QString (DATADIR) + QString ("/Kvantum"));
    if (kv.exists())
    {
        const QStringList folders = kv.entryList (QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &folder : folders)
        {
            QString path = QString (DATADIR) + QString ("/Kvantum/%1").arg (folder);
            if (!folder.contains ("#") && isThemeDir (path))
            {
                if (!list.contains (folder) // a user theme with the same name takes priority
                    && !list.contains (folder + modifiedSuffix_))
                {
                    rootList.append (folder);
                }
                if (isLightWithDarkDir (path)
                    && !list.contains (folder + "Dark")
                    && !list.contains (folder + "Dark" + modifiedSuffix_))
                {
                    rootList.append (folder + "Dark");
                }
            }
        }
    }
    kv = QDir (QString (DATADIR) + QString ("/themes"));
    if (kv.exists())
    {
        const QStringList folders = kv.entryList (QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &folder : folders)
        {
            QString path = QString (DATADIR) + QString ("/themes/%1/Kvantum").arg (folder);
            if (!folder.contains ("#") && isThemeDir (path))
            {
                if (!list.contains (folder) // a user theme with the same name takes priority
                    && !list.contains (folder + modifiedSuffix_)
                    // a root theme inside 'DATADIR/Kvantum/' with the same name takes priority
                    && !rootList.contains (folder))
                {
                    rootList.append (folder);
                }
                if (isLightWithDarkDir (path)
                    && !list.contains (folder + "Dark")
                    && !list.contains (folder + "Dark" + modifiedSuffix_)
                    && !rootList.contains (folder + "Dark"))
                {
                    rootList.append (folder + "Dark");
                }
            }
        }
    }
    list.append (rootList);
    list.sort();

    /* add the whole list to the combobox */
    bool hasDefaultThenme (false);
    if (list.isEmpty() || !list.contains("Kvantum" + modifiedSuffix_))
    {
        list.prepend (kvDefault_);
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
    bool isGlobalConfig = false;
    if (!QFile::exists (configFile))
    { // go to a global config file
        configFile = QString();
        QStringList confList = QStandardPaths::standardLocations (QStandardPaths::ConfigLocation);
        confList.removeOne (xdg_config_home);
        for (const QString &thisConf : static_cast<const QStringList&>(confList))
        {
            QString thisFile = QString ("%1/Kvantum/kvantum.kvconfig").arg (thisConf);
            if (QFile::exists (thisFile))
            { // my be a user config seen by root because of a bad sudo but won't be written to
                configFile = thisFile;
                isGlobalConfig = true;
                break;
            }
        }
    }
    if (!configFile.isEmpty())
    {
        QSettings settings (configFile, QSettings::NativeFormat);
        if (settings.contains ("theme"))
        {
            kvconfigTheme_ = settings.value ("theme").toString();
            if (kvconfigTheme_.isEmpty())
                theme = kvDefault_;
            else if (kvconfigTheme_ == "Default#")
                theme = "Kvantum" + modifiedSuffix_;
            else if (kvconfigTheme_.endsWith ("#"))
                theme = kvconfigTheme_.left (kvconfigTheme_.length() - 1) + modifiedSuffix_;
            else
                theme = kvconfigTheme_;
            int index = ui->comboBox->findText (theme);
            if (index > -1)
                ui->comboBox->setCurrentIndex (index);
            else
            {
                /* if there's a modified version of this theme, use it instead */
                if (!kvconfigTheme_.endsWith ("#"))
                    index = ui->comboBox->findText (theme + modifiedSuffix_);
                if (index > -1)
                {
                    kvconfigTheme_ += "#";
                    theme += modifiedSuffix_;
                    ui->comboBox->setCurrentIndex (index);
                    if (!isGlobalConfig)
                        settings.setValue ("theme", kvconfigTheme_); // correct the config file
                    QCoreApplication::processEvents();
                    restyleWindow();
                    if (process_->state() == QProcess::Running)
                        preview();
                }
                else // remove from settings if its folder is deleted
                {
                    if (list.contains ("Kvantum" + modifiedSuffix_))
                    {
                        if (!isGlobalConfig)
                            settings.setValue ("theme", "Default#");
                        kvconfigTheme_ = "Default#";
                        theme = "Kvantum" + modifiedSuffix_;
                    }
                    else
                    {
                        if (!isGlobalConfig)
                            settings.remove ("theme");
                        kvconfigTheme_ = QString();
                        theme = kvDefault_;
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
                    /* we remove # for simplicity and add it only at writeOrigAppLists() */
                    appTheme.remove (appTheme.count() - 1, 1);
                    if (ui->comboBox->findText ((appTheme == "Default" ? "Kvantum" : appTheme) + modifiedSuffix_) == -1)
                    {
                        nonexistent = true;
                        continue;
                    }
                }
                /* see if the theme is incorrect but its modified version exists */
                else
                {
                    QString str = appTheme == "Default" ? kvDefault_ : appTheme;
                    if (ui->comboBox->findText (str) == -1)
                    {
                        nonexistent = true;
                        if (ui->comboBox->findText (str + modifiedSuffix_) == -1)
                            continue;
                    }
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
        theme = kvDefault_;
        /* remove Default# because there's no config */
        QString theCopy = QString ("%1/Kvantum/Default#/Default#.kvconfig").arg (xdg_config_home);
        QFile::remove (theCopy);
    }

    QString comment = getComment (ui->comboBox->currentText());
    ui->comboBox->setToolTip (comment);
    ui->comboBox->setWhatsThis (comment);

    /* connect to combobox signal */
#if QT_VERSION >= 0x050700
    connect (ui->comboBox, QOverload<const QString &>::of(&QComboBox::currentIndexChanged),
             this, &KvantumManager::selectionChanged);
#else
    connect (ui->comboBox, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::currentIndexChanged),
             this, &KvantumManager::selectionChanged);
#endif
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
            if (appThemes_.value (curTxt).isEmpty())
                ui->appsEdit->clear();
            else
                ui->appsEdit->setText (appThemes_.value (curTxt).join (","));
        }

        connect (ui->appCombo, &ComboBox::textChangedSignal, this, &KvantumManager::assignAppTheme);
    }

    QLabel *statusLabel = ui->statusBar->findChild<QLabel *>();
    statusLabel->setText ("<b>" + tr ("Active theme:") + QString ("</b> %1").arg (theme));
}
/*************************/
void KvantumManager::preview()
{
    QString binDir = QApplication::applicationDirPath();
    QString previewExe = binDir + "/kvantumpreview";
    previewExe += " -style kvantum";
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
            QString sourceDir = rootThemeDir (source);
            QString _themeConfig = QString ("%1/%2.kvconfig").arg (sourceDir).arg (source);
            if (!sourceDir.isEmpty() && QFile::exists (_themeConfig)) // otherwise, the root theme is just an SVG image
                themeConfig = _themeConfig;
        }
        if (QFile::copy (themeConfig, theCopy))
        {
            QFile::setPermissions (theCopy, QFile::permissions (theCopy) | QFileDevice::WriteOwner);
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
/*************************/
static QString boolToStr (bool b)
{
    if (b) return QString ("true");
    else return QString ("false");
}
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
        hackKeys.insert("middle_click_scroll", boolToStr (ui->checkBoxScrollJump->isChecked()));
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
        generalKeys.insert("no_window_pattern", boolToStr (ui->checkBoxPattern->isChecked()));
        generalKeys.insert("left_tabs", boolToStr (ui->checkBoxleftTab->isChecked()));
        generalKeys.insert("joined_inactive_tabs", boolToStr (ui->checkBoxJoinTab->isChecked()));
        generalKeys.insert("scroll_arrows", boolToStr (!ui->checkBoxNoScrollArrow->isChecked()));
        generalKeys.insert("scrollbar_in_view", boolToStr (ui->checkBoxScrollIn->isChecked()));
        generalKeys.insert("transient_scrollbar", boolToStr (ui->checkBoxTransient->isChecked()));
        generalKeys.insert("transient_groove", boolToStr (ui->checkBoxTransientGroove->isChecked()));
        generalKeys.insert("scrollable_menu", boolToStr (ui->checkBoxScrollableMenu->isChecked()));
        generalKeys.insert("tree_branch_line", boolToStr (ui->checkBoxTree->isChecked()));
        generalKeys.insert("groupbox_top_label", boolToStr (ui->checkBoxGroupLabel->isChecked()));
        generalKeys.insert("fill_rubberband", boolToStr (ui->checkBoxRubber->isChecked()));
        generalKeys.insert("menubar_mouse_tracking",  boolToStr (ui->checkBoxMenubar->isChecked()));
        generalKeys.insert("merge_menubar_with_toolbar", boolToStr (ui->checkBoxMenuToolbar->isChecked()));
        generalKeys.insert("group_toolbar_buttons", boolToStr (ui->checkBoxGroupToolbar->isChecked()));
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
        generalKeys.insert("hide_combo_checkboxes", boolToStr (ui->checkBoxHideComboCheckboxes->isChecked()));
        generalKeys.insert("translucent_windows", boolToStr (ui->checkBoxTrans->isChecked()));
        generalKeys.insert("reduce_window_opacity", str.setNum (ui->spinReduceOpacity->value()));
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
        generalKeys.insert("scroll_min_extent", str.setNum (ui->spinMinScrollLength->value()));

        QString opaque = ui->opaqueEdit->text();
        opaque = opaque.simplified();
        opaque.remove (" ");
        generalKeys.insert("opaque", opaque);

        themeSettings.beginGroup ("Hacks");
        bool restyle = false;
        if (themeSettings.value ("normal_default_pushbutton").toBool() != ui->checkBoxNormalBtn->isChecked()
            || themeSettings.value ("iconless_pushbutton").toBool() != ui->checkBoxIconlessBtn->isChecked()
            || themeSettings.value ("middle_click_scroll").toBool() != ui->checkBoxScrollJump->isChecked()
            || qMin(qMax(themeSettings.value ("tint_on_mouseover").toInt(),0),100) != ui->spinTint->value()
            || qMin(qMax(themeSettings.value ("disabled_icon_opacity").toInt(),0),100) != ui->spinOpacity->value())
        {
            restyle = true;
        }
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
        themeSettings.endGroup();

        themeSettings.beginGroup ("General");
        if (themeSettings.value ("composite").toBool() == ui->checkBoxNoComposite->isChecked()
            || themeSettings.value ("translucent_windows").toBool() != ui->checkBoxTrans->isChecked()
            || qMin(qMax(themeSettings.value ("reduce_window_opacity").toInt(),0),90) != ui->spinReduceOpacity->value()
            || toDrag(themeSettings.value ("x11drag").toString()) != ui->comboX11Drag->currentIndex()
            || themeSettings.value ("inline_spin_indicators").toBool() != ui->checkBoxInlineSpin->isChecked()
            || themeSettings.value ("vertical_spin_indicators").toBool() != ui->checkBoxVSpin->isChecked()
            || themeSettings.value ("combo_menu").toBool() != ui->checkBoxComboMenu->isChecked()
            || themeSettings.value ("hide_combo_checkboxes").toBool() != ui->checkBoxHideComboCheckboxes->isChecked()
            || themeSettings.value ("animate_states").toBool() != ui->checkBoxAnimation->isChecked()
            || themeSettings.value ("no_window_pattern").toBool() != ui->checkBoxPattern->isChecked()
            || themeSettings.value ("left_tabs").toBool() != ui->checkBoxleftTab->isChecked()
            || themeSettings.value ("joined_inactive_tabs").toBool() != ui->checkBoxJoinTab->isChecked()
            || themeSettings.value ("scroll_arrows").toBool() == ui->checkBoxNoScrollArrow->isChecked()
            || themeSettings.value ("scrollbar_in_view").toBool() != ui->checkBoxScrollIn->isChecked()
            || themeSettings.value ("transient_scrollbar").toBool() != ui->checkBoxTransient->isChecked()
            || themeSettings.value ("transient_groove").toBool() != ui->checkBoxTransientGroove->isChecked()
            || themeSettings.value ("groupbox_top_label").toBool() != ui->checkBoxGroupLabel->isChecked()
            || themeSettings.value ("button_contents_shift").toBool() != ui->checkBoxButtonShift->isChecked()
            || qMin(qMax(themeSettings.value ("button_icon_size").toInt(),16),64) != ui->spinButton->value()
            || qMin(qMax(themeSettings.value ("layout_spacing").toInt(),2),16) != ui->spinLayout->value()
            || qMin(qMax(themeSettings.value ("layout_margin").toInt(),2),16) != ui->spinLayoutMargin->value()
            || qMin(qMax(themeSettings.value ("spin_button_width").toInt(),16),32) != ui->spinSpinBtnWidth->value()
            || qMin(qMax(themeSettings.value ("scroll_min_extent").toInt(),16),100) != ui->spinMinScrollLength->value())
        {
            restyle = true;
        }
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
        themeSettings.endGroup();

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
                    if (line.contains (QRegularExpression ("^\\s*\\b" + it.key() + "(?=\\s*\\=)")))
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
                    if (line.contains (QRegularExpression ("^\\s*\\b" + it.key() + "(?=\\s*\\=)")))
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
                if (lines.at (i).contains (QRegularExpression ("^\\s*\\[\\s*\\bHacks\\b\\s*\\]")))
                    break;
            }
            if (i == lines.count())
            {
                lines << "" << QString ("[Hacks]");
                ++i;
            }
            for (j = i+1; j < lines.count(); ++j)
            {
                if (lines.at (j).contains (QRegularExpression ("^\\s*\\[")))
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
                if (lines.at (i).contains (QRegularExpression ("^\\s*\\[\\s*%\\bGeneral\\b\\s*\\]")))
                    break;
            }
            if (i == lines.count())
            {
                lines << "" << QString ("[%General]");
                ++i;
            }
            for (j = i+1; j < lines.count(); ++j)
            {
                if (lines.at (j).contains (QRegularExpression ("^\\s*\\[")))
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

        ui->statusBar->showMessage (tr ("Configuration saved."), 10000);
        QString theme;
        if (kvconfigTheme_ == "Default#")
            theme = "Kvantum" + modifiedSuffix_;
        else if (kvconfigTheme_.endsWith ("#"))
            theme = kvconfigTheme_.left (kvconfigTheme_.length() - 1) + modifiedSuffix_;
        else
            theme = kvconfigTheme_;
        QLabel *statusLabel = ui->statusBar->findChild<QLabel *>();
        statusLabel->setText ("<b>" + tr ("Active theme:") + QString ("</b> %1").arg (theme));

        if (wasRootTheme && kvconfigTheme_.endsWith ("#"))
        {
            ui->restoreButton->show();
            ui->configLabel->setText (tr ("These are the settings that can be safely changed.<br>For the others, edit this file:")
                                      + QString ("<br><i>~/.config/Kvantum/%1/<b>%1.kvconfig</b></i>").arg (kvconfigTheme_));
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
        /* first, add the theme name if its key doesn't exist */
        if (!settings.contains ("theme") && !kvconfigTheme_.isEmpty())
            settings.setValue ("theme", kvconfigTheme_);

        settings.beginGroup ("Applications");
        QHashIterator<QString, QStringList> i (origAppThemes_);
        QString appTheme;
        while (i.hasNext())
        {
            i.next();
            appTheme = i.key();
            /* add # */
            int indx = ui->comboBox->findText ((appTheme == "Default" ? "Kvantum" : appTheme) + modifiedSuffix_);
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
                        "<center><b>" + tr ("Do you want to revert to the default (root) settings of this theme?") + "</b></center>",
                        QMessageBox::Yes | QMessageBox::No,
                        this);
    msgBox.setInformativeText ("<center><i>" + tr ("You will lose the changes you might have made.") + "</i></center>");
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
        QString rootDir = rootThemeDir (_kvconfigTheme_);
        QString _themeConfig = QString ("%1/%2.kvconfig").arg (rootDir).arg (_kvconfigTheme_);
        if (!rootDir.isEmpty() && QFile::exists (_themeConfig))
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
                                   .arg (kvconfigTheme_ == "Default#" ? tr ("the default theme") : _kvconfigTheme_),
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
    ui->reduceOpacityLabel->setEnabled (checked);
    ui->spinReduceOpacity->setEnabled (checked);
    if (!checked)
    {
        ui->checkBoxBlurWindow->setChecked (false);
        if (!ui->checkBoxNoComposite->isChecked())
          ui->checkBoxBlurPopup->setEnabled (true);
    }
}
/*************************/
void KvantumManager::comboMenu (bool checked)
{
    ui->checkBoxHideComboCheckboxes->setEnabled (checked);
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
    if (desktop_ == QByteArray ("kde"))
    {
        ui->labelSmall->setEnabled (!checked);
        ui->spinSmall->setEnabled (!checked);
        ui->labelLarge->setEnabled (!checked);
        ui->spinLarge->setEnabled (!checked);
        ui->checkBoxClick->setEnabled (!checked);
    }
    else
    {
        QSet<QByteArray> gtkDesktops = QSet<QByteArray>() << "gnome" << "unity" << "pantheon";
        if (gtkDesktops.contains(desktop_))
        {
            ui->labelX11Drag->setEnabled (!checked);
            //ui->comboX11Drag->setEnabled (!checked);
            ui->checkBoxIconlessBtn->setEnabled (!checked);
            ui->checkBoxIconlessMenu->setEnabled (!checked);
            ui->checkBoxNoComposite->setEnabled (!checked);
            ui->checkBoxBlurPopup->setEnabled (!ui->checkBoxNoComposite->isChecked()
                                               && !ui->checkBoxBlurWindow->isChecked()
                                               && !checked);
            ui->checkBoxTrans->setEnabled (!ui->checkBoxNoComposite->isChecked() && !checked);
            bool enableTrans (!ui->checkBoxNoComposite->isChecked()
                             && ui->checkBoxTrans->isChecked()
                             && !checked);
            ui->opaqueLabel->setEnabled (enableTrans);
            ui->opaqueEdit->setEnabled (enableTrans);
            ui->reduceOpacityLabel->setEnabled(enableTrans);
            ui->spinReduceOpacity->setEnabled(enableTrans);
            ui->checkBoxBlurWindow->setEnabled (enableTrans);
        }
        else ui->checkBoxDE->setEnabled (false);
    }
}
/*************************/
void KvantumManager::trantsientScrollbarEnbled (bool checked)
{
    ui->checkBoxTransientGroove->setEnabled (checked);
    ui->checkBoxScrollIn->setEnabled (!checked);
    ui->checkBoxNoScrollArrow->setEnabled (!checked);
}
/*************************/
void KvantumManager::showWhatsThis()
{
    QWhatsThis::enterWhatsThisMode();
}
/*************************/
void KvantumManager::aboutDialog()
{
    QMessageBox::about (this, tr ("About Kvantum Manager"),
                        "<center><b><big>" + tr ("Kvantum Manager") + " 0.10.7</big></b><br><br>"
                        + tr ("A tool for intsalling, selecting<br>and configuring <a href='https://github.com/tsujan/Kvantum'>Kvantum</a> themes") + "<br><br>"
                        + tr ("Author: <a href='mailto:tsujan2000@gmail.com?Subject=My%20Subject'>Pedram Pourang (aka. Tsu Jan)</a> </center><br>"));
}

}
