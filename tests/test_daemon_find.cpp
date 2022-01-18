/*
 * Copyright (C) 2021 Canonical, Ltd.
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
#include "mock_vm_image_vault.h"
#include "mock_vm_workflow_provider.h"

#include <src/daemon/daemon.h>

#include <multipass/format.h>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
auto workflow_description_for(const std::string& workflow_name)
{
    return fmt::format("This is the {} workflow", workflow_name);
}
} // namespace

struct DaemonFind : public mpt::DaemonTestFixture
{
    mpt::MockPlatform::GuardedMock attr{mpt::MockPlatform::inject<NiceMock>()};
    mpt::MockPlatform* mock_platform = attr.first;
};

TEST_F(DaemonFind, blankQueryReturnsAllData)
{
    auto mock_image_host = std::make_unique<NiceMock<mpt::MockImageHost>>();
    auto mock_workflow_provider = std::make_unique<NiceMock<mpt::MockVMWorkflowProvider>>();

    static constexpr auto workflow1_name = "foo";
    static constexpr auto workflow2_name = "bar";

    EXPECT_CALL(*mock_workflow_provider, all_workflows()).WillOnce([] {
        std::vector<mp::VMImageInfo> workflow_info;
        mp::VMImageInfo info1, info2;

        info1.aliases.append(workflow1_name);
        info1.release_title = QString::fromStdString(workflow_description_for(workflow1_name));
        workflow_info.push_back(info1);

        info2.aliases.append(workflow2_name);
        info2.release_title = QString::fromStdString(workflow_description_for(workflow2_name));
        workflow_info.push_back(info2);

        return workflow_info;
    });

    config_builder.image_hosts.clear();
    config_builder.image_hosts.push_back(std::move(mock_image_host));
    config_builder.workflow_provider = std::move(mock_workflow_provider);
    mp::Daemon daemon{config_builder.build()};

    std::stringstream stream;
    send_command({"find"}, stream);

    EXPECT_THAT(stream.str(), AllOf(HasSubstr(mpt::default_alias), HasSubstr(mpt::default_release_info),
                                    HasSubstr(mpt::another_alias), HasSubstr(mpt::another_release_info),
                                    HasSubstr(fmt::format("{}:{}", mpt::snapcraft_remote, mpt::snapcraft_alias)),
                                    HasSubstr(mpt::snapcraft_release_info),
                                    HasSubstr(fmt::format("{}:{}", mpt::custom_remote, mpt::custom_alias)),
                                    HasSubstr(mpt::custom_release_info), HasSubstr(workflow1_name),
                                    HasSubstr(workflow_description_for(workflow1_name)), HasSubstr(workflow2_name),
                                    HasSubstr(workflow_description_for(workflow2_name))));

    EXPECT_EQ(total_lines_of_output(stream), 7);
}

TEST_F(DaemonFind, queryForDefaultReturnsExpectedData)
{
    auto mock_image_vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

    EXPECT_CALL(*mock_image_vault, all_info_for(_)).WillOnce([](const mp::Query& query) {
        mpt::MockImageHost mock_image_host;
        std::vector<std::pair<std::string, mp::VMImageInfo>> info;
        info.push_back(std::make_pair(mpt::release_remote, mock_image_host.mock_bionic_image_info));

        return info;
    });

    config_builder.vault = std::move(mock_image_vault);
    mp::Daemon daemon{config_builder.build()};

    std::stringstream stream;
    send_command({"find", "default"}, stream);

    EXPECT_THAT(stream.str(), AllOf(HasSubstr(mpt::default_alias), HasSubstr(mpt::default_release_info)));

    EXPECT_EQ(total_lines_of_output(stream), 2);
}

TEST_F(DaemonFind, queryForWorkflowReturnsExpectedData)
{
    auto mock_image_vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    auto mock_workflow_provider = std::make_unique<NiceMock<mpt::MockVMWorkflowProvider>>();

    static constexpr auto workflow_name = "foo";

    EXPECT_CALL(*mock_image_vault, all_info_for(_)).WillOnce(Throw(std::runtime_error("")));

    EXPECT_CALL(*mock_workflow_provider, info_for(_)).WillOnce([](auto...) {
        mp::VMImageInfo info;

        info.aliases.append(workflow_name);
        info.release_title = QString::fromStdString(workflow_description_for(workflow_name));

        return info;
    });

    config_builder.vault = std::move(mock_image_vault);
    config_builder.workflow_provider = std::move(mock_workflow_provider);
    mp::Daemon daemon{config_builder.build()};

    std::stringstream stream;
    send_command({"find", workflow_name}, stream);

    EXPECT_THAT(stream.str(), AllOf(HasSubstr(workflow_name), HasSubstr(workflow_description_for(workflow_name))));

    EXPECT_EQ(total_lines_of_output(stream), 2);
}

TEST_F(DaemonFind, unknownQueryReturnsError)
{
    auto mock_image_vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    auto mock_workflow_provider = std::make_unique<NiceMock<mpt::MockVMWorkflowProvider>>();

    EXPECT_CALL(*mock_image_vault, all_info_for(_)).WillOnce(Throw(std::runtime_error("")));

    EXPECT_CALL(*mock_workflow_provider, info_for(_)).WillOnce(Throw(std::out_of_range("")));

    config_builder.vault = std::move(mock_image_vault);
    config_builder.workflow_provider = std::move(mock_workflow_provider);
    mp::Daemon daemon{config_builder.build()};

    constexpr auto phony_name = "phony";
    std::stringstream stream;
    send_command({"find", phony_name}, trash_stream, stream);

    EXPECT_THAT(stream.str(),
                HasSubstr(fmt::format("Unable to find an image or workflow matching \"{}\"", phony_name)));
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

    EXPECT_EQ(total_lines_of_output(stream), 3);
}
