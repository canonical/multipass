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
#include "mock_singleton_helpers.h"

#include <multipass/platform.h>
#include <multipass/socket.h>

namespace multipass::test
{
class MockPlatform : public platform::Platform
{
public:
    MockPlatform(const PrivatePass& pass) : platform::Platform(pass)
    {
        EXPECT_CALL(*this, set_server_socket_restrictions)
            .Times(testing::AnyNumber())
            .WillRepeatedly(testing::Return());
        // get_cpus() gained a mock override (see #5061). Default it to an effectively
        // unlimited host so tests that don't care about the CPU ceiling are unaffected
        // and host-independent; tests exercising the ceiling override this with EXPECT_CALL.
        // The value must exceed the largest cpu count any existing test sets (some reuse a
        // generic numeric value of 134217728 across all numeric properties).
        // NOTE: get_cpus() is also called by the daemon_info RPC (daemon.cpp); no test
        // currently exercises DaemonInfoRequest, but any future one will see this default
        // rather than a real count -- override it there with EXPECT_CALL if it matters.
        ON_CALL(*this, get_cpus()).WillByDefault(testing::Return(2'000'000'000));
    };

    MOCK_METHOD((std::map < std::string, NetworkInterfaceInfo) >,
                get_network_interfaces_info,
                (),
                (const, override));
    MOCK_METHOD(bool, is_backend_supported, (const QString&), (const, override));
    MOCK_METHOD(int, chown, (const char*, unsigned int, unsigned int), (const, override));
    MOCK_METHOD(bool,
                set_permissions,
                (const std::filesystem::path&, std::filesystem::perms, bool),
                (const, override));
    MOCK_METHOD(bool, take_ownership, (const std::filesystem::path&), (const, override));
    MOCK_METHOD(void, setup_permission_inheritance, (bool), (const, override));
    MOCK_METHOD(bool, link, (const char*, const char*), (const, override));
    MOCK_METHOD(bool, symlink, (const char*, const char*, bool), (const, override));
    MOCK_METHOD(int, utime, (const char*, int, int), (const, override));
    MOCK_METHOD(void,
                create_alias_script,
                (const std::string&, const AliasDefinition&),
                (const, override));
    MOCK_METHOD(void, remove_alias_script, (const std::string&), (const, override));
    MOCK_METHOD(void,
                set_server_socket_restrictions,
                (const std::string&, const bool),
                (const, override));
    MOCK_METHOD(QString, multipass_storage_location, (), (const, override));
    MOCK_METHOD(int, get_cpus, (), (const, override));
    MOCK_METHOD(SettingSpec::Set, extra_daemon_settings, (), (const, override));
    MOCK_METHOD(SettingSpec::Set, extra_client_settings, (), (const, override));
    MOCK_METHOD(QString, daemon_config_home, (), (const, override));
    MOCK_METHOD(QString, default_driver, (), (const, override));
    MOCK_METHOD(QString, default_privileged_mounts, (), (const, override));
    MOCK_METHOD(QString, get_username, (), (const, override));
    MOCK_METHOD(std::string, bridge_nomenclature, (), (const, override));
    MOCK_METHOD(bool, subnet_used_locally, (Subnet), (const, override));
    MOCK_METHOD(Subnet, get_preferred_subnet, (const std::filesystem::path&), (const, override));
    MOCK_METHOD(std::filesystem::path, get_root_cert_dir, (), (const, override));
    MOCK_METHOD(void, shutdown_socket, (Socket), (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockPlatform, Platform);
};
} // namespace multipass::test
