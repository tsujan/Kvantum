#ifndef KVANTUMMANAGER_H
#define KVANTUMMANAGER_H

//#include <QtGui>
#include "ui_kvantummanager.h"

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
    void useTheme();
    void txtChanged (const QString &txt);
    void tabChanged (int index);

private:
    //void closeEvent (QCloseEvent *event);
    void notWritable();
    bool isThemeDir (const QString &folder);
    void updateThemeList();
    Ui::KvantumManager *ui;
};

#endif // KVANTUMMANAGER_H
