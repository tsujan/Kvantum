/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014 <tsujan2000@gmail.com>
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

#include <QApplication>
#include <QTranslator>
#include <QLibraryInfo>

#include "KvantumPreview.h"

int main (int argc, char *argv[])
{
  QApplication::setApplicationName ("KvantumViewer");
  QApplication viewer (argc,argv);
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
  viewer.setAttribute(Qt::AA_UseHighDpiPixmaps, true);
#endif

  QStringList langs (QLocale::system().uiLanguages());
  QString lang; // bcp47Name() doesn't work under vbox
  if (!langs.isEmpty())
      lang = langs.first().replace ('-', '_');
  QTranslator qtTranslator;
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
  if (!qtTranslator.load ("qt_" + lang, QLibraryInfo::location (QLibraryInfo::TranslationsPath)))
#else
  if (!qtTranslator.load ("qt_" + lang, QLibraryInfo::path (QLibraryInfo::TranslationsPath)))
#endif
  { // shouldn't be needed
    if (!langs.isEmpty())
    {
      lang = langs.first().split (QLatin1Char ('_')).first();
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
      qtTranslator.load ("qt_" + lang, QLibraryInfo::location (QLibraryInfo::TranslationsPath));
#else
      (void)qtTranslator.load ("qt_" + lang, QLibraryInfo::path (QLibraryInfo::TranslationsPath));
#endif
    }
  }
  viewer.installTranslator (&qtTranslator);

  QTranslator KPTranslator;
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
  KPTranslator.load ("kvantumpreview_" + lang, QStringLiteral (DATADIR) + "/kvantumpreview/translations");
#else
  (void)KPTranslator.load ("kvantumpreview_" + lang, QStringLiteral (DATADIR) + "/kvantumpreview/translations");
#endif
  viewer.installTranslator (&KPTranslator);

  KvantumPreview k (NULL);
  k.show();
  QList<QTabWidget *> list = k.findChildren<QTabWidget*>();
  if (!list.isEmpty())
  {
    QTabWidget *tw = list.at (0);
    tw->tabBar()->setUsesScrollButtons (true);
  }
  QObject::connect (&viewer, &QApplication::lastWindowClosed, &viewer, &QApplication::quit);
  return viewer.exec();
}
