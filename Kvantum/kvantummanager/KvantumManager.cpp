/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2018-2025 <tsujan2000@gmail.com>
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
#include "svgicons.h"
#include "ui_about.h"
#include <QFileDevice>
#include <QTextStream>
#include <QTimer>
#include <QStandardPaths>
#include <QFileDialog>
#include <QSettings>
#include <QMessageBox>
#include <QStyleFactory>
#include <QScreen>
#include <QWhatsThis>
#include <QScrollBar>
#include <QAbstractItemView>
#include <QWindow>
#include <QToolButton>
#include <QShortcut>
#include <QGraphicsDropShadowEffect>
#include <QUrl>
#include <QDesktopServices>
#include <QRandomGenerator>
#include <QLocale>

namespace KvManager {

static const QStringList windowGroups = (QStringList() << "Window" << "WindowTranslucent"
                                                       << "Dialog" << "DialogTranslucent");

KvantumManager::KvantumManager (QWidget *parent) : QMainWindow (parent), ui (new Ui::KvantumManager)
{
    ui->setupUi (this);

    confPageVisited_ = false;
    modifiedSuffix_ = " (" + tr ("modified") + ")";
    kvDefault_ = "Kvantum (" + tr ("default") + ")";

    centerDefaultDocTabs_ = centerDefaultNormalTabs_ = false; // not really needed

    ui->openTheme->setIcon (symbolicIcon::icon (":/Icons/data/document-open.svg"));
    ui->deleteTheme->setIcon (symbolicIcon::icon (":/Icons/data/edit-delete.svg"));
    ui->useTheme->setIcon (symbolicIcon::icon (":/Icons/data/dialog-ok.svg"));
    ui->restoreButton->setIcon (symbolicIcon::icon (":/Icons/data/document-revert.svg"));
    ui->saveButton->setIcon (symbolicIcon::icon (":/Icons/data/document-save.svg"));
    ui->removeAppButton->setIcon (symbolicIcon::icon (":/Icons/data/edit-delete.svg"));
    ui->saveAppButton->setIcon (symbolicIcon::icon (":/Icons/data/document-save.svg"));
    ui->whatsthisButton->setIcon (symbolicIcon::icon (":/Icons/data/help-whatsthis.svg"));
    ui->aboutButton->setIcon (symbolicIcon::icon (":/Icons/data/help-about.svg"));
    ui->quit->setIcon (symbolicIcon::icon (":/Icons/data/application-exit.svg"));

    ui->toolBox->setItemIcon (0, symbolicIcon::icon (":/Icons/data/system-software-install.svg"));
    ui->toolBox->setItemIcon (1, symbolicIcon::icon (":/Icons/data/preferences-desktop-theme.svg"));
    ui->toolBox->setItemIcon (2, symbolicIcon::icon (":/Icons/data/preferences-system.svg"));
    ui->toolBox->setItemIcon (3, symbolicIcon::icon (":/Icons/data/applications-system.svg"));

    /* The default clear icon doesn't follow the color palette when the theme is changed.
       So, we replace it with our SVG icon. */
    const bool rtl (QApplication::layoutDirection() == Qt::RightToLeft);
    QList<QToolButton*> list = ui->comboBox->lineEdit()->findChildren<QToolButton*>();
    if (!list.isEmpty())
    {
        QToolButton *clearButton = list.at (0);
        if (clearButton)
            clearButton->setIcon (symbolicIcon::icon (rtl ? ":/Icons/data/edit-clear-rtl.svg"
                                                          : ":/Icons/data/edit-clear.svg"));
    }
    list = ui->appCombo->lineEdit()->findChildren<QToolButton*>();
    if (!list.isEmpty())
    {
        QToolButton *clearButton = list.at (0);
        if (clearButton)
            clearButton->setIcon (symbolicIcon::icon (rtl ? ":/Icons/data/edit-clear-rtl.svg"
                                                          : ":/Icons/data/edit-clear.svg"));
    }

    lastPath_ = QDir::home().path();
    process_ = new QProcess (this);

    /* this is just for protection against a bad sudo */
    char * _xdg_config_home = getenv ("XDG_CONFIG_HOME");
    if (!_xdg_config_home)
        xdg_config_home = QString ("%1/.config").arg (QDir::homePath());
    else
        xdg_config_home = QString (_xdg_config_home);

    desktop_ = qgetenv ("XDG_CURRENT_DESKTOP").toLower();

    ui->comboClick->insertItems (0, QStringList() << tr ("Follow Style")
                                                  << tr ("Single Click")
                                                  << tr ("Double Click"));

    ui->comboToolButton->insertItems (0, QStringList() << tr ("Follow Style")
                                                       << tr ("Icon Only")
                                                       << tr ("Text Only")
                                                       << tr ("Text Beside Icon")
                                                       << tr ("Text Under Icon"));

    ui->comboDialogButton->insertItems (0, QStringList() << tr ("Follow Style")
                                                         << tr ("KDE Layout")
                                                         << tr ("Gnome Layout")
                                                         << tr ("Mac Layout")
                                                         << tr ("Windows Layout")
                                                         << tr ("Android Layout"));

    ui->comboX11Drag->insertItems (0, QStringList() << tr ("Titlebar")
                                                    << tr ("Menubar")
                                                    << tr ("Menubar and primary toolbar")
                                                    << tr ("Anywhere possible"));

    ui->appsEdit->setClearButtonEnabled (true);

    QLabel *statusLabel = new QLabel();
    statusLabel->setTextInteractionFlags (Qt::TextSelectableByMouse);
    ui->statusBar->addWidget (statusLabel);

    /* set kvconfigTheme_ and connect to combobox signals */
    updateThemeList();

    effect_ = new QGraphicsOpacityEffect;
    animation_ = new QPropertyAnimation;

    setAttribute (Qt::WA_AlwaysShowToolTips);
    /* set tooltip as "whatsthis" if the latter doesn't exist */
    QList<QWidget*> widgets = findChildren<QWidget*>();
    for (int i = 0; i < widgets.count(); ++i)
    {
        QWidget *w = widgets.at (i);
        QString tip = w->toolTip();
        if (tip.isEmpty()) continue;
        if (w != ui->tab && w->whatsThis().isEmpty())
            w->setWhatsThis (tooTipToWhatsThis (tip));
        /* sadly, Qt 5.12 sees most tooltip texts as rich texts */
        w->setToolTip ("<p style='white-space:pre'>" + tip + "</p>");
    }
    showAnimated (ui->installLabel, 0, 1500);

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
    connect (ui->tabWidget, &QTabWidget::currentChanged, this, &KvantumManager::setTabWidgetFocus);
    connect (ui->saveAppButton, &QAbstractButton::clicked, this, &KvantumManager::writeAppLists);
    connect (ui->removeAppButton, &QAbstractButton::clicked, this, &KvantumManager::removeAppList);
    connect (ui->preview, &QAbstractButton::clicked, this, &KvantumManager::preview);
    connect (ui->aboutButton, &QAbstractButton::clicked, this, &KvantumManager::aboutDialog);
    connect (ui->whatsthisButton, &QAbstractButton::clicked, this, &KvantumManager::showWhatsThis);
    connect (ui->checkBoxComboMenu, &QAbstractButton::clicked, this, &KvantumManager::comboMenu);
    connect (ui->comboX11Drag, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        ui->checkBoxBtnDrag->setEnabled (index > 1);
    });

    /* vertical toolbars could be styled only if all toolbars can */
    connect (ui->checkBoxToolbar, &QAbstractButton::clicked, [this] (bool checked) {
        if (checked)
            ui->checkBoxVToolbar->setChecked (false);
        ui->checkBoxVToolbar->setEnabled (!checked);
    });

    /* in these cases, show a message box */
    connect (ui->checkBoxNoninteger, &QAbstractButton::clicked, [this] (bool checked) {
        if (checked) return;
        QMessageBox msgBox (QMessageBox::Warning,
                            tr ("Kvantum"),
                            "<center>" + ui->checkBoxNoninteger->toolTip() + "</center>\n",
                            QMessageBox::Close,
                            this);
        msgBox.exec();
    });
    ui->checkBoxKineticScrolling->setIcon (symbolicIcon::icon (":/Icons/data/dialog-warning.svg"));
    connect (ui->checkBoxKineticScrolling, &QAbstractButton::clicked, [this] (bool checked) {
        if (!checked) return;
        QString txt = ui->checkBoxKineticScrolling->toolTip().split ("\n\n", Qt::SkipEmptyParts).last();
        QMessageBox msgBox (QMessageBox::Warning,
                            tr ("Kvantum"),
                            "<center>" + txt + "</center>\n",
                            QMessageBox::Close,
                            this);
        msgBox.exec();
    });

    /* we want to open the config file but QLabel's link implementation has problems */
    ui->configLabel->setOpenExternalLinks (false);
    connect (ui->configLabel, &QLabel::linkActivated, this, &KvantumManager::openUserConfigFile);

    /* some useful shortcuts */
    QShortcut *shortcut = new QShortcut (QKeySequence (Qt::CTRL | Qt::Key_Down), this);
    connect (shortcut, &QShortcut::activated, ui->toolBox, [this] {
        if (QApplication::activePopupWidget()) return; // workaround for the popup completer
        int indx = ui->toolBox->currentIndex() + 1;
        if (indx >= ui->toolBox->count()) indx = 0;
        ui->toolBox->setCurrentIndex (indx);
    });
    shortcut = new QShortcut (QKeySequence (Qt::CTRL | Qt::Key_Up), this);
    connect (shortcut, &QShortcut::activated, ui->toolBox, [this] {
        if (QApplication::activePopupWidget()) return;
        int indx = ui->toolBox->currentIndex() - 1;
        if (indx < 0) indx = ui->toolBox->count() - 1;
        ui->toolBox->setCurrentIndex (indx);
    });
    shortcut = new QShortcut (QKeySequence (Qt::CTRL | Qt::Key_Tab), this);
    connect (shortcut, &QShortcut::activated, ui->tabWidget, [this] {
        if (!ui->tabWidget->isVisible()) return;
        int indx = ui->tabWidget->currentIndex() + 1;
        if (indx >= ui->tabWidget->count()) indx = 0;
        ui->tabWidget->setCurrentIndex (indx);
    });
    shortcut = new QShortcut (QKeySequence (Qt::CTRL | Qt::Key_Backtab), this);
    connect (shortcut, &QShortcut::activated, ui->tabWidget, [this] {
        if (!ui->tabWidget->isVisible()) return;
        int indx = ui->tabWidget->currentIndex() - 1;
        if (indx < 0) indx = ui->tabWidget->count() - 1;
        ui->tabWidget->setCurrentIndex (indx);
    });

    if (auto viewport = ui->toolBox->widget (2)->parentWidget())
    {
        viewport->installEventFilter (this); // see eventFilter()
        if (auto scrollArea = qobject_cast<QAbstractScrollArea*>(viewport->parentWidget()))
            scrollArea->setFocusPolicy (Qt::NoFocus); // keep the focus inside
    }

    /* get ready for translucency */
    setAttribute (Qt::WA_NativeWindow, true);
    if (QWindow *window = windowHandle())
    {
        QSurfaceFormat format = window->format();
        format.setAlphaBufferSize (8);
        window->setFormat (format);
    }

    QIcon icn = QIcon::fromTheme ("kvantum", QIcon (":/Icons/kvantumpreview/data/kvantum.svg"));
    setWindowIcon (icn);
    ui->preview->setIcon (icn);

    resize (minimumSizeHint().expandedTo (QSize (600, 400)));
    show();
}
/*************************/
KvantumManager::~KvantumManager()
{
    animatedWidgets_.clear();
    delete animation_;
    delete ui;
}
/*************************/
void KvantumManager::closeEvent (QCloseEvent *event)
{
    process_->terminate();
    process_->waitForFinished();
    event->accept();
}
/*************************/
bool KvantumManager::eventFilter (QObject *watched, QEvent *event)
{
    /* prevent the conf from being scrolled
       when its tab bar is scrolled by the mouse wheel
       (actually, this is a workaround for a Qt problem) */
    if (event->type() == QEvent::Wheel
        && watched == ui->toolBox->widget (2)->parentWidget()
        && ui->tabWidget->tabBar()->underMouse())
    {
        return true;
    }
    return QMainWindow::eventFilter (watched, event);
}
/*************************/
QString KvantumManager::tooTipToWhatsThis (const QString &tip)
{
    if (tip.isEmpty()) return QString();
    QStringList simplified;
    QStringList paragraphs = tip.split ("\n\n");
    for (int i = 0; i < paragraphs.size(); ++i)
        simplified.append (paragraphs.at (i).simplified());
    return simplified.join ("\n\n");
}
/*************************/
// Gives the focus to the first enabled widget of the active tab.
void KvantumManager::setTabWidgetFocus()
{
    if (auto tab = ui->tabWidget->currentWidget())
    {
        if (tab->isAncestorOf (QApplication::focusWidget())) return;
        auto w = tab->findChild<QWidget*>();
        QList<QWidget*> disabled;
        while (w && !w->isEnabled())
        {
            if (disabled.contains (w)) return;
            disabled << w;
            w = w->nextInFocusChain();
            if (!tab->isAncestorOf (w)) return;
        }
        if (w) w->setFocus();
    }
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
                                                          /*| QFileDialog::DontUseNativeDialog*/);
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
            && (QFile::exists (folderPath + QString ("/%1.kvconfig").arg (fileBaseName))
                || QFile::exists (folderPath + QString ("/%1.svg").arg (fileBaseName))))
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
    bool customThemeDeleted (false);
    if (theme == "Kvantum" + modifiedSuffix_)
        theme = "Default#";
    else if (theme.endsWith (modifiedSuffix_))
    {
        theme.replace (modifiedSuffix_, "#");
        customThemeDeleted = true;
    }
    else if (theme == kvDefault_)
        return;

    QString lightTheme;
    if (theme.length() > 4 && theme.endsWith ("Dark"))
    { // dark theme inside light theme folder
      lightTheme = theme.left (theme.length() - 4);
    }

    QString homeDir = QDir::homePath();
    QString file;
    QString dir = QString ("%1/Kvantum/%2").arg (xdg_config_home).arg (theme);
    if (!removeDir (dir)) // removeDir() returns true if dir doesn't exist
    {
        canNotBeRemoved (dir, true);
        return;
    }
    if (!lightTheme.isEmpty())
    {
        file = QString ("%1/Kvantum/%2/%3.kvconfig")
               .arg (xdg_config_home).arg (lightTheme).arg (theme);
        if (QFile::exists (file) && !QFile::remove (file))
        {
            canNotBeRemoved (file, false);
            customThemeDeleted = false;
        }
        file  = QString ("%1/Kvantum/%2/%3.svg")
                .arg (xdg_config_home).arg (lightTheme).arg (theme);
        if (QFile::exists (file) && !QFile::remove (file))
        {
            canNotBeRemoved (file, false);
            customThemeDeleted = false;
        }
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
        {
            canNotBeRemoved (file, false);
            customThemeDeleted = false;
        }
        file = QString ("%1/.themes/%2/Kvantum/%3.svg")
               .arg (homeDir).arg (lightTheme).arg (theme);
        if (QFile::exists (file) && !QFile::remove (file))
        {
            canNotBeRemoved (file, false);
            customThemeDeleted = false;
        }
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
        {
            canNotBeRemoved (file, false);
            customThemeDeleted = false;
        }
        file = QString ("%1/.local/share/themes/%2/Kvantum/%3.svg")
               .arg (homeDir).arg (lightTheme).arg (theme);
        if (QFile::exists (file) && !QFile::remove (file))
        {
            canNotBeRemoved (file, false);
            customThemeDeleted = false;
        }
    }

    QString configFile = QString ("%1/Kvantum/kvantum.kvconfig").arg (xdg_config_home);
    if (QFile::exists (configFile))
    {
        QSettings settings (configFile, QSettings::NativeFormat);
        if (settings.contains ("theme") && theme == settings.value ("theme").toString())
        { // the active theme is removed...
            if (customThemeDeleted)
            { // ... if it was customized, go to its root version
                kvconfigTheme_ = theme.left (theme.length() - 1);
                settings.setValue ("theme", kvconfigTheme_);
            }
            else if (isThemeDir (QString ("%1/Kvantum/Default#").arg (xdg_config_home)))
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
void KvantumManager::showAnimated (QWidget *w, int type, int duration)
{
    if (animation_->state() != QAbstractAnimation::Stopped)
    { // end the previous animation
        if (animation_->targetObject())
        {
            animation_->targetObject()->setProperty (animation_->propertyName().constData(),
                                                     animation_->endValue());
        }
        animation_->stop();
    }
    w->show();
    if (type < 0)
    { // decide on the animation type
        static int lastType = QRandomGenerator::global()->bounded(1, 3);
        if (animatedWidgets_.contains (w->objectName()))
            type = 0; // only one position animation for each widget
        else
        {
            animatedWidgets_.insert (w->objectName());
            type = lastType == 1 ? 2 : 1;
            lastType = type;
        }
    }

    /* First silence Qt's debug messages "QPropertyAnimation: you're trying to animate
       a non-existing property X of your QObject". */
    animation_->setPropertyName (QByteArray());

    if (type == 0)
    { // opacity animation
        w->setGraphicsEffect (effect_);
        animation_->setTargetObject (effect_);
        animation_->setDuration (duration > 0 ? duration : 1000);
        animation_->setPropertyName ("opacity");
        animation_->setEasingCurve (QEasingCurve::OutQuad);
        animation_->setStartValue (0.0);
        animation_->setEndValue (1.0);
    }
    else
    { // position animation
        animation_->setTargetObject (w);
        animation_->setDuration (duration > 0 ? duration : 700);
        animation_->setPropertyName ("pos");
        animation_->setEasingCurve (QEasingCurve::OutExpo);
        animation_->setStartValue (QPoint (w->x() + (type == 1 ? 1 : -1) * w->rect().width() / 2,
                                   w->y()));
        animation_->setEndValue (w->pos());
    }
    animation_->start();

    /* Qt has a scroll bug that shows up with SH_UnderlineShortcut set to
       "false" and interferes with label scrolling. This is a workaround. */
    connect (animation_, &QAbstractAnimation::finished,
             w, QOverload<>::of(&QWidget::update),
             Qt::UniqueConnection);
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
    showAnimated (ui->usageLabel);

    ui->useTheme->setEnabled (false);

    QCoreApplication::processEvents(); // needed if the config file is created by this method
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

    defaultSettings.beginGroup ("Focus");
    ui->checkBoxFocusRect->setChecked (!defaultSettings.value ("frame").toBool());
    defaultSettings.endGroup();

    defaultSettings.beginGroup ("Hacks");
    ui->checkBoxDolphin->setChecked (defaultSettings.value ("transparent_dolphin_view").toBool());
    ui->checkBoxPcmanfmSide->setChecked (defaultSettings.value ("transparent_pcmanfm_sidepane").toBool());
    ui->checkBoxPcmanfmView->setChecked (defaultSettings.value ("transparent_pcmanfm_view").toBool());
    ui->checkBoxBlurTranslucent->setChecked (defaultSettings.value ("blur_translucent").toBool());
    ui->checkBoxBlurActive->setChecked (defaultSettings.value ("blur_only_active_window").toBool());
    ui->checkBoxKtitle->setChecked (defaultSettings.value ("transparent_ktitle_label").toBool());
    ui->checkBoxMenuTitle->setChecked (defaultSettings.value ("transparent_menutitle").toBool());
    ui->checkBoxDark->setChecked (defaultSettings.value ("respect_darkness").toBool());
    ui->checkBoxGrip->setChecked (defaultSettings.value ("force_size_grip").toBool());
    ui->checkBoxScrollJump->setChecked (defaultSettings.value ("middle_click_scroll").toBool());
    ui->checkBoxKineticScrolling->setChecked (defaultSettings.value ("kinetic_scrolling").toBool());
    ui->checkBoxNoninteger->setChecked (!defaultSettings.value ("noninteger_translucency").toBool());
    ui->checkBoxNormalBtn->setChecked (defaultSettings.value ("normal_default_pushbutton").toBool());
    ui->checkBoxIconlessBtn->setChecked (defaultSettings.value ("iconless_pushbutton").toBool());
    ui->checkBoxIconlessMenu->setChecked (defaultSettings.value ("iconless_menu").toBool());
    ui->checkBoxToolbar->setChecked (defaultSettings.value ("single_top_toolbar").toBool());
    ui->checkBoxVToolbar->setChecked (!ui->checkBoxToolbar->isChecked()
                                      && defaultSettings.value ("style_vertical_toolbars").toBool());
    ui->checkBoxVToolbar->setEnabled (!ui->checkBoxToolbar->isChecked());
    ui->checkBoxTint->setChecked (defaultSettings.value ("no_selection_tint").toBool());
    ui->checkBoxCenteredForms->setChecked (defaultSettings.value ("centered_forms").toBool());
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
    ui->checkBoxInactiveness->setChecked (defaultSettings.value ("no_inactiveness").toBool());

    /* tab aligning keys are a little different */
    bool centerDefaultDocTabs_ = defaultSettings.value ("center_doc_tabs").toBool();
    bool centerDefaultNormalTabs_ = defaultSettings.value ("center_normal_tabs").toBool();
    ui->checkBoxleftTab->setChecked (defaultSettings.value ("left_tabs").toBool()
                                     && !(centerDefaultDocTabs_ && centerDefaultNormalTabs_));
    ui->checkBoxleftTab->setEnabled (!centerDefaultDocTabs_ && !centerDefaultNormalTabs_);

    ui->checkBoxJoinTab->setChecked (defaultSettings.value ("joined_inactive_tabs").toBool());
    if (defaultSettings.contains ("scroll_arrows")) // it's true by default
        ui->checkBoxNoScrollArrow->setChecked (!defaultSettings.value ("scroll_arrows").toBool());
    else
        ui->checkBoxNoScrollArrow->setChecked (false);
    ui->checkBoxScrollIn->setChecked (defaultSettings.value ("scrollbar_in_view").toBool());
    ui->checkBoxTransient->setChecked (defaultSettings.value ("transient_scrollbar").toBool());
    ui->checkBoxTransientGroove->setChecked (defaultSettings.value ("transient_groove").toBool());
    if (defaultSettings.contains ("scrollable_menu")) // it's true by default
        ui->checkBoxScrollableMenu->setChecked (defaultSettings.value ("scrollable_menu").toBool());
    else
        ui->checkBoxScrollableMenu->setChecked (true);
    ui->checkBoxTree->setChecked (defaultSettings.value ("tree_branch_line").toBool());
    ui->checkBoxGroupLabel->setChecked (defaultSettings.value ("groupbox_top_label").toBool());
    ui->checkBoxRubber->setChecked (defaultSettings.value ("fill_rubberband").toBool());
    if (defaultSettings.contains ("menubar_mouse_tracking")) // it's true by default
        ui->checkBoxMenubar->setChecked (defaultSettings.value ("menubar_mouse_tracking").toBool());
    else
        ui->checkBoxMenubar->setChecked (true);
    ui->checkBoxMenuToolbar->setChecked (defaultSettings.value ("merge_menubar_with_toolbar").toBool());
    ui->checkBoxGroupToolbar->setChecked (defaultSettings.value ("group_toolbar_buttons").toBool());
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
    if (defaultSettings.contains ("click_behavior"))
    {
        index = defaultSettings.value ("click_behavior").toInt();
        if (index > 2 || index < 0) index = 0;
    }
    ui->comboClick->setCurrentIndex (index);
    index = 0;
    if (defaultSettings.contains ("toolbutton_style"))
    {
        index = defaultSettings.value ("toolbutton_style").toInt();
        if (index > 4 || index < 0) index = 0;
    }
    ui->comboToolButton->setCurrentIndex (index);
    index = 0;
    if (defaultSettings.contains ("dialog_button_layout"))
    {
        index = defaultSettings.value ("dialog_button_layout").toInt();
        if (index > 5 || index < 0) index = 0;
    }
    ui->comboDialogButton->setCurrentIndex (index);
    ui->comboX11Drag->setCurrentIndex (toDrag (defaultSettings.value ("x11drag").toString()));
    ui->checkBoxBtnDrag->setChecked (defaultSettings.value ("drag_from_buttons").toBool());
    ui->checkBoxBtnDrag->setEnabled (ui->comboX11Drag->currentIndex() > 1);
    if (defaultSettings.contains ("respect_DE"))
        ui->checkBoxDE->setChecked (defaultSettings.value ("respect_DE").toBool());
    else
        ui->checkBoxDE->setChecked (true);
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

    ui->checkBoxShadowlessPopup->setChecked (defaultSettings.value ("shadowless_popup").toBool());

    int radius = 0;
    if (defaultSettings.contains ("menu_blur_radius")) // 0 by default
        radius = qMin (qMax (defaultSettings.value ("menu_blur_radius").toInt(), 0), 10);
    ui->spinMenuBlur->setValue (radius);
    radius = 0;
    if (defaultSettings.contains ("tooltip_blur_radius")) // 0 by default
        radius = qMin (qMax (defaultSettings.value ("tooltip_blur_radius").toInt(), 0), 10);
    ui->spinTooltipBlur->setValue (radius);

    /* all contrast effect values are 1 by default */
    if (defaultSettings.contains ("contrast"))
    {
        ui->spinContrast->setValue (qBound (static_cast<qreal>(0),
                                            defaultSettings.value ("contrast").toReal(),
                                            static_cast<qreal>(2)));
    }
    else ui->spinContrast->setValue (static_cast<qreal>(1));
    if (defaultSettings.contains ("intensity"))
    {
        ui->spinIntensity->setValue (qBound (static_cast<qreal>(0),
                                             defaultSettings.value ("intensity").toReal(),
                                             static_cast<qreal>(2)));
    }
    else ui->spinIntensity->setValue (static_cast<qreal>(1));
    if (defaultSettings.contains ("saturation"))
    {
        ui->spinSaturation->setValue (qBound (static_cast<qreal>(0),
                                              defaultSettings.value ("saturation").toReal(),
                                              static_cast<qreal>(2)));
    }
    else ui->spinSaturation->setValue (static_cast<qreal>(1));

    tmp = 0;
    if (defaultSettings.contains ("reduce_window_opacity")) // it's 0 by default
        tmp = qMin (qMax (defaultSettings.value ("reduce_window_opacity").toInt(), -90), 90);
    ui->spinReduceOpacity->setValue (tmp);

    tmp = 0;
    if (defaultSettings.contains ("reduce_menu_opacity")) // it's 0 by default
        tmp = qMin (qMax (defaultSettings.value ("reduce_menu_opacity").toInt(), 0), 90);
    ui->spinReduceMenuOpacity->setValue (tmp);

    tmp = 16;
    if (defaultSettings.contains ("small_icon_size"))
        tmp = defaultSettings.value ("small_icon_size").toInt();
    tmp = qMin (qMax (tmp,16), 48);
    ui->spinSmall->setValue (tmp);

    tmp = 32;
    if (defaultSettings.contains ("large_icon_size"))
        tmp = defaultSettings.value ("large_icon_size").toInt();
    tmp = qMin (qMax (tmp,24), 128);
    ui->spinLarge->setValue (tmp);

    tmp = 16;
    if (defaultSettings.contains ("button_icon_size"))
        tmp = defaultSettings.value ("button_icon_size").toInt();
    tmp = qMin (qMax (tmp,16), 64);
    ui->spinButton->setValue (tmp);

    tmp = 22;
    if (defaultSettings.contains ("toolbar_icon_size"))
    {
        QString str = defaultSettings.value ("toolbar_icon_size").toString();
        if (str == "font")
            ui->spinToolbar->setValue (15);
        else
        {
            tmp = defaultSettings.value ("toolbar_icon_size").toInt();
            tmp = qMin (qMax (tmp,16), 64);
            ui->spinToolbar->setValue (tmp);
        }
    }
    else if (defaultSettings.value ("slim_toolbars").toBool())
        ui->spinToolbar->setValue (16);
    else
        ui->spinToolbar->setValue (tmp);

    tmp = 3;
    if (defaultSettings.contains ("layout_spacing"))
        tmp = defaultSettings.value ("layout_spacing").toInt();
    tmp = qMin (qMax (tmp,2), 16);
    ui->spinLayout->setValue (tmp);

    tmp = 6;
    if (defaultSettings.contains ("layout_margin"))
        tmp = defaultSettings.value ("layout_margin").toInt();
    tmp = qMin (qMax (tmp,2), 16);
    ui->spinLayoutMargin->setValue (tmp);

    tmp = 0;
    if (defaultSettings.contains ("submenu_overlap"))
        tmp = defaultSettings.value ("submenu_overlap").toInt();
    tmp = qMin (qMax (tmp,0), 16);
    ui->spinOverlap->setValue (tmp);

    tmp = 16;
    if (defaultSettings.contains ("spin_button_width"))
        tmp = defaultSettings.value ("spin_button_width").toInt();
    tmp = qMin (qMax (tmp,16), 32);
    ui->spinSpinBtnWidth->setValue (tmp);

    tmp = 36;
    if (defaultSettings.contains ("scroll_min_extent"))
        tmp = defaultSettings.value ("scroll_min_extent").toInt();
    tmp = qMin (qMax (tmp,16), 100);
    ui->spinMinScrollLength->setValue (tmp);

    defaultSettings.endGroup();

    respectDE (ui->checkBoxDE->isChecked());
}
/*************************/
void KvantumManager::fitConfPageToContents()
{
     /* Avoid scrollbars in the conf page.
        NOTE: The layout of the conf page should be completely
              calculated when this function is called. */
    if (auto viewport = ui->toolBox->widget (2)->parentWidget())
    {
        if (auto scrollArea = qobject_cast<QAbstractScrollArea*>(viewport->parentWidget()))
        {
            QSize diff = viewport->childrenRect().size() - scrollArea->size();
            if (diff.width() > 0 || diff.height() > 0)
            {
                QSize newSize = size().expandedTo (size() + diff);
                QRect sr;
                if (QWindow *win = windowHandle())
                {
                    if (QScreen *sc = win->screen())
                        sr = sc->availableGeometry();
                }
                if (sr.isNull())
                {
                    if (QScreen *pScreen = QApplication::primaryScreen())
                        sr = pScreen->availableGeometry();
                }
                if (!sr.isNull())
                {
                    newSize = newSize.boundedTo (sr.size()
                                                 // the window frame size
                                                 - (frameGeometry().size() - size()));
                }
                resize (newSize);
            }
        }
    }
}
/*************************/
void KvantumManager::restyleWindow()
{
    const QWidgetList topLevels = QApplication::topLevelWidgets();
    for (QWidget *widget : topLevels)
    { // this is needed with Qt >= 5.13.1 but is harmless otherwise
        widget->setAttribute (Qt::WA_NoSystemBackground, false);
        widget->setAttribute (Qt::WA_TranslucentBackground, false);
    }
    QApplication::setStyle (QStyleFactory::create ("kvantum"));
    // Qt5 has QEvent::ThemeChange
    const QWidgetList widgets = QApplication::allWidgets();
    for (QWidget *widget : widgets)
    {
        QEvent event (QEvent::ThemeChange);
        QApplication::sendEvent (widget, &event);
    }

    QTimer::singleShot (0, this, [this] {
        /* this may be needed if the previous theme didn't have combo menus */
        for (int i = 0; i < 2; ++i)
        {
            QComboBox *combo = (i == 0 ? ui->comboBox : ui->appCombo);
            QList<QScrollBar*> widgets = combo->findChildren<QScrollBar*>();
            for (int j = 0; j < widgets.size(); ++j)
            {
                QPalette palette = widgets.at (j)->palette();
                palette.setColor (QPalette::Window,
                                  QApplication::palette().color (QPalette::Window));
                palette.setColor (QPalette::Base,
                                  QApplication::palette().color (QPalette::Base));
                widgets.at (j)->setPalette (palette);
            }
            if (QAbstractItemView *cv = combo->completer()->popup())
            {
                QPalette palette = cv->palette();
                palette.setColor (QPalette::Text,
                                  QApplication::palette().color (QPalette::Text));
                palette.setColor (QPalette::HighlightedText,
                                  QApplication::palette().color (QPalette::HighlightedText));
                palette.setColor (QPalette::Base,
                                  QApplication::palette().color (QPalette::Base));
                palette.setColor (QPalette::Window,
                                  QApplication::palette().color (QPalette::Window));
                palette.setColor (QPalette::Highlight,
                                  QApplication::palette().color (QPalette::Highlight));
                cv->setPalette (palette);
            }
        }
        /* avoid scrollbars in the conf page */
        if (ui->toolBox->currentIndex() == 2)
            fitConfPageToContents();
    });
}
/*************************/
void KvantumManager::tabChanged (int index)
{
    ui->statusBar->clearMessage();
    if (index == 0)
    {
        showAnimated (ui->installLabel, 0);
        ui->openTheme->setFocus();
    }
    else if (index == 1 || index == 3)
    {
        if (index == 1)
        {
            ui->comboBox->setFocus();
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
                showAnimated (ui->usageLabel);
            else
            { // WARNING: QComboBox::setCurrentText() doesn't set the current index.
                int index = ui->comboBox->findText (activeTheme);
                if (index > -1)
                    ui->comboBox->setCurrentIndex (index); // sets tooltip, animation, etc.
            }
        }
        else
        {
            showAnimated (ui->appLabel);
            ui->appCombo->setFocus();
        }
    }
    else if (index == 2)
    {
        ui->opaqueEdit->clear();
        defaultThemeButtons();
        setTabWidgetFocus();

        if (kvconfigTheme_.isEmpty())
        {
            ui->configLabel->setText (tr ("These are the settings that can be safely changed.<br>For the others, click <i>Save</i> and then edit this file:")
                                      + "<br><i>~/.config/Kvantum/Default#/<b>Default#.kvconfig</b></i>");
            showAnimated (ui->configLabel);
            ui->checkBoxPattern->setEnabled (false);
        }
        else
        {
            /* a config other than the default Kvantum one */
            QString themeDir = userThemeDir (kvconfigTheme_);
            QString themeConfig = QString ("%1/%2.kvconfig").arg (themeDir).arg (kvconfigTheme_);
            userConfigFile_ = themeConfig;
            QString userSvg = QString ("%1/%2.svg").arg (themeDir).arg (kvconfigTheme_);

            /* If themeConfig doesn't exist but userSvg does, themeConfig be created by copying
               the default config below and this message will be correct. If neither themeConfig
               nor userSvg exists, this message will be replaced by the one that follows it. */
            ui->configLabel->setText (tr ("These are the settings that can be safely changed.<br>For the others, edit this file:")
                                      + QString ("<a href='%2'><br><i>%1").arg (themeDir).arg (userConfigFile_) + QString ("/<b>%1.kvconfig</b></i></a>").arg (kvconfigTheme_));
            if (!QFile::exists (themeConfig) && !QFile::exists (userSvg))
            { // no user theme but a root one
                ui->configLabel->setText (tr ("These are the settings that can be safely changed.<br>For the others, click <i>Save</i> and then edit this file:")
                                          + QString ("<br><i>~/.config/Kvantum/%1#/<b>%1#.kvconfig</b></i>").arg (kvconfigTheme_));
            }
            showAnimated (ui->configLabel);

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
                    QString rootDir = rootThemeDir (kvconfigTheme_);
                    themeConfig = QString ("%1/%2.kvconfig").arg (rootDir).arg (kvconfigTheme_);
                }
            }

            if (QFile::exists (themeConfig)) // doesn't exist for a root theme without config
            {
                /* NOTE: The existence of keys should be checked because of inheritance. */

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
                if (themeSettings.contains ("no_inactiveness"))
                    ui->checkBoxInactiveness->setChecked (themeSettings.value ("no_inactiveness").toBool());

                bool centerDocTabs = centerDefaultDocTabs_;
                bool centerNormalTabs = centerDefaultNormalTabs_;
                if (themeSettings.contains ("center_doc_tabs"))
                    centerDocTabs = themeSettings.value ("center_doc_tabs").toBool();
                if (themeSettings.contains ("center_normal_tabs"))
                    centerNormalTabs = themeSettings.value ("center_normal_tabs").toBool();
                if (centerDocTabs && centerNormalTabs)
                    ui->checkBoxleftTab->setChecked (false);
                else if (themeSettings.contains ("left_tabs"))
                    ui->checkBoxleftTab->setChecked (themeSettings.value ("left_tabs").toBool());
                ui->checkBoxleftTab->setEnabled (!centerDocTabs && !centerNormalTabs);

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
                if (themeSettings.contains ("click_behavior"))
                {
                    int index = themeSettings.value ("click_behavior").toInt();
                    if (index > 2 || index < 0) index = 0;
                    ui->comboClick->setCurrentIndex (index);
                }
                if (themeSettings.contains ("toolbutton_style"))
                {
                    int index = themeSettings.value ("toolbutton_style").toInt();
                    if (index > 4 || index < 0) index = 0;
                    ui->comboToolButton->setCurrentIndex (index);
                }
                if (themeSettings.contains ("dialog_button_layout"))
                {
                    int index = themeSettings.value ("dialog_button_layout").toInt();
                    if (index > 5 || index < 0) index = 0;
                    ui->comboDialogButton->setCurrentIndex (index);
                }
                if (themeSettings.contains ("x11drag"))
                    ui->comboX11Drag->setCurrentIndex(toDrag (themeSettings.value ("x11drag").toString()));
                if (themeSettings.contains ("drag_from_buttons"))
                    ui->checkBoxBtnDrag->setChecked (themeSettings.value ("drag_from_buttons").toBool());
                ui->checkBoxBtnDrag->setEnabled (ui->comboX11Drag->currentIndex() > 1);
                if (themeSettings.contains ("respect_DE"))
                    ui->checkBoxDE->setChecked (themeSettings.value ("respect_DE").toBool());
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

                ui->checkBoxShadowlessPopup->setChecked (themeSettings.value ("shadowless_popup").toBool());

                if (themeSettings.contains ("menu_blur_radius"))
                    ui->spinMenuBlur->setValue (qMin (qMax (themeSettings.value ("menu_blur_radius").toInt(), 0), 10));
                if (themeSettings.contains ("tooltip_blur_radius"))
                    ui->spinTooltipBlur->setValue (qMin (qMax (themeSettings.value ("tooltip_blur_radius").toInt(), 0), 10));

                if (themeSettings.contains ("contrast"))
                {
                    ui->spinContrast->setValue (qBound (static_cast<qreal>(0),
                                                        themeSettings.value ("contrast").toReal(),
                                                        static_cast<qreal>(2)));
                }
                if (themeSettings.contains ("intensity"))
                {
                    ui->spinIntensity->setValue (qBound (static_cast<qreal>(0),
                                                        themeSettings.value ("intensity").toReal(),
                                                        static_cast<qreal>(2)));
                }
                if (themeSettings.contains ("saturation"))
                {
                    ui->spinSaturation->setValue (qBound (static_cast<qreal>(0),
                                                        themeSettings.value ("saturation").toReal(),
                                                        static_cast<qreal>(2)));
                }

                if (themeSettings.contains ("reduce_window_opacity"))
                {
                    int rwo = themeSettings.value ("reduce_window_opacity").toInt();
                    rwo = qMin (qMax (rwo, -90), 90);
                    ui->spinReduceOpacity->setValue (rwo);
                }
                if (themeSettings.contains ("reduce_menu_opacity"))
                {
                    int rmo = themeSettings.value ("reduce_menu_opacity").toInt();
                    rmo = qMin (qMax (rmo, 0), 90);
                    ui->spinReduceMenuOpacity->setValue (rmo);
                }
                if (themeSettings.contains ("small_icon_size"))
                {
                    int icnSize = themeSettings.value ("small_icon_size").toInt();
                    icnSize = qMin (qMax (icnSize,16), 48);
                    ui->spinSmall->setValue (icnSize);
                }
                if (themeSettings.contains ("large_icon_size"))
                {
                    int icnSize = themeSettings.value ("large_icon_size").toInt();
                    icnSize = qMin (qMax (icnSize,24), 128);
                    ui->spinLarge->setValue (icnSize);
                }
                if (themeSettings.contains ("button_icon_size"))
                {
                    int icnSize = themeSettings.value ("button_icon_size").toInt();
                    icnSize = qMin (qMax (icnSize,16), 64);
                    ui->spinButton->setValue (icnSize);
                }
                if (themeSettings.contains ("toolbar_icon_size"))
                {
                    QString str = themeSettings.value ("toolbar_icon_size").toString();
                    if (str == "font")
                        ui->spinToolbar->setValue (15);
                    else
                    {
                        int icnSize = themeSettings.value ("toolbar_icon_size").toInt();
                        icnSize = qMin (qMax (icnSize,16), 64);
                        ui->spinToolbar->setValue (icnSize);
                    }
                }
                else if (themeSettings.contains ("slim_toolbars"))
                    ui->spinToolbar->setValue (16);
                if (themeSettings.contains ("layout_spacing"))
                {
                    int theSize = themeSettings.value ("layout_spacing").toInt();
                    theSize = qMin (qMax (theSize,2), 16);
                    ui->spinLayout->setValue (theSize);
                }
                if (themeSettings.contains ("layout_margin"))
                {
                    int theSize = themeSettings.value ("layout_margin").toInt();
                    theSize = qMin (qMax (theSize,2), 16);
                    ui->spinLayoutMargin->setValue (theSize);
                }
                if (themeSettings.contains ("submenu_overlap"))
                {
                    int theSize = themeSettings.value ("submenu_overlap").toInt();
                    theSize = qMin (qMax (theSize,0), 16);
                    ui->spinOverlap->setValue (theSize);
                }
                if (themeSettings.contains ("spin_button_width"))
                {
                    int theSize = themeSettings.value ("spin_button_width").toInt();
                    theSize = qMin (qMax (theSize,16), 32);
                    ui->spinSpinBtnWidth->setValue (theSize);
                }
                if (themeSettings.contains ("scroll_min_extent"))
                {
                    int theSize = themeSettings.value ("scroll_min_extent").toInt();
                    theSize = qMin (qMax (theSize,16), 100);
                    ui->spinMinScrollLength->setValue (theSize);
                }
                themeSettings.endGroup();

                themeSettings.beginGroup ("Focus");
                ui->checkBoxFocusRect->setChecked (themeSettings.contains ("frame")
                                                   && !themeSettings.value ("frame").toBool());
                themeSettings.endGroup();

                themeSettings.beginGroup ("Hacks");
                ui->checkBoxDolphin->setChecked (themeSettings.value ("transparent_dolphin_view").toBool());
                ui->checkBoxPcmanfmSide->setChecked (themeSettings.value ("transparent_pcmanfm_sidepane").toBool());
                ui->checkBoxPcmanfmView->setChecked (themeSettings.value ("transparent_pcmanfm_view").toBool());
                ui->checkBoxBlurTranslucent->setChecked (themeSettings.value ("blur_translucent").toBool());
                ui->checkBoxBlurActive->setChecked (themeSettings.value ("blur_only_active_window").toBool());
                ui->checkBoxKtitle->setChecked (themeSettings.value ("transparent_ktitle_label").toBool());
                ui->checkBoxMenuTitle->setChecked (themeSettings.value ("transparent_menutitle").toBool());
                ui->checkBoxDark->setChecked (themeSettings.value ("respect_darkness").toBool());
                ui->checkBoxGrip->setChecked (themeSettings.value ("force_size_grip").toBool());
                ui->checkBoxScrollJump->setChecked (themeSettings.value ("middle_click_scroll").toBool());
                ui->checkBoxKineticScrolling->setChecked (themeSettings.value ("kinetic_scrolling").toBool());
                ui->checkBoxNoninteger->setChecked (!themeSettings.value ("noninteger_translucency").toBool());
                ui->checkBoxNormalBtn->setChecked (themeSettings.value ("normal_default_pushbutton").toBool());
                ui->checkBoxIconlessBtn->setChecked (themeSettings.value ("iconless_pushbutton").toBool());
                ui->checkBoxIconlessMenu->setChecked (themeSettings.value ("iconless_menu").toBool());
                ui->checkBoxToolbar->setChecked (themeSettings.value ("single_top_toolbar").toBool());
                ui->checkBoxVToolbar->setChecked (!ui->checkBoxToolbar->isChecked()
                                                  && themeSettings.value ("style_vertical_toolbars").toBool());
                ui->checkBoxVToolbar->setEnabled (!ui->checkBoxToolbar->isChecked());
                ui->checkBoxTint->setChecked (themeSettings.value ("no_selection_tint").toBool());
                ui->checkBoxCenteredForms->setChecked (themeSettings.value ("centered_forms").toBool());
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

        trantsientScrollbarEnbled (ui->checkBoxTransient->isChecked());

        if (!confPageVisited_ )
        {
            confPageVisited_ = true;
            /* a single-shot timer is needed for the layout to be fully calculated */
            QTimer::singleShot (0, this, [this] {
                fitConfPageToContents();
            });
        }
    }
}
/*************************/
void KvantumManager::openUserConfigFile (const QString& /*link*/)
{
    if (userConfigFile_.isEmpty()) return;
    QUrl url (userConfigFile_);
    /* QDesktopServices::openUrl() may resort to "xdg-open", which isn't
       the best choice. "gio" is always reliable, so we check it first. */
    if (!QProcess::startDetached ("gio", QStringList() << "open" << url.toString()))
        QDesktopServices::openUrl (url);
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
        if (!QLocale::system().name().isEmpty())
        {
            const QString lang = "[" + QLocale::system().name() + "]";
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
void KvantumManager::selectionChanged (int /*index*/)
{
    QString txt = ui->comboBox->currentText();
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
        showAnimated (ui->usageLabel);
    }
    else
    {
        ui->useTheme->setEnabled (true);
        ui->usageLabel->hide();
    }

    QString comment = getComment (txt);
    ui->comboBox->setWhatsThis (tooTipToWhatsThis (comment));
    ui->comboBox->setToolTip ("<p style='white-space:pre'>" + comment + "</p>");
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
        QStringList appList = editTxt.split (",", Qt::SkipEmptyParts);
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
    ui->appCombo->setWhatsThis (tooTipToWhatsThis (comment));
    ui->appCombo->setToolTip ("<p style='white-space:pre'>" + comment + "</p>");
}
/*************************/
void KvantumManager::updateThemeList (bool updateAppThemes)
{
    /* may be connected before */
    disconnect (ui->comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &KvantumManager::selectionChanged);
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
                    QString _folder = folder.left (folder.length() - 1);
                    if (!rootThemeDir (_folder).isEmpty())
                        list.append (_folder + modifiedSuffix_);
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
    if (!rootList.isEmpty())
        list.append (rootList);
    if (!list.isEmpty())
        list.sort();

    /* add the whole list to the combobox */
    bool hasDefaultThenme (false);
    if (list.isEmpty() || !list.contains ("Kvantum" + modifiedSuffix_))
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
        //ui->comboBox->insertSeparator (1); // too short without combo menu
        if (updateAppThemes)
        {
            ui->appCombo->insertSeparator (1);
            //ui->appCombo->insertSeparator (1);
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
                appList = settings.value (appThemes.at (i)).toStringList();
                appList.removeDuplicates();
                appTheme = appThemes.at (i);
                if (appTheme.endsWith ("#"))
                {
                    /* we remove # for simplicity and add it only at writeOrigAppLists() */
                    appTheme.remove (appTheme.size() - 1, 1);
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

    ui->useTheme->setEnabled (false); // the current theme is selected
    QString comment = getComment (ui->comboBox->currentText());
    ui->comboBox->setWhatsThis (tooTipToWhatsThis (comment));
    ui->comboBox->setToolTip ("<p style='white-space:pre'>" + comment + "</p>");

    /* connect to combobox signal */
    connect (ui->comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
             this, &KvantumManager::selectionChanged);
    /* put the app themes list in the text edit */
    if (updateAppThemes)
    {
        QString curTxt = ui->appCombo->currentText();
        if (!curTxt.isEmpty())
        {
            comment = getComment (curTxt, false);
            ui->appCombo->setWhatsThis (tooTipToWhatsThis (comment));
            ui->appCombo->setToolTip ("<p style='white-space:pre'>" + comment + "</p>");

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
    process_->terminate();
    process_->waitForFinished();
    process_->start ("kvantumpreview", QStringList() << "-style" << "kvantum");
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
        userConfigFile_ = themeConfig;
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
        QMap<QString, QString> hackKeys, hackKeysMissing, generalKeys, generalKeysMissing, focusKeys, focusKeysMissing;
        QString str;
        hackKeys.insert ("transparent_dolphin_view", boolToStr (ui->checkBoxDolphin->isChecked()));
        hackKeys.insert ("transparent_pcmanfm_sidepane", boolToStr (ui->checkBoxPcmanfmSide->isChecked()));
        hackKeys.insert ("transparent_pcmanfm_view", boolToStr (ui->checkBoxPcmanfmView->isChecked()));
        hackKeys.insert ("blur_translucent", boolToStr (ui->checkBoxBlurTranslucent->isChecked()));
        hackKeys.insert ("blur_only_active_window", boolToStr (ui->checkBoxBlurActive->isChecked()));
        hackKeys.insert ("transparent_ktitle_label", boolToStr (ui->checkBoxKtitle->isChecked()));
        hackKeys.insert ("transparent_menutitle", boolToStr (ui->checkBoxMenuTitle->isChecked()));
        hackKeys.insert ("respect_darkness", boolToStr (ui->checkBoxDark->isChecked()));
        hackKeys.insert ("force_size_grip", boolToStr (ui->checkBoxGrip->isChecked()));
        hackKeys.insert ("middle_click_scroll", boolToStr (ui->checkBoxScrollJump->isChecked()));
        hackKeys.insert ("kinetic_scrolling", boolToStr (ui->checkBoxKineticScrolling->isChecked()));
        hackKeys.insert ("noninteger_translucency", boolToStr (!ui->checkBoxNoninteger->isChecked()));
        hackKeys.insert ("normal_default_pushbutton", boolToStr (ui->checkBoxNormalBtn->isChecked()));
        hackKeys.insert ("iconless_pushbutton", boolToStr (ui->checkBoxIconlessBtn->isChecked()));
        hackKeys.insert ("iconless_menu", boolToStr (ui->checkBoxIconlessMenu->isChecked()));
        hackKeys.insert ("single_top_toolbar", boolToStr (ui->checkBoxToolbar->isChecked()));
        hackKeys.insert ("style_vertical_toolbars", boolToStr (ui->checkBoxVToolbar->isChecked()));
        hackKeys.insert ("no_selection_tint", boolToStr (ui->checkBoxTint->isChecked()));
        hackKeys.insert ("centered_forms", boolToStr (ui->checkBoxCenteredForms->isChecked()));
        hackKeys.insert ("tint_on_mouseover", str.setNum (ui->spinTint->value()));
        hackKeys.insert ("disabled_icon_opacity", str.setNum (ui->spinOpacity->value()));
        hackKeys.insert ("lxqtmainmenu_iconsize", str.setNum (ui->spinLxqtMenu->value()));

        generalKeys.insert ("composite", boolToStr (!ui->checkBoxNoComposite->isChecked()));
        generalKeys.insert ("animate_states", boolToStr (ui->checkBoxAnimation->isChecked()));
        generalKeys.insert ("no_window_pattern", boolToStr (ui->checkBoxPattern->isChecked()));
        generalKeys.insert ("no_inactiveness", boolToStr (ui->checkBoxInactiveness->isChecked()));
        generalKeys.insert ("left_tabs", boolToStr (ui->checkBoxleftTab->isChecked()));
        generalKeys.insert ("joined_inactive_tabs", boolToStr (ui->checkBoxJoinTab->isChecked()));
        generalKeys.insert ("scroll_arrows", boolToStr (!ui->checkBoxNoScrollArrow->isChecked()));
        generalKeys.insert ("scrollbar_in_view", boolToStr (ui->checkBoxScrollIn->isChecked()));
        generalKeys.insert ("transient_scrollbar", boolToStr (ui->checkBoxTransient->isChecked()));
        generalKeys.insert ("transient_groove", boolToStr (ui->checkBoxTransientGroove->isChecked()));
        generalKeys.insert ("scrollable_menu", boolToStr (ui->checkBoxScrollableMenu->isChecked()));
        generalKeys.insert ("tree_branch_line", boolToStr (ui->checkBoxTree->isChecked()));
        generalKeys.insert ("groupbox_top_label", boolToStr (ui->checkBoxGroupLabel->isChecked()));
        generalKeys.insert ("fill_rubberband", boolToStr (ui->checkBoxRubber->isChecked()));
        generalKeys.insert ("menubar_mouse_tracking",  boolToStr (ui->checkBoxMenubar->isChecked()));
        generalKeys.insert ("merge_menubar_with_toolbar", boolToStr (ui->checkBoxMenuToolbar->isChecked()));
        generalKeys.insert ("group_toolbar_buttons", boolToStr (ui->checkBoxGroupToolbar->isChecked()));
        generalKeys.insert ("alt_mnemonic", boolToStr (ui->checkBoxAlt->isChecked()));
        generalKeys.insert ("tooltip_delay", str.setNum (ui->spinTooltipDelay->value()));
        generalKeys.insert ("submenu_delay", str.setNum (ui->spinSubmenuDelay->value()));
        generalKeys.insert ("click_behavior", str.setNum (ui->comboClick->currentIndex()));
        generalKeys.insert ("toolbutton_style", str.setNum (ui->comboToolButton->currentIndex()));
        generalKeys.insert ("dialog_button_layout", str.setNum (ui->comboDialogButton->currentIndex()));
        generalKeys.insert ("x11drag", toStr(static_cast<Drag>(ui->comboX11Drag->currentIndex())));
        generalKeys.insert ("drag_from_buttons", boolToStr (ui->checkBoxBtnDrag->isChecked()));
        generalKeys.insert ("respect_DE", boolToStr (ui->checkBoxDE->isChecked()));
        generalKeys.insert ("inline_spin_indicators", boolToStr (ui->checkBoxInlineSpin->isChecked()));
        generalKeys.insert ("vertical_spin_indicators", boolToStr (ui->checkBoxVSpin->isChecked()));
        generalKeys.insert ("combo_as_lineedit", boolToStr (ui->checkBoxComboEdit->isChecked()));
        generalKeys.insert ("combo_menu", boolToStr (ui->checkBoxComboMenu->isChecked()));
        generalKeys.insert ("hide_combo_checkboxes", boolToStr (ui->checkBoxHideComboCheckboxes->isChecked()));
        generalKeys.insert ("translucent_windows", boolToStr (ui->checkBoxTrans->isChecked()));
        generalKeys.insert ("reduce_window_opacity", str.setNum (ui->spinReduceOpacity->value()));
        generalKeys.insert ("reduce_menu_opacity", str.setNum (ui->spinReduceMenuOpacity->value()));
        generalKeys.insert ("popup_blurring", boolToStr (ui->checkBoxBlurPopup->isChecked()));
        generalKeys.insert ("shadowless_popup", boolToStr (ui->checkBoxShadowlessPopup->isChecked()));
        generalKeys.insert ("blurring", boolToStr (ui->checkBoxBlurWindow->isChecked()));

        generalKeys.insert ("menu_blur_radius", str.setNum (ui->spinMenuBlur->value()));
        generalKeys.insert ("tooltip_blur_radius", str.setNum (ui->spinTooltipBlur->value()));

        generalKeys.insert ("contrast", str.setNum (ui->spinContrast->value(), 'f', 2));
        generalKeys.insert ("intensity", str.setNum (ui->spinIntensity->value(), 'f', 2));
        generalKeys.insert ("saturation", str.setNum (ui->spinSaturation->value(), 'f', 2));

        generalKeys.insert ("small_icon_size", str.setNum (ui->spinSmall->value()));
        generalKeys.insert ("large_icon_size", str.setNum (ui->spinLarge->value()));
        generalKeys.insert ("button_icon_size", str.setNum (ui->spinButton->value()));
        int icnSize = ui->spinToolbar->value();
        if (icnSize == 15)
            generalKeys.insert ("toolbar_icon_size", "font");
        else
            generalKeys.insert ("toolbar_icon_size", str.setNum (icnSize));
        generalKeys.insert ("layout_spacing", str.setNum (ui->spinLayout->value()));
        generalKeys.insert ("layout_margin", str.setNum (ui->spinLayoutMargin->value()));
        generalKeys.insert ("submenu_overlap", str.setNum (ui->spinOverlap->value()));
        generalKeys.insert ("spin_button_width", str.setNum (ui->spinSpinBtnWidth->value()));
        generalKeys.insert ("scroll_min_extent", str.setNum (ui->spinMinScrollLength->value()));

        QString opaque = ui->opaqueEdit->text();
        opaque = opaque.simplified();
        opaque.remove (" ");
        generalKeys.insert ("opaque", opaque);

        focusKeys.insert ("frame", boolToStr (!ui->checkBoxFocusRect->isChecked()));

        bool restyle = false;
        QMap<QString, QString>::iterator it;

        themeSettings.beginGroup ("Focus");
        if (themeSettings.value ("frame").toBool() == ui->checkBoxFocusRect->isChecked())
          restyle = true;
        it = focusKeys.begin();
        while (!focusKeys.isEmpty() && it != focusKeys.end())
        {
            if (!themeSettings.contains (it.key()))
            {
                focusKeysMissing.insert (it.key(), focusKeys.value (it.key()));
                it = focusKeys.erase (it);
            }
            else ++it;
        }
        themeSettings.endGroup();

        themeSettings.beginGroup ("Hacks");
        if (themeSettings.value ("normal_default_pushbutton").toBool() != ui->checkBoxNormalBtn->isChecked()
            || themeSettings.value ("iconless_pushbutton").toBool() != ui->checkBoxIconlessBtn->isChecked()
            || themeSettings.value ("middle_click_scroll").toBool() != ui->checkBoxScrollJump->isChecked()
            || themeSettings.value ("kinetic_scrolling").toBool() != ui->checkBoxKineticScrolling->isChecked()
            || qMin (qMax (themeSettings.value ("tint_on_mouseover").toInt(), 0), 100) != ui->spinTint->value()
            || qMin (qMax (themeSettings.value ("disabled_icon_opacity").toInt(), 0), 100) != ui->spinOpacity->value()
            || themeSettings.value ("noninteger_translucency").toBool() == ui->checkBoxNoninteger->isChecked()
            || themeSettings.value ("blur_only_active_window").toBool() != ui->checkBoxBlurActive->isChecked())
        {
            restyle = true;
        }
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
            || qMin (qMax (themeSettings.value ("reduce_window_opacity").toInt(), -90), 90) != ui->spinReduceOpacity->value()
            || qMin (qMax (themeSettings.value ("reduce_menu_opacity").toInt(), 0), 90) != ui->spinReduceMenuOpacity->value()
            || themeSettings.value ("blurring").toBool() != ui->checkBoxBlurWindow->isChecked()
            || themeSettings.value ("popup_blurring").toBool() != ui->checkBoxBlurPopup->isChecked()
            || themeSettings.value ("shadowless_popup").toBool() != ui->checkBoxShadowlessPopup->isChecked()

            || qMin (qMax (themeSettings.value ("menu_blur_radius").toInt(), 0), 10) != ui->spinMenuBlur->value()
            || qMin (qMax (themeSettings.value ("tooltip_blur_radius").toInt(), 0), 10) != ui->spinTooltipBlur->value()

            || qBound (static_cast<qreal>(0), themeSettings.value ("contrast").toReal(), static_cast<qreal>(2))
               != static_cast<qreal>(ui->spinContrast->value())
            || qBound (static_cast<qreal>(0), themeSettings.value ("intensity").toReal(), static_cast<qreal>(2))
               != static_cast<qreal>(ui->spinIntensity->value())
            || qBound (static_cast<qreal>(0), themeSettings.value ("saturation").toReal(), static_cast<qreal>(2))
               != static_cast<qreal>(ui->spinSaturation->value())

            || toDrag (themeSettings.value ("x11drag").toString()) != ui->comboX11Drag->currentIndex()
            || themeSettings.value ("drag_from_buttons").toBool() != ui->checkBoxBtnDrag->isChecked()
            || themeSettings.value ("inline_spin_indicators").toBool() != ui->checkBoxInlineSpin->isChecked()
            || themeSettings.value ("vertical_spin_indicators").toBool() != ui->checkBoxVSpin->isChecked()
            || themeSettings.value ("combo_menu").toBool() != ui->checkBoxComboMenu->isChecked()
            || themeSettings.value ("hide_combo_checkboxes").toBool() != ui->checkBoxHideComboCheckboxes->isChecked()
            || themeSettings.value ("animate_states").toBool() != ui->checkBoxAnimation->isChecked()
            || themeSettings.value ("no_window_pattern").toBool() != ui->checkBoxPattern->isChecked()
            || themeSettings.value ("no_inactiveness").toBool() != ui->checkBoxInactiveness->isChecked()
            || themeSettings.value ("left_tabs").toBool() != ui->checkBoxleftTab->isChecked()
            || themeSettings.value ("joined_inactive_tabs").toBool() != ui->checkBoxJoinTab->isChecked()
            || themeSettings.value ("scroll_arrows").toBool() == ui->checkBoxNoScrollArrow->isChecked()
            || themeSettings.value ("scrollbar_in_view").toBool() != ui->checkBoxScrollIn->isChecked()
            || themeSettings.value ("transient_scrollbar").toBool() != ui->checkBoxTransient->isChecked()
            || themeSettings.value ("transient_groove").toBool() != ui->checkBoxTransientGroove->isChecked()
            || themeSettings.value ("groupbox_top_label").toBool() != ui->checkBoxGroupLabel->isChecked()
            || qMin (qMax (themeSettings.value ("button_icon_size").toInt(), 16), 64) != ui->spinButton->value()
            || qMin (qMax (themeSettings.value ("layout_spacing").toInt(), 2), 16) != ui->spinLayout->value()
            || qMin (qMax (themeSettings.value ("layout_margin").toInt(), 2), 16) != ui->spinLayoutMargin->value()
            || qMin (qMax (themeSettings.value ("spin_button_width").toInt(), 16), 32) != ui->spinSpinBtnWidth->value()
            || qMin (qMax (themeSettings.value ("scroll_min_extent").toInt(), 16), 100) != ui->spinMinScrollLength->value()
            || qMin (qMax (themeSettings.value ("tooltip_delay").toInt(), -1), 9999) != ui->spinTooltipDelay->value())
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
        QString section;
        QRegularExpressionMatch match;
        while (!in.atEnd())
        {
            bool found = false;
            QString line = in.readLine();

            /* first, find the current section */
            if (line.indexOf (QRegularExpression ("^\\s*\\[(.*)\\]"), 0, &match) > -1)
            {
                section = match.captured (1);
                section = section.simplified();
            }

            /* modify the existing key-value pair, considering the current section */
            if (section == "Focus" && !focusKeys.isEmpty())
            {
                for (it = focusKeys.begin(); it != focusKeys.end(); ++it)
                {
                    if (line.contains (QRegularExpression ("^\\s*" + it.key() + "(?=\\s*\\=)")))
                    {
                        line = QString ("%1=%2").arg (it.key()).arg (focusKeys.value (it.key()));
                        /* it.key() shouldn't be removed because it may be repeated */
                        found = true;
                        break;
                    }
                }
            }
            if (!found && section == "Hacks" && !hackKeys.isEmpty())
            {
                for (it = hackKeys.begin(); it != hackKeys.end(); ++it)
                {
                    if (line.contains (QRegularExpression ("^\\s*" + it.key() + "(?=\\s*\\=)")))
                    {
                        line = QString ("%1=%2").arg (it.key()).arg (hackKeys.value (it.key()));
                        found = true;
                        break;
                    }
                }
            }
            if (!found && section == "%General" && !generalKeys.isEmpty())
            {
                for (it = generalKeys.begin(); it != generalKeys.end(); ++it)
                {
                    if (line.contains (QRegularExpression ("^\\s*" + it.key() + "(?=\\s*\\=)")))
                    {
                        line = QString ("%1=%2").arg (it.key()).arg (generalKeys.value (it.key()));
                        break;
                    }
                }
            }
            lines.append (line);
        }
        file.close();

        int i, j;
        if (!generalKeysMissing.isEmpty())
        {
            for (i = 0; i < lines.count(); ++i)
            { // the last section overrides the previous ones if they exist
                if (lines.at (lines.count() - i - 1).contains (QRegularExpression ("^\\s*\\[\\s*%General\\s*\\]")))
                    break;
            }
            if (i == lines.count())
            { // the section doesn't exist; add it to the end of the file
                lines << "" << QString ("[%General]");
                ++i; // the line of the created section
            }
            else i = lines.count() - i - 1; // the line of the found section
            for (j = i+1; j < lines.count(); ++j)
            { // find the next section
                if (lines.at (j).contains (QRegularExpression ("^\\s*\\[")))
                    break;
            }
            while (j-1 >= 0 && lines.at (j-1).isEmpty()) --j; // after the last key of the current section
            for (it = generalKeysMissing.begin(); it != generalKeysMissing.end(); ++it)
            { // add the missing keys to the found/created section (in the alphabetical order)
                lines.insert (j, QString ("%1=%2").arg (it.key()).arg (generalKeysMissing.value (it.key())));
                ++j;
            }
        }
        if (!hackKeysMissing.isEmpty())
        {
            for (i = 0; i < lines.count(); ++i)
            {
                if (lines.at (lines.count() - i - 1).contains (QRegularExpression ("^\\s*\\[\\s*Hacks\\s*\\]")))
                    break;
            }
            if (i == lines.count())
            {
                lines << "" << QString ("[Hacks]");
                ++i;
            }
            else i = lines.count() - i - 1;
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
        if (!focusKeysMissing.isEmpty())
        {
            for (i = 0; i < lines.count(); ++i)
            {
                if (lines.at (lines.count() - i - 1).contains (QRegularExpression ("^\\s*\\[\\s*Focus\\s*\\]")))
                    break;
            }
            if (i == lines.count())
            {
                lines << "" << QString ("[Focus]");
                ++i;
            }
            else i = lines.count() - i - 1;
            for (j = i+1; j < lines.count(); ++j)
            {
                if (lines.at (j).contains (QRegularExpression ("^\\s*\\[")))
                    break;
            }
            while (j-1 >= 0 && lines.at (j-1).isEmpty()) --j;
            for (it = focusKeysMissing.begin(); it != focusKeysMissing.end(); ++it)
            {
                lines.insert (j, QString ("%1=%2").arg (it.key()).arg (focusKeysMissing.value (it.key())));
                ++j;
            }
        }

        if (!lines.isEmpty())
        {
            if (!file.open (QIODevice::WriteOnly | QIODevice::Text))
                return;
            QTextStream out (&file);
            for (int i = 0; i < lines.count(); ++i)
                out << lines.at (i) << "\n";
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
                                      + QString ("<a href='%2'><br><i>~/.config/Kvantum/%1/<b>%1.kvconfig</b></i></a>").arg (kvconfigTheme_).arg (userConfigFile_));
            showAnimated (ui->configLabel);
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
            QStringList appList = editTxt.split (",", Qt::SkipEmptyParts);
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
    ui->appsEdit->setText ("");
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
        _kvconfigTheme_.remove (QString ("#"));
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

    ui->statusBar->showMessage (tr ("Restored the root default settings of %1")
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
    ui->reduceMenuOpacityLabel->setEnabled (!checked);
    ui->spinReduceMenuOpacity->setEnabled (!checked);
    ui->checkBoxBlurPopup->setEnabled (!checked && !ui->checkBoxBlurWindow->isChecked());
    ui->checkBoxShadowlessPopup->setEnabled (!checked);
    ui->checkBoxTrans->setEnabled (!checked);
    ui->contrastBox->setEnabled (!checked);
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
        ui->checkBoxScrollJump->setEnabled (!checked);
    }
    else
    {
        QSet<QByteArray> gtkDesktops = QSet<QByteArray>() << "gnome" << "pantheon";
        if (gtkDesktops.contains (desktop_))
        {
            //ui->labelX11Drag->setEnabled (!checked);
            //ui->comboX11Drag->setEnabled (!checked);
            //ui->checkBoxBtnDrag->setEnabled (!checked);
            ui->checkBoxIconlessBtn->setEnabled (!checked);
            ui->checkBoxIconlessMenu->setEnabled (!checked);
            //ui->checkBoxNoComposite->setEnabled (!checked);
            //ui->checkBoxShadowlessPopup->setEnabled (!ui->checkBoxNoComposite->isChecked());
            //ui->checkBoxTrans->setEnabled (!ui->checkBoxNoComposite->isChecked() && !checked);
            bool enableTrans (!ui->checkBoxNoComposite->isChecked()
                              && ui->checkBoxTrans->isChecked()
                              && !checked);
            /*ui->opaqueLabel->setEnabled (enableTrans);
            ui->opaqueEdit->setEnabled (enableTrans);
            ui->reduceOpacityLabel->setEnabled (enableTrans);
            ui->spinReduceOpacity->setEnabled (enableTrans);*/
            ui->checkBoxBlurWindow->setEnabled (enableTrans);
            ui->checkBoxBlurPopup->setEnabled (!ui->checkBoxNoComposite->isChecked()
                                               && !ui->checkBoxBlurWindow->isChecked()
                                               && !checked);
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
    class AboutDialog : public QDialog {
    public:
        explicit AboutDialog (QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags()) : QDialog (parent, f) {
            aboutUi.setupUi (this);
            aboutUi.textLabel->setOpenExternalLinks (true);

            QGraphicsOpacityEffect *opacity = new QGraphicsOpacityEffect;
            aboutUi.titleLabel->setGraphicsEffect (opacity);
            QPropertyAnimation *animation = new QPropertyAnimation (opacity, "opacity", this);
            animation->stop();
            animation->setDuration (1500);
            animation->setStartValue (0.0);
            animation->setEndValue (1.0);
            animation->start();

            QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect();
            if (qGray (QApplication::palette().color (QPalette::Window).rgb()) > 100)
                shadow->setColor (QColor (0, 0, 0, 60));
            else
                shadow->setColor (QColor (0, 0, 0, 180));
            shadow->setOffset (0.5, 3);
            shadow->setBlurRadius (15);
            aboutUi.iconLabel->setGraphicsEffect (shadow);
        }
        void setTabTexts (const QString& first, const QString& sec) {
            aboutUi.tabWidget->setTabText (0, first);
            aboutUi.tabWidget->setTabText (1, sec);
        }
        void setMainIcon (const QIcon& icn) {
            aboutUi.iconLabel->setPixmap (icn.pixmap (64, 64));
        }
        void settMainTitle (const QString& title) {
            aboutUi.titleLabel->setText (title);
        }
        void setMainText (const QString& txt) {
            aboutUi.textLabel->setText (txt);
        }
    private:
        Ui::AboutDialog aboutUi;
    };

    AboutDialog dialog (this);
    dialog.setMainIcon (QIcon::fromTheme ("kvantum", QIcon (":/Icons/kvantumpreview/data/kvantum.svg")));
    dialog.settMainTitle (QString ("<center><b><big>%1 %2</big></b></center><br>").arg (qApp->applicationName()).arg (qApp->applicationVersion()));
    dialog.setMainText ("<center> " + tr ("A tool for installing, selecting<br>and configuring <a href='https://github.com/tsujan/Kvantum'>Kvantum</a> themes") + " </center>\n<center> "
                        + tr ("Author: <a href='mailto:tsujan2000@gmail.com?Subject=My%20Subject'>Pedram Pourang (aka. Tsu Jan)</a> </center><br>"));
    dialog.setTabTexts (tr ("About Kvantum Manager"), tr ("Translators"));
    dialog.setWindowTitle (tr ("About Kvantum Manager"));
    dialog.setWindowModality (Qt::WindowModal);
    dialog.exec();
}

}
