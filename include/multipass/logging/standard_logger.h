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

#ifndef MULTIPASS_STANDARD_LOGGER_H
#define MULTIPASS_STANDARD_LOGGER_H

#include <iosfwd>
#include <multipass/logging/logger.h>

namespace multipass
{
namespace logging
{
class StandardLogger : public Logger
{
public:
    /**
     * Default StandardLogger constructor
     *
     * @param [in] level Level of the logger.
     * The log calls with level below this will be filtered out.
     */
    StandardLogger(Level level);

    /**
     * Construct a new Standard Logger object
     *
     * @param [in] level Level of the logger. The log calls
     * with level below this will be filtered out.
     * @param [in] target ostream to write the output to
     */
    StandardLogger(Level level, std::ostream& target);

    /**
     * Log a message to the StandardLogger's target ostream.
     *
     * @param [in] level Log level
     * @param [in] category Log category
     * @param [in] message Log message
     */
    void log(Level level, std::string_view category, std::string_view message) const override;

private:
    std::ostream& target; // < Target ostream to write the log messages.
};
} // namespace logging
} // namespace multipass
#endif // MULTIPASS_STANDARD_LOGGER_H
