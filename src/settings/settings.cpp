/*
 * Copyright (C) 2019-2021 Canonical, Ltd.
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

#include <multipass/settings/settings.h>

#include <memory>
#include <stdexcept>

namespace mp = multipass;

mp::Settings::Settings(const Singleton<Settings>::PrivatePass& pass) : Singleton<Settings>::Singleton{pass}
{
}

void mp::Settings::register_handler(std::unique_ptr<SettingsHandler> handler)
{
    // TODO@ricab verify not null (would be nice to have gsl::not_null)
    handlers.push_back(std::move(handler));
}

std::set<QString> multipass::Settings::keys() const
{
    std::set<QString> ret{};
    if (!handlers.empty())
    {
        auto it = handlers.cbegin();
        ret = (*it)->keys();

        while (++it != handlers.cend())
            ret.merge((*it)->keys());
    }

    return ret;
}

// TODO try installing yaml backend
QString mp::Settings::get(const QString& key) const // TODO@ricab extract code common with set
{
    for (const auto& handler : handlers)
    {
        try
        {
            return handler->get(key);
        }
        catch (const UnrecognizedSettingException&)
        {
            continue;
        }
    }

    throw UnrecognizedSettingException{key};
}

void mp::Settings::set(const QString& key, const QString& val) // TODO@ricab extract code common with get
{
    auto success = false;
    for (const auto& handler : handlers)
    {
        try
        {
            handler->set(key, val);
            success = true; // don't return yet, give all handlers a chance to react
        }
        catch (const UnrecognizedSettingException&)
        {
            continue;
        }
    }

    if (!success)
        throw UnrecognizedSettingException{key};
}
