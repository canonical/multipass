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

#include "common.h"
#include "mock_virtual_machine.h"

#include <multipass/guest_mount_record.h>

#include <string>

namespace multipass::test
{
// Derive the command that gets used to un/deploy a GuestMountRecord
inline std::string mount_record_cmd(const std::string& source,
                                    const std::string& target,
                                    void (GuestMountRecord::*update)(VirtualMachine&) const)
{
    testing::NiceMock<MockVirtualMachine> recorder;
    std::string cmd;
    EXPECT_CALL(recorder, ssh_exec)
        .WillOnce(testing::DoAll(testing::SaveArg<0>(&cmd), testing::Return("")));

    (GuestMountRecord{source, target}.*update)(recorder);
    return cmd;
}
} // namespace multipass::test
