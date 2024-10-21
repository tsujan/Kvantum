// Taken from QtCurve (C) Craig Drummond, 2007 - 2010

/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2021 <tsujan2000@gmail.com>
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

#ifndef SHORTCUTHANDLER_H
#define SHORTCUTHANDLER_H

#include <QObject>
#include <QSet>
#include <QList>

class QWidget;

namespace Kvantum {

class ShortcutHandler: public QObject {
    Q_OBJECT

public:
    explicit ShortcutHandler(QObject *parent=0);
    virtual ~ShortcutHandler();

    bool hasSeenAlt(const QWidget *widget) const;
    bool
    isAltDown() const {
        return itsAltDown_;
    }
    bool showShortcut(const QWidget *widget) const;

private Q_SLOTS:
    void widgetDestroyed(QObject *o);

protected:
    void updateWidget(QWidget *w);
    bool eventFilter(QObject *watched, QEvent *event);

private:
    bool itsAltDown_;
    QSet<QWidget*> itsSeenAlt_;
    QSet<QWidget*> itsUpdated_;
    QList<QWidget*> itsOpenMenus_;
};

}

#endif
