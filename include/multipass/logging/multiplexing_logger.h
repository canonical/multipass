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

#ifndef MULTIPASS_MULTIPLEXING_LOGGER_H
#define MULTIPASS_MULTIPLEXING_LOGGER_H

#include "logger.h"

#include <memory>
#include <shared_mutex>
#include <vector>

namespace multipass
{
namespace logging
{
class MultiplexingLogger : public Logger
{
public:
    explicit MultiplexingLogger(UPtr system_logger);
    void log(Level level, CString category, CString message) const override;
    void add_logger(const Logger* logger);
    void remove_logger(const Logger* logger);

private:
    UPtr system_logger;
    mutable std::shared_timed_mutex mutex;
    std::vector<const Logger*> loggers;
};
} // namespace logging
} // namespace multipass

#endif // MULTIPASS_MULTIPLEXING_LOGGER_H
