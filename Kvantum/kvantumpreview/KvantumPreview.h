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
    splitter_2->setSizes (sizes);
    treeWidget->sortItems (0, Qt::AscendingOrder);
#if QT_VERSION < 0x050000
    QTabBar *tb = tabWidget->findChild<QTabBar*>(QLatin1String("qt_tabwidget_tabbar"));
    if (tb) tb->setUsesScrollButtons (false);
#else
    tabWidget->tabBar()->setUsesScrollButtons (false);
#endif
    QPushButton *pb1 = new QPushButton (QIcon (":/Icons/data/icon.svg"), QString());
    tabWidget_2->setCornerWidget (pb1, Qt::TopRightCorner);
    QPushButton *pb2 = new QPushButton (QIcon (":/Icons/data/icon.svg"), QString());
    tabWidget_5->setCornerWidget (pb2, Qt::BottomLeftCorner);
    //subwindow->setWindowState(Qt::WindowMaximized);
    connect (actionTest, &QAction::changed, this, &KvantumPreview::toggleLayout);
    connect (actionDocMode, &QAction::toggled, this, &KvantumPreview::KvDocMode);
    connect (checkBoxDocMode, &QAbstractButton::toggled, this, &KvantumPreview::docMode);
    connect (checkBoxFlat, &QAbstractButton::toggled, this, &KvantumPreview::makeFlat);
    connect (checkBoxRaise, &QAbstractButton::toggled, this, &KvantumPreview::makeAutoRaise);
    connect (checkBox_7, &QCheckBox::stateChanged, this, &KvantumPreview::setDisabledState);
    QActionGroup *aGroup = new QActionGroup (this);
    actionMenu_radio->setActionGroup (aGroup);
    actionMenu_radio1->setActionGroup (aGroup);
    actionMenuButton->setMenu (menuFile);
    toolButton_8->setMenu (menuFile);
  }
  ~KvantumPreview() {}

private slots:
  void toggleLayout() {
    if (QApplication::layoutDirection() == Qt::LeftToRight)
      QApplication::setLayoutDirection (Qt::RightToLeft);
    else
      QApplication::setLayoutDirection (Qt::LeftToRight);

    /* FIXME Why isn't the close button position
       updated after changing layout direction? */
    tabWidget_2->setTabsClosable (false);
    tabWidget_2->setTabsClosable (true);
    tabWidget_5->setTabsClosable (false);
    tabWidget_5->setTabsClosable (true);
    tabWidget_6->setTabsClosable (false);
    tabWidget_6->setTabsClosable (true);
  }
  void KvDocMode (bool checked) {
    tabWidget->setDocumentMode (checked);
  }
  void docMode (bool checked) {
    tabWidget_2->setDocumentMode (checked);
    tabWidget_3->setDocumentMode (checked);
    tabWidget_4->setDocumentMode (checked);
    tabWidget_5->setDocumentMode (checked);
    tabWidget_6->setDocumentMode (checked);
  }
  void makeFlat (bool checked) {
    pushButton->setFlat (checked);
    pushButton_4->setFlat (checked);
    pushButton_2->setFlat (checked);
    pushButton_3->setFlat (checked);
    pushButton_5->setFlat (checked);
    pushButton_9->setFlat (checked);
    pushButton_11->setFlat (checked);
    pushButton_10->setFlat (checked);
    pushButton_12->setFlat (checked);
    pushButton_6->setFlat (checked);
    pushButton_7->setFlat (checked);
  }
  void makeAutoRaise (bool checked) {
    toolButton->setAutoRaise (checked);
    toolButton_2->setAutoRaise (checked);
    toolButton_3->setAutoRaise (checked);
    toolButton_4->setAutoRaise (checked);
    toolButton_5->setAutoRaise (checked);
    toolButton_6->setAutoRaise (checked);
    toolButton_7->setAutoRaise (checked);
    toolButton_8->setAutoRaise (checked);
    toolButton_16->setAutoRaise (checked);
    toolButton_15->setAutoRaise (checked);
    toolButton_12->setAutoRaise (checked);
    toolButton_9->setAutoRaise (checked);
    toolButton_14->setAutoRaise (checked);
    toolButton_13->setAutoRaise (checked);
    toolButton_10->setAutoRaise (checked);
    toolButton_11->setAutoRaise (checked);
    toolButton_17->setAutoRaise (checked);
    toolButton_18->setAutoRaise (checked);
  }
  void setDisabledState (int state) {
    checkBox_8->setCheckState((Qt::CheckState)state);
  }
};

#endif // KVANTUMPREVIEW_H
