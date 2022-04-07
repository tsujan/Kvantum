/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2018-2022 <tsujan2000@gmail.com>
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
#include <QLineEdit>

namespace KvManager {

class ComboBox : public QComboBox
{
    Q_OBJECT
public:
    ComboBox (QWidget *parent = nullptr) : QComboBox (parent)
    {
        setEditable (true);
        setInsertPolicy (QComboBox::NoInsert);
        completer()->setCompletionMode (QCompleter::PopupCompletion);
        completer()->setFilterMode (Qt::MatchContains);
        lineEdit()->setClearButtonEnabled (true);
        connect (this, QOverload<int>::of(&QComboBox::currentIndexChanged),
                 this, &ComboBox::textChangedSlot);
    }

signals:
    void textChangedSignal (const QString &oldText, const QString &newText);

private slots:
    void textChangedSlot (int /*index*/)
    {
        QString newText = currentText();
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
            int indx = findText (currentText(), Qt::MatchContains);
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
