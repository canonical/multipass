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

#include <multipass/default_vm_workflow_provider.h>
#include <multipass/exceptions/workflow_minimum_exception.h>
#include <multipass/memory_size.h>
#include <multipass/url_downloader.h>
#include <multipass/utils.h>

#include "extra_assertions.h"
#include "mock_logger.h"
#include "mock_url_downloader.h"
#include "path.h"
#include "temp_dir.h"

#include <QFileInfo>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;

using namespace std::chrono_literals;
using namespace testing;

namespace
{
const QString test_workflows_zip{"/test-workflows.zip"};
const QString multipass_workflows_zip{"/multipass-workflows.zip"};

struct VMWorkflowProvider : public Test
{
    QString workflows_zip_url{QUrl::fromLocalFile(mpt::test_data_path()).toString() + test_workflows_zip};
    mp::URLDownloader url_downloader{std::chrono::seconds(10)};
    mpt::TempDir cache_dir;
    std::chrono::seconds default_ttl{1s};
};
} // namespace

TEST_F(VMWorkflowProvider, downloadsZipToExpectedLocation)
{
    mp::DefaultVMWorkflowProvider workflow_provider{workflows_zip_url, &url_downloader, cache_dir.path(), default_ttl};

    const QFileInfo original_zip{mpt::test_data_path() + test_workflows_zip};
    const QFileInfo downloaded_zip{cache_dir.path() + multipass_workflows_zip};

    EXPECT_TRUE(downloaded_zip.exists());
    EXPECT_EQ(downloaded_zip.size(), original_zip.size());
}

TEST_F(VMWorkflowProvider, invalidImageSchemeThrows)
{
    mp::DefaultVMWorkflowProvider workflow_provider{workflows_zip_url, &url_downloader, cache_dir.path(), default_ttl};

    mp::VirtualMachineDescription vm_desc{0, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    MP_EXPECT_THROW_THAT(workflow_provider.fetch_workflow_for("invalid-image-workflow", vm_desc), std::runtime_error,
                         mpt::match_what(StrEq("Unsupported image scheme in Workflow")));
}

TEST_F(VMWorkflowProvider, fetchTestWorkflow1ReturnsExpectedInfo)
{
    mp::DefaultVMWorkflowProvider workflow_provider{workflows_zip_url, &url_downloader, cache_dir.path(), default_ttl};

    mp::VirtualMachineDescription vm_desc{0, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    auto query = workflow_provider.fetch_workflow_for("test-workflow1", vm_desc);

    auto yaml_as_str = mp::utils::emit_yaml(vm_desc.vendor_data_config);

    EXPECT_EQ(query.release, "default");
    EXPECT_EQ(vm_desc.num_cores, 2);
    EXPECT_EQ(vm_desc.mem_size, mp::MemorySize("2G"));
    EXPECT_EQ(vm_desc.disk_space, mp::MemorySize("25G"));
    EXPECT_THAT(yaml_as_str, AllOf(HasSubstr("runcmd"), HasSubstr("echo \"Have fun!\"")));
}

TEST_F(VMWorkflowProvider, fetchTestWorkflow2ReturnsExpectedInfo)
{
    mp::DefaultVMWorkflowProvider workflow_provider{workflows_zip_url, &url_downloader, cache_dir.path(), default_ttl};

    mp::VirtualMachineDescription vm_desc{0, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    auto query = workflow_provider.fetch_workflow_for("test-workflow2", vm_desc);

    EXPECT_EQ(query.release, "bionic");
    EXPECT_EQ(query.remote_name, "daily");
    EXPECT_EQ(vm_desc.num_cores, 4);
    EXPECT_EQ(vm_desc.mem_size, mp::MemorySize("4G"));
    EXPECT_EQ(vm_desc.disk_space, mp::MemorySize("50G"));
    EXPECT_TRUE(vm_desc.vendor_data_config.IsNull());
}

TEST_F(VMWorkflowProvider, givenCoresLessThanMinimumThrows)
{
    mp::DefaultVMWorkflowProvider workflow_provider{workflows_zip_url, &url_downloader, cache_dir.path(), default_ttl};

    mp::VirtualMachineDescription vm_desc{1, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    MP_EXPECT_THROW_THAT(workflow_provider.fetch_workflow_for("test-workflow1", vm_desc), mp::WorkflowMinimumException,
                         mpt::match_what(AllOf(HasSubstr("Number of CPUs"), HasSubstr("2"))));
}

TEST_F(VMWorkflowProvider, givenMemLessThanMinimumThrows)
{
    mp::DefaultVMWorkflowProvider workflow_provider{workflows_zip_url, &url_downloader, cache_dir.path(), default_ttl};

    mp::VirtualMachineDescription vm_desc{0, mp::MemorySize{"1G"}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    MP_EXPECT_THROW_THAT(workflow_provider.fetch_workflow_for("test-workflow1", vm_desc), mp::WorkflowMinimumException,
                         mpt::match_what(AllOf(HasSubstr("Memory size"), HasSubstr("2G"))));
}

TEST_F(VMWorkflowProvider, givenDiskSpaceLessThanMinimumThrows)
{
    mp::DefaultVMWorkflowProvider workflow_provider{workflows_zip_url, &url_downloader, cache_dir.path(), default_ttl};

    mp::VirtualMachineDescription vm_desc{0, {}, mp::MemorySize{"20G"}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    MP_EXPECT_THROW_THAT(workflow_provider.fetch_workflow_for("test-workflow1", vm_desc), mp::WorkflowMinimumException,
                         mpt::match_what(AllOf(HasSubstr("Disk space"), HasSubstr("25G"))));
}

TEST_F(VMWorkflowProvider, higherOptionsIsNotOverriden)
{
    mp::DefaultVMWorkflowProvider workflow_provider{workflows_zip_url, &url_downloader, cache_dir.path(), default_ttl};

    mp::VirtualMachineDescription vm_desc{
        4, mp::MemorySize{"4G"}, mp::MemorySize{"50G"}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    workflow_provider.fetch_workflow_for("test-workflow1", vm_desc);

    EXPECT_EQ(vm_desc.num_cores, 4);
    EXPECT_EQ(vm_desc.mem_size, mp::MemorySize("4G"));
    EXPECT_EQ(vm_desc.disk_space, mp::MemorySize("50G"));
}

TEST_F(VMWorkflowProvider, infoForReturnsExpectedInfo)
{
    mp::DefaultVMWorkflowProvider workflow_provider{workflows_zip_url, &url_downloader, cache_dir.path(), default_ttl};

    auto workflow = workflow_provider.info_for("test-workflow2");

    ASSERT_EQ(workflow.aliases.size(), 1);
    EXPECT_EQ(workflow.aliases[0], "test-workflow2");
    EXPECT_EQ(workflow.release_title, "Another test workflow");
}

TEST_F(VMWorkflowProvider, allWorkflowsReturnsExpectedInfo)
{
    mp::DefaultVMWorkflowProvider workflow_provider{workflows_zip_url, &url_downloader, cache_dir.path(), default_ttl};

    auto workflows = workflow_provider.all_workflows();

    EXPECT_EQ(workflows.size(), 3ul);

    EXPECT_TRUE(std::find_if(workflows.cbegin(), workflows.cend(), [](const mp::VMImageInfo& workflow_info) {
                    return ((workflow_info.aliases.size() == 1) && (workflow_info.aliases[0] == "test-workflow1") &&
                            (workflow_info.release_title == "The first test workflow"));
                }) != workflows.cend());

    EXPECT_TRUE(std::find_if(workflows.cbegin(), workflows.cend(), [](const mp::VMImageInfo& workflow_info) {
                    return ((workflow_info.aliases.size() == 1) && (workflow_info.aliases[0] == "test-workflow2") &&
                            (workflow_info.release_title == "Another test workflow"));
                }) != workflows.cend());
}

TEST_F(VMWorkflowProvider, doesNotUpdateWorkflowsWhenNotNeeded)
{
    mpt::MockURLDownloader mock_url_downloader;

    EXPECT_CALL(mock_url_downloader, download_to(_, _, _, _, _))
        .Times(1)
        .WillRepeatedly([](auto, const QString& file_name, auto...) {
            QFile file(file_name);
            file.open(QFile::WriteOnly);
        });

    mp::DefaultVMWorkflowProvider workflow_provider{workflows_zip_url, &mock_url_downloader, cache_dir.path(),
                                                    default_ttl};

    workflow_provider.all_workflows();
}

TEST_F(VMWorkflowProvider, updatesWorkflowsWhenNeeded)
{
    mpt::MockURLDownloader mock_url_downloader;
    EXPECT_CALL(mock_url_downloader, download_to(_, _, _, _, _))
        .Times(2)
        .WillRepeatedly([](auto, const QString& file_name, auto...) {
            QFile file(file_name);

            if (!file.exists())
                file.open(QFile::WriteOnly);
        });

    mp::DefaultVMWorkflowProvider workflow_provider{workflows_zip_url, &mock_url_downloader, cache_dir.path(),
                                                    std::chrono::milliseconds(0)};

    workflow_provider.all_workflows();
}

TEST_F(VMWorkflowProvider, downloadFailureOnStartupLogsErrorAndDoesNotThrow)
{
    const std::string error_msg{"There is a problem, Houston."};
    mpt::MockURLDownloader mock_url_downloader;
    EXPECT_CALL(mock_url_downloader, download_to(_, _, _, _, _)).WillOnce(Throw(std::runtime_error(error_msg)));

    auto logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs(mpl::Level::error);
    logger_scope.mock_logger->expect_log(mpl::Level::error,
                                         fmt::format("Cannot get workflows on start up: {}", error_msg));

    EXPECT_NO_THROW(
        mp::DefaultVMWorkflowProvider(workflows_zip_url, &mock_url_downloader, cache_dir.path(), default_ttl));
}

TEST_F(VMWorkflowProvider, downloadFailureDuringUpdateThrows)
{
    const std::string error_msg{"There is a problem, Houston."};
    mpt::MockURLDownloader mock_url_downloader;
    EXPECT_CALL(mock_url_downloader, download_to(_, _, _, _, _))
        .Times(2)
        .WillOnce([](auto, const QString& file_name, auto...) {
            QFile file(file_name);
            file.open(QFile::WriteOnly);
        })
        .WillRepeatedly(Throw(std::runtime_error(error_msg)));

    mp::DefaultVMWorkflowProvider workflow_provider{workflows_zip_url, &mock_url_downloader, cache_dir.path(),
                                                    std::chrono::milliseconds(0)};

    MP_EXPECT_THROW_THAT(workflow_provider.all_workflows(), std::runtime_error, mpt::match_what(StrEq(error_msg)));
}
