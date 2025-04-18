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

#ifndef KVANTUMMANAGER_H
#define KVANTUMMANAGER_H

//#include <QtGui>
#include "ui_kvantummanager.h"
#include <QProcess>
#include <QCloseEvent>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>

namespace KvManager {

namespace Ui {
class KvantumManager;
}

class KvantumManager : public QMainWindow
{
    Q_OBJECT

public:
    KvantumManager (QWidget *parent = nullptr);
    ~KvantumManager();

protected:
    void closeEvent (QCloseEvent *event);
    bool eventFilter (QObject *watched, QEvent *event);

private slots:
    void openTheme();
    void installTheme();
    void deleteTheme();
    void useTheme();
    void txtChanged (const QString &txt);
    void tabChanged (int index);
    void selectionChanged (int index);
    void assignAppTheme (const QString &previousTheme, const QString &newTheme);
    void preview();
    bool copyRootTheme (QString source, QString target);
    void writeConfig();
    void writeAppLists();
    void removeAppList();
    void restoreDefault();
    void isTranslucent (bool checked);
    void notCompisited (bool checked);
    void comboMenu (bool checked);
    void popupBlurring (bool checked);
    void respectDE (bool checked);
    void trantsientScrollbarEnbled (bool checked);
    void showWhatsThis();
    void aboutDialog();
    void openUserConfigFile (const QString &link);
    void setTabWidgetFocus();

private:
    void fitConfPageToContents();
    QString tooTipToWhatsThis (const QString &tip);
    void notWritable (const QString &path);
    void canNotBeRemoved (const QString &path, bool isDir);
    bool isThemeDir (const QString &folderPath) const;
    bool fileBelongsToThemeDir (const QString &fileBaseName, const QString &folderPath) const;
    QString userThemeDir (const QString &themeName) const;
    QString rootThemeDir (const QString &themeName) const;
    bool isLightWithDarkDir (const QString &folderPath) const;
    void updateThemeList (bool updateAppThemes = true);
    void showAnimated (QWidget *w, int type = -1, int duration = 0);
    void defaultThemeButtons();
    void restyleWindow();
    void writeOrigAppLists();
    QString getComment (const QString &comboText, bool setState = true);
    // to be independent of '../style/drag/windowmanager.h'
    enum Drag {
        DRAG_NONE,
        DRAG_MENUBAR_ONLY,
        DRAG_MENUBAR_AND_PRIMARY_TOOLBAR,
        DRAG_ALL,

        DRAG_COUNT
    };
    Drag toDrag (const QString &str) {
        for (int i = 0; i < DRAG_COUNT; ++i) {
            if (toStr (static_cast<Drag>(i)) == str)
                return static_cast<Drag>(i);
        }
        // backward compatibility
        return (str == "true" || str == "1") ? DRAG_ALL : DRAG_NONE;
    }
    QString toStr (Drag drag) {
        switch (drag) {
            default:
            case DRAG_ALL: return "all";
            case DRAG_NONE: return "none";
            case DRAG_MENUBAR_ONLY: return "menubar";
            case DRAG_MENUBAR_AND_PRIMARY_TOOLBAR: return "menubar_and_primary_toolbar";
        }
    }

    Ui::KvantumManager *ui;
    /* Remember the last opened folder */
    QString lastPath_;
    /* For running Kvantum Preview */
    QProcess *process_;
    /* The user condig directory */
    QString xdg_config_home;
    /* Theme name in the kvconfig file */
    QString kvconfigTheme_;
    /* Animations */
    QGraphicsOpacityEffect *effect_;
    QPropertyAnimation *animation_;
    QSet<QString> animatedWidgets_;
    /* For DE-specific settings */
    QByteArray desktop_;

    QHash<QString, QStringList> appThemes_, origAppThemes_;
    bool confPageVisited_;
    QString modifiedSuffix_;
    QString kvDefault_;
    QString userConfigFile_;

    bool centerDefaultDocTabs_, centerDefaultNormalTabs_;
};

}

#endif // KVANTUMMANAGER_H
