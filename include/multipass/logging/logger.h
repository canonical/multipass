/*
 * Copyright (C) 2018-2021 Canonical, Ltd.
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

#ifndef MULTIPASS_LOGGER_H
#define MULTIPASS_LOGGER_H

#include <multipass/logging/cstring.h>
#include <multipass/logging/level.h>

#include <memory>
#include <string>

namespace multipass
{
namespace logging
{

class Logger
{
public:
    using UPtr = std::unique_ptr<Logger>;
    virtual ~Logger() = default;
    virtual void log(Level level, CString category, CString message) const = 0;
    Level get_logging_level()
    {
        return logging_level;
    };

protected:
    Logger(Level logging_level) : logging_level{logging_level} {};
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    const Level logging_level{Level::error};
};
} // namespace logging
} // namespace multipass
#endif // MULTIPASS_LOGGER_H
