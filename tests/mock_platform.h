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

#ifndef MULTIPASS_MOCK_PLATFORM_H
#define MULTIPASS_MOCK_PLATFORM_H

#include "common.h"
#include "mock_singleton_helpers.h"

#include <multipass/platform.h>

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
    };

    MOCK_METHOD((std::map < std::string, NetworkInterfaceInfo) >, get_network_interfaces_info, (), (const, override));
    MOCK_METHOD(QString, get_blueprints_url_override, (), (const, override));
    MOCK_METHOD(bool, is_remote_supported, (const std::string&), (const, override));
    MOCK_METHOD(bool, is_backend_supported, (const QString&), (const, override));
    MOCK_METHOD(bool, is_alias_supported, (const std::string&, const std::string&), (const, override));
    MOCK_METHOD(int, chmod, (const char*, unsigned int), (const, override));
    MOCK_METHOD(int, chown, (const char*, unsigned int, unsigned int), (const, override));
    MOCK_METHOD(bool, link, (const char*, const char*), (const, override));
    MOCK_METHOD(bool, symlink, (const char*, const char*, bool), (const, override));
    MOCK_METHOD(int, utime, (const char*, int, int), (const, override));
    MOCK_METHOD(void, create_alias_script, (const std::string&, const AliasDefinition&), (const, override));
    MOCK_METHOD(void, remove_alias_script, (const std::string&), (const, override));
    MOCK_METHOD(void, set_server_socket_restrictions, (const std::string&, const bool), (const, override));
    MOCK_METHOD(QString, multipass_storage_location, (), (const, override));
    MOCK_METHOD(SettingSpec::Set, extra_daemon_settings, (), (const, override));
    MOCK_METHOD(SettingSpec::Set, extra_client_settings, (), (const, override));
    MOCK_METHOD(QString, daemon_config_home, (), (const, override));
    MOCK_METHOD(QString, default_driver, (), (const, override));
    MOCK_METHOD(QString, default_privileged_mounts, (), (const, override));
    MOCK_METHOD(bool, is_image_url_supported, (), (const, override));
    MOCK_METHOD(QString, get_username, (), (const, override));
    MOCK_METHOD(std::string, bridge_nomenclature, (), (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockPlatform, Platform);
};
} // namespace multipass::test

#endif // MULTIPASS_MOCK_PLATFORM_H
