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
        completer()->setCompletionMode (QCompleter::PopupCompletion);
#if QT_VERSION >= 0x050200
        completer()->setFilterMode (Qt::MatchContains);
        lineEdit()->setClearButtonEnabled (true);
#endif
        connect (this, SIGNAL (currentIndexChanged (const QString&)),
                 this, SLOT (textChangedSlot (const QString&)));
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
               if no item starts with the current text */
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
