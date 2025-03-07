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

#include "win_event_logger.h"

#include <fmt/format.h>

#include <windows.h>

#include <array>

namespace mpl = multipass::logging;

namespace
{
constexpr auto as_eventlog_type(mpl::Level level) noexcept
{
    switch (level)
    {
    case mpl::Level::info:
    case mpl::Level::debug:
    case mpl::Level::trace:
        return EVENTLOG_INFORMATION_TYPE;
    case mpl::Level::error:
        return EVENTLOG_ERROR_TYPE;
    case mpl::Level::warning:
        return EVENTLOG_WARNING_TYPE;
    }
    return EVENTLOG_AUDIT_FAILURE;
}

class EventSource
{
public:
    EventSource(const std::string& name) : event_source{RegisterEventSource(nullptr, name.c_str())}
    {
    }

    ~EventSource()
    {
        DeregisterEventSource(event_source);
    }

    HANDLE get() const
    {
        return event_source;
    }

private:
    HANDLE event_source;
};
} // namespace

mpl::EventLogger::EventLogger(mpl::Level level) : logging_level{level}
{
}

void mpl::EventLogger::log(mpl::Level level, std::string_view category, std::string_view message) const
{
    const static std::string event_source_name{"Multipass"};
    if (level <= logging_level)
    {
        /**
         * FIXME: It's not ideal that the code Register/Deregister the event source
         * in every single log call. We should refactor this.
         */
        EventSource ev_source(event_source_name);
        constexpr static WORD category_id{0};
        constexpr static WORD event_id{1};
        constexpr static PSID security_id{nullptr};
        constexpr static DWORD binary_size{0};
        constexpr static LPVOID raw_data{nullptr};

        const auto log_msg = fmt::format("[{}] {}\n", category, message);
        std::array<const char*, 1> messages = {log_msg.c_str()};
        const WORD num_strings{static_cast<WORD>(messages.size())};

        ReportEvent(ev_source.get(),
                    as_eventlog_type(level),
                    category_id,
                    event_id,
                    security_id,
                    num_strings,
                    binary_size,
                    messages.data(),
                    raw_data);
    }
}
