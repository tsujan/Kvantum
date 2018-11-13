/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2018 <tsujan2000@gmail.com>
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

#include "KvCommand.h"
#include <QDir>
#include <QSettings>

namespace KvManager {

static const QString getHomeConfig()
{
    QString xdg_config_home;
    char * _xdg_config_home = getenv ("XDG_CONFIG_HOME");
    if (!_xdg_config_home)
        xdg_config_home = QString ("%1/.config").arg (QDir::homePath());
    else
        xdg_config_home = QString (_xdg_config_home);
    return xdg_config_home;
}

static bool isThemeDir (const QString &folderPath)
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

static bool isLightWithDarkDir (const QString &folderPath)
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

static const QStringList getAllThemes()
{
    const QString xdg_config_home = getHomeConfig();
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
                list << folder;
                if (isLightWithDarkDir (path))
                    list << (folder + "Dark");
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
                    list << folder;
                if (isLightWithDarkDir (path) && !list.contains (folder + "Dark"))
                    list << (folder + "Dark");
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
                    list << folder;
                if (isLightWithDarkDir (path) && !list.contains (folder + "Dark"))
                    list << (folder + "Dark");
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
                    && !list.contains (folder + "#"))
                {
                    rootList << folder;
                }
                if (isLightWithDarkDir (path)
                    && !list.contains (folder + "Dark")
                    && !list.contains (folder + "Dark" + "#"))
                {
                    rootList << (folder + "Dark");
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
                    && !list.contains (folder + "#")
                    // a root theme inside 'DATADIR/Kvantum/' with the same name takes priority
                    && !rootList.contains (folder))
                {
                    rootList << folder;
                }
                if (isLightWithDarkDir (path)
                    && !list.contains (folder + "Dark")
                    && !list.contains (folder + "Dark" + "#")
                    && !rootList.contains (folder + "Dark"))
                {
                    rootList << (folder + "Dark");
                }
            }
        }
    }
    if (!rootList.isEmpty())
        list << rootList;

    if (list.isEmpty() || !list.contains ("Default#"))
        list << "Default";

    return list;
}

bool setTheme (const QString& theme)
{
    QString theTheme = theme;
    const QStringList themes = getAllThemes();
    if (!themes.contains (theTheme))
    {
        if (!theTheme.endsWith("#") && themes.contains (theTheme + "#"))
            theTheme += "#";
        else return false;
    }
    QString configFile = QString ("%1/Kvantum/kvantum.kvconfig").arg (getHomeConfig());
    QSettings settings (configFile, QSettings::NativeFormat);
    if (!settings.isWritable()) return false;

    if (settings.value ("theme").toString() != theTheme)
        settings.setValue ("theme", theTheme);

    return true;
}

bool assignTheme (const QString& theme, const QStringList& apps)
{
    QString configFile = QString ("%1/Kvantum/kvantum.kvconfig").arg (getHomeConfig());
    QSettings settings (configFile, QSettings::NativeFormat);
    if (!settings.isWritable()) return false;

    if (theme.isEmpty()) // remove all assignments
        settings.remove ("Applications");
    else
    {
        QString theTheme = theme;
        const QStringList themes = getAllThemes();
        if (!themes.contains (theTheme))
        {
            if (!theTheme.endsWith("#") && themes.contains (theTheme + "#"))
                theTheme += "#";
            else return false;
        }

        settings.beginGroup ("Applications");
        if (apps.isEmpty())
            settings.remove (theTheme);
        else
            settings.setValue (theTheme, apps);
        settings.endGroup();
    }

    return true;
}

}
