#include "KvantumManager.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication::setApplicationName ("KvantumManager");
    QApplication a (argc, argv);
#if QT_VERSION >= 0x050500
    a.setAttribute(Qt::AA_UseHighDpiPixmaps, true);
#endif
    KvantumManager km (NULL);
    km.show();

    return a.exec();
}
