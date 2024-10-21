/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2019 <tsujan2000@gmail.com>
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

#ifndef THEMECONFIG_H
#define THEMECONFIG_H

#include "specs5.h"

class QSettings;

namespace Kvantum {

/*
   Class that loads theme settings.
 */
class ThemeConfig {
  public:
    ThemeConfig(const QString &theme);
    ~ThemeConfig();

    /*
       Loads a configuration from the filename of the given theme.
     */
    void load(const QString &theme);

    /*
       Sets the parent configuration of this theme config.
       Since we do not need any farther ancestor here, to
       avoid infinite loops, we set the parent only if it
       has no parent itself.
     */
    void setParent(ThemeConfig *parent)
    {
      if (!parent->parentConfig_)
        parentConfig_ = parent;
    }

    /* Returns the frame spec of the given widget. */
    frame_spec getFrameSpec(const QString &elementName);
    /* Returns the interior spec of the given widget. */
    interior_spec getInteriorSpec(const QString &elementName);
    /* Returns the indicator spec of the given widget. */
    indicator_spec getIndicatorSpec(const QString &elementName);
    /* Returns the label (text+icon) spec of the given widget. */
    label_spec getLabelSpec(const QString &elementName);
    /* Returns the size spec of the given widget. */
    size_spec getSizeSpec(const QString &elementName);
    /* Returns only those theme specs that are related to compositing. */
    theme_spec getCompositeSpec();
    /* Returns the theme spec of this theme. */
    theme_spec getThemeSpec();
    /* Returns the general color spec of this theme. */
    color_spec getColorSpec() const;
    /* Returns the hacks spec of this theme. */
    hacks_spec getHacksSpec() const;

    /* returns the list of supported elements for which settings are recognized */
    //static QStringList getManagedElements();

  private:
    /*
       Returns the value of the given key belonging to the given group.
       If the key is not found in the given group, the group of the
       "inherits" string will be searched for it. If the key is not
       found in it either and it also inherits from another group,
       the latter will be searched, and so forth. If still nothing
       is found and there is a parent config, it will be searched
       in the same way. This method is protected from infinite loops.
     */
    QVariant getValue(const QString &group, const QString& key, const QString &inherits) const;
    /*
       Returns the value of the given key belonging to the given group
       from the theme config file.
     */
    QVariant getValue(const QString &group, const QString& key) const;

    QSettings *settings_;
    ThemeConfig *parentConfig_;
    /*
       Remember specifications instead of getting them again and again!
    */
    QHash<QString, frame_spec> fSpecs_;
    QHash<QString, interior_spec> iSpecs_;
    QHash<QString, indicator_spec> dSpecs_;
    QHash<QString, label_spec> lSpecs_;
    QHash<QString, size_spec> sSpecs_;
    theme_spec compositeSpecs_;

    bool isX11_;

    bool nonIntegerScale;
};

}

#endif // THEMECONFIG_H
