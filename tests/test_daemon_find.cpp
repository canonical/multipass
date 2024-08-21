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
#include "mock_platform.h"
#include "mock_settings.h"
#include "mock_vm_blueprint_provider.h"
#include "mock_vm_image_vault.h"

#include <src/daemon/daemon.h>

#include <multipass/constants.h>
#include <multipass/exceptions/download_exception.h>
#include <multipass/format.h>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
auto blueprint_description_for(const std::string& blueprint_name)
{
    return fmt::format("This is the {} blueprint", blueprint_name);
}
} // namespace

struct DaemonFind : public mpt::DaemonTestFixture
{
    void SetUp() override
    {
        EXPECT_CALL(mock_settings, register_handler).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_settings, unregister_handler).Times(AnyNumber());
        EXPECT_CALL(mock_settings, get(Eq(mp::winterm_key))).WillRepeatedly(Return("none"));
    }

    mpt::MockPlatform::GuardedMock attr{mpt::MockPlatform::inject<NiceMock>()};
    mpt::MockPlatform* mock_platform = attr.first;

    mpt::MockSettings::GuardedMock mock_settings_injection = mpt::MockSettings::inject<StrictMock>();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;
};

TEST_F(DaemonFind, blankQueryReturnsAllData)
{
    auto mock_image_host = std::make_unique<NiceMock<mpt::MockImageHost>>();
    auto mock_blueprint_provider = std::make_unique<NiceMock<mpt::MockVMBlueprintProvider>>();

    static constexpr auto blueprint1_name = "foo";
    static constexpr auto blueprint2_name = "bar";

    EXPECT_CALL(*mock_blueprint_provider, all_blueprints()).WillOnce([] {
        std::vector<mp::VMImageInfo> blueprint_info;
        mp::VMImageInfo info1, info2;

        info1.aliases.append(blueprint1_name);
        info1.release_title = QString::fromStdString(blueprint_description_for(blueprint1_name));
        blueprint_info.push_back(info1);

        info2.aliases.append(blueprint2_name);
        info2.release_title = QString::fromStdString(blueprint_description_for(blueprint2_name));
        blueprint_info.push_back(info2);

        return blueprint_info;
    });

    config_builder.image_hosts.clear();
    config_builder.image_hosts.push_back(std::move(mock_image_host));
    config_builder.blueprint_provider = std::move(mock_blueprint_provider);
    mp::Daemon daemon{config_builder.build()};

    std::stringstream stream;
    send_command({"find"}, stream);

    EXPECT_THAT(stream.str(), AllOf(HasSubstr(mpt::default_alias), HasSubstr(mpt::default_release_info),
                                    HasSubstr(mpt::another_alias), HasSubstr(mpt::another_release_info),
                                    HasSubstr(fmt::format("{}:{}", mpt::custom_remote, mpt::custom_alias)),
                                    HasSubstr(mpt::custom_release_info), HasSubstr(blueprint1_name),
                                    HasSubstr(blueprint_description_for(blueprint1_name)), HasSubstr(blueprint2_name),
                                    HasSubstr(blueprint_description_for(blueprint2_name))));

    EXPECT_THAT(stream.str(),
                Not(AllOf(HasSubstr(fmt::format("{}:{}", mpt::snapcraft_remote, mpt::snapcraft_alias)),
                          HasSubstr(mpt::snapcraft_release_info))));

    EXPECT_EQ(total_lines_of_output(stream), 9);
}

TEST_F(DaemonFind, queryForDefaultReturnsExpectedData)
{
    auto mock_image_vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    auto mock_blueprint_provider = std::make_unique<NiceMock<mpt::MockVMBlueprintProvider>>();

    EXPECT_CALL(*mock_image_vault, all_info_for(_)).WillOnce([](const mp::Query& query) {
        mpt::MockImageHost mock_image_host;
        std::vector<std::pair<std::string, mp::VMImageInfo>> info;
        info.push_back(std::make_pair(mpt::release_remote, mock_image_host.mock_bionic_image_info));

        return info;
    });

    config_builder.vault = std::move(mock_image_vault);
    config_builder.blueprint_provider = std::move(mock_blueprint_provider);
    mp::Daemon daemon{config_builder.build()};

    std::stringstream stream;
    send_command({"find", "default", "--only-images"}, stream);

    EXPECT_THAT(stream.str(), AllOf(HasSubstr(mpt::default_alias), HasSubstr(mpt::default_release_info)));
    EXPECT_THAT(stream.str(), Not(HasSubstr("No blueprints found.")));

    EXPECT_EQ(total_lines_of_output(stream), 3);
}

TEST_F(DaemonFind, queryForBlueprintReturnsExpectedData)
{
    auto mock_image_vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    auto mock_blueprint_provider = std::make_unique<NiceMock<mpt::MockVMBlueprintProvider>>();

    static constexpr auto blueprint_name = "foo";

    config_builder.vault = std::move(mock_image_vault);
    config_builder.blueprint_provider = std::move(mock_blueprint_provider);
    mp::Daemon daemon{config_builder.build()};

    std::stringstream stream;
    send_command({"find", blueprint_name, "--only-blueprints"}, stream);

    EXPECT_THAT(stream.str(), AllOf(HasSubstr(blueprint_name), HasSubstr(blueprint_description_for(blueprint_name))));
    EXPECT_THAT(stream.str(), Not(HasSubstr("No images found.")));

    EXPECT_EQ(total_lines_of_output(stream), 3);
}

TEST_F(DaemonFind, unknownQueryReturnsEmpty)
{
    auto mock_image_vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    auto mock_blueprint_provider = std::make_unique<NiceMock<mpt::MockVMBlueprintProvider>>();

    EXPECT_CALL(*mock_image_vault, all_info_for(_)).WillOnce(Throw(std::runtime_error("")));

    EXPECT_CALL(*mock_blueprint_provider, info_for(_)).WillOnce(Return(std::nullopt));

    config_builder.vault = std::move(mock_image_vault);
    config_builder.blueprint_provider = std::move(mock_blueprint_provider);
    mp::Daemon daemon{config_builder.build()};

    constexpr auto phony_name = "phony";
    std::stringstream stream;
    send_command({"find", phony_name}, stream);

    EXPECT_THAT(stream.str(), HasSubstr("No images or blueprints found."));
}

TEST_F(DaemonFind, forByRemoteReturnsExpectedData)
{
    NiceMock<mpt::MockImageHost> mock_image_host;
    auto mock_image_vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

    EXPECT_CALL(*mock_image_vault, image_host_for(_)).WillOnce([&mock_image_host](auto...) {
        return &mock_image_host;
    });

    EXPECT_CALL(mock_image_host, all_images_for(_, _)).WillOnce([&mock_image_host](auto...) {
        std::vector<mp::VMImageInfo> images_info;

        images_info.push_back(mock_image_host.mock_bionic_image_info);
        images_info.push_back(mock_image_host.mock_another_image_info);

        return images_info;
    });

    config_builder.vault = std::move(mock_image_vault);
    mp::Daemon daemon{config_builder.build()};

    constexpr auto remote_name = "release:";
    std::stringstream stream;
    send_command({"find", remote_name}, stream);

    EXPECT_THAT(stream.str(), AllOf(HasSubstr(fmt::format("{}{}", remote_name, mpt::default_alias)),
                                    HasSubstr(mpt::default_release_info),
                                    HasSubstr(fmt::format("{}{}", remote_name, mpt::another_alias)),
                                    HasSubstr(mpt::another_release_info)));

    EXPECT_EQ(total_lines_of_output(stream), 4);
}

TEST_F(DaemonFind, invalidRemoteNameAndEmptySearchString)
{
    auto mock_image_vault = std::make_unique<NiceMock<const mpt::MockVMImageVault>>();

    constexpr std::string_view remote_name = "nonsense";
    const std::string error_msg = fmt::format(
        "Remote \'{}\' is not found. Please use `multipass find` for supported remotes and images.", remote_name);

    EXPECT_CALL(*mock_image_vault, image_host_for(_)).Times(1).WillOnce([&error_msg]() {
        throw std::runtime_error(error_msg);
        return nullptr;
    });

    config_builder.vault = std::move(mock_image_vault);
    mp::Daemon daemon{config_builder.build()};

    const std::string full_name = std::string(remote_name) + ":";
    std::stringstream cerr_Stream;
    // std::cout is the place holder here.
    send_command({"find", full_name}, std::cout, cerr_Stream);

    EXPECT_THAT(cerr_Stream.str(), HasSubstr(error_msg));
    EXPECT_EQ(total_lines_of_output(cerr_Stream), 1);
}

TEST_F(DaemonFind, invalidRemoteNameAndNonEmptySearchString)
{
    auto mock_image_vault = std::make_unique<NiceMock<const mpt::MockVMImageVault>>();

    constexpr std::string_view remote_name = "nonsense";
    const std::string error_msg = fmt::format(
        "Remote \'{}\' is not found. Please use `multipass find` for supported remotes and images.", remote_name);

    EXPECT_CALL(*mock_image_vault, image_host_for(_)).Times(1).WillOnce([&error_msg]() {
        throw std::runtime_error(error_msg);
        return nullptr;
    });

    config_builder.vault = std::move(mock_image_vault);
    mp::Daemon daemon{config_builder.build()};

    constexpr std::string_view search_string = "default";
    const std::string full_name = std::string(remote_name) + ":" + std::string(search_string);
    std::stringstream cerr_Stream;
    // std::cout is the place holder here.
    send_command({"find", full_name}, std::cout, cerr_Stream);

    EXPECT_THAT(cerr_Stream.str(), HasSubstr(error_msg));
    EXPECT_EQ(total_lines_of_output(cerr_Stream), 1);
}

TEST_F(DaemonFind, findWithoutForceUpdateCheckUpdateManifestsCall)
{
    auto mock_image_host = std::make_unique<NiceMock<mpt::MockImageHost>>();

    // only the daemon constructor invoke it once
    EXPECT_CALL(*mock_image_host, update_manifests(false)).Times(1);
    // overwrite the default emplaced StubVMImageHost
    config_builder.image_hosts[0] = std::move(mock_image_host);
    const mp::Daemon daemon{config_builder.build()};

    send_command({"find"});
}

TEST_F(DaemonFind, UpdateManifestsThrowTriggersTheFailedCaseEventHandlerOfAsyncPeriodicDownloadTask)
{
    auto mock_image_host = std::make_unique<NiceMock<mpt::MockImageHost>>();

    // daemon constructor which constructs update_manifests_all_task calls this.
    EXPECT_CALL(*mock_image_host, update_manifests(false)).Times(1).WillOnce([]() {
        throw mp::DownloadException{"dummy_url", "dummy_cause"};
    });

    // overwrite the default emplaced StubVMImageHost
    config_builder.image_hosts[0] = std::move(mock_image_host);
    const mp::Daemon daemon{config_builder.build()};

    // need it because mp::Daemon destructor which destructs qfuture and qfuturewatcher does not wait the async task to
    // finish. As a consequence, the event handler is not guaranteed to be called without send_command({"find"});
    send_command({"find"});
}

TEST_F(DaemonFind, findForceUpdateCheckUpdateManifestsCalls)
{
    auto mock_image_host = std::make_unique<NiceMock<mpt::MockImageHost>>();

    // daemon constructor invoke it first and find --force-update invoke it with force flag true after
    const testing::InSequence sequence; // Force the following expectations to occur in order
    EXPECT_CALL(*mock_image_host, update_manifests(false)).Times(1);
    EXPECT_CALL(*mock_image_host, update_manifests(true)).Times(1);

    config_builder.image_hosts[0] = std::move(mock_image_host);
    const mp::Daemon daemon{config_builder.build()};
    send_command({"find", "--force-update"});
}

TEST_F(DaemonFind, findForceUpdateRemoteCheckUpdateManifestsCalls)
{
    // this unit test requires and mock image_host_for of vault, so
    // auto image_host = config->vault->image_host_for(remote);
    // can have a legal value in image_host variable and avoid seg fault in the next
    // line

    auto mock_image_host = std::make_unique<NiceMock<mpt::MockImageHost>>();
    auto mock_image_vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

    EXPECT_CALL(*mock_image_vault, image_host_for(_)).WillOnce([image_host_ptr = mock_image_host.get()](auto...) {
        return image_host_ptr;
    });

    const testing::InSequence sequence;
    EXPECT_CALL(*mock_image_host, update_manifests(false)).Times(1);
    EXPECT_CALL(*mock_image_host, update_manifests(true)).Times(1);

    config_builder.vault = std::move(mock_image_vault);
    config_builder.image_hosts[0] = std::move(mock_image_host);
    const mp::Daemon daemon{config_builder.build()};

    send_command({"find", "release:", "--force-update"});
}

TEST_F(DaemonFind, findForceUpdateRemoteSearchNameCheckUpdateManifestsCalls)
{
    auto mock_image_host = std::make_unique<NiceMock<mpt::MockImageHost>>();

    const testing::InSequence sequence;
    EXPECT_CALL(*mock_image_host, update_manifests(false)).Times(1);
    EXPECT_CALL(*mock_image_host, update_manifests(true)).Times(1);

    config_builder.image_hosts[0] = std::move(mock_image_host);
    const mp::Daemon daemon{config_builder.build()};

    send_command({"find", "release:22.04", "--force-update"});
}
