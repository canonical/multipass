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

#include <multipass/logging/log.h>

#include <multipass/format.h>

#include <QString>
#include <QtGlobal>

#include <shared_mutex>
#include <stdexcept>

namespace mpl = multipass::logging;

namespace
{
std::shared_timed_mutex mutex;
std::shared_ptr<multipass::logging::Logger> global_logger;

mpl::Level to_level(QtMsgType type)
{
    switch (type)
    {
    case QtDebugMsg:
        return mpl::Level::debug;
    case QtInfoMsg:
        return mpl::Level::info;
    case QtWarningMsg:
        return mpl::Level::warning;
    case QtCriticalMsg:
    case QtFatalMsg:
        return mpl::Level::error;
    }
    throw std::invalid_argument("Unknown Qt log message type");
}

void qt_message_handler(QtMsgType type, const QMessageLogContext&, const QString& message)
{
    auto msg = message.toLocal8Bit();
    mpl::log(to_level(type), "Qt", msg.constData());
}
} // namespace

void mpl::log(Level level, CString category, CString message)
{
    std::shared_lock<decltype(mutex)> lock{mutex};
    if (global_logger)
        global_logger->log(level, category, message);
    else
        fmt::print(stderr, "[{}] [{}] {}\n", as_string(level).c_str(), category.c_str(), message.c_str());
}

mpl::Level mpl::get_logging_level()
{
    if (global_logger)
    {
        return global_logger->get_logging_level();
    }

    return Level::error;
}

void mpl::set_logger(std::shared_ptr<Logger> logger)
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    global_logger = std::move(logger);
    qInstallMessageHandler(qt_message_handler);
}

auto mpl::get_logger() -> Logger* // for tests, don't rely on it lasting
{
    return global_logger.get();
}
