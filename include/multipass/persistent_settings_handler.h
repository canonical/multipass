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

#ifndef MULTIPASS_PERSISTENT_SETTINGS_HANDLER_H
#define MULTIPASS_PERSISTENT_SETTINGS_HANDLER_H

#include "disabled_copy_move.h"
#include "settings_handler.h"

#include <map>
#include <memory>
#include <mutex>

namespace multipass
{
class PersistentSetting : public DisabledCopyMove
{
public:
    using UPtr = std::unique_ptr<PersistentSetting>;
    virtual ~PersistentSetting() = default;

    virtual QString get_key() const = 0;
    virtual QString get_default() const = 0;
    virtual QString interpret(const QString& val) const = 0;
};

class BasicPersistentSetting : public PersistentSetting
{
public:
    BasicPersistentSetting(QString key, QString default_);
    QString get_key() const override;
    QString get_default() const override;
    QString interpret(const QString& val) const override;

protected:
    QString key;
    QString default_;
};

class PersistentSettingsHandler : public SettingsHandler
{
public:
    PersistentSettingsHandler(QString filename, std::map<QString, QString> defaults);
    QString get(const QString& key) const override;
    void set(const QString& key, const QString& val) const override;
    std::set<QString> keys() const override;

private:
    const QString& get_default(const QString& key) const; // throws on unknown key

private:
    QString filename;
    std::map<QString, QString> defaults;
    mutable std::mutex mutex;
};
} // namespace multipass

#endif // MULTIPASS_PERSISTENT_SETTINGS_HANDLER_H
