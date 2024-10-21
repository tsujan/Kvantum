// Adapted from Qt

/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2016-2023 <tsujan2000@gmail.com>
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

#ifndef ANIMATION_H
#define ANIMATION_H

#include <QAbstractAnimation>
#include <QDateTime>

namespace Kvantum {

class Animation : public QAbstractAnimation
{
  Q_OBJECT

public:
  Animation(QObject *target);
  virtual ~Animation();

  QObject *target() const;

  int duration() const override;
  void setDuration(int duration);

  int delay() const;
  void setDelay(int delay);

  /*QTime startTime() const;
  void setStartTime(const QTime &time);*/

  /* Qt's doc isn't specific on this but it says that
     there are "normally" 60 updates per second. */
  enum FrameRate {
      DefaultFps,
      SixtyFps,
      ThirtyFps,
      TwentyFps
  };

  FrameRate frameRate() const;
  void setFrameRate(FrameRate fps);

  virtual void updateTarget();

public Q_SLOTS:
  void start();

protected:
  virtual void updateCurrentTime(int time) override;
  virtual bool isUpdateNeeded() const;

private:
  int delay_;
  int duration_;
  //QTime startTime_;
  FrameRate fps_;
  int skip_;
};

/*************************/

class ProgressbarAnimation : public Animation
{
    Q_OBJECT

public:
  ProgressbarAnimation(QObject *target);

  void updateTarget() override;

  int pixels() const;

private:
    int pixels_;
};

/*************************/

class NumberAnimation : public Animation
{
  Q_OBJECT

public:
  NumberAnimation(QObject *target);

  qreal startValue() const;
  void setStartValue(qreal value);

  qreal endValue() const;
  void setEndValue(qreal value);

  qreal currentValue() const;

protected:
  bool isUpdateNeeded() const override;

private:
  qreal start_;
  qreal end_;
  mutable qreal prev_;
};

/*************************/

class ScrollbarAnimation : public NumberAnimation
{
  Q_OBJECT

public:
  enum Mode {
      Activating,
      Deactivating
  };

  ScrollbarAnimation(Mode mode, QObject *target);

  Mode mode() const;

private slots:
  void updateCurrentTime(int time) override;

private:
  Mode mode_;
};

}

#endif // ANIMATION_H
