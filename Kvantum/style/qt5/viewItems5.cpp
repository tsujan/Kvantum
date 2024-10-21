// Adapted from Qt -> "qcommonstyle.cpp" to control how view-items are drawn.

/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2021-2024 <tsujan2000@gmail.com>
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

#include "Kvantum5.h"

#include <QtMath>
#include <QPainter>
#include <QTextLayout>

namespace Kvantum
{

static QSizeF viewItemTextLayout(QTextLayout &textLayout,
                                int lineWidth,
                                int maxHeight = -1,
                                int *lastVisibleLine = nullptr)
{
  if (lastVisibleLine)
    *lastVisibleLine = -1;
  qreal height = 0;
  qreal widthUsed = 0;
  textLayout.beginLayout();
  int i = 0;
  while (true)
  {
    QTextLine line = textLayout.createLine();
    if (!line.isValid())
      break;
    line.setLineWidth(lineWidth);
    line.setPosition(QPointF(0, height));
    height += line.height();
    widthUsed = qMax(widthUsed, line.naturalTextWidth());
    // we assume that the height of the next line is the same as the current one
    if (maxHeight > 0 && lastVisibleLine && height + line.height() > maxHeight)
    {
      const QTextLine nextLine = textLayout.createLine();
      *lastVisibleLine = nextLine.isValid() ? i : -1;
      break;
    }
    ++i;
  }
  textLayout.endLayout();
  return QSizeF(widthUsed, height);
}

QSize Style::viewItemSize(const QStyleOptionViewItem *option, int role) const
{
  const QWidget *widget = option->widget;
  switch (role) {
  case Qt::CheckStateRole:
    if (option->features & QStyleOptionViewItem::HasCheckIndicator)
    {
      return QSize(pixelMetric(QStyle::PM_IndicatorWidth, option, widget),
                   pixelMetric(QStyle::PM_IndicatorHeight, option, widget));
    }
    break;
  case Qt::DisplayRole:
    if (option->features & QStyleOptionViewItem::HasDisplay)
    {
      QTextOption textOption;
      textOption.setWrapMode(QTextOption::WordWrap);
      QTextLayout textLayout(option->text, option->font);
      textLayout.setTextOption(textOption);
      const bool wrapText = option->features & QStyleOptionViewItem::WrapText;
      const int frameHMargin = pixelMetric(QStyle::PM_FocusFrameHMargin, option, widget) + 1;
      const int textIconSpacing = getLabelSpec(QStringLiteral("ItemView")).tispace;
      QRect bounds = option->rect;
      switch (option->decorationPosition) {
        case QStyleOptionViewItem::Left:
        case QStyleOptionViewItem::Right: {
          if (wrapText && bounds.isValid())
          {
            int width = bounds.width() - 2 * frameHMargin;
            if (option->features & QStyleOptionViewItem::HasDecoration)
              width -= (option->decorationSize.width() + textIconSpacing);
            bounds.setWidth(width);
          }
          else
            bounds.setWidth(INT_MAX/256);
          break;
        }
        case QStyleOptionViewItem::Top:
        case QStyleOptionViewItem::Bottom:
          if (wrapText)
            bounds.setWidth(bounds.isValid() ? bounds.width() - 2 * frameHMargin
                                             : option->decorationSize.width());
          else
            bounds.setWidth(INT_MAX/256);
          break;
        default:
          break;
      }

      if (wrapText && option->features & QStyleOptionViewItem::HasCheckIndicator)
        bounds.setWidth(bounds.width() - pixelMetric(QStyle::PM_IndicatorWidth) - textIconSpacing);

      const int lineWidth = bounds.width();
      const QSizeF size = viewItemTextLayout(textLayout, lineWidth);
      return QSize(qCeil(size.width()), qCeil(size.height()));
    }
    break;
  case Qt::DecorationRole:
    if (option->features & QStyleOptionViewItem::HasDecoration)
      return option->decorationSize;
    break;
  default:
      break;
  }

  return QSize(0, 0);
}

void Style::viewItemLayout(const QStyleOptionViewItem *opt,  QRect *checkRect,
                           QRect *pixmapRect, QRect *textRect, bool sizehint) const
{
  *pixmapRect = QRect(QPoint(0, 0), viewItemSize(opt, Qt::DecorationRole));
  *textRect = QRect(QPoint(0, 0), viewItemSize(opt, Qt::DisplayRole));
  *checkRect = QRect(QPoint(0, 0), viewItemSize(opt, Qt::CheckStateRole));

  const QWidget *widget = opt->widget;
  const bool hasCheck = checkRect->isValid();
  const bool hasPixmap = pixmapRect->isValid();
  const bool hasText = true; // irrelevant (see below); kept for code editing
  const bool emptyText = !textRect->isValid(); // used with horizontal items
  const int frameHMargin = pixelMetric(QStyle::PM_FocusFrameHMargin, opt, widget) + 1;
  const int frameVMargin = pixelMetric(QStyle::PM_FocusFrameVMargin, opt, widget);
  const int pixmapMargin = hasPixmap ? frameHMargin : 0;
  const int checkMargin = hasCheck ? frameHMargin : 0;
  const int x = opt->rect.left();
  const int y = opt->rect.top();
  int w = 0, h = 0;

  const int textIconSpacing = getLabelSpec(QStringLiteral("ItemView")).tispace;

  /* if there is no text, we still want a decent height
     for the size hint and the editor */
  if (!textRect->isValid())
  {
    textRect->setHeight(opt->fontMetrics.height());
    textRect->setWidth(2);
  }
  const int textMargin = hasText ? frameHMargin : 0;

  QSize pm(0, 0);
  if (hasPixmap)
    pm = pixmapRect->size();
  if (sizehint) // give enough space to the view-item, regardless of opt->rect
  {
    /* the check button width will be taken into account later */
    if (opt->decorationPosition == QStyleOptionViewItem::Left
        || opt->decorationPosition == QStyleOptionViewItem::Right)
    {
      w = textRect->width() + pm.width() + (hasPixmap && hasText ? textIconSpacing : 0)
          + 2*frameHMargin;
      h = qMax(checkRect->height(), qMax(textRect->height(), pm.height()))
          + 2*frameVMargin;
    }
    else
    {
      w = qMax(textRect->width(), pm.width())
          + 2*frameHMargin;
      h = qMax(checkRect->height(),
               pm.height() + textRect->height() + (hasPixmap && hasText ? textIconSpacing : 0))
          + 2*frameVMargin;
    }
  }
  else
  {
    w = opt->rect.width();
    h = opt->rect.height();
  }

  int cw = 0;
  QRect check;
  if (hasCheck)
  {
    cw = checkRect->width() + ((hasPixmap || hasText) ? textIconSpacing : 0);
    if (sizehint)
    {
      w += cw;
      if (opt->direction == Qt::RightToLeft)
        check.setRect(x+w-checkRect->width(), y, checkRect->width(), h);
      else // include the left margin (the exact width isn't important)
        check.setRect(x, y, checkRect->width(), h);
    }
    else
    {
      if (opt->direction == Qt::RightToLeft)
        check.setRect(x+w-checkMargin-checkRect->width(), y, checkRect->width(), h);
      else
        check.setRect(x+checkMargin, y, checkRect->width(), h);
    }
  }

  /* consider the available space when drawing vertical view-items */
  int hMargin = 0, vMargin = 0, vSpacing = 0;
  if (!sizehint
      && (opt->decorationPosition == QStyleOptionViewItem::Top
          || opt->decorationPosition == QStyleOptionViewItem::Bottom))
  {
    int hSpacing = 0;
    cw = checkRect->width();
    if (hasCheck && (hasPixmap || hasText))
    {
      hSpacing = qMin(4, textIconSpacing);
      if (opt->rect.width() - qMax(textRect->width(), pm.width()) - checkRect->width() - hSpacing > 2*frameHMargin)
      {
        hSpacing = qMin(opt->rect.width() - qMax(textRect->width(), pm.width()) - checkRect->width() - 2*frameHMargin,
                        textIconSpacing);
      }
      cw += hSpacing;
    }
    hMargin = qMin(qMax((opt->rect.width() - qMax(textRect->width(), pm.width()) - checkRect->width() - hSpacing) / 2, 0),
                   frameHMargin);
    if (hasPixmap && hasText)
    {
      vSpacing = qMin(4, textIconSpacing);
      if (opt->rect.height() - pm.height() - textRect->height() - vSpacing > 2*frameVMargin)
      {
        vSpacing = qMin(opt->rect.height() - pm.height() - textRect->height() - 2*frameVMargin,
                        textIconSpacing);
      }
    }
    vMargin = qMax((opt->rect.height() - pm.height() - textRect->height() - vSpacing) / 2, 0);
    if (hasText)
      vMargin = qMin(vMargin, frameVMargin);
  }

  QRect display;
  QRect decoration;
  switch (opt->decorationPosition) {
    case QStyleOptionViewItem::Top: {
      if (opt->direction == Qt::RightToLeft)
      {
        if (sizehint)
        {
          decoration.setRect(x,
                             y,
                             w - cw - checkMargin,
                             frameVMargin + pm.height());
          display.setRect(x,
                          y + frameVMargin + pm.height(),
                          w - cw - checkMargin,
                          h - pm.height() - frameVMargin);
        }
        else
        {
          check.setRect(x + w - hMargin - checkRect->width(),
                        y,
                        checkRect->width(),
                        h);
          decoration.setRect(x + hMargin,
                             y + vMargin,
                             w - cw - 2*hMargin,
                             pm.height());
          display.setRect(x + hMargin,
                          y + vMargin + pm.height() + vSpacing,
                          w - cw - 2*hMargin,
                          h - pm.height() - 2*vMargin - vSpacing);
        }
      }
      else
      {
        if (sizehint)
        {
          decoration.setRect(x + cw + checkMargin,
                             y,
                             w - cw - checkMargin,
                             frameVMargin + pm.height());
          display.setRect(x + cw + checkMargin,
                          y + frameVMargin + pm.height(),
                          w - cw - checkMargin,
                          h - pm.height() - frameVMargin);
        }
        else
        {
          check.setRect(x + hMargin,
                        y,
                        checkRect->width(),
                        h);
          decoration.setRect(x + cw + hMargin,
                             y + vMargin,
                             w - cw - 2*hMargin,
                             pm.height());
          display.setRect(x + cw + hMargin,
                          y + vMargin + pm.height() + vSpacing,
                          w - cw - 2*hMargin,
                          h - pm.height() - 2*vMargin - vSpacing);
        }
      }
      break;
    }
    case QStyleOptionViewItem::Bottom: {
      if (opt->direction == Qt::RightToLeft)
      {
        if (sizehint)
        {
          decoration.setRect(x,
                             y + h - frameVMargin - pm.height(),
                             w - cw - checkMargin,
                             frameVMargin + pm.height());
          display.setRect(x,
                          y,
                          w - cw - checkMargin,
                          h - pm.height() - frameVMargin);
        }
        else
        {
          check.setRect(x + w - hMargin - checkRect->width(),
                        y,
                        checkRect->width(),
                        h);
          decoration.setRect(x + hMargin,
                             y + h - vMargin - pm.height(),
                             w - cw - 2*hMargin,
                             pm.height());
          display.setRect(x + hMargin,
                          y + vMargin,
                          w - cw - 2*hMargin,
                          h - pm.height() - 2*vMargin - vSpacing);
        }
      }
      else
      {
        if (sizehint)
        {
          decoration.setRect(x + cw + checkMargin,
                             y + h - frameVMargin - pm.height(),
                             w - cw - checkMargin,
                             frameVMargin + pm.height());
          display.setRect(x + cw + checkMargin,
                          y,
                          w - cw - checkMargin,
                          h - pm.height() - frameVMargin);
        }
        else
        {
          check.setRect(x + hMargin,
                        y,
                        checkRect->width(),
                        h);
          decoration.setRect(x + cw + hMargin,
                             y + h - vMargin - pm.height(),
                             w - cw - 2*hMargin,
                             pm.height());
          display.setRect(x + cw + hMargin,
                          y + vMargin,
                          w - cw - 2*hMargin,
                          h - pm.height() - 2*vMargin - vSpacing);
        }
      }
      break;
    }
    case QStyleOptionViewItem::Left: {
      /* let the text use the right margin only if it's left aligned and there is no space */
      if (opt->direction == Qt::LeftToRight)
      { // checkbox-icon-text
        if (sizehint)
        {
          decoration.setRect(x + cw + checkMargin,
                             y,
                             pm.width() + (hasCheck ? 0 : pixmapMargin),
                             h);
          display.setRect(hasPixmap ? decoration.right() + 1 + textIconSpacing : x + textMargin + cw,
                          y,
                          w - pm.width() - frameHMargin - (hasPixmap ? textIconSpacing : 0) - cw,
                          h);
        }
        else
        {
          decoration.setRect(x+pixmapMargin+cw, y, pm.width(), h);
          if (emptyText)
          { // give both margins to the display rectangle because it can be used by a widget
            display.setRect(hasPixmap ? decoration.right() + 1 + textIconSpacing
                                      : x + checkMargin + cw,
                            y,
                            w - pm.width() - (hasPixmap ? textIconSpacing : 0) - cw
                              - (hasPixmap || hasCheck ? frameHMargin : 0),
                            h);
            break;
          }
          if ((opt->displayAlignment & Qt::AlignHCenter)
              || (opt->displayAlignment & Qt::AlignJustify)
              || textRect->width() <= w - pm.width() - 2*frameHMargin - (hasPixmap ? textIconSpacing : 0) - cw)
          {
            display.setRect(hasPixmap ? decoration.right() + 1 + textIconSpacing : x + textMargin + cw,
                            y,
                            w - pm.width() - 2*frameHMargin - (hasPixmap ? textIconSpacing : 0) - cw,
                            h);
          }
          else if (opt->displayAlignment & Qt::AlignRight)
          {
            display.setRect(hasPixmap ? decoration.right() + 1 + textIconSpacing : x + checkMargin + cw,
                            y,
                            w - pm.width() - frameHMargin - (hasPixmap ? textIconSpacing : 0) - cw
                              - (hasPixmap || hasCheck ? frameHMargin : 0),
                            h);
          }
          else
          {
            display.setRect(hasPixmap ? decoration.right() + 1 + textIconSpacing : x + textMargin + cw,
                            y,
                            w - pm.width() - frameHMargin - (hasPixmap ? textIconSpacing : 0) - cw,
                            h);
          }
        }
      }
      else
      { // text-icon-checkbox
        if (sizehint)
        {
          decoration.setRect(x + w - cw - pixmapMargin - pm.width(),
                             y,
                             pm.width() + (hasCheck ? 0 : pixmapMargin),
                             h);
          display.setRect(x,
                          y,
                          w - pm.width() - frameHMargin - (hasPixmap ? textIconSpacing : 0) - cw,
                          h);
        }
        else
        {
          decoration.setRect(x+w-pixmapMargin-cw-pm.width(), y, pm.width(), h);
          if (emptyText)
          {
            display.setRect(x,
                            y,
                            w - pm.width() - (hasPixmap ? textIconSpacing : 0) - cw
                              - (hasPixmap || hasCheck ? frameHMargin : 0),
                            h);
            break;
          }
          if ((opt->displayAlignment & Qt::AlignHCenter)
              || (opt->displayAlignment & Qt::AlignJustify)
              || textRect->width() <= w - pm.width() - 2*frameHMargin - (hasPixmap ? textIconSpacing : 0) - cw)
          {
            display.setRect(x + frameHMargin,
                            y,
                            w - pm.width() - 2*frameHMargin - (hasPixmap ? textIconSpacing : 0) - cw,
                            h);
          }
          else if (opt->displayAlignment & Qt::AlignRight)
          { // really left aligned
            display.setRect(x + frameHMargin,
                            y,
                            w - pm.width() - frameHMargin - (hasPixmap ? textIconSpacing : 0) - cw
                              - (hasPixmap || hasCheck ? frameHMargin : 0),
                            h);
          }
          else
          { // really right aligned
            display.setRect(x,
                            y,
                            w - pm.width() - frameHMargin - (hasPixmap ? textIconSpacing : 0) - cw,
                            h);
          }
        }
      }
      break;
    }
    case QStyleOptionViewItem::Right: {
      if (opt->direction == Qt::LeftToRight)
      { // checkbox-text-icon
        if (sizehint)
        {
          decoration.setRect(x + w - pixmapMargin - pm.width(),
                             y,
                             pm.width() + pixmapMargin,
                             h);
          display.setRect(x + textMargin + cw,
                          y,
                          w - pm.width() - frameHMargin - (hasPixmap ? textIconSpacing : 0) - cw,
                          h);
        }
        else
        {
          decoration.setRect(x+w-pixmapMargin-pm.width(), y, pm.width(), h);
          if (emptyText)
          {
            display.setRect(x + checkMargin + cw,
                            y,
                            w - pm.width() - (hasPixmap ? textIconSpacing + frameHMargin : 0)
                              - cw - checkMargin,
                            h);
            break;
          }
          if ((opt->displayAlignment & Qt::AlignHCenter)
              || (opt->displayAlignment & Qt::AlignJustify)
              || textRect->width() <= w - pm.width() - 2*frameHMargin - (hasPixmap ? textIconSpacing : 0) - cw)
          {
            display.setRect(x + textMargin + cw,
                            y,
                            w - pm.width() - 2*frameHMargin - (hasPixmap ? textIconSpacing : 0) - cw,
                            h);
          }
          else if (opt->displayAlignment & Qt::AlignRight)
          {
            display.setRect(x + checkMargin + cw,
                            y,
                            w - pm.width() - frameHMargin - (hasPixmap ? textIconSpacing : 0)
                              - cw - checkMargin,
                            h);
          }
          else
          {
            display.setRect(x + textMargin + cw,
                            y,
                            w - pm.width() - frameHMargin - (hasPixmap ? textIconSpacing + frameHMargin : 0) - cw,
                            h);
          }
        }
      }
      else
      { // icon-text-checkbox
        if (sizehint)
        {
          decoration.setRect(x,
                             y,
                             pm.width() + pixmapMargin,
                             h);
          display.setRect(hasPixmap ? decoration.right() + 1 + textIconSpacing : x + textMargin,
                          y,
                          w - pm.width() - frameHMargin - (hasPixmap ? textIconSpacing : 0) - cw,
                          h);
        }
        else
        {
          decoration.setRect(x+pixmapMargin, y, pm.width(), h);
          if (emptyText)
          {
            display.setRect(hasPixmap ? decoration.right() + 1 + textIconSpacing
                                      : x,
                            y,
                            w - pm.width() - (hasPixmap ? textIconSpacing + frameHMargin : 0)
                              - cw - checkMargin,
                            h);
            break;
          }
          if ((opt->displayAlignment & Qt::AlignHCenter)
              || (opt->displayAlignment & Qt::AlignJustify)
              || textRect->width() <= w - pm.width() - 2*frameHMargin - (hasPixmap ? textIconSpacing : 0) - cw)
          {
            display.setRect(hasPixmap ? decoration.right() + 1 + textIconSpacing : x + textMargin,
                            y,
                            w - pm.width() - 2*frameHMargin - (hasPixmap ? textIconSpacing : 0) - cw,
                            h);
          }
          else if (opt->displayAlignment & Qt::AlignRight)
          { // really left aligned
            display.setRect(hasPixmap ? decoration.right() + 1 + textIconSpacing : x + textMargin,
                            y,
                            w - pm.width() - frameHMargin - (hasPixmap ? textIconSpacing : 0)
                              - cw - checkMargin,
                            h);
          }
          else
          { // really right aligned
            display.setRect(hasPixmap ? decoration.right() + 1 + textIconSpacing : x,
                            y,
                            w - pm.width() - (hasPixmap ? textIconSpacing + frameHMargin : 0) - frameHMargin - cw,
                            h);
          }
        }
      }
      break;
    }
    default:
      /* the decoration position is invalid */
      decoration = *pixmapRect;
      break;
  }

  if (!sizehint)
  { // for painting
    *checkRect = QStyle::alignedRect(opt->direction, Qt::AlignCenter,
                                     checkRect->size(), check);
    *pixmapRect = QStyle::alignedRect(opt->direction, opt->decorationAlignment,
                                      pixmapRect->size(), decoration);
    if (opt->showDecorationSelected)
      *textRect = display; // the text takes all available space
    else
      *textRect = QStyle::alignedRect(opt->direction, opt->displayAlignment,
                                      textRect->size().boundedTo(display.size()), display);
  }
  else
  { // for getting the sizes
    *checkRect = check;
    *pixmapRect = decoration;
    *textRect = display;
  }
}

QString Style::calculateElidedText(const QString &text,
                                   const QTextOption &textOption,
                                   const QFont &font,
                                   const QRect &textRect,
                                   const Qt::Alignment valign,
                                   Qt::TextElideMode textElideMode,
                                   int flags,
                                   bool lastVisibleLineShouldBeElided,
                                   QPointF *paintStartPosition) const
{
  QTextLayout textLayout(text, font);
  textLayout.setTextOption(textOption);

  // In AlignVCenter mode when more than one line is displayed and the height only allows
  // some of the lines, it makes no sense to display those. From a users perspective it makes
  // more sense to see the start of the text instead of something inbetween.
  const bool vAlignmentOptimization = paintStartPosition && valign.testFlag(Qt::AlignVCenter);

  int lastVisibleLine = -1;
  viewItemTextLayout(textLayout,
                     textRect.width(),
                     vAlignmentOptimization ? textRect.height() : -1,
                     &lastVisibleLine);

  const QRectF boundingRect = textLayout.boundingRect();
  // don't care about LTR/RTL here, only need the height
  const QRect layoutRect = QStyle::alignedRect(Qt::LayoutDirectionAuto, valign,
                                               boundingRect.size().toSize(), textRect);

  if (paintStartPosition)
      *paintStartPosition = QPointF(textRect.x(), layoutRect.top());

  QString ret;
  qreal height = 0;
  const int lineCount = textLayout.lineCount();
  for (int i = 0; i < lineCount; ++i)
  {
    const QTextLine line = textLayout.lineAt(i);
    height += line.height();

    // above visible rect
    if (height + layoutRect.top() <= textRect.top())
    {
      if (paintStartPosition)
        paintStartPosition->ry() += line.height();
      continue;
    }

    const int start = line.textStart();
    const int length = line.textLength();
    const bool drawElided = line.naturalTextWidth() > textRect.width();
    bool elideLastVisibleLine = lastVisibleLine == i;
    if (!drawElided && i + 1 < lineCount && lastVisibleLineShouldBeElided)
    {
      const QTextLine nextLine = textLayout.lineAt(i + 1);
      const int nextHeight = height + nextLine.height() / 2;
      // elide when less than the next half line is visible
      if (nextHeight + layoutRect.top() > textRect.height() + textRect.top())
        elideLastVisibleLine = true;
    }

    QString text = textLayout.text().mid(start, length);
    if (drawElided || elideLastVisibleLine)
    {
      if (elideLastVisibleLine)
      {
        if (text.endsWith(QChar::LineSeparator))
            text.chop(1);
        text += QChar(0x2026);
      }
      QFontMetricsF fm(font);
      ret += fm.elidedText(text, textElideMode, textRect.width(), flags);

      // No newline for the last (visible or real) line.
      // Sometimes drawElided is true but no eliding is done so the text
      // ends with QChar::LineSeparator -- don't add another one.
      if (i < lineCount - 1 && !ret.endsWith(QChar::LineSeparator))
        ret += QChar::LineSeparator;
    }
    else
      ret += text;

    // below visible text, can stop
    if ((height + layoutRect.top() >= textRect.bottom())
        || (lastVisibleLine >= 0 && lastVisibleLine == i))
    {
        break;
    }
  }
  return ret;
}

void Style::viewItemDrawText(QPainter *p, const QStyleOptionViewItem *option, const QRect &rect) const
{
  const bool wrapText = option->features & QStyleOptionViewItem::WrapText;
  QTextOption textOption;
  textOption.setWrapMode(wrapText ? QTextOption::WordWrap : QTextOption::ManualWrap);
  textOption.setTextDirection(option->direction);
  textOption.setAlignment(QStyle::visualAlignment(option->direction, option->displayAlignment));

  QPointF paintPosition;
  const QString newText = calculateElidedText(option->text,
                                              textOption,
                                              option->font,
                                              rect,
                                              option->displayAlignment,
                                              option->textElideMode,
                                              0,
                                              true,
                                              &paintPosition);

  QTextLayout textLayout(newText, option->font);
  textLayout.setTextOption(textOption);
  viewItemTextLayout(textLayout, rect.width());
  textLayout.draw(p, paintPosition);
}

void Style::drawViewItem(const QStyleOption *option,
                         QPainter *painter,
                         const QWidget *widget) const
{
  if (const QStyleOptionViewItem *opt = qstyleoption_cast<const QStyleOptionViewItem*>(option))
  {
    painter->save();
    painter->setClipRect(opt->rect);

    QRect checkRect = subElementRect(SE_ItemViewItemCheckIndicator, opt, widget);
    QRect iconRect = subElementRect(SE_ItemViewItemDecoration, opt, widget);
    QRect textRect = subElementRect(SE_ItemViewItemText, opt, widget);

    /* first draw the background */
    drawPrimitive(PE_PanelItemViewItem, opt, painter, widget);

    /* draw the check mark */
    if (opt->features & QStyleOptionViewItem::HasCheckIndicator)
    {
      QStyleOptionViewItem o(*opt);
      o.rect = checkRect;
      o.state = o.state & ~QStyle::State_HasFocus;
      switch (opt->checkState) {
        case Qt::Unchecked:
          o.state |= QStyle::State_Off;
          break;
        case Qt::PartiallyChecked:
          o.state |= QStyle::State_NoChange;
          break;
        case Qt::Checked:
          o.state |= QStyle::State_On;
          break;
      }
      drawPrimitive(QStyle::PE_IndicatorCheckBox, &o, painter, widget);
    }

    /* draw the icon */
    QIcon::Mode mode = QIcon::Normal;
    if (!(opt->state & QStyle::State_Enabled))
      mode = QIcon::Disabled;
    else if (opt->state & QStyle::State_Selected)
      mode = QIcon::Selected;
    QIcon::State state = opt->state & QStyle::State_Open ? QIcon::On : QIcon::Off;
    opt->icon.paint(painter, iconRect, opt->decorationAlignment, mode, state);

    /* draw the text */
    if (!opt->text.isEmpty())
    {
      QPalette::ColorGroup cg = opt->state & QStyle::State_Enabled ?
                                QPalette::Normal : QPalette::Disabled;
      /* QCommonStyle checks QStyle::State_Active,
         which isn't consistent with inactive base color */
      if (cg == QPalette::Normal && isWidgetInactive(widget))
          cg = QPalette::Inactive;

      if (opt->state & QStyle::State_Selected)
        painter->setPen(opt->palette.color(cg, QPalette::HighlightedText));
      else
        painter->setPen(opt->palette.color(cg, QPalette::Text));
      if (opt->state & QStyle::State_Editing)
      {
        painter->setPen(opt->palette.color(cg, QPalette::Text));
        painter->drawRect(textRect.adjusted(0, 0, -1, -1));
      }
      viewItemDrawText(painter, opt, textRect);
    }

    /* draw the focus rect */
    if (opt->state & QStyle::State_HasFocus)
    {
      QStyleOptionFocusRect o;
      o.QStyleOption::operator=(*opt);
      o.rect = subElementRect(SE_ItemViewItemFocusRect, opt, widget);
      drawPrimitive(QStyle::PE_FrameFocusRect, &o, painter, widget);
    }

    painter->restore();
  }
}

}
