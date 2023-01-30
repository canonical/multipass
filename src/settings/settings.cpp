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

#include <multipass/settings/settings.h>

#include <memory>
#include <stdexcept>

namespace mp = multipass;

mp::Settings::Settings(const Singleton<Settings>::PrivatePass& pass) : Singleton<Settings>::Singleton{pass}
{
}

auto mp::Settings::register_handler(std::unique_ptr<SettingsHandler> handler) -> SettingsHandler*
{
    assert(handler && "can't have null settings handler"); // TODO use a `not_null` type (e.g. gsl::not_null)

    auto ret = handler.get();
    handlers.push_back(std::move(handler));

    return ret;
}

void mp::Settings::unregister_handler(SettingsHandler* handler)
{
    auto it = std::find_if(handlers.begin(), handlers.end(), [handler](const auto& uptr) { // trust me clang-format
        return uptr.get() == handler;
    });

    if (it != handlers.end())
        handlers.erase(it);
}

std::set<QString> multipass::Settings::keys() const
{
    std::set<QString> ret{};
    if (!handlers.empty())
    {
        auto it = handlers.cbegin();
        assert(*it && "can't have null settings handler"); // TODO use a `not_null` type (e.g. gsl::not_null)
        ret = (*it)->keys();

        while (++it != handlers.cend())
        {
            assert(*it && "can't have null settings handler"); // TODO use a `not_null` type (e.g. gsl::not_null)
            ret.merge((*it)->keys());
        }
    }

    return ret;
}

// TODO try installing yaml backend
QString mp::Settings::get(const QString& key) const
{
    for (const auto& handler : handlers)
    {
        try
        {
            assert(handler && "can't have null settings handler"); // TODO use a `not_null` type (e.g. gsl::not_null)
            return handler->get(key);
        }
        catch (const UnrecognizedSettingException&)
        {
            continue;
        }
    }

    throw UnrecognizedSettingException{key};
}

void mp::Settings::set(const QString& key, const QString& val)
{
    auto success = false;
    for (const auto& handler : handlers)
    {
        try
        {
            assert(handler && "can't have null settings handler"); // TODO use a `not_null` type (e.g. gsl::not_null)
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
