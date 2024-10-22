/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2024 <tsujan2000@gmail.com>
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
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QActionGroup>
#include <QScreen>
#include <QWindow>

class KvantumPreview : public QMainWindow, private Ui::KvantumPreviewBase {
  Q_OBJECT

public:
  KvantumPreview (QWidget *parent = nullptr) : QMainWindow (parent) {
    setupUi (this);

    QList<int> sizes; sizes << 50 << 50;
    splitter->setSizes (sizes);
    splitter_2->setSizes (sizes);
    treeWidget->sortItems (0, Qt::AscendingOrder);
    treeWidget->setAlternatingRowColors (true);
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
#if (QT_VERSION >= QT_VERSION_CHECK(6,7,0))
    connect (checkBox_7, &QCheckBox::checkStateChanged, this, &KvantumPreview::setDisabledState);
#else
    connect (checkBox_7, &QCheckBox::stateChanged, this, &KvantumPreview::setDisabledState);
#endif
    QActionGroup *aGroup = new QActionGroup (this);
    actionMenu_radio->setActionGroup (aGroup);
    actionMenu_radio1->setActionGroup (aGroup);
    pushButton_8->setMenu (menuFile);
    actionMenuButton->setMenu (menuFile);
    toolButton_8->setMenu (menuFile);

    /* set the current time and locale in date-time widgets */
    auto cur = QDateTime::currentDateTime();
    dateTimeEdit->setDateTime (cur);
    dateTimeEdit_2->setDateTime (cur);
    auto l = QLocale(locale().language(), locale().territory());
    dateTimeEdit->setLocale (l);
    dateTimeEdit_2->setLocale (l);

    /* a workaround for a bug in QPushButton with a shared menu */
    pushButton_8->setFocusPolicy(Qt::NoFocus);

    toolBar_2->addSeparator();
    QLabel *label = new QLabel ("<center><b><i>Kvantum</i></b></center>");
    toolBar_2->addWidget (label);
    toolBar_2->addSeparator();
    QComboBox *cb = new QComboBox();
    cb->addItems (QStringList() << "Kvantum" << "Qt" << "C++");
    toolBar_2->addWidget (cb);
    toolBar_2->addSeparator();
    QLineEdit *lineedit = new QLineEdit("Kvantum");
    lineedit->setPlaceholderText ("Placeholder");
    lineedit->setClearButtonEnabled (true);
    toolBar_2->addWidget (lineedit);
    toolBar_2->addSeparator();
    toolBar_2->addWidget (new QSpinBox());

    /* add a toolbar below the top ones */
    addToolBarBreak(Qt::TopToolBarArea);
    QToolBar *extraToolBar = new QToolBar (this);
    extraToolBar->setFloatable(true);
    extraToolBar->setMovable(true);
    addToolBar (extraToolBar);
    /* date-time editor with popup */
    dte_ = new QDateTimeEdit();
    dte_->setDateTime (cur);
    dte_->setLocale (l);
    dte_->setCalendarPopup (true);
    extraToolBar->addWidget (dte_);
    extraToolBar->addSeparator();
    /* progress-bar */
    QProgressBar *pb = new QProgressBar();
    pb->setValue (50);
    extraToolBar->addWidget (pb);
    /* spacer */
    QWidget *spacer = new QWidget();
    spacer->setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Preferred);
    extraToolBar->addWidget (spacer);

    /* add a progress-bar to the first cell of the table widget */
    if (tableWidget->rowCount() > 0 && tableWidget->columnCount() > 0) {
      QWidget *container = new QWidget();
      QHBoxLayout *layout = new QHBoxLayout (container);
      layout->setAlignment (Qt::AlignCenter);
      layout->setContentsMargins (1, 1, 1, 1);
      pb = new QProgressBar();
      pb->setValue (50);
      layout->addWidget (pb);
      container->setLayout (layout);
      if (auto item = tableWidget->item (0, 0))
        item->setText (QString());
      tableWidget->setCellWidget (0, 0, container);
      if (auto vh = tableWidget->verticalHeader())
        vh->setMinimumSectionSize (qMax (layout->minimumSize().height(), vh->minimumSectionSize()));
    }

    QSize ag;
    if (QWindow *win = windowHandle()) {
      if (QScreen *sc = win->screen())
        ag = sc->availableGeometry().size() - QSize (50, 70);
    }
    if (ag.isEmpty()) {
      if (QScreen *pScreen = QApplication::primaryScreen())
        ag = pScreen->availableGeometry().size() - QSize (50, 70);
    }
    if (!ag.isEmpty()) {
      int tabBarWidth = tabWidget->tabBar()->sizeHint().width() + 20;
      if (tabBarWidth > ag.width())
        tabWidget->tabBar()->setUsesScrollButtons (true);
      else
        tabWidget->tabBar()->setUsesScrollButtons (false); // to prevent tab scroll buttons at startup (-> main.cpp)
      resize (QSize (qMin (qMax (tabBarWidth,
                                 /* also, prevent toolbar arrow */
                                 qMax (size().width(),
                                       toolBar->sizeHint().width() + toolBar_2->sizeHint().width())),
                           ag.width()),
                     qMin (size().height(), ag.height())));

      splitter->setSizes (QList<int>() << 100 << 100 << 100);
    }
  }

  ~KvantumPreview() {}

private slots:
  void toggleLayout() {
    /* resetting of formats is needed for having correct text alignments */
    auto f = dateTimeEdit->displayFormat();

    if (QApplication::layoutDirection() == Qt::LeftToRight)
      QApplication::setLayoutDirection (Qt::RightToLeft);
    else
      QApplication::setLayoutDirection (Qt::LeftToRight);

    dateTimeEdit->setDisplayFormat (f);
    dateTimeEdit_2->setDisplayFormat (f);
    dte_->setDisplayFormat (f);
    dte_->setDisplayFormat (f);

    /* FIXME Why isn't the close button position
       updated after changing layout direction? */
    tabWidget_2->setTabsClosable (false);
    tabWidget_2->setTabsClosable (true);
    tabWidget_5->setTabsClosable (false);
    tabWidget_5->setTabsClosable (true);
    tabWidget_6->setTabsClosable (false);
    tabWidget_6->setTabsClosable (true);

    /* needed for having the correct size when
       the right text margin is more than the left one */
    /*for (auto &a : toolBar_2->actions()) {
      if (qobject_cast<QSpinBox*>(toolBar_2->widgetForAction (a))) {
        toolBar_2->removeAction (a);
        toolBar_2->addAction (a);
        break;
      }
    }*/
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
    pushButton_8->setFlat (checked);
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
    checkBox_8->setCheckState(static_cast<Qt::CheckState>(state));
  }

private:
  QDateTimeEdit *dte_;
};

#endif // KVANTUMPREVIEW_H
