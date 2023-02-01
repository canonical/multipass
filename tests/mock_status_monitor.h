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

#ifndef MULTIPASS_MOCK_STATUS_MONITOR_H
#define MULTIPASS_MOCK_STATUS_MONITOR_H

#include "common.h"

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
    MOCK_METHOD0(on_suspend, void());
    MOCK_METHOD1(on_restart, void(const std::string&));
    MOCK_METHOD2(persist_state_for, void(const std::string&, const VirtualMachine::State&));
    MOCK_METHOD2(update_metadata_for, void(const std::string&, const QJsonObject&));
    MOCK_METHOD1(retrieve_metadata_for, QJsonObject(const std::string&));
};
}
}
#endif // MULTIPASS_MOCK_STATUS_MONITOR_H
