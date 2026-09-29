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

#include "common.h"
#include "daemon_test_fixture.h"
#include "mock_cert_provider.h"
#include "mock_image_host.h"
#include "mock_permission_utils.h"
#include "mock_platform.h"
#include "mock_settings.h"
#include "mock_utils.h"
#include "mock_vm_image_vault.h"

#include <src/daemon/daemon.h>

#include <multipass/constants.h>
#include <multipass/exceptions/download_exception.h>
#include <multipass/format.h>

using namespace testing;

namespace multipass::test
{

struct DaemonRemotes : public DaemonTestFixture
{
    void SetUp() override
    {
        auto vault = std::make_unique<NiceMock<MockVMImageVault>>();
        mock_vault = vault.get();
        config_builder.vault = std::move(vault);

        EXPECT_CALL(mock_settings, register_handler).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_settings, unregister_handler).Times(AnyNumber());
        EXPECT_CALL(mock_settings, get(Eq(winterm_key))).WillRepeatedly(Return("none"));
        ON_CALL(mock_utils, contents_of(_)).WillByDefault(Return(root_cert));
    }

    void TearDown() override
    {
        mock_vault = nullptr;
        config_builder.vault.reset();
    }

    MockPlatform::GuardedMock attr{MockPlatform::inject<NiceMock>()};
    MockPlatform* mock_platform = attr.first;

    MockSettings::GuardedMock mock_settings_injection = MockSettings::inject<StrictMock>();
    MockSettings& mock_settings = *mock_settings_injection.first;

    const MockPermissionUtils::GuardedMock mock_permission_utils_injection =
        MockPermissionUtils::inject<NiceMock>();
    MockPermissionUtils& mock_permission_utils = *mock_permission_utils_injection.first;

    MockUtils::GuardedMock mock_utils_injection{MockUtils::inject<NiceMock>()};
    MockUtils& mock_utils = *mock_utils_injection.first;

    NiceMock<MockVMImageVault>* mock_vault = nullptr;
};

TEST_F(DaemonRemotes, returnsKnownRemotes)
{
    const auto remote1 = std::string{"remote1"};
    const auto remote2 = std::string{"remote2"};

    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(*mock_vault, fetch_remotes())
        .WillOnce(Return(std::vector<std::string>{remote1, remote2}));

    std::stringstream stream;
    send_command({"images", "--remotes"}, stream);

    EXPECT_THAT(stream.str(), AllOf(HasSubstr(remote1), HasSubstr(remote2)));
    EXPECT_EQ(total_lines_of_output(stream), 3);
}

} // namespace multipass::test
