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
        EXPECT_CALL(mock_settings, get(Eq(mp::driver_key))).WillRepeatedly(Return("nohk")); // TODO hk migration, remove
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
                                    HasSubstr(fmt::format("{}:{}", mpt::snapcraft_remote, mpt::snapcraft_alias)),
                                    HasSubstr(mpt::snapcraft_release_info),
                                    HasSubstr(fmt::format("{}:{}", mpt::custom_remote, mpt::custom_alias)),
                                    HasSubstr(mpt::custom_release_info), HasSubstr(blueprint1_name),
                                    HasSubstr(blueprint_description_for(blueprint1_name)), HasSubstr(blueprint2_name),
                                    HasSubstr(blueprint_description_for(blueprint2_name))));

    EXPECT_EQ(total_lines_of_output(stream), 10);
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
