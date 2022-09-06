/*
 * Copyright (C) 2019-2022 Canonical, Ltd.
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

#ifndef MULTIPASS_MOCK_MOUNT_HANDLER_H
#define MULTIPASS_MOCK_MOUNT_HANDLER_H

#include <multipass/mount_handlers/mount_handler.h>

#include "stub_ssh_key_provider.h"

namespace multipass
{
namespace test
{
class MockMountHandler : public MountHandler
{
public:
    MockMountHandler() : MountHandler{key_provider}
    {
    }

    MOCK_METHOD(void, init_mount, (VirtualMachine*, const std::string&, const VMMount&), (override));
    MOCK_METHOD(void, start_mount,
                (VirtualMachine*, ServerVariant, const std::string&, const std::chrono::milliseconds&), (override));
    MOCK_METHOD(void, stop_mount, (const std::string&, const std::string&), (override));
    MOCK_METHOD(void, stop_all_mounts_for_instance, (const std::string&), (override));
    MOCK_METHOD(bool, has_instance_already_mounted, (const std::string&, const std::string&), (const, override));

private:
    StubSSHKeyProvider key_provider;
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_MOCK_MOUNT_HANDLER_H
