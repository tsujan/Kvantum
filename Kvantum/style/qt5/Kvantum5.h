/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2023 <tsujan2000@gmail.com>
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
#include <QToolButton>
#include <QTextOption>

#include "shortcuthandler5.h"
#include "windowmanager5.h"
#include "ThemeConfig5.h"
#include "blurhelper5.h"
#include "animation5.h"

// definitions shared by source files
#define SLIDER_TICK_SIZE 5 // 10 at most
#define ANIMATION_FRAME 40 // in ms

class QSvgRenderer;

namespace Kvantum {

/*#if QT_VERSION >= 0x050000
template <typename T> using KvPointer = QPointer<T>;
#else
template <typename T> using KvPointer = QWeakPointer<T>;
#endif*/

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

    virtual ~KvComboItemDelegate() {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
      if (proxy_)
        proxy_.data()->paint(painter, option, index);
      else
        QItemDelegate::paint(painter, option, index);
    }

    virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
      QSize size(proxy_ ? proxy_.data()->sizeHint(option, index)
                        : QItemDelegate::sizeHint(option, index));
      if (size.isValid())
        size.rheight() += 2*margin_;
      return size;
    }

  private:
    //KvPointer<QAbstractItemDelegate> proxy_;
    QPointer<QAbstractItemDelegate> proxy_;
    int margin_;
};

class Style : public QCommonStyle {
  Q_OBJECT
  Q_CLASSINFO("X-KDE-CustomElements","true")

  public:
    Style(bool useDark);
    ~Style();

    void polish(QWidget *widget);
    void polish(QApplication *app);
    void polish(QPalette &palette);
    void unpolish(QWidget *widget);
    void unpolish(QApplication *app);
    QPalette standardPalette() const;

    virtual bool eventFilter(QObject *o, QEvent *e);

    virtual int pixelMetric(QStyle::PixelMetric metric,
                            const QStyleOption *option = nullptr,
                            const QWidget *widget = nullptr) const;
    virtual QRect subElementRect(QStyle::SubElement element,
                                 const QStyleOption *option,
                                 const QWidget *widget = nullptr) const;
    virtual QRect subControlRect(QStyle::ComplexControl control,
                                 const QStyleOptionComplex *option,
                                 QStyle::SubControl subControl,
                                 const QWidget *widget = nullptr) const;
    QSize sizeFromContents(QStyle::ContentsType type,
                           const QStyleOption *option,
                           const QSize &contentsSize,
                           const QWidget *widget = nullptr) const;

    virtual void drawPrimitive(QStyle::PrimitiveElement element,
                               const QStyleOption *option,
                               QPainter *painter,
                               const QWidget *widget = nullptr) const;
    virtual void drawControl(QStyle::ControlElement element,
                             const QStyleOption *option,
                             QPainter *painter,
                             const QWidget *widget = nullptr) const;
    virtual void drawComplexControl(QStyle::ComplexControl control,
                                    const QStyleOptionComplex *option,
                                    QPainter *painter,
                                    const QWidget *widget = nullptr) const;
    virtual int styleHint(QStyle::StyleHint hint,
                          const QStyleOption *option = nullptr,
                          const QWidget *widget = nullptr,
                          QStyleHintReturn *returnData = nullptr) const;
    virtual QStyle::SubControl hitTestComplexControl(QStyle::ComplexControl control,
                                                     const QStyleOptionComplex *option,
                                                     const QPoint &position,
                                                     const QWidget *widget = nullptr) const;

    virtual QPixmap generatedIconPixmap(QIcon::Mode iconMode,
                                        const QPixmap &pixmap,
                                        const QStyleOption *option) const;

    virtual void drawItemText(QPainter *painter, const QRect &rect, int flags,
                              const QPalette &pal, bool enabled, const QString &text,
                              QPalette::ColorRole textRole = QPalette::NoRole) const;

    virtual void drawItemPixmap(QPainter *painter, const QRect &rect,
                                int alignment, const QPixmap &pixmap) const;

    QIcon standardIcon(QStyle::StandardPixmap standardIcon,
                       const QStyleOption *option = nullptr,
                       const QWidget *widget = nullptr) const;

    enum CustomElements {
      CE_Kv_KCapacityBar = CE_CustomBase + 0x00FFFF00,
    };

  private:
    /* For handling disabled icons with all icon engines: */
    enum KvIconMode {
      Normal,
      Selected,
      Active,
      Disabled,
      DisabledSelected
    };

    /* Set up a theme with the given name. If there is no name,
       the default theme will be used. If the config or SVG file of
       the theme is missing, that of the default theme will be used. */
    void setTheme(const QString &baseThemeName, bool useDark);

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
    frame_spec getFrameSpec(const QString &widgetName) const {
      return settings_->getFrameSpec(widgetName);
    }
    /* Return the interior spec of the given widget from the theme config file. */
    interior_spec getInteriorSpec(const QString &widgetName) const {
      return settings_->getInteriorSpec(widgetName);
    }
    /* Return the indicator spec of the given widget from the theme config file. */
    indicator_spec getIndicatorSpec(const QString &widgetName) const {
      return settings_->getIndicatorSpec(widgetName);
    }
    /* Return the label (text+icon) spec of the given widget from the theme config file. */
    label_spec getLabelSpec(const QString &widgetName) const;
    /* Return the size spec of the given widget from the theme config file */
    size_spec getSizeSpec(const QString &widgetName) const {
      return settings_->getSizeSpec(widgetName);
    }

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
    bool renderIndicator(QPainter *painter,
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
                     const QPixmap &px = QPixmap(), // should have the correct size with HDPI
                     QSize iconSize = QSize(0,0),
                     const Qt::ToolButtonStyle tialign = Qt::ToolButtonTextBesideIcon, // relative positions of text and icon
                     bool centerLoneIcon = true // centered icon with empty text?
                    ) const;

    /* Draws a focus rectangle. */
    void drawFocusRect(QPainter *painter, const QRect &rect, const QString &element) const;

    /* Draws the lineedit of an editable combobox. */
    void drawComboLineEdit(const QStyleOption *option,
                           QPainter *painter,
                           const QWidget *lineedit,
                           const QWidget *combo,
                           const QString &group,
                           bool fillInterior) const;

    /* Gets a pixmap with a proper size from an icon considering HDPI. */
    QPixmap getPixmapFromIcon(const QIcon &icon,
                              const KvIconMode iconmode,
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
                         const Qt::ToolButtonStyle tialign = Qt::ToolButtonTextBesideIcon // text-icon alignment
                        ) const;

    /* Return the remaining QRect after subtracting the frames. */
    QRect interiorRect(const QRect &bounds, const frame_spec &fspec) const;
    /* Return the remaining QRect after subtracting the frames and text margins. */
    QRect labelRect(const QRect &bounds, const frame_spec &f,const label_spec &t) const {
      return interiorRect(bounds,f).adjusted(t.left,t.top,-t.right,-t.bottom);
    }

    QWidget* getParent(const QWidget *widget, int level) const;
    bool enoughContrast(const QColor &col1, const QColor &col2) const;
    QColor overlayColor(const QColor &bgCol, const QColor &overlayCol) const;

    /* Can an expanded border be drawn for this frame? */
    bool hasExpandedBorder(const frame_spec &fspec) const;
    /* Does a flat downward arrow exist? */
    bool flatArrowExists(const QString &indicatorElement) const;
    /* Do some elements (like checkbox/radio-buttons for menu/view-item) exit? */
    bool elementExists(const QString &elementName) const;

    /* Get menu margins, including its shadow. */
    int getMenuMargin(bool horiz) const;
    /* Get pure shadow dimensions of menus/tooltips (when compositing is available). */
    void getShadow(const QString &widgetName, int thicknessH, int thicknessV);
    void getShadow(const QString &widgetName, int thickness) {
      getShadow(widgetName,thickness,thickness);
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
    KvIconMode getIconMode(int state, bool isInactive, const label_spec &lspec) const;

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

    /* Gets color from #rrggbbaa. */
    QColor getFromRGBA(const QString &str) const;

    /* Is the window of this widget inactive? */
    bool isWidgetInactive(const QWidget *widget) const;

    /* Used only with combo menus. */
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

    /* Find the kind of this tool-button among a group of tool-buttons (on a toolbar). */
    int whichGroupedTBtn(const QToolButton *tb, const QWidget *parentBar, bool &drawSeparator) const;

    /* The extra combo box width needed by frames and spacings. */
    int extraComboWidth(const QStyleOptionComboBox *opt, bool hasIcon) const;

    /* For transient scrollbars: */
    void startAnimation(Animation *animation) const;
    void stopAnimation(const QObject *target) const;

    /* Forcing palettes when needed: */
    void forcePalette(QWidget *widget, const QPalette &p) const;

    /* Checking whether dragging from buttons is enabled and in progress: */
    bool btnDragInProgress() const;

    /* These methods are for item views:*/
    QSize viewItemSize(const QStyleOptionViewItem *option, int role) const;
    void viewItemLayout(const QStyleOptionViewItem *opt,  QRect *checkRect,
                        QRect *pixmapRect, QRect *textRect, bool sizehint) const;
    QString calculateElidedText(const QString &text,
                                const QTextOption &textOption,
                                const QFont &font,
                                const QRect &textRect,
                                const Qt::Alignment valign,
                                Qt::TextElideMode textElideMode,
                                int flags,
                                bool lastVisibleLineShouldBeElided,
                                QPointF *paintStartPosition) const;
    void viewItemDrawText(QPainter *p, const QStyleOptionViewItem *option, const QRect &rect) const;
    void drawViewItem(const QStyleOption *option,
                      QPainter *painter,
                      const QWidget *widget) const;
    bool isViewItemCached(const QStyleOptionViewItem &option) const {
        return cachedOption_ != nullptr
               && (option.widget == cachedOption_->widget
                   && option.index == cachedOption_->index
                   && option.state == cachedOption_->state
                   && option.rect == cachedOption_->rect
                   && option.text == cachedOption_->text
                   && option.direction == cachedOption_->direction
                   && option.displayAlignment == cachedOption_->displayAlignment
                   && option.decorationAlignment == cachedOption_->decorationAlignment
                   && option.decorationPosition == cachedOption_->decorationPosition
                   && option.decorationSize == cachedOption_->decorationSize
                   && option.features == cachedOption_->features
                   && option.icon.isNull() == cachedOption_->icon.isNull()
                   && option.font == cachedOption_->font
                   && option.viewItemPosition == cachedOption_->viewItemPosition);
    }

  private slots:
    void forgetPopupOrigin(QObject *o);

    void forgetMovedMenu(QObject *o);

    void setAnimationOpacity();
    void setAnimationOpacityOut();

    /* Removes a widget from the list of translucent ones. */
    void noTranslucency(QObject *o);

    /* Removes a button from all special lists. */
    void removeFromSet(QObject *o);

    void removeAnimation(QObject *animation); // For transient scrollbars

  private:
    QSvgRenderer *defaultRndr_, *themeRndr_;
    ThemeConfig *defaultSettings_, *themeSettings_, *settings_;

    QString xdg_config_home;

    QTimer *opacityTimer_, *opacityTimerOut_;
    mutable int animationOpacity_, animationOpacityOut_; // A value >= 100 stops state change animation.
    /* The start state for state change animation */
    mutable QString animationStartState_, animationStartStateOut_;
    /* The widget whose state change is animated */
    QPointer<QWidget> animatedWidget_, animatedWidgetOut_;
    QHash<QWidget*, QPointer<QWidget>> popupOrigins_;

    /* List of menus that are moved because of their shadows. */
    QSet<const QWidget*> movedMenus_;

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
    /* So far, only VirtualBox has introduced itself as "Qt-subapplication" and
       doesn't accept compositing. */
    bool subApp_;
    /* Some apps shouldn't have translucent windows. */
    bool isOpaque_;

    /* Hacks */
    bool isDolphin_;
    bool isPcmanfm_;

    bool isKrita_; // Because of bugs in Krita 5.1.0

    /* The size of the slider handle with no tick mark (if it exists) */
    mutable int ticklessSliderHandleSize_;

    /* For identifying KisSliderSpinBox */
    bool isKisSlider_;

    /* Keep track of the sunken button (used instead of a private header for menu positioning). */
    //mutable KvPointer<QWidget> sunkenButton_;
    mutable QPointer<QWidget> sunkenButton_;

    /* For not getting the menu/tooltip shadows repeatedly.
       They're used to find blurred regions and also to position menus correctly. */
    QList<qreal> menuShadow_;
    QList<int> realMenuShadow_;
    QList<qreal> tooltipShadow_;

    /* Is this DE GTK-based? Currently Gnome and Pantheon are supported. */
    bool gtkDesktop_;
    /* This will be useful if we want to disable compositing under some DEs. */
    bool noComposite_;
    /* For correct updating on mouseover with active tab overlapping */
    QRect tabHoverRect_;

    /* For enforcing the text color of inactive selected items. */
    bool hasInactiveSelItemCol_;
    /* Does the toggled (active but unfocused) view-item have a high contrast with the pressed one? */
    bool toggledItemHasContrast_;

    /* The standard palette (for not setting it frequently): */
    mutable QPalette standardPalette_;

    /* List of menus drawn by Kvantum and their margins
       (for preventing redundant computations): */
    mutable QHash<const QWidget*, QList<int>> drawnMenus_;

    // For not searching the SVG file too often:
    mutable QHash<const QString, bool>expandedBorders_;
    mutable QHash<const QString, bool>flatArrows_;
    mutable QHash<const QString, bool>elements_;

    mutable QHash<const QObject*, Animation*> animations_; // For transient scrollbars

    // For item views:
    mutable QStyleOptionViewItem *cachedOption_;
    mutable QRect decorationRect_, displayRect_, checkRect_;
};
}

#endif
