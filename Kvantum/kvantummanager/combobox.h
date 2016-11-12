#ifndef COMBOBOX_H
#define COMBOBOX_H

#include <QComboBox>

namespace KvManager {

class ComboBox : public QComboBox
{
    Q_OBJECT
public:
    ComboBox (QWidget *parent = NULL) : QComboBox (parent)
    {
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

private:
    QString oldText_;  
};

}

#endif // COMBOBOX_H
