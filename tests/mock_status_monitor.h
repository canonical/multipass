/*
 * Copyright (C) 2017 Canonical, Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#ifndef MULTIPASS_MOCK_STATUS_MONITOR_H
#define MULTIPASS_MOCK_STATUS_MONITOR_H

#include <gmock/gmock.h>
#include <multipass/vm_status_monitor.h>

namespace multipass
{
namespace test
{
struct MockVMStatusMonitor : public VMStatusMonitor
{
    MOCK_METHOD0(on_resume, void());
    MOCK_METHOD0(on_stop, void());
    MOCK_METHOD0(on_shutdown, void());
    MOCK_METHOD1(on_restart, void(const std::string&));
};
}
}
#endif // MULTIPASS_MOCK_STATUS_MONITOR_H
