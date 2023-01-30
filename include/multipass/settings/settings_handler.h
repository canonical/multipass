/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef MULTIPASS_SETTINGS_HANDLER_H
#define MULTIPASS_SETTINGS_HANDLER_H

#include <multipass/disabled_copy_move.h>

#include <QString>

#include <set>

namespace multipass
{
class SettingsHandler : private DisabledCopyMove
{
public:
    SettingsHandler() = default;
    virtual ~SettingsHandler() = default;

    /**
     * Obtain the keys that this SettingsHandler handles.
     * @return The set of keys that this SettingsHandler handles.
     */
    virtual std::set<QString> keys() const = 0;

    /**
     * Get the value of the setting specified by @c key.
     * @param key The key identifying the requested setting.
     * @return A string representation of the value of the specified setting, according to this SettingsHandler's
     * interpretation.
     * @throws UnrecognizedSettingException When @c key does not identify a setting that this handler recognizes.
     * @note Descendents are free to throw other exceptions as well.
     */
    virtual QString get(const QString& key) const = 0;

    /**
     * Set the value of the setting specified by @c key to @val, according to this SettingsHandler's interpretation.
     * @param key The key identifying the setting to modify.
     * @param val A string representation of the value to assign to the setting. The actual value is derived according
     * to this SettingsHandler's interpretation.
     * @throws UnrecognizedSettingException When @c key does not identify a setting that this handler recognizes.
     * @throws InvalidSettingException When @c val does not represent a valid value for the setting identified by
     * @c key, according to this SettingHandler's interpretation.
     * @note Descendents are free to throw other exceptions as well.
     */
    virtual void set(const QString& key, const QString& val) = 0;
};

} // namespace multipass

#endif // MULTIPASS_SETTINGS_HANDLER_H
