// Taken from QtCurve (C) Craig Drummond, 2007 - 2010

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

#include "shortcuthandler.h"

#include <QEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMenuBar>

namespace Kvantum {
ShortcutHandler::ShortcutHandler(QObject *parent) : QObject(parent),
                                                    itsAltDown_(false)
{
}

ShortcutHandler::~ShortcutHandler()
{
}

bool ShortcutHandler::hasSeenAlt(const QWidget *widget) const
{
    if (widget && !widget->isEnabled())
        return false;

    if (qobject_cast<const QMenu*>(widget)) {
        return itsOpenMenus_.count() && itsOpenMenus_.last()==widget;
    } else {
        return (itsOpenMenus_.isEmpty() &&
                itsSeenAlt_.contains((QWidget*)(widget->window())));
    }
    return false;
}

bool ShortcutHandler::showShortcut(const QWidget *widget) const
{
    return itsAltDown_ && hasSeenAlt(widget);
}

void ShortcutHandler::widgetDestroyed(QObject *o)
{
    itsUpdated_.remove(static_cast<QWidget *>(o));
    itsOpenMenus_.removeAll(static_cast<QWidget *>(o));
}

void ShortcutHandler::updateWidget(QWidget *w)
{
    if(!itsUpdated_.contains(w))
    {
        itsUpdated_.insert(w);
        w->update();
#if QT_VERSION < 0x050000
        connect(w, SIGNAL(destroyed(QObject *)), this, SLOT(widgetDestroyed(QObject *)));
#else
        connect(w, &QObject::destroyed, this, &ShortcutHandler::widgetDestroyed);
#endif
    }
}

bool ShortcutHandler::eventFilter(QObject *o, QEvent *e)
{
    if (!o->isWidgetType())
        return QObject::eventFilter(o, e);

    QWidget *widget = qobject_cast<QWidget*>(o);
    switch (e->type()) {
    case QEvent::KeyPress:
        if (Qt::Key_Alt == static_cast<QKeyEvent*>(e)->key()) {
            itsAltDown_ = true;
            if (qobject_cast<QMenu*>(widget)) {
                itsSeenAlt_.insert(widget);
                updateWidget(widget);
                if (widget->parentWidget() &&
                    widget->parentWidget()->window()) {
                    itsSeenAlt_.insert(widget->parentWidget()->window());
                }
            } else {
                widget = widget->window();
                itsSeenAlt_.insert(widget);
                QList<QWidget*> l = widget->findChildren<QWidget*>();
                for (int pos = 0;pos < l.size();++pos) {
                    QWidget *w = l.at(pos);
                    if (!(w->isWindow() || !w->isVisible())) {
                        updateWidget(w);
                    }
                }
                QList<QMenuBar*> m = widget->findChildren<QMenuBar*>();
                for (int i = 0; i < m.size(); ++i)
                    updateWidget(m.at(i));
            }
        }
        break;
    case QEvent::WindowDeactivate:
    case QEvent::KeyRelease:
        if (QEvent::WindowDeactivate == e->type() ||
            Qt::Key_Alt == static_cast<QKeyEvent*>(e)->key()) {
            itsAltDown_ = false;
            QSet<QWidget*>::ConstIterator it(itsUpdated_.constBegin()),
                end(itsUpdated_.constEnd());
            for (;it != end;++it)
                (*it)->update();
            if (!itsUpdated_.contains(widget))
                widget->update();
            itsSeenAlt_.clear();
            itsUpdated_.clear();
        }
        break;
    case QEvent::Show:
        if (qobject_cast<QMenu*>(widget)) {
            QWidget *prev = itsOpenMenus_.count() ? itsOpenMenus_.last() : 0L;
            itsOpenMenus_.append(widget);
            if (itsAltDown_ && prev)
                prev->update();
#if QT_VERSION < 0x050000
            connect(widget, SIGNAL(destroyed(QObject*)), this, SLOT(widgetDestroyed(QObject*)));
#else
            connect(widget, &QObject::destroyed, this, &ShortcutHandler::widgetDestroyed);
#endif
        }
        break;
    case QEvent::Hide:
        if (qobject_cast<QMenu*>(widget)) {
            itsSeenAlt_.remove(widget);
            itsUpdated_.remove(widget);
            itsOpenMenus_.removeAll(widget);
            if (itsAltDown_) {
                if (itsOpenMenus_.count()) {
                    itsOpenMenus_.last()->update();
                } else if (widget->parentWidget() &&
                           widget->parentWidget()->window())
                    widget->parentWidget()->window()->update();
            }
        }
        break;
    case QEvent::Close:
        // Reset widget when closing
        itsSeenAlt_.remove(widget);
        itsUpdated_.remove(widget);
        itsSeenAlt_.remove(widget->window());
        itsOpenMenus_.removeAll(widget);
        if (itsAltDown_) {
            if (itsOpenMenus_.count()) {
                itsOpenMenus_.last()->update();
            } else if (widget->parentWidget() &&
                       widget->parentWidget()->window())
                widget->parentWidget()->window()->update();
        }
        break;
    default:
        break;
    }
    return QObject::eventFilter(o, e);
}
}
