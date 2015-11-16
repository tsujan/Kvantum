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
    void copyRootTheme (QString source, QString target);
    void writeConfig();
    void restoreDefault();
    void isTranslucent (bool checked);
    void notCompisited (bool checked);
    void popupBlurring (bool checked);
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
