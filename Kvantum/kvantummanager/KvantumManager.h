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
    void copyDefaultTheme (QString name);
    void wrtieConfig();
    void restoreDefault();
    void transparency (bool checked);
    void aboutDialog();

private:
    void closeEvent (QCloseEvent *event);
    void notWritable();
    bool isThemeDir (const QString &folder);
    void updateThemeList();
    void showAnimated (QWidget *w, int duration);
    Ui::KvantumManager *ui;
    /* Remember the last opened folder */
    QString lastPath;
    /* For running Kvantum Preview */
    QProcess *process;
    QString xdg_config_home;
    /* Theme name in the kvconfig file */
    QString kvconfigTheme;
    QGraphicsOpacityEffect *effect;
    QPropertyAnimation *animation;
};

#endif // KVANTUMMANAGER_H
