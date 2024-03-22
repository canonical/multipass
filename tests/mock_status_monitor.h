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
    MOCK_METHOD(void, on_resume, (), (override));
    MOCK_METHOD(void, on_shutdown, (), (override));
    MOCK_METHOD(void, on_suspend, (), (override));
    MOCK_METHOD(void, on_restart, (const std::string&), (override));
    MOCK_METHOD(void, persist_state_for, (const std::string&, const VirtualMachine::State&), (override));
    MOCK_METHOD(void, update_metadata_for, (const std::string&, const QJsonObject&), (override));
    MOCK_METHOD(QJsonObject, retrieve_metadata_for, (const std::string&), (override));
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_MOCK_STATUS_MONITOR_H
