#include "KvantumManager.h"
#include <QApplication>
#include <QStyleFactory>
#include <QMessageBox>
#include <QTranslator>
#include <QLibraryInfo>

int main(int argc, char *argv[])
{
    QApplication::setApplicationName ("KvantumManager");
    QApplication a (argc, argv);
#if QT_VERSION >= 0x050500
    a.setAttribute (Qt::AA_UseHighDpiPixmaps, true);
#endif

    QStringList langs (QLocale::system().uiLanguages());
    QString lang; // bcp47Name() doesn't work under vbox
    if (!langs.isEmpty())
        lang = langs.first().replace ('-', '_');

    QTranslator qtTranslator;
    if (!qtTranslator.load ("qt_" + lang, QLibraryInfo::location (QLibraryInfo::TranslationsPath)))
    { // not needed; doesn't happen
        if (!langs.isEmpty())
        {
            lang = langs.first().split (QLatin1Char ('-')).first();
            qtTranslator.load ("qt_" + lang, QLibraryInfo::location (QLibraryInfo::TranslationsPath));
        }
    }
    a.installTranslator (&qtTranslator);

    QTranslator KMTranslator;
    KMTranslator.load ("kvantummanager_" + lang, DATADIR "/kvantummanager/translations");
    a.installTranslator (&KMTranslator);

    /* for Kvantum Manager to do its job, it should by styled by Kvantum */
#if QT_VERSION >= 0x050000
    a.setAttribute (Qt::AA_DontCreateNativeWidgetSiblings, true); // for translucency
    if (!QStyleFactory::keys().contains ("kvantum"))
#else
    if (!QStyleFactory::keys().contains ("Kvantum"))
#endif
    {
        QMessageBox msgBox (QMessageBox::Critical,
                            QObject::tr ("Kvantum"),
                            "<center><b>" + QObject::tr ("Kvantum is not installed on your system.") + "</b></center>\n"
                            "<p><center>" + QObject::tr ("Please first install the Kvantum style plugin!") + "</center><p>",
                            QMessageBox::Close);
        msgBox.exec();
        return 1;
    }
    if (QApplication::style()->objectName() != "kvantum")
        QApplication::setStyle (QStyleFactory::create ("kvantum"));
    KvManager::KvantumManager km (lang, NULL);
    km.show();

    return a.exec();
}
