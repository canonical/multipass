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

#ifndef MULTIPASS_SETTINGS_H
#define MULTIPASS_SETTINGS_H

#include "settings_handler.h"

#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/singleton.h>

#include <QString>
#include <QVariant>

#include <set>

#define MP_SETTINGS multipass::Settings::instance()

namespace multipass
{
class Settings : public Singleton<Settings>
{
public:
    Settings(const Singleton<Settings>::PrivatePass&);

    virtual SettingsHandler* register_handler(std::unique_ptr<SettingsHandler> handler); // no nulls please
    virtual void unregister_handler(SettingsHandler* handler); // no-op if handler isn't registered

    /**
     * Obtain the keys that this Settings singleton knows about.
     * @return The set of keys that this Settings singleton knows about.
     */
    virtual std::set<QString> keys() const;

    /**
     * Get the value of the setting specified by @c key, as returned by the first registered handler that handles it.
     * @param key The key identifying the requested setting.
     * @return A string representation of the value of the specified setting, according to the corresponding
     * SettingsHandler's interpretation.
     * @throws UnrecognizedSettingException When @c key does not identify a setting that any registered handler
     * recognizes.
     * @note May also throw any other exceptions that occur when handling.
     */
    virtual QString get(const QString& key) const;

    /**
     * Set the value of the setting specified by @c key to @val, according to the interpretation of first registered
     * handler that handles the respective setting.
     * @param key The key identifying the setting to modify.
     * @param val A string representation of the value to assign to the setting. The actual value is derived according
     * to the corresponding SettingsHandler's interpretation.
     * @throws UnrecognizedSettingException When @c key does not identify a setting that any registered handler
     * recognizes.
     * @throws InvalidSettingException When @c val does not represent a valid value for the setting identified by
     * @c key, according to the corresponding SettingHandler's interpretation.
     * @note May also throw any other exceptions that occur when handling.
     */
    virtual void set(const QString& key, const QString& val);

    /**
     * Obtain a setting as a certain type
     * @tparam T The type to obtain the setting as
     * @param key The key that identifies the setting
     * @return The value that the setting converts to, when interpreted as type @p T. Follows the conversion rules used
     * by @c QVariant.
     * If the current @em value cannot be converted, a default @p T value is returned.
     * @throws UnsupportedSettingValueType<T> If QVariant cannot convert from a @c QString to type @c T. Note that
     * this is only thrown when the type itself can't be converted to. The actual value may still fail to convert, in
     * which case a default @c T value is returned.
     */
    template <typename T>
    T get_as(const QString& key) const;

private:
    std::vector<std::unique_ptr<SettingsHandler>> handlers;
};
} // namespace multipass

template <typename T>
T multipass::Settings::get_as(const QString& key) const
{
    auto var = QVariant{get(key)};
    if (var.canConvert<T>()) // Note: this only checks if the types themselves are convertible, not the values
        return var.value<T>();
    throw UnsupportedSettingValueType<T>(key);
}

#endif // MULTIPASS_SETTINGS_H
