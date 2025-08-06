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
#include <QItemDelegate>
#include <QAbstractItemView>

#include "shortcuthandler4.h"
#include "windowmanager4.h"
#include "ThemeConfig4.h"
#include "blurhelper4.h"

QT_BEGIN_NAMESPACE
class QSvgRenderer;
QT_END_NAMESPACE

namespace Kvantum {

// Used only to give appropriate top and bottom margins to
// combo popup items (adapted from the Breeze style plugin).
class KvComboItemDelegate : public QItemDelegate
{
  Q_OBJECT

  public:
    KvComboItemDelegate(int margin, QAbstractItemView *parent) :
      QItemDelegate(parent),
      proxy_(parent->itemDelegate())
    {
      margin_ = margin;
    }

    virtual ~KvComboItemDelegate(void) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
      if (proxy_)
        proxy_.data()->paint(painter, option, index);
      else
        QItemDelegate::paint(painter, option, index);
    }

    virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
      QSize size(proxy_ ? proxy_.data()->sizeHint(option, index)
                        : QItemDelegate::sizeHint(option, index));
      if (size.isValid())
         size.rheight() += 2*margin_;
      return size;
    }

  private:
    QWeakPointer<QAbstractItemDelegate> proxy_;
    int margin_;
};

class Style : public QCommonStyle {
  Q_OBJECT
  Q_CLASSINFO("X-KDE-CustomElements","true")

  public:
    Style();
    ~Style();

    void polish(QWidget *widget);
    void polish(QApplication *app);
    void polish(QPalette &palette);
    void unpolish(QWidget *widget);
    void unpolish(QApplication *app);

    virtual bool eventFilter(QObject *o, QEvent *e);

    virtual int pixelMetric(PixelMetric metric,
                            const QStyleOption *option = 0,
                            const QWidget *widget = 0) const;
    virtual QRect subElementRect(SubElement element,
                                 const QStyleOption *option,
                                 const QWidget *widget = 0) const;
    virtual QRect subControlRect(ComplexControl control,
                                 const QStyleOptionComplex *option,
                                 SubControl subControl,
                                 const QWidget *widget = 0) const;
    QSize sizeFromContents(ContentsType type,
                           const QStyleOption *option,
                           const QSize &contentsSize,
                           const QWidget *widget = 0) const;

    virtual void drawPrimitive(PrimitiveElement element,
                               const QStyleOption *option,
                               QPainter *painter,
                               const QWidget *widget = 0) const;
    virtual void drawControl(ControlElement element,
                             const QStyleOption *option,
                             QPainter *painter,
                             const QWidget *widget = 0) const;
    virtual void drawComplexControl(ComplexControl control,
                                    const QStyleOptionComplex *option,
                                    QPainter *painter,
                                    const QWidget *widget = 0) const;
    virtual int styleHint(StyleHint hint,
                          const QStyleOption *option = 0,
                          const QWidget *widget = 0,
                          QStyleHintReturn *returnData = 0) const;
    virtual SubControl hitTestComplexControl(ComplexControl control,
                                             const QStyleOptionComplex *option,
                                             const QPoint &position,
                                             const QWidget *widget = 0) const;

    virtual QPixmap generatedIconPixmap(QIcon::Mode iconMode,
                                        const QPixmap &pixmap,
                                        const QStyleOption *option) const;

    enum CustomElements {
      CE_Kv_KCapacityBar = CE_CustomBase + 0x00FFFF00,
    };

  protected slots:
    QIcon standardIconImplementation(StandardPixmap standardIcon,
                                     const QStyleOption *option = 0,
                                     const QWidget *widget = 0) const;

  private:
    /* Set up a theme with the given name. If there is no name,
       the default theme will be used. If the config or SVG file of
       the theme is missing, that of the default theme will be used. */
    void setTheme(const QString &themeName);

    /* Use the default config. */
    void setBuiltinDefaultTheme();

    /* Set theme dependencies. */
    void setupThemeDeps();

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

    /* Return the state of the given widget. */
    QString getState(const QStyleOption *option, const QWidget *widget) const;
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
                     frame_spec fspec, // frame spec
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
    bool renderInterior(QPainter *painter,
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
                         Qt::Alignment alignment = Qt::AlignCenter,
                         int vOffset = 0) const;

    /* Generic method that draws a label (text and/or icon) inside the frame. */
    void renderLabel(
                     const QStyleOption *option,
                     QPainter *painter,
                     const QRect &bounds, // frame bounds
                     const frame_spec &fspec, // frame spec
                     const label_spec &lspec, // label spec
                     int talign, // text alignment
                     const QString &text,
                     QPalette::ColorRole textRole, // text color role
                     int state = 1, // widget state (0->disabled, 1->normal, 2->focused, 3->pressed, 4->toggled)
                     bool isInactive = false,
                     const QPixmap &px = QPixmap(),
                     QSize iconSize = QSize(0,0),
                     const Qt::ToolButtonStyle tialign = Qt::ToolButtonTextBesideIcon, // relative positions of text and icon
                     bool centerLoneIcon = true // centered icon with empty text?
                    ) const;

    /* Draws the lineedit of an editable combobox. */
    void drawComboLineEdit (const QStyleOption *option,
                            QPainter *painter,
                            const QWidget *lineedit,
                            const QWidget *combo) const;

    /* Gets a pixmap with a proper size from an icon. */
    QPixmap getPixmapFromIcon(const QIcon &icon,
                              const QIcon::Mode iconmode,
                              const QIcon::State iconstate,
                              QSize iconSize) const;

    /* Returns a pixmap tinted by the highlight color. */
    QPixmap tintedPixmap(const QStyleOption *option,
                         const QPixmap &px,
                         const qreal tintPercentage) const;

    /* Returns a translucent pixmap for use with disabled widgets. */
    QPixmap translucentPixmap(const QPixmap &px,
                              const qreal opacityPercentage) const;

    /* Draws background of translucent top widgets. */
    void drawBg(QPainter *p, const QWidget *widget) const;

    /* Generic method to compute the ideal size of a widget. */
    QSize sizeCalculated(const QFont &font, // font to determine width/height
                         const frame_spec &fspec,
                         const label_spec &lspec,
                         const size_spec &sspec,
                         const QString &text,
                         const QSize iconSize,
                         const Qt::ToolButtonStyle tialign = Qt::ToolButtonTextBesideIcon, // text-icon alignment
                         // use real heights of multiline texts?
                         bool realHeight = false) const;

    /* Return the remaining QRect after subtracting the frames. */
    QRect interiorRect(const QRect &bounds, const frame_spec &fspec) const;
    /* Return the remaining QRect after subtracting the frames and text margins. */
    QRect labelRect(const QRect &bounds, const frame_spec &f,const label_spec &t) const {
      return interiorRect(bounds,f).adjusted(t.left,t.top,-t.right,-t.bottom);
    }

    /* Can an expanded border be drawn for this frame? */
    bool hasExpandedBorder(const frame_spec fspec) const;

    /* Get menu margins, including its shadow. */
    int getMenuMargin(bool horiz) const;
    /* Get pure shadow dimensions of menus/tooltips. */
    QList<int> getShadow(const QString &widgetName, int thicknessH, int thicknessV);
    QList<int> getShadow(const QString &widgetName, int thickness) {
      return getShadow(widgetName,thickness,thickness);
    }

    /* If this menubar is merged with a toolbar, return the toolbar height! */
    int mergedToolbarHeight(const QWidget *menubar) const;
    /* Is this a toolbar that should be styled? */
    bool isStylableToolbar(const QWidget *w, bool allowInvisible = false) const;
    /* Get the stylable toolbar containing this widget. */
    QWidget* getStylableToolbarContainer(const QWidget *w, bool allowInvisible = false) const;
    /* Does a widget without interior SVG element have
       a high contrast with its container (toolbar/menubar)? */
    bool hasHighContrastWithContainer(const QWidget *w, const QColor color) const;

    /* Consider monochrome icons that reverse color when selected. */
    QIcon::Mode getIconMode(int state, label_spec lspec) const;

    /* A method for forcing (push and tool) button text colors. */
    void forceButtonTextColor(QWidget *widget, QColor col) const;
    void forceButtonTextColor(const QWidget *widget, QColor col) const
    {
      forceButtonTextColor(const_cast<QWidget*>(widget), col);
    }

    bool hasParent(const QWidget *widget, const char *className) const
    {
      if (!widget) return false;
      while ((widget = widget->parentWidget()))
      {
        if (widget->inherits(className))
          return true;
      }
      return false;
    }

    /* The extra combo box width needed by frames and spacings. */
    int extraComboWidth(const QStyleOptionComboBox *opt, bool hasIcon) const;

  private slots:
    /* Called on timer timeout to advance busy progress bars. */
    void advanceProgressbar();
    /* Removes a widget from the list of translucent ones. */
    void noTranslucency(QObject *o);
    /* Removes a button from all special lists. */
    void removeFromSet(QObject *o);

  private:
    QSvgRenderer *defaultRndr_, *themeRndr_;
    ThemeConfig *defaultSettings_, *themeSettings_, *settings_;

    QString xdg_config_home;

    QTimer *progressTimer_;

    /* List of busy progress bars */
    QMap<QWidget*,int> progressbars_;
    /* List of windows, tooltips and menus that are (made) translucent */
    QSet<const QWidget*> translucentWidgets_;
    mutable QSet<QWidget*> forcedTranslucency_;

    ShortcutHandler *itsShortcutHandler_;
    WindowManager *itsWindowManager_;
    BlurHelper *blurHelper_;

    /* The general specification of the theme */
    theme_spec tspec_;
    /* The hacking specification of the theme */
    hacks_spec hspec_;
    /* The color specification of the theme */
    color_spec cspec_;
    /* All general info about tabs */
    bool hasActiveIndicator_, joinedActiveTab_, joinedActiveFloatingTab_, hasFloatingTabs_;

    /* LibreOffice and Plasma need workarounds. */
    bool isLibreoffice_, isPlasma_;
    /* So far, only VirtualBox has introduced
       itself as "Qt-subapplication" and doesn't
       accept compositing. */
    bool subApp_;
    /* Some apps shouldn't have translucent windows. */
    bool isOpaque_;

    /* Hacks */
    bool isDolphin_;
    bool isPcmanfm_;

    /* The size of the slider handle with no tick mark (if it exists) */
    mutable int ticklessSliderHandleSize_;

    /* For identifying KisSliderSpinBox */
    bool isKisSlider_;

    /* Keep track of the sunken button (used instead of a private header for menu positioning). */
    mutable QWeakPointer<QWidget> sunkenButton_;

    /* For not getting the menu shadows repeatedly.
       They're used to position submenus correctly. */
    QList<int> menuShadow_;

    /* Under Gnome and Unity, we disable compositing because otherwise, DE shadows
       would be drawn around Kvantum's menu shadows. Other DEs have their own ways
       of preventing that or the user could disable compositing with Kvantum Manager. */
    bool noComposite_;
    /* For correct updating on mouseover with active tab overlapping */
    QRect tabHoverRect_;

    /* for enforcing the text color of inactive selected items */
    bool hasInactiveSelItemCol_;
    /* Does the toggled (active but unfocused) view-item have a high contrast with the pressed one? */
    bool toggledItemHasContrast_;
};
}

#endif
