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
#if QT_VERSION >= 0x050500
  viewer.setAttribute(Qt::AA_UseHighDpiPixmaps, true);
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
  viewer.installTranslator (&qtTranslator);

  QTranslator KPTranslator;
  KPTranslator.load ("kvantumpreview_" + lang, DATADIR "/kvantumpreview/translations");
  viewer.installTranslator (&KPTranslator);

  KvantumPreview k (NULL);
  k.show();
  QList<QTabWidget *> list = k.findChildren<QTabWidget*>();
  if (!list.isEmpty())
  {
    QTabWidget *tw = list.at (0);
#if QT_VERSION < 0x050000
    QTabBar *tb = tw->findChild<QTabBar*>(QLatin1String("qt_tabwidget_tabbar"));
    if (tb) tb->setUsesScrollButtons (true);
#else
    tw->tabBar()->setUsesScrollButtons (true);
#endif
  }
  QObject::connect (&viewer, &QApplication::lastWindowClosed, &viewer, &QApplication::quit);
  return viewer.exec();
}
