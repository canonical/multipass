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

#include <multipass/settings/settings_handler.h>

namespace multipass
{
class RemoteSettingsHandler : public SettingsHandler
{
public:
    explicit RemoteSettingsHandler(QString key_prefix);

    QString get(const QString& key) const override;
    void set(const QString& key, const QString& val) const override;
    std::set<QString> keys() const override;

private:
    QString key_prefix;
};
} // namespace multipass

#ifndef MULTIPASS_REMOTE_SETTINGS_HANDLER_H
#define MULTIPASS_REMOTE_SETTINGS_HANDLER_H

#endif // MULTIPASS_REMOTE_SETTINGS_HANDLER_H
