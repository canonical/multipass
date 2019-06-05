/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#ifndef MULTIPASS_DAEMON_KILLER_H
#define MULTIPASS_DAEMON_KILLER_H

#include <memory>

namespace multipass
{
struct DaemonKiller
{
    virtual bool kill_daemon() const = 0;

    using UPtr = std::unique_ptr<DaemonKiller>;
    static UPtr make();
};
} // namespace multipass

inline auto multipass::DaemonKiller::make() -> UPtr // FIXME move to platform
{
    class FIXMEDaemonKiller : public DaemonKiller
    {
        bool kill_daemon() const override
        {
            return false;
        }
    };

    return std::make_unique<FIXMEDaemonKiller>();
}

#endif // MULTIPASS_DAEMON_KILLER_H
