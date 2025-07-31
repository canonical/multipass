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
#include "file_operations.h"
#include "mock_cert_provider.h"
#include "mock_cert_store.h"
#include "mock_client_rpc.h"
#include "mock_daemon.h"
#include "mock_permission_utils.h"
#include "mock_platform.h"
#include "mock_standard_paths.h"
#include "mock_utils.h"
#include "stub_terminal.h"
#include "temp_dir.h"

#include <multipass/cli/client_common.h>
#include <multipass/utils.h>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct TestClientCommon : public mpt::DaemonTestFixture
{
    TestClientCommon()
    {
        ON_CALL(mpt::MockStandardPaths::mock_instance(),
                writableLocation(mp::StandardPaths::GenericDataLocation))
            .WillByDefault(Return(temp_dir.path()));
        ON_CALL(*mock_utils, contents_of(_)).WillByDefault(Return(mpt::root_cert));
        // delegate some functions to the orignal implementation
        ON_CALL(*mock_utils,
                make_dir(A<const QDir&>(), A<const QString&>(), A<std::filesystem::perms>()))
            .WillByDefault([](const QDir& dir,
                              const QString& name,
                              std::filesystem::perms permissions) -> mp::Path {
                return MP_UTILS.Utils::make_dir(dir, name, permissions);
            });
        ON_CALL(*mock_utils, make_dir(A<const QDir&>(), A<std::filesystem::perms>()))
            .WillByDefault([](const QDir& dir, std::filesystem::perms permissions) -> mp::Path {
                return MP_UTILS.Utils::make_dir(dir, permissions);
            });
    }

    mpt::MockDaemon make_secure_server()
    {
        EXPECT_CALL(*mock_cert_provider, PEM_certificate()).Times(1);
        EXPECT_CALL(*mock_cert_provider, PEM_signing_key()).Times(1);

        config_builder.server_address = server_address;
        config_builder.cert_provider = std::move(mock_cert_provider);

        return mpt::MockDaemon(config_builder.build());
    }

    std::unique_ptr<NiceMock<mpt::MockCertProvider>> mock_cert_provider{
        std::make_unique<NiceMock<mpt::MockCertProvider>>()};
    std::unique_ptr<mpt::MockCertStore> mock_cert_store{std::make_unique<mpt::MockCertStore>()};
    const mpt::MockUtils::GuardedMock utils_attr{mpt::MockUtils::inject<NiceMock>()};
    const mpt::MockUtils* mock_utils = utils_attr.first;

    const mpt::MockPermissionUtils::GuardedMock mock_permission_utils_injection =
        mpt::MockPermissionUtils::inject<NiceMock>();

    const std::string server_address{"localhost:50052"};
    mpt::TempDir temp_dir;
};
} // namespace

TEST_F(TestClientCommon, usesCommonCertWhenItExists)
{
    const auto common_cert_dir =
        MP_UTILS.make_dir(temp_dir.path(), QString(mp::common_client_cert_dir).remove(0, 1));
    const auto common_client_cert_file = common_cert_dir + "/" + mp::client_cert_file;
    const auto common_client_key_file = common_cert_dir + "/" + mp::client_key_file;

    mpt::make_file_with_content(common_client_cert_file, mpt::cert);
    mpt::make_file_with_content(common_client_key_file, mpt::key);

    EXPECT_TRUE(mp::client::make_channel(server_address, *mp::client::get_cert_provider()));
}

TEST_F(TestClientCommon, noValidCertsCreatesNewCommonCert)
{
    const auto common_cert_dir = temp_dir.path() + mp::common_client_cert_dir;

    const auto [mock_platform, _] = mpt::MockPlatform::inject<NiceMock>();
    // move the multipass_root_cert.pem into the temporary directory so it will be deleted
    // automatically later
    EXPECT_CALL(*mock_platform, get_root_cert_dir())
        .WillRepeatedly(Return(std::filesystem::path{common_cert_dir.toStdU16String()}));

    EXPECT_CALL(*mock_cert_store, empty).WillOnce(Return(false));
    config_builder.client_cert_store = std::move(mock_cert_store);

    mpt::MockDaemon daemon{make_secure_server()};

    EXPECT_TRUE(mp::client::make_channel(server_address, *mp::client::get_cert_provider()));
    EXPECT_TRUE(QFile::exists(common_cert_dir + "/" + mp::client_cert_file));
    EXPECT_TRUE(QFile::exists(common_cert_dir + "/" + mp::client_key_file));
}

TEST(TestClientHandleUserPassword, defaultHasNoPassword)
{
    auto client = std::make_unique<mpt::MockClientReaderWriter<mp::MountRequest, mp::MountReply>>();
    std::stringstream trash_stream;
    mpt::StubTerminal term(trash_stream, trash_stream, trash_stream);

    EXPECT_CALL(*client, Write(Property(&mp::MountRequest::password, IsEmpty()), _)).Times(1);

    mp::cmd::handle_password(client.get(), &term);
}
