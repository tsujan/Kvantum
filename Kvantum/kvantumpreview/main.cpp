/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2024 <tsujan2000@gmail.com>
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
  QApplication viewer (argc, argv);
  viewer.setApplicationName ("Kvantum Preview");

  QStringList langs (QLocale::system().uiLanguages());
  QString lang; // bcp47Name() doesn't work under vbox
  if (!langs.isEmpty())
    lang = langs.first().replace ('-', '_');
  QTranslator qtTranslator;
  if (!qtTranslator.load ("qt_" + lang, QLibraryInfo::path (QLibraryInfo::TranslationsPath)))
  { // shouldn't be needed
    if (!langs.isEmpty())
    {
      lang = langs.first().split (QLatin1Char ('_')).first();
      (void)qtTranslator.load ("qt_" + lang, QLibraryInfo::path (QLibraryInfo::TranslationsPath));
    }
  }
  viewer.installTranslator (&qtTranslator);

  QTranslator KPTranslator;
  (void)KPTranslator.load ("kvantumpreview_" + lang, QStringLiteral (DATADIR) + "/kvantumpreview/translations");
  viewer.installTranslator (&KPTranslator);

  KvantumPreview k (nullptr);
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
