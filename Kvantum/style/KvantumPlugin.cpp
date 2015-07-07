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

#include "KvantumPlugin.h"
#include "Kvantum.h"

namespace Kvantum {
#if QT_VERSION < 0x050000
QStringList KvantumPlugin::keys() const
{
  return QStringList() << "Kvantum";
}
#endif

QStyle *KvantumPlugin::create(const QString &key)
{
  if (key.toLower() == "kvantum")
    return new Style;

  return 0;
}

#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(kvantum, KvantumPlugin)
#endif
}
