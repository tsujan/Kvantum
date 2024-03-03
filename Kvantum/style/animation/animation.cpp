// Adapted from Qt

/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2016-2024 <tsujan2000@gmail.com>
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

#include "animation.h"

#include <QCoreApplication>
#include <QEvent>
#include <QVariant>

namespace Kvantum {

static const qreal ScrollBarFadeOutDuration = 500.0;
static const qreal ScrollBarFadeOutDelay = 500.0;

Animation::Animation(QObject *target) : QAbstractAnimation(target),
    delay_(0),
    duration_(-1),
    //startTime_(QTime::currentTime()),
    fps_(ThirtyFps),
    skip_(0)
{
}

Animation::~Animation()
{
}

QObject *Animation::target() const
{
  return parent();
}

int Animation::duration() const
{
  return duration_;
}

void Animation::setDuration(int duration)
{
  duration_ = duration;
}

int Animation::delay() const
{
  return delay_;
}

void Animation::setDelay(int delay)
{
  delay_ = delay;
}

/*QTime Animation::startTime() const
{
  return startTime_;
}

void Animation::setStartTime(const QTime &time)
{
  startTime_ = time;
}*/

Animation::FrameRate Animation::frameRate() const
{
  return fps_;
}

void Animation::setFrameRate(FrameRate fps)
{
  fps_ = fps;
}

void Animation::updateTarget()
{
  if (target() == nullptr) return;
  QEvent event(QEvent::StyleAnimationUpdate);
  event.setAccepted(false);
  QCoreApplication::sendEvent(target(), &event);
  if (!event.isAccepted())
    stop();
}

void Animation::start()
{
  skip_ = 0;
  QAbstractAnimation::start(QAbstractAnimation::DeleteWhenStopped);
}

bool Animation::isUpdateNeeded() const
{
  return currentTime() > delay_;
}

void Animation::updateCurrentTime(int)
{
  if (++skip_ >= fps_)
  {
    skip_ = 0;
    if (target() && isUpdateNeeded())
      updateTarget();
  }
}

/*************************/

ProgressbarAnimation::ProgressbarAnimation(QObject *target) : Animation(target),
    pixels_(0)
{
  setFrameRate(TwentyFps);
}

int ProgressbarAnimation::pixels() const {
  return pixels_;
}

void ProgressbarAnimation::updateTarget()
{
  if (pixels_ > INT_MAX - 2)
    pixels_ = 0;
  else
    pixels_ += 2;
  Animation::updateTarget();
}

/*************************/

NumberAnimation::NumberAnimation(QObject *target) :
    Animation(target),
    start_(0.0),
    end_(1.0),
    prev_(0.0)
{
  setDuration(250);
}

qreal NumberAnimation::startValue() const
{
  return start_;
}

void NumberAnimation::setStartValue(qreal value)
{
  start_ = value;
}

qreal NumberAnimation::endValue() const
{
  return end_;
}

void NumberAnimation::setEndValue(qreal value)
{
  end_ = value;
}

qreal NumberAnimation::currentValue() const
{
  qreal step = static_cast<qreal>(currentTime() - delay()) / static_cast<qreal>(duration() - delay());
  return start_ + qMax(0.0, step) * (end_ - start_);
}

bool NumberAnimation::isUpdateNeeded() const
{
  if (Animation::isUpdateNeeded())
  {
    qreal current = currentValue();
    if (!qFuzzyCompare(prev_, current))
    {
      prev_ = current;
      return true;
    }
  }
  return false;
}

/*************************/

ScrollbarAnimation::ScrollbarAnimation(Mode mode, QObject *target) :
    NumberAnimation(target),
    mode_(mode)
{
  switch (mode) {
  case Activating: // not used by Kvantum
    setDuration(ScrollBarFadeOutDuration);
    setStartValue(0.0);
    setEndValue(1.0);
    break;
  case Deactivating:
    setDuration(ScrollBarFadeOutDelay + ScrollBarFadeOutDuration);
    setDelay(ScrollBarFadeOutDelay);
    setStartValue(1.0);
    setEndValue(0.0);
    break;
  }
}

ScrollbarAnimation::Mode ScrollbarAnimation::mode() const
{
  return mode_;
}

void ScrollbarAnimation::updateCurrentTime(int time)
{
  NumberAnimation::updateCurrentTime(time);
  /* make sure the target is updated in the end */
  if (mode_ == Deactivating && qFuzzyIsNull(currentValue()))
    updateTarget();
}

}
