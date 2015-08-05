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

#ifndef KVANTUM_H
#define KVANTUM_H

#include <QCommonStyle>
#include <QMap>

#include "shortcuthandler.h"
#include "drag/windowmanager.h"
#include "themeconfig/ThemeConfig.h"
#include "blur/blurhelper.h"

class QSvgRenderer;

namespace Kvantum {
class Style : public QCommonStyle {
  Q_OBJECT
  Q_CLASSINFO("X-KDE-CustomElements","true")

  public:
    Style();
    ~Style();

    /*
       Set the name of the user specific theme. If there
       is no themename, the default config will be used.
     */
    void setUserTheme(const QString &themename);
    /*
       Use the default config.
     */
    void setBuiltinDefaultTheme();

    void polish(QWidget *widget);
    void polish(QApplication *app);
    void polish(QPalette &palette);
    void unpolish(QWidget *widget);
    void unpolish(QApplication *app);

    virtual bool eventFilter(QObject *o, QEvent *e);

    virtual int pixelMetric (PixelMetric metric,
                             const QStyleOption *option = 0,
                             const QWidget *widget = 0) const;
    virtual QRect subElementRect (SubElement element,
                                  const QStyleOption *option,
                                  const QWidget *widget = 0) const;
    virtual QRect subControlRect (ComplexControl control,
                                  const QStyleOptionComplex *option,
                                  SubControl subControl,
                                  const QWidget *widget = 0) const;
    QSize sizeFromContents (ContentsType type,
                            const QStyleOption *option,
                            const QSize &contentsSize,
                            const QWidget *widget = 0) const;

    virtual void drawPrimitive (PrimitiveElement element,
                                const QStyleOption *option,
                                QPainter *painter,
                                const QWidget *widget = 0) const;
    virtual void drawControl (ControlElement element,
                              const QStyleOption *option,
                              QPainter *painter,
                              const QWidget *widget = 0) const;
    virtual void drawComplexControl (ComplexControl control,
                                     const QStyleOptionComplex *option,
                                     QPainter *painter,
                                     const QWidget *widget = 0 ) const;
    virtual int styleHint(StyleHint hint,
                          const QStyleOption *option = 0,
                          const QWidget *widget = 0,
                          QStyleHintReturn *returnData = 0) const;
    virtual SubControl hitTestComplexControl (ComplexControl control,
                                              const QStyleOptionComplex *option,
                                              const QPoint &position,
                                              const QWidget *widget = 0) const;

    /* A solution for Qt5's problem with translucent windows.*/
    void setSurfaceFormat(QWidget *w) const;
    void setSurfaceFormat(const QWidget *w) const
    {
      setSurfaceFormat(const_cast<QWidget*>(w));
    }

    /* A method for forcing (push and tool) button text colors. */
    void forceButtonTextColor(QWidget *widget, QColor col) const;
    void forceButtonTextColor(const QWidget *widget, QColor col) const
    {
      forceButtonTextColor(const_cast<QWidget*>(widget), col);
    }

    enum CustomElements {
      CE_Kv_KCapacityBar = CE_CustomBase + 0x00FFFF00,
    };

#if QT_VERSION >= 0x050000
    QIcon standardIcon (StandardPixmap standardIcon,
                        const QStyleOption *option = 0,
                        const QWidget *widget = 0) const;
#else
  protected slots:
    QIcon standardIconImplementation (StandardPixmap standardIcon,
                                      const QStyleOption *option = 0,
                                      const QWidget *widget = 0) const;
#endif

  private:
    /* Render the element from the SVG file into the given bounds. */
    bool renderElement(QPainter *painter,
                       const QString &element,
                       const QRect &bounds,
                       int hsize = 0, int vsize = 0, // pattern sizes
                       bool usePixmap = false // first make a QPixmap for drawing
                      ) const;
    /* Render the (vertical) slider ticks. */
    void renderSliderTick(QPainter *painter,
                          const QString &element,
                          const QRect &ticksRect,
                          const int interval,
                          const int available,
                          const int min,
                          const int max,
                          bool above, // left
                          bool inverted) const;

    /* Return the frame spec of the given widget from the theme config file. */
    inline frame_spec getFrameSpec(const QString &widgetName) const;
    /* Return the interior spec of the given widget from the theme config file. */
    inline interior_spec getInteriorSpec(const QString &widgetName) const;
    /* Return the indicator spec of the given widget from the theme config file. */
    inline indicator_spec getIndicatorSpec(const QString &widgetName) const;
    /* Return the label (text+icon) spec of the given widget from the theme config file. */
    inline label_spec getLabelSpec(const QString &widgetName) const;
    /* Return the size spec of the given widget from the theme config file */
    inline size_spec getSizeSpec(const QString &widgetName) const;

    /* Generic method that draws a frame. */
    void renderFrame(QPainter *painter,
                     const QRect &bounds, // frame bounds
                     const frame_spec &fspec, // frame spec
                     const QString &element, // frame SVG element (basename)
                     int d = 0, // distance of the attached tab from the edge
                     int l = 0, // length of the attached tab
                     int f1 = 0, // width of tab's left frame
                     int f2 = 0, // width of tab's right frame
                     int tp = 0, // tab position
                     bool grouped = false, // is among grouped similar widgets?
                     bool usePixmap = false, // first make a QPixmap for drawing
                     bool drawBorder = true // draw a border with maximum rounding if possible
                    ) const;

    /* Generic method that draws an interior. */
    void renderInterior(QPainter *painter,
                        const QRect &bounds, // frame bounds
                        const frame_spec &fspec, // frame spec
                        const interior_spec &ispec, // interior spec
                        const QString &element, // interior SVG element
                        bool grouped = false, // is among grouped similar widgets?
                        bool usePixmap = false // first make a QPixmap for drawing
                       ) const;

    /* Generic method that draws an indicator. */
    void renderIndicator(QPainter *painter,
                         const QRect &bounds, // frame bounds
                         const frame_spec &fspec, // frame spec
                         const indicator_spec &dspec, // indicator spec
                         const QString &element, // indicator SVG element
                         Qt::LayoutDirection ld = Qt::LeftToRight,
                         Qt::Alignment alignment = Qt::AlignCenter) const;

    /* Generic method that draws a label (text and/or icon) inside the frame. */
    void renderLabel(
                     QPainter *painter,
                     const QPalette &palette,
                     const QRect &bounds, // frame bounds
                     const frame_spec &fspec, // frame spec
                     const label_spec &lspec, // label spec
                     int talign, // text alignment
                     const QString &text,
                     QPalette::ColorRole textRole, // text color role
                     int state = 1, // widget state (0->disabled, 1->normal, 2->focused, 3->pressed, 4->toggled)
                     Qt::LayoutDirection ld = Qt::LeftToRight,
                     const QPixmap &icon = QPixmap(),
                     QSize iconSize = QSize(0,0),
                     const Qt::ToolButtonStyle tialign = Qt::ToolButtonTextBesideIcon // relative positions of text and icon
                    ) const;

    /* Draws background of translucent top widgets. */
    void drawBg(QPainter *p, const QWidget *widget) const;

    /* Generic method to compute the ideal size of a widget. */
    QSize sizeCalculated(const QFont &font, // font to determine width/height
                         const frame_spec &fspec,
                         const label_spec &lspec,
                         const size_spec &sspec,
                         const QString &text,
                         const QSize iconSize,
                         // text-icon alignment
                         const Qt::ToolButtonStyle tialign = Qt::ToolButtonTextBesideIcon) const;

    /* Return a normalized rect, i.e. a square. */
    QRect squaredRect(const QRect &r) const;

    /* Return the remaining QRect after subtracting the frames. */
    QRect interiorRect(const QRect &bounds, frame_spec fspec) const;
    /* Return the remaining QRect after subtracting the frames and text margins. */
    QRect labelRect(const QRect &bounds, frame_spec f,label_spec t) const {
      return interiorRect(bounds,f).adjusted(t.left,t.top,-t.right,-t.bottom);
    }

    /* Get pure shadow dimensions of menus/tooltips. */
    QList<int> getShadow(const QString &widgetName, int thicknessH, int thicknessV);
    QList<int> getShadow(const QString &widgetName, int thickness) {
      return getShadow(widgetName,thickness,thickness);
    }

  private slots:
    /* Called on timer timeout to advance busy progress bars. */
    void advanceProgresses();
    /* Removes a widget from the list of translucent ones. */
    void noTranslucency(QObject *o);
    /* Removes a button from all special lists. */
    void removeFromSet(QObject *o);

  private:
    QSvgRenderer *defaultRndr, *themeRndr;
    ThemeConfig *defaultSettings, *themeSettings, *settings;

    QString xdg_config_home;

    QTimer *progresstimer;

    /* List of busy progress bars. */
    QMap<QWidget *,int> progressbars;
    /* List of windows, tooltips and menus that are made translucent. */
    QSet<const QWidget*> translucentWidgets;

    ShortcutHandler *itsShortcutHandler;
    WindowManager *itsWindowManager;
    BlurHelper* blurHelper;

    /* Set theme dependencies. */
    void setupThemeDeps();

    /* The general specification of the theme. */
    theme_spec tspec;

    /* LibreOffice and Plasma need workarounds. */
    bool isLibreoffice;
    bool isPlasma;
    /* So far, only VirtualBox has introduced
       itself as "Qt-subapplication" and doesn't
       accept compositing. */
    bool subApp;
    /* Some apps shouldn't have translucent windows. */
    bool isOpaque;

    /* Hacks */
    bool isDolphin;
    bool isKonsole;
    bool isYakuake;

    /* For identifying KisSliderSpinBox. */
    bool isKisSlider;

    /* Search for the toolbutton flat indicator just once! */
    bool hasFlatIndicator;
};
}

#endif
