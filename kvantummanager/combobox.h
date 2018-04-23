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

#ifndef COMBOBOX_H
#define COMBOBOX_H

#include <QComboBox>
#include <QCompleter>
#if QT_VERSION >= 0x050200
#include <QLineEdit>
#endif

namespace KvManager {

class ComboBox : public QComboBox
{
    Q_OBJECT
public:
    ComboBox (QWidget *parent = NULL) : QComboBox (parent)
    {
        setEditable (true);
        setInsertPolicy (QComboBox::NoInsert);
        completer()->setCompletionMode (QCompleter::PopupCompletion);
#if QT_VERSION >= 0x050200
        completer()->setFilterMode (Qt::MatchContains);
        lineEdit()->setClearButtonEnabled (true);
#endif
#if QT_VERSION >= 0x050700
        connect (this, QOverload<const QString &>::of(&QComboBox::currentIndexChanged),
                 this, &ComboBox::textChangedSlot);
#else
        connect (this, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::currentIndexChanged),
                 this, &ComboBox::textChangedSlot);
#endif
    }

signals:
    void textChangedSignal (const QString &oldText, const QString &newText);

private slots:
    void textChangedSlot (const QString &newText)
    {
        emit textChangedSignal (oldText_, newText);
        oldText_ = newText;
    }

protected:
    void focusOutEvent (QFocusEvent *e)
    {
        if (currentText().isEmpty())
        {
            if (currentIndex() > -1)
                setCurrentIndex (currentIndex());
        }
        else
        {
            /* return to the current index on focusing out
               if no item contains the current text */
#if QT_VERSION >= 0x050200
            int indx = findText (currentText(), Qt::MatchContains);
#else
            int indx = findText (currentText(), Qt::MatchStartsWith);
#endif
            if (indx == -1)
            {
                if (currentIndex() > -1)
                    setCurrentIndex (currentIndex());
            }
            else // needed because of the popup
                setCurrentIndex (indx);
        }
        QComboBox::focusOutEvent (e);
    }

private:
    QString oldText_;
};

}

#endif // COMBOBOX_H
