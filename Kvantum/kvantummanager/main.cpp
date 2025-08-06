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
#include "KvCommand.h"
#include <QApplication>
#include <QStyleFactory>
#include <QMessageBox>
#include <QTranslator>
#include <QLibraryInfo>
#include <QTextStream>

int main (int argc, char *argv[])
{
    const QString name = "Kvantum Manager";
    const QString version = "1.1.6";

    QStringList options;
    for (int i = 1; i < argc; ++i)
        options << QString::fromUtf8 (argv[i]);
    if (!options.isEmpty())
    {
        if (options.at (0) == "--help" || options.at (0) == "-h")
        {
            QTextStream out (stdout);
            out << "Kvantum Manager - A GUI for configuring Kvantum\n"\
                   "Usage:\n\n"\
                   "Start the GUI:\n	kvantummanager\n"\
                   "Show this help:\n	kvantummanager --help\n"\
                   "Show version information:\n	kvantummanager --version\n"\
                   "Activate a theme:\n	kvantummanager --set THEME\n"\
                   "Assign a theme to some application(s):\n	kvantummanager --assign THEME APP1 APP2 ...\n"\
                   "Remove a theme assignment:\n	kvantummanager --noAssign THEME\n"\
                   "Remove all theme assignments:\n	kvantummanager --noAssign-All\n\n"\
                   "NOTE1: With theme assignment, APP1, APP2, etc.\n"\
                   "       should be the names of executables.\n" \
                   "NOTE2: The GUI will be shown in the case of errors.\n"\
                   "NOTE3: Please close the GUI of Kvantum Manager\n"\
                   "       while using command-line options because\n"\
                   "       it will not be updated automatically!"
                << Qt::endl;
            return 0;
        }
        if (options.at (0) == "--version" || options.at (0) == "-v")
        {
            QTextStream out (stdout);
            out << name << " " << version
                << Qt::endl;
            return 0;
        }
    }

    QApplication a (argc, argv);
    a.setApplicationName (name);
    a.setApplicationVersion (version);

    if (!options.isEmpty() && options.at (0) == "--noAssign-All")
    {
        if (KvManager::assignTheme (QString(), QStringList()))
            return 0;
    }
    else if (options.count() > 1)
    {
        if (options.at (0) == "--set")
        {
            if (KvManager::setTheme (options.at (1)))
                return 0;
        }
        else if (options.at (0) == "--noAssign")
        {
            if (KvManager::assignTheme (options.at (1), QStringList()))
                return 0;
        }
        else if (options.count() > 2 && options.at (0) == "--assign")
        {
            options.removeFirst();
            const QString theme = options.at (0);
            options.removeFirst();
            if (KvManager::assignTheme (theme, options))
                return 0;
        }
    }

    QTranslator qtTranslator;
    if (qtTranslator.load ("qt_" + QLocale::system().name(), QLibraryInfo::path (QLibraryInfo::TranslationsPath)))
        a.installTranslator (&qtTranslator);

    QTranslator KMTranslator;
    if (KMTranslator.load ("kvantummanager_" + QLocale::system().name(), QStringLiteral (DATADIR) + "/kvantummanager/translations"))
        a.installTranslator (&KMTranslator);

    /* for Kvantum Manager to do its job, it should by styled by Kvantum */
    a.setAttribute (Qt::AA_DontCreateNativeWidgetSiblings, true); // for translucency
    if (!QStyleFactory::keys().contains ("kvantum"))
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
    KvManager::KvantumManager km (nullptr);

    return a.exec();
}
