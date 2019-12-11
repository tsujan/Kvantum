/* Adapted from:
 * Cantata - Qt5 Graphical MPD Client for Linux, Windows, macOS, Haiku
 * File: support/monoicon.cpp
 * Copyright: 2011-2018 Craig Drummond
 * License: GPL-3.0+
 * Homepage: https://github.com/CDrummond/cantata
 */

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

#include <QFile>
#include <QIconEngine>
#include <QSvgRenderer>
#include <QPainter>
#include <QPixmapCache>
#include <QRect>
#include <QFontDatabase>
#include <QApplication>
#include <QPalette>

#include "svgicons.h"

namespace KvManager {

class symbolicIconEngine : public QIconEngine
{
public:
    symbolicIconEngine (const QString &file) : fileName (file) {}

    ~symbolicIconEngine() override {}

    symbolicIconEngine * clone() const override {
        return new symbolicIconEngine (fileName);
    }

    void paint (QPainter *painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override {
        Q_UNUSED (state)

        QColor col;
        if (mode == QIcon::Disabled)
            col = QApplication::palette().color (QPalette::Disabled, QPalette::WindowText);
        else if (mode == QIcon::Selected)
            col = QApplication::palette().highlightedText().color();
        else
            col = QApplication::palette().windowText().color();
        QString key = fileName
                      + "-" + QString::number (rect.width())
                      + "-" + QString::number (rect.height())
                      + "-" + col.name();
        QPixmap pix;
        if (!QPixmapCache::find (key, &pix))
        {
            pix = QPixmap (rect.width(), rect.height());
            pix.fill (Qt::transparent);
            if (!fileName.isEmpty())
            {
                QSvgRenderer renderer;
                QFile f (fileName);
                QByteArray bytes;
                if (f.open (QIODevice::ReadOnly))
                    bytes = f.readAll();
                if (!bytes.isEmpty())
                    bytes.replace ("#000", col.name().toLatin1());
                renderer.load (bytes);
                QPainter p (&pix);
                renderer.render (&p, QRect (0, 0, rect.width(), rect.height()));
            }
            QPixmapCache::insert (key, pix);
        }
        painter->drawPixmap (rect.topLeft(), pix);
    }

    QPixmap pixmap (const QSize& size, QIcon::Mode mode, QIcon::State state) override {
        QPixmap pix (size);
        pix.fill (Qt::transparent);
        QPainter painter (&pix);
        paint (&painter, QRect (QPoint (0, 0), size), mode, state);
        return pix;
    }

private:
    QString fileName;
};

QIcon symbolicIcon::icon (const QString& fileName) {
    return QIcon (new symbolicIconEngine (fileName));
}

}
