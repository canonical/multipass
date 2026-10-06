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

#include <multipass/rpc/multipass.pb.h>

#include <src/daemon/daemon.h>
#include <tests/unit/daemon_test_fixture.h>
#include <tests/unit/mock_permission_utils.h>
#include <tests/unit/mock_platform.h>
#include <tests/unit/mock_vm_image_vault.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <utility>

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
    }

    void TearDown() override
    {
        mock_vault = nullptr;
        config_builder.vault.reset();
    }

    MockPlatform::GuardedMock mock_platform_guard = MockPlatform::inject<NiceMock>();

    MockPermissionUtils::GuardedMock mock_permission_utils_guard =
        MockPermissionUtils::inject<NiceMock>();

    NiceMock<MockVMImageVault>* mock_vault = nullptr;
};

TEST_F(DaemonRemotes, returnsKnownRemotesExceptDefault)
{
    const auto remote1 = std::string{"remote1"};
    const auto remote2 = std::string{"remote2"};
    const auto default_remote = std::string{""};

    EXPECT_CALL(*mock_vault, fetch_remotes())
        .WillOnce(Return(std::vector<std::string>{remote1, remote2, default_remote}));

    auto daemon = mp::Daemon{config_builder.build()};
    EXPECT_THAT(process_request(daemon, RemotesRequest{}, &Daemon::remotes),
                Property(&RemotesReply::remotes, ElementsAre(remote1, remote2)));
}

} // namespace multipass::test
