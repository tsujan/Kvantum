#ifndef KVANTUMMANAGER_H
#define KVANTUMMANAGER_H

//#include <QtGui>
#include "ui_kvantummanager.h"
#include <QProcess>
#include <QCloseEvent>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>

namespace Ui {
class KvantumManager;
}

class KvantumManager : public QMainWindow
{
    Q_OBJECT

public:
    KvantumManager (QWidget *parent = 0);
    ~KvantumManager();

private slots:
    void openTheme();
    void installTheme();
    void deleteTheme();
    void useTheme();
    void txtChanged (const QString &txt);
    void tabChanged (int index);
    void selectionChanged (const QString &txt);
    void preview();
    bool copyRootTheme (QString source, QString target);
    void writeConfig();
    void restoreDefault();
    void isTranslucent (bool checked);
    void notCompisited (bool checked);
    void popupBlurring (bool checked);
    void showWhatsThis();
    void aboutDialog();

private:
    void closeEvent (QCloseEvent *event);
    void notWritable (const QString &path);
    bool isThemeDir (const QString &folderPath);
    QString userThemeDir (const QString &themeName);
    void updateThemeList();
    void showAnimated (QWidget *w, int duration);
    void defaultThemeButtons();
    void resizeConfPage (bool thirdPage);
    void restyleWindow();
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
            if (toStr ((Drag)i) == str)
                return (Drag)i;
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
    QString xdg_config_home;
    /* Theme name in the kvconfig file */
    QString kvconfigTheme_;
    QGraphicsOpacityEffect *effect_;
    QPropertyAnimation *animation_;
};

#endif // KVANTUMMANAGER_H
