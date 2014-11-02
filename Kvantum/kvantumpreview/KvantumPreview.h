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

#ifndef KVANTUMPREVIEW_H
#define KVANTUMPREVIEW_H

#include "ui_KvantumPreviewBase.h"

class KvantumPreview : public QMainWindow, private Ui::KvantumPreviewBase {
  Q_OBJECT

public:
  KvantumPreview (QWidget *parent = 0) : QMainWindow (parent)
  {
    setupUi (this);
    QList<int> sizes; sizes << 50 << 50;
    splitter->setSizes (sizes);
    //subwindow->setWindowState(Qt::WindowMaximized);
    connect (actionTest, SIGNAL (changed()), this, SLOT (toggleLayout()));
  }
  ~KvantumPreview() {}

private slots:
  void toggleLayout() {
    if (QApplication::layoutDirection () == Qt::LeftToRight)
      QApplication::setLayoutDirection (Qt::RightToLeft);
    else
      QApplication::setLayoutDirection (Qt::LeftToRight);

    /* FIXME Why isn't the close button position
       updated after changing layout direction? */
    tabWidget_2->setTabsClosable(false);
    tabWidget_2->setTabsClosable(true);
    tabWidget_5->setTabsClosable(false);
    tabWidget_5->setTabsClosable(true);
    tabWidget_6->setTabsClosable(false);
    tabWidget_6->setTabsClosable(true);
  }
};

#endif // KVANTUMPREVIEW_H
