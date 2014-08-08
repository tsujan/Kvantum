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
  }
  ~KvantumPreview() {}
};

#endif // KVANTUMPREVIEW_H
