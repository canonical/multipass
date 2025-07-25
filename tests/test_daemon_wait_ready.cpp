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
#include "mock_image_host.h"
#include "mock_settings.h"
#include "mock_permission_utils.h"
#include "mock_utils.h"
#include "mock_cert_provider.h"

#include <src/daemon/daemon.h>

#include <multipass/exceptions/download_exception.h>
#include <multipass/format.h>

#include <future>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

struct DaemonWaitReady : public mpt::DaemonTestFixture
{
    void SetUp() override
    {
        EXPECT_CALL(mock_settings, register_handler).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_settings, unregister_handler).Times(AnyNumber());
        ON_CALL(mock_utils, contents_of(_)).WillByDefault(Return(mpt::root_cert));
    }
    
    mpt::MockSettings::GuardedMock mock_settings_injection =
        mpt::MockSettings::inject<StrictMock>();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;

    const mpt::MockPermissionUtils::GuardedMock mock_permission_utils_injection =
        mpt::MockPermissionUtils::inject<NiceMock>();
    mpt::MockPermissionUtils& mock_permission_utils = *mock_permission_utils_injection.first;

    mpt::MockUtils::GuardedMock mock_utils_injection{mpt::MockUtils::inject<NiceMock>()};
    mpt::MockUtils& mock_utils = *mock_utils_injection.first;

    const std::string wait_msg = fmt::format("Waiting for Multipass daemon to be ready");
};

TEST_F(DaemonWaitReady, checkUpdateManifestCall)
{
    auto mock_image_host = std::make_unique<NiceMock<mpt::MockImageHost>>();

    EXPECT_CALL(*mock_image_host, update_manifests(false)).Times(1).WillOnce(Return());

    config_builder.image_hosts[0] = std::move(mock_image_host);

    mp::Daemon daemon{config_builder.build()};

    std::stringstream out_stream, cerr_stream;
    send_command({"wait-ready"}, out_stream, cerr_stream);

    EXPECT_THAT(out_stream.str(), HasSubstr(wait_msg));
    EXPECT_TRUE(cerr_stream.str().empty());
}

TEST_F(DaemonWaitReady, updateManifestsThrowTriggersTheFailedCaseEventHandlerOfAsyncPeriodicDownloadTask)
{
    auto mock_image_host = std::make_unique<NiceMock<mpt::MockImageHost>>();

    EXPECT_CALL(*mock_image_host, update_manifests(false)).Times(1).WillOnce([]() {
        throw mp::DownloadException{"dummy_url", "dummy_cause"};
    });

    config_builder.image_hosts[0] = std::move(mock_image_host);
    mp::Daemon daemon{config_builder.build()};
    
    send_command({"wait-ready"});
}