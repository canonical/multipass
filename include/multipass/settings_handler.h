/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#include "disabled_copy_move.h"

#include <QString>

#include <set>

namespace multipass
{
class SettingsHandler : private DisabledCopyMove
{
public:
    SettingsHandler() = default;
    virtual ~SettingsHandler() = default;


    virtual QString get(const QString& key) const = 0;                  // should throw on unknown key @TODO@ricab doc
    virtual void set(const QString& key, const QString& val) const = 0; // should throw on unknown key or bad settings

    /**
     * Obtain the keys, or key templates, that this SettingHandler handles.
     * @warning Templates are meant for human interpretation (e.g. <tt>local.@<instance@>.cpus</tt>). They cannot be
     * used in get/set as actual keys.
     * @return The set of keys or key templates that this SettingHandler handles.
     */
    virtual std::set<QString> keys() const = 0;
};

} // namespace multipass

#endif // MULTIPASS_SETTINGS_HANDLER_H
