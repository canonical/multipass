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

#pragma once

#include <concepts>
#include <string>
#include <utility>
#include <vector>

namespace multipass
{
class UserMessages
{
public:
    using iterator = std::vector<std::string>::iterator;
    using const_iterator = std::vector<std::string>::const_iterator;

    iterator begin()
    {
        return _messages.begin();
    }
    iterator end()
    {
        return _messages.end();
    }

    const_iterator begin() const
    {
        return _messages.begin();
    }
    const_iterator end() const
    {
        return _messages.end();
    }

    void clear()
    {
        _messages.clear();
    }

    template <std::convertible_to<std::string> String>
    void add_message(String&& message)
    {
        _messages.emplace_back(std::forward<String>(message));
    }

    UserMessages() = default;

    // Disable copying to avoid string copies
    UserMessages(const UserMessages&) = delete;
    UserMessages& operator=(const UserMessages&) = delete;

    UserMessages(UserMessages&&) = default;
    UserMessages& operator=(UserMessages&&) = default;

private:
    // The message vector is wrapped to minimize the API surface
    std::vector<std::string> _messages;
};
} // namespace multipass
