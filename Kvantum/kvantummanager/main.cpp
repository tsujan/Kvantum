#include "KvantumManager.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication::setApplicationName ("KvantumManager");
    QApplication a (argc, argv);
    KvantumManager km (NULL);
    km.show();

    return a.exec();
}
