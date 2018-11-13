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

#include "Kvantum.h"

#include <QSvgRenderer>
#include <QApplication>
#include <QPixmapCache>
#include <QPainter>

namespace Kvantum
{

/* Here, instead of using the render() method of QSvgRenderer
   directly, we first make a QPixmap for drawing SVG elements. */
static inline void drawSvgElement(QSvgRenderer *renderer, QPainter *painter, QRect bounds, QString element,
                                  qreal pixelRatio)
{
  QPixmap pixmap = QPixmap((QSizeF(bounds.size())*pixelRatio).toSize());
  pixmap.fill(QColor(Qt::transparent));
  QPainter p;
  p.begin(&pixmap);
  renderer->render(&p,element);
  p.end();
  painter->drawPixmap(bounds,pixmap,pixmap.rect());
}

bool Style::renderElement(QPainter *painter,
                          const QString &element,
                          const QRect &bounds,
                          int hsize, int vsize, // pattern sizes
                          bool usePixmap // first make a QPixmap for drawing
                         ) const
{
  if (element.isEmpty() || !bounds.isValid() || painter->opacity() == 0)
    return true;

  QSvgRenderer *renderer = nullptr;
  QString _element(element);

  if (themeRndr_ && themeRndr_->isValid()
      && (themeRndr_->elementExists(_element)
          || themeRndr_->elementExists(_element.remove("-inactive"))
          // fall back to the normal state if other states aren't found
          || themeRndr_->elementExists(_element.replace("-toggled","-normal")
                                               .replace("-pressed","-normal")
                                               .replace("-focused","-normal"))))
  {
    renderer = themeRndr_;
  }
  /* always use the default SVG image (which doesn't contain
     any object for the inactive state) as fallback */
  else if (defaultRndr_ && defaultRndr_->isValid())
  {
    _element = element;
    if (defaultRndr_->elementExists(_element.remove("-inactive"))
        // even the default theme may not have all states
        || defaultRndr_->elementExists(_element.replace("-toggled","-normal")
                                               .replace("-pressed","-normal")
                                               .replace("-focused","-normal")))
    {
      renderer = defaultRndr_;
    }
  }
  if (!renderer) return false;

  if (static_cast<qreal>(qRound(pixelRatio_)) != pixelRatio_)
  { // in this special case, we prevent one-pixel gaps between rectangles as far as possible
    painter->save();
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    usePixmap = true;
  }

  if (hsize < 0) // means no tiling pattern (for windows/dialogs)
  {
    if (renderer->elementExists(_element+"-pattern"))
    {
      if (usePixmap)
        drawSvgElement(renderer,painter,bounds,_element,pixelRatio_);
      else
        renderer->render(painter,_element,bounds);
    }
    else if (hsize == -2) // translucency without overlay pattern
    {
      QColor wc = QApplication::palette().color(QPalette::Window);
      wc.setAlpha(240);
      painter->fillRect(bounds, wc);
    }
  }
  else if (hsize > 0 || vsize > 0)
  {
    /* draw the pattern over the background
       if a separate pattern element exists */
    if (renderer->elementExists(_element+"-pattern"))
    {
      if (usePixmap)
        drawSvgElement(renderer,painter,bounds,_element,pixelRatio_);
      else
        renderer->render(painter,_element,bounds);
      _element = _element+"-pattern";
    }

    int width = hsize > 0 ? hsize : bounds.width();
    int height = vsize > 0 ? vsize : bounds.height();
    QString str = QString("%1-%2-%3").arg(_element)
                                     .arg(QString().setNum(width))
                                     .arg(QString().setNum(height));
    QPixmap pixmap;
    if (!QPixmapCache::find(str, &pixmap))
    {
      pixmap = QPixmap(width, height);
      pixmap.fill(QColor(Qt::transparent));
      QPainter p;
      p.begin(&pixmap);
      renderer->render(&p,_element);
      p.end();
      QPixmapCache::insert(str, pixmap);
    }
    painter->drawTiledPixmap(bounds,pixmap);
  }
  else
  {
    if (usePixmap)
      drawSvgElement(renderer,painter,bounds,_element,pixelRatio_);
    else
      renderer->render(painter,_element,bounds);
  }

  if (static_cast<qreal>(qRound(pixelRatio_)) != pixelRatio_)
    painter->restore();

  return true;
}

void Style::renderSliderTick(QPainter *painter,
                             const QString &element,
                             const QRect &ticksRect,
                             const int interval,
                             const int available,
                             const int min,
                             const int max,
                             bool above,
                             bool inverted) const
{
  if (!ticksRect.isValid())
    return;

  QSvgRenderer *renderer = 0;
  QString _element(element);

  if (themeRndr_ && themeRndr_->isValid()
      && (themeRndr_->elementExists(_element)
          || (_element.contains("-inactive")
              && themeRndr_->elementExists(_element.remove("-inactive")))))
  {
    renderer = themeRndr_;
  }
  else if (defaultRndr_ && defaultRndr_->isValid()
           && defaultRndr_->elementExists(_element.remove("-inactive")))
  {
    renderer = defaultRndr_;
  }
  else
    return;

  if (interval < 1) return;

  int thickness = 1;
  int len = pixelMetric(PM_SliderLength);
  int x = ticksRect.x();
  int y = ticksRect.y();
  if (!above)
  {
    painter->save();
    QTransform m;
    m.translate(2*x+ticksRect.width(), 0);
    m.scale(-1,1);
    painter->setTransform(m, true);
  }
  int current = min;
  while (current <= max)
  {
    const int position = sliderPositionFromValue(min,max,current,available,inverted) + len/2;
    renderer->render(painter,_element,QRect(x,
                                            y+position,
                                            SLIDER_TICK_SIZE,
                                            thickness));

    current += interval;
  }
  if (!above)
    painter->restore();
}

void Style::renderFrame(QPainter *painter,
                        const QRect &bounds, // frame bounds
                        frame_spec fspec, // frame spec
                        const QString &element, // frame SVG element
                        int d, // distance of the attached tab from the edge
                        int l, // length of the attached tab
                        int f1, // width of tab's left frame
                        int f2, // width of tab's right frame
                        int tp, // tab position
                        bool grouped, // is among grouped similar widgets?
                        bool usePixmap, // first make a QPixmap for drawing
                        bool drawBorder // draw a border with maximum rounding if possible
                       ) const
{
  if (!bounds.isValid() || !fspec.hasFrame || painter->opacity() == 0)
    return;

  int x0,y0,x1,y1,w,h;
  bounds.getRect(&x0,&y0,&w,&h);
  /* for "historical" reasons, we have to add 1
     (-> QRect documentation) */
  x1 = bounds.bottomRight().x() + 1;
  y1 = bounds.bottomRight().y() + 1;

  int Left,Top,Right,Bottom;
  Left = Top = Right = Bottom = 0;

  bool isInactive(false);
  QString state;
  QStringList list = element.split("-");
  int count = list.count();
  if (count > 2 && list.at(count - 1) == "inactive")
  {
    state = "-" + list.at(count - 2);
    isInactive = true;
  }
  else if (count > 1)
  {
    state = "-" + list.at(count - 1);
    QStringList states;
    states << "-normal" << "-focused" << "-pressed" << "-toggled";
    if (!states.contains(state))
      state = QString();
  }

  // search for expanded frame element
  QString realElement = fspec.expandedElement;
  if (fspec.expansion <= 0 || realElement.isEmpty())
    realElement = element;
  else if (!state.isEmpty())
  {
    realElement += state;
    if (isInactive)
      realElement += "-inactive";
  }
  else if (element.endsWith("-default")) // default button
    realElement += "-default";
  else if (element.endsWith("-focus")) // focus element
    realElement += "-focus";

  QString element1(realElement); // the element that will be drawn
  QString element0(realElement); // used just for checking
  element0 = "expand-"+element0;
  bool isHAttached(fspec.isAttached && fspec.HPos != 2);
  if (isHAttached)
    grouped = true;
  int e = grouped ? h : qMin(h,w);
  bool drawExpanded = false;

  /* WARNING: The following conditions cover all cases
              and should not be changed without due care. */

  /* still round the corners if the "expand-" element is found */
  if (fspec.expansion > 0
      && ((e <= fspec.expansion && (isHAttached ? 2*w >= h : (!grouped || w >= h)))
          || (themeRndr_ && themeRndr_->isValid()
              && (themeRndr_->elementExists(element0.remove("-inactive"))
                  // fall back to the normal state
                  || (!state.isEmpty()
                      && themeRndr_->elementExists(element0.replace(state,"-normal")))))))
  {
    drawExpanded = true; // can change below
    fspec.left = fspec.leftExpanded;
    fspec.right = fspec.rightExpanded;
    fspec.top = fspec.topExpanded;
    fspec.bottom = fspec.bottomExpanded;
  }
  if (!isLibreoffice_ && fspec.expansion > 0 && drawExpanded
      && (!fspec.isAttached || fspec.VPos == 2)
      && (h <= 2*w || (fspec.HPos != 1 && fspec.HPos != -1)))
  {
    bool topElementMissing(!drawBorder);
    /* find the element that should be drawn (element1) */
    element0 = "border-"+realElement;
    if (drawBorder && themeRndr_ && themeRndr_->isValid()
        && (themeRndr_->elementExists(element0.remove("-inactive")+"-top")
            || (!state.isEmpty() && themeRndr_->elementExists(element0.replace(state,"-normal")+"-top"))))
    {
      element1 = element0;
      if (isInactive)
        element1 = element1 + "-inactive";
    }
    else
    {
      element0 = "expand-"+realElement;
      if (themeRndr_ && themeRndr_->isValid()
          && (themeRndr_->elementExists(element0.remove("-inactive")+"-top")
              || (!state.isEmpty() && themeRndr_->elementExists(element0.replace(state,"-normal")+"-top"))))
      {
        element1 = element0;
        if (isInactive)
          element1 = element1 + "-inactive";
        drawBorder = false;
      }
      else
      {
        drawBorder = false; // don't waste CPU time
        topElementMissing = true;
      }
    }

    /* find the main sizes for drawing expanded frames */
    e = qMin(e,fspec.expansion);
    int H = h;
    if (grouped) H = e;
    if (!isHAttached)
    {
      /* to get smoother gradients, we use QTransform in this special case
         but not when the rect is grouped (as in grouped toolbuttons inside
         vertical toolbars or small progressbar indicators) */
      if (h > w && !grouped && !topElementMissing)
      {
        QRect r;
        r.setRect(y0, x0, h, w);
        QTransform m;
        m.scale(-1,1);
        m.rotate(90);
        painter->save();
        painter->setTransform(m, true);
        renderFrame(painter,r,fspec,realElement,d,l,f1,f2,tp,grouped,usePixmap);
        painter->restore();
        return;
      }
      if (h > w/* && grouped*/)
        e = qMin(e,w);  // only here e may be greater than w
      if (e%2 == 0)
      {
        Left = Top = Right = Bottom = e/2;
      }
      else
      {
        Left = Top = (e+1)/2;
        Right = Bottom = (e-1)/2;
      }
    }
    else
    {
      int X = 0;
      /* here, this is always true: (H <= 2*w || fspec.HPos == 0) */
      if (H%2 == 0)
      {
        X = Top = Bottom = H/2;
      }
      else
      {
        X = Top = (H+1)/2;
        Bottom = (H-1)/2;
      }
      if (fspec.HPos == -1)
      {
        Left = X;
        Right = qMin(fspec.right,w/2);
      }
      else if (fspec.HPos == 1)
      {
        Right = X;
        Left = qMin(fspec.left,w/2);
      }
    }
  }
  else
  {
    element1 = element;
    drawBorder = false;
    drawExpanded = false;
    Left = fspec.left;
    Right = fspec.right;
    Top = fspec.top;
    Bottom = fspec.bottom;

    /* extreme cases */
    if (fspec.left + fspec.right > w)
    {
      if (isHAttached)
      {
        if (fspec.HPos == -1)
        {
          if (fspec.left > w) Left = w;
        }
        else if (fspec.HPos == 1)
        {
          if (fspec.right > w) Right = w;
        }
      }
      else
      {
        if (w%2 == 0)
        {
          Left = Right = w/2;
        }
        else
        {
          Left = (w+1)/2;
          Right = (w-1)/2;
        }
      }
    }
    if (fspec.top + fspec.bottom > h)
    {
      if (fspec.isAttached && fspec.VPos != 2)
      {
        if (fspec.VPos == -1)
        {
          if (fspec.top > h) Top = h;
        }
        else if (fspec.VPos == 1)
        {
          if (fspec.bottom > h) Bottom = h;
        }
      }
      else
      {
        if (h%2 == 0)
        {
          Top = Bottom = h/2;
        }
        else
        {
          Top = (h+1)/2;
          Bottom = (h-1)/2;
        }
      }
    }

    if (Left == 0 && Top == 0 && Right == 0 && Bottom == 0) return;
  }

  if (!fspec.isAttached || (fspec.HPos == 2 && fspec.VPos == 2))
  {
    /*********
     ** Top **
     *********/
    if (l > 0 && tp == QTabWidget::North)
    {
      renderElement(painter,element1+"-top",
                    QRect(x0+Left,
                          y0,
                          d-x0-Left,
                          Top),
                    fspec.ps,0,usePixmap);
      renderElement(painter,element1+"-top",
                    QRect(d+l,
                          y0,
                          x0+w-Left-d-l,
                          Top),
                    fspec.ps,0,usePixmap);
     /* left and right junctions */
     if (d-x0-Left >= 0)
       renderElement(painter,element1+"-top-leftjunct",
                      QRect(d,
                            y0,
                            f1,
                            Top),
                      0,0,usePixmap);
     if (x0+w-Left-d-l >= 0)
       renderElement(painter,element1+"-top-rightjunct",
                      QRect(d+l-f2,
                            y0,
                            f2,
                            Top),
                      0,0,usePixmap);
    }
    else
      renderElement(painter,element1+"-top",
                    QRect(x0+Left,y0,w-Left-Right,Top),
                    fspec.ps,0,usePixmap);

    /************
     ** Bottom **
     ************/
    if (l > 0 && tp == QTabWidget::South)
    {
      renderElement(painter,element1+"-bottom",
                    QRect(x0+Left,
                          y1-Bottom,
                          d-x0-Left,
                          Bottom),
                    fspec.ps,0,usePixmap);
      renderElement(painter,element1+"-bottom",
                    QRect(d+l,
                          y1-Bottom,
                          x0+w-Left-d-l,
                          Bottom),
                    fspec.ps,0,usePixmap);
      if (d-x0-Left >= 0)
        renderElement(painter,element1+"-bottom-leftjunct",
                      QRect(d,
                            y1-Bottom,
                            f2,
                            Bottom),
                      0,0,usePixmap);
      if (x0+w-Left-d-l >= 0)
        renderElement(painter,element1+"-bottom-rightjunct",
                      QRect(d+l-f1,
                            y1-Bottom,
                            f1,
                            Bottom),
                      0,0,usePixmap);
    }
    else
      renderElement(painter,element1+"-bottom",
                    QRect(x0+Left,y1-Bottom,w-Left-Right,Bottom),
                    fspec.ps,0,usePixmap);

    /**********
     ** Left **
     **********/
    if (l > 0 && tp == QTabWidget::West)
    {
      renderElement(painter,element1+"-left",
                    QRect(x0,
                          y0+Top,
                          Left,
                          d-y0-Top),
                    0,fspec.ps,usePixmap);
      renderElement(painter,element1+"-left",
                    QRect(x0,
                          d+l,
                          Left,
                          y0+h-Bottom-d-l),
                    0,fspec.ps,usePixmap);
      if (y0+h-Bottom-d-l >= 0)
        renderElement(painter,element1+"-left-leftjunct",
                      QRect(x0,
                            d+l-f2,
                            Left,
                            f2),
                      0,0,usePixmap);
      if (d-y0-Top >= 0)
        renderElement(painter,element1+"-left-rightjunct",
                      QRect(x0,
                            d,
                            Left,
                            f1),
                      0,0,usePixmap);
    }
    else
      renderElement(painter,element1+"-left",
                    QRect(x0,y0+Top,Left,h-Top-Bottom),
                    0,fspec.ps,usePixmap);

    /***********
     ** Right **
     ***********/
    if (l > 0 && tp == QTabWidget::East)
    {
      renderElement(painter,element1+"-right",
                    QRect(x1-Right,
                          y0+Top,
                          Right,
                          d-y0-Top),
                    0,fspec.ps,usePixmap);
      renderElement(painter,element1+"-right",
                    QRect(x1-Right,
                          d+l,
                          Right,
                          y0+h-Bottom-d-l),
                    0,fspec.ps,usePixmap);
      if (d-y0-Top >= 0)
        renderElement(painter,element1+"-right-leftjunct",
                      QRect(x1-Right,
                            d,
                            Right,
                            f1),
                      0,0,usePixmap);
      if (y0+h-Bottom-d-l >= 0)
        renderElement(painter,element1+"-right-rightjunct",
                      QRect(x1-Right,
                            d+l-f2,
                            Right,
                            f2),
                      0,0,usePixmap);
    }
    else
      renderElement(painter,element1+"-right",
                    QRect(x1-Right,y0+Top,Right,h-Top-Bottom),
                    0,fspec.ps,usePixmap);

    /*************
     ** Topleft **
     *************/
    QString  _element = element1+"-topleft";
    if (l > 0)
    {
      if (tp == QTabWidget::North && d < Left)
        _element = element1+"-left";
      else if (tp == QTabWidget::West && d < Top)
        _element = element1+"-top";
    }
    renderElement(painter,_element,
                  QRect(x0,y0,Left,Top),
                  0,0,usePixmap);

    /**************
     ** Topright **
     **************/
    _element = element1+"-topright";
    if (l > 0)
    {
      if (tp == QTabWidget::North && w-d-l < Right)
        _element = element1+"-right";
      else if (tp == QTabWidget::East && d < Top)
        _element = element1+"-top";
    }
    renderElement(painter,_element,
                  QRect(x1-Right,y0,Right,Top),
                  0,0,usePixmap);

    /****************
     ** Bottomleft **
     ****************/
    _element = element1+"-bottomleft";
    if (l > 0)
    {
      if (tp == QTabWidget::South && d < Left)
        _element = element1+"-left";
      else if (tp == QTabWidget::West && h-d-l < Bottom)
        _element = element1+"-bottom";
    }
    renderElement(painter,_element,
                  QRect(x0,y1-Bottom,Left,Bottom),
                  0,0,usePixmap);

    /*****************
     ** Bottomright **
     *****************/
    _element = element1+"-bottomright";
    if (l > 0)
    {
      if (tp == QTabWidget::South && w-d-l < Right)
        _element = element1+"-right";
      else if (tp == QTabWidget::East && h-d-l < Bottom)
        _element = element1+"-bottom";
    }
    renderElement(painter,_element,
                  QRect(x1-Right,y1-Bottom,Right,Bottom),
                  0,0,usePixmap);
  }
  else // with attachment
  {
    if (fspec.HPos == 0 && fspec.VPos == 0)
      return;

    /* to simplify calculations, we first get margins */
    int left = 0, right = 0, top = 0, bottom = 0;
    if (fspec.HPos == -1 || fspec.HPos == 2)
      left = Left;
    if (fspec.HPos == 1 || fspec.HPos == 2)
      right = Right;
    if (fspec.VPos == -1  || fspec.VPos == 2)
      top = Top;
    if (fspec.VPos == 1 || fspec.VPos == 2)
      bottom = Bottom;

    /*********
     ** Top **
     *********/
    if (top > 0)
    {
      renderElement(painter,element1+"-top",
                    QRect(x0+left,y0,w-left-right,top),
                    fspec.ps,0,usePixmap);

      // topleft corner
      if (left > 0)
        renderElement(painter,element1+"-topleft",
                      QRect(x0,y0,left,top),
                      0,0,usePixmap);
      // topright corner
      if (right > 0)
        renderElement(painter,element1+"-topright",
                      QRect(x1-right,y0,right,top),
                      0,0,usePixmap);
    }

    /************
     ** Bottom **
     ************/
    if (bottom > 0)
    {
      renderElement(painter,element1+"-bottom",
                    QRect(x0+left,y1-bottom,w-left-right,bottom),
                    fspec.ps,0,usePixmap);

      // bottomleft corner
      if (left > 0)
        renderElement(painter,element1+"-bottomleft",
                      QRect(x0,y1-bottom,left,bottom),
                      0,0,usePixmap);
      // bottomright corner
      if (right > 0)
        renderElement(painter,element1+"-bottomright",
                      QRect(x1-right,y1-bottom,right,bottom),
                      0,0,usePixmap);
    }

    /**********
     ** Left **
     **********/
    if (left > 0)
      renderElement(painter,element1+"-left",
                    QRect(x0,y0+top,left,h-top-bottom),
                    0,fspec.ps,usePixmap);

    /***********
     ** Right **
     ***********/
    if (right > 0)
      renderElement(painter,element1+"-right",
                    QRect(x1-right,y0+top,right,h-top-bottom),
                    0,fspec.ps,usePixmap);
  }


  if (drawExpanded && Top + Bottom != h) // when needed and there is space...
  { // ... draw the "interior"
    if (grouped && fspec.isAttached)
    {
      if (fspec.HPos == 0)
        Right = Left = 0;
      else if (fspec.HPos == -1)
        Right = 0;
      else if (fspec.HPos == 1)
        Left = 0;
    }
    renderElement(painter,element1,
                  bounds.adjusted(Left,Top,-Right,-Bottom),
                  0,0,usePixmap);
  }
  if (drawBorder) // draw inside this rectangle to make a border
  {
    /* the expansion should be less here; otherwise, the border wouldn't be smooth */
    frame_spec Fspec = fspec;
    Fspec.expansion = fspec.expansion - fspec.top - fspec.bottom;
    if (Fspec.expansion <= 0) Fspec.expansion = 1;
    renderFrame(painter,
                bounds.adjusted((fspec.isAttached && (fspec.HPos == 1 || fspec.HPos == 0)) ? 0 : fspec.left,
                                fspec.top,
                                (fspec.isAttached && (fspec.HPos == -1 || fspec.HPos == 0)) ?  0: -fspec.right,
                                -fspec.bottom),
                Fspec,element,d,l,f1,f2,tp,grouped,usePixmap,false); // this time, don't draw any border
  }
}

bool Style::renderInterior(QPainter *painter,
                           const QRect &bounds, // frame bounds
                           const frame_spec &fspec, // frame spec
                           const interior_spec &ispec, // interior spec
                           const QString &element, // interior SVG element
                           bool grouped, // is among grouped similar widgets?
                           bool usePixmap // first make a QPixmap for drawing
                          ) const
{
  if (!bounds.isValid() || !ispec.hasInterior || painter->opacity() == 0)
    return false;

  int w = bounds.width(); int h = bounds.height();
  if (!isLibreoffice_ && fspec.expansion > 0 && !ispec.element.isEmpty())
  {
    bool isHAttached(fspec.isAttached && fspec.HPos != 2);
    if (isHAttached)
      grouped = true;
    int e = grouped ? h : qMin(h,w);
    QString frameElement(fspec.expandedElement);
    if (frameElement.isEmpty())
      frameElement = fspec.element;
    QString element0(element);
    /* the interior used for partial frame expansion has the frame name */
    element0 = element0.remove("-inactive").replace(ispec.element, frameElement);
    element0 = "expand-"+element0;
    if (((e <= fspec.expansion && (isHAttached ? 2*w >= h : (!grouped || w >= h)))
         || (themeRndr_ && themeRndr_->isValid()
             && (themeRndr_->elementExists(element0)
                 || themeRndr_->elementExists(element0.replace("-toggled","-normal")
                                                      .replace("-pressed","-normal")
                                                      .replace("-focused","-normal")))))
        && (!fspec.isAttached || fspec.VPos == 2)
        /* there's no right/left expanded element */
        && (h <= 2*w || (fspec.HPos != 1 && fspec.HPos != -1)))
    {
      return false;
    }
  }

  /* extreme cases */
  if (fspec.isAttached// && (fspec.HPos != 2 || fspec.VPos != 2)
      && ((fspec.HPos == -1 && fspec.left >= w)
          || (fspec.HPos == 1 && fspec.right >= w)
          || (fspec.VPos == -1 && fspec.top >= h)
          || (fspec.VPos == 1 && fspec.bottom >= h)))
  {
      return false;
  }

  return renderElement(painter,element,interiorRect(bounds,fspec),
                       ispec.px,ispec.py,usePixmap);
}

bool Style::renderIndicator(QPainter *painter,
                            const QRect &bounds, // frame bounds
                            const frame_spec &fspec, // frame spec
                            const indicator_spec &dspec, // indicator spec
                            const QString &element, // indicator SVG element
                            Qt::LayoutDirection ld,
                            Qt::Alignment alignment,
                            int vOffset) const
{
  if (!bounds.isValid()) return true;
  QRect interior = interiorRect(bounds,fspec);
  int s;
  if (!interior.isValid())
    s = qMin(bounds.width(), bounds.height());
  else
    s = qMin(interior.width(), interior.height());
  /* make the indicator smaller if there isn't enough space */
  s = qMin(s, dspec.size);

  if (interior.height() - s >= vOffset)
    interior.adjust(0,-vOffset,0,-vOffset);

  return renderElement(painter,element,
                       alignedRect(ld,alignment,QSize(s,s),interior));
}

void Style::renderLabel(
                        const QStyleOption *option,
                        QPainter *painter,
                        const QRect &bounds, // frame bounds
                        const frame_spec &fspec, // frame spec
                        const label_spec &lspec, // label spec
                        int talign, // text alignment
                        const QString &text,
                        QPalette::ColorRole textRole, // text color role
                        int state, // widget state (0->disabled, 1->normal, 2->focused, 3->pressed, 4->toggled)
                        bool isInactive,
                        const QPixmap &px,
                        QSize iconSize,
                        const Qt::ToolButtonStyle tialign, // relative positions of text and icon
                        bool centerLoneIcon // centered icon with empty text?
                       ) const
{
  // compute text and icon rect
  QRect r;
  if (/*!isPlasma_ &&*/ // we ignore Plasma text margins just for push and tool buttons and menubars
      tialign != Qt::ToolButtonIconOnly
      && (!text.isEmpty() || !centerLoneIcon))
    r = labelRect(bounds,fspec,lspec);
  else
    r = interiorRect(bounds,fspec);

  if (!r.isValid())
    return;

  if (px.isNull() || !iconSize.isValid())
    iconSize = QSize(0,0);

  QRect ricon = r;
  QRect rtext = r;
  Qt::LayoutDirection ld = option->direction;

  if (tialign == Qt::ToolButtonTextBesideIcon)
  {
    ricon = alignedRect(ld,
                        Qt::AlignVCenter | Qt::AlignLeft,
                        iconSize,
                        r);
    rtext = QRect(ld == Qt::RightToLeft ?
                    r.x()
                    : r.x()+iconSize.width() + (px.isNull() ? 0 : lspec.tispace),
                  r.y(),
                  r.width()-ricon.width() - (px.isNull() ? 0 : lspec.tispace),
                  r.height());
  }
  else if (tialign == Qt::ToolButtonTextUnderIcon)
  {
    ricon = alignedRect(ld,
                        Qt::AlignTop | Qt::AlignHCenter,
                        iconSize,
                        r);
    rtext = QRect(r.x(),
                  r.y()+iconSize.height() + (px.isNull() ? 0 : lspec.tispace),
                  r.width(),
                  r.height()-ricon.height() - (px.isNull() ? 0 : lspec.tispace));
  }
  else if (tialign == Qt::ToolButtonIconOnly && !px.isNull())
  {
    /* center the icon considering text margins (r is the interior rect here) */
    int horizOffset = 0, vertOffset = 0;
    if (r.width() > iconSize.width()
        && lspec.left+fspec.left + lspec.right+fspec.right > 0)
    {
      qreal rDiff = static_cast<qreal>(lspec.left+fspec.left - lspec.right-fspec.right)
                    / static_cast<qreal>(lspec.left+fspec.left + lspec.right+fspec.right);
      horizOffset = qRound(static_cast<qreal>(r.width()-iconSize.width()) * rDiff / 2.0);
    }
    if (r.height() > iconSize.height()
        && lspec.top+fspec.top + lspec.bottom+fspec.bottom > 0)
    {
      qreal rDiff = static_cast<qreal>(lspec.top+fspec.top - lspec.bottom-fspec.bottom)
                    / static_cast<qreal>(lspec.top+fspec.top + lspec.bottom+fspec.bottom);
      vertOffset = qRound(static_cast<qreal>(r.height()-iconSize.height()) * rDiff / 2.0);
    }
    ricon = alignedRect(ld,
                        Qt::AlignCenter,
                        iconSize,
                        r.adjusted(horizOffset, vertOffset, horizOffset, vertOffset));
  }

  if (tialign != Qt::ToolButtonIconOnly && text.isEmpty() && !px.isNull() && centerLoneIcon)
  {
    /* center the icon considering text margins (r is the interior rect here) */
    int horizOffset = 0, vertOffset = 0;
    if (r.width() > iconSize.width()
        && lspec.left+fspec.left + lspec.right+fspec.right > 0)
    {
      qreal rDiff = static_cast<qreal>(lspec.left+fspec.left - lspec.right-fspec.right)
                    / static_cast<qreal>(lspec.left+fspec.left + lspec.right+fspec.right);
      horizOffset = qRound(static_cast<qreal>(r.width()-iconSize.width()) * rDiff / 2.0);
    }
    if (r.height() > iconSize.height()
        && lspec.top+fspec.top + lspec.bottom+fspec.bottom > 0)
    {
      qreal rDiff = static_cast<qreal>(lspec.top+fspec.top - lspec.bottom-fspec.bottom)
                    / static_cast<qreal>(lspec.top+fspec.top + lspec.bottom+fspec.bottom);
      vertOffset = qRound(static_cast<qreal>(r.height()-iconSize.height()) * rDiff / 2.0);
    }
    ricon = alignedRect(ld,
                        Qt::AlignCenter,
                        iconSize,
                        r.adjusted(horizOffset, vertOffset, horizOffset, vertOffset));
  }

  if (tialign != Qt::ToolButtonTextOnly && !px.isNull())
  {
    // the pixmap should have been enlarged by pixelRatio_
    QRect iconRect = alignedRect(ld, Qt::AlignCenter, (QSizeF(px.size())/pixelRatio_).toSize(), ricon);

    if (!(option->state & State_Enabled))
    {
      qreal opacityPercentage = static_cast<qreal>(hspec_.disabled_icon_opacity);
      if (opacityPercentage < 100)
        painter->drawPixmap(iconRect,translucentPixmap(px, opacityPercentage));
      else
        painter->drawPixmap(iconRect,px);
    }
    else
    {
      qreal tintPercentage = static_cast<qreal>(hspec_.tint_on_mouseover);
      if (tintPercentage > 0 && (option->state & State_MouseOver))
        painter->drawPixmap(iconRect, tintedPixmap(option,px,tintPercentage));
      else
        painter->drawPixmap(iconRect,px);
    }
  }

  if (((isPlasma_ && px.isNull()) // Why do some Plasma toolbuttons pretend to have only icons?
       || tialign != Qt::ToolButtonIconOnly)
      && !text.isEmpty())
  {
    // draw text based on its direction, not based on the layout direction
    painter->save();
    if (text.isRightToLeft())
    {
      painter->setLayoutDirection(Qt::RightToLeft);
      if (option->direction == Qt::LeftToRight)
      {
        if (talign & Qt::AlignLeft)
        {
          talign &= ~Qt::AlignLeft;
          talign |= Qt::AlignRight;
        }
        else if (talign & Qt::AlignRight)
        {
          talign &= ~Qt::AlignRight;
          talign |= Qt::AlignLeft;
        }
      }
    }
    else
    {
      painter->setLayoutDirection(Qt::LeftToRight);
      if (option->direction == Qt::RightToLeft)
      {
        if (talign & Qt::AlignLeft)
        {
          talign &= ~Qt::AlignLeft;
          talign |= Qt::AlignRight;
        }
        else if (talign & Qt::AlignRight)
        {
          talign &= ~Qt::AlignRight;
          talign |= Qt::AlignLeft;
        }
      }
    }

    if (lspec.boldFont)
    {
      QFont f(painter->font());
      f.setWeight(lspec.boldness);
      painter->save();
      painter->setFont(f);
    }
    if (lspec.italicFont)
    {
      QFont f(painter->font());
      f.setItalic(true);
      painter->save();
      painter->setFont(f);
    }

    QColor normalColor = getFromRGBA(lspec.normalColor);
    if (state != 0 && !(isPlasma_ && tialign == Qt::ToolButtonIconOnly))
    {
      QColor focusColor = getFromRGBA(lspec.focusColor);
      QColor pressColor = getFromRGBA(lspec.pressColor);
      QColor toggleColor = getFromRGBA(lspec.toggleColor);
      QColor progColor = getFromRGBA(cspec_.progressIndicatorTextColor);

      if (lspec.hasShadow)
      {
        QColor shadowColor = getFromRGBA(lspec.shadowColor);
        /* the shadow should have enough contrast with the text */
        if (shadowColor.isValid()
            && ((state == 1 && (!normalColor.isValid() || enoughContrast(normalColor, shadowColor)))
                || (state == 2 && (!focusColor.isValid() || enoughContrast(focusColor, shadowColor)))
                || (state == 3 && (!pressColor.isValid() || enoughContrast(pressColor, shadowColor)))
                || (state == 4 && (!toggleColor.isValid() || enoughContrast(toggleColor, shadowColor)))
                || (state == -1 && (!progColor.isValid() || enoughContrast(progColor, shadowColor)))))
        {
          QColor col;
          if (isInactive)
          {
            col = getFromRGBA(lspec.inactiveShadowColor);
            if (!col.isValid())
              col = shadowColor;
          }
          else
            col = shadowColor;
          painter->save();
          if (lspec.a < 255)
            col.setAlpha(lspec.a);
          painter->setPen(QPen(col));
          for (int i=0; i<lspec.depth; i++)
          {
            int xShift = lspec.xshift + i * (lspec.xshift < 0 ? -1 : 1);
            int yShift = lspec.yshift + i * (lspec.yshift < 0 ? -1 : 1);
            painter->drawText(rtext.adjusted(xShift,yShift,
                                             xShift,yShift),
                              talign,text);
          }
          painter->restore();
        }
      }

      if (state == 1)
      {
        QColor col;
        if (isInactive)
        {
          col = getFromRGBA(lspec.normalInactiveColor);
          if (!col.isValid())
            col = normalColor;
        }
        else
          col = normalColor;
        if (col.isValid())
        {
          painter->save();
          painter->setPen(QPen(col));
          painter->drawText(rtext,talign,text);
          painter->restore();
          if (lspec.boldFont)
            painter->restore();
          if (lspec.italicFont)
            painter->restore();
          painter->restore();
          return;
        }
      }
      else if (state == 2)
      {
        QColor col;
        if (isInactive)
        {
          col = getFromRGBA(lspec.focusInactiveColor);
          if (!col.isValid())
            col = focusColor;
        }
        else
          col = focusColor;
        if (col.isValid())
        {
          painter->save();
          painter->setPen(QPen(col));
          painter->drawText(rtext,talign,text);
          painter->restore();
          if (lspec.boldFont)
            painter->restore();
          if (lspec.italicFont)
            painter->restore();
          painter->restore();
          return;
        }
      }
      else if (state == 3)
      {
        QColor col;
        if (isInactive)
        {
          col = getFromRGBA(lspec.pressInactiveColor);
          if (!col.isValid())
            col = pressColor;
        }
        else
          col = pressColor;
        if (col.isValid())
        {
          painter->save();
          painter->setPen(QPen(col));
          painter->drawText(rtext,talign,text);
          painter->restore();
          if (lspec.boldFont)
            painter->restore();
          if (lspec.italicFont)
            painter->restore();
          painter->restore();
          return;
        }
      }
      else if (state == 4)
      {
        QColor col;
        if (isInactive)
        {
          col = getFromRGBA(lspec.toggleInactiveColor);
          if (!col.isValid())
            col = toggleColor;
        }
        else
          col = toggleColor;
        if (col.isValid())
        {
          painter->save();
          painter->setPen(QPen(col));
          painter->drawText(rtext,talign,text);
          painter->restore();
          if (lspec.boldFont)
            painter->restore();
          if (lspec.italicFont)
            painter->restore();
          painter->restore();
          return;
        }
      }
      else if (state == -1)
      {
        QColor col;
        if (isInactive)
        {
          col = getFromRGBA(cspec_.progressInactiveIndicatorTextColor);
          if (!col.isValid())
            col = progColor;
        }
        else
          col = progColor;
        if (col.isValid())
        {
          painter->save();
          painter->setPen(col);
          painter->drawText(rtext,talign,text);
          painter->restore();
          if (lspec.boldFont)
            painter->restore();
          if (lspec.italicFont)
            painter->restore();
          painter->restore();
          return;
        }
      }
    }
    /* if this is a dark-and-light theme, the disabled color may not be suitable */
    else if (state == 0
             && enoughContrast(normalColor, option->palette.color(QPalette::Text)))
    {
      painter->save();
      normalColor.setAlpha(102); // 0.4 * normalColor.alpha()
      painter->setPen(normalColor);
      painter->drawText(rtext,talign,text);
      painter->restore();
      if (lspec.boldFont)
        painter->restore();
      if (lspec.italicFont)
        painter->restore();
      painter->restore();
      return;
    }

    QCommonStyle::drawItemText(painter,
                               rtext,
                               talign,
                               option->palette,
                               state == 0 ? false: true,
                               text,
                               textRole);

    if (lspec.boldFont)
      painter->restore();
    if (lspec.italicFont)
      painter->restore();
    painter->restore();
  }
}

}
