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
#include "mock_logger.h"
#include "mock_platform.h"
#include "mock_poco_zip_utils.h"
#include "mock_url_downloader.h"
#include "path.h"
#include "temp_dir.h"

#include <multipass/default_vm_blueprint_provider.h>
#include <multipass/exceptions/blueprint_exceptions.h>
#include <multipass/exceptions/download_exception.h>
#include <multipass/memory_size.h>
#include <multipass/url_downloader.h>
#include <multipass/yaml_node_utils.h>

#include <Poco/Exception.h>

#include <QFileInfo>

#include <chrono>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;

using namespace std::chrono_literals;
using namespace testing;

namespace
{
const QString test_blueprints_zip{"/test-blueprints.zip"};
const QString multipass_blueprints_zip{"/multipass-blueprints.zip"};
const char* sha256_checksum = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

struct VMBlueprintProvider : public Test
{
    QString blueprints_zip_url{QUrl::fromLocalFile(mpt::test_data_path()).toString() + test_blueprints_zip};
    mp::URLDownloader url_downloader{std::chrono::seconds(10)};
    mpt::TempDir cache_dir;
    std::chrono::seconds default_ttl{1s};
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
};
} // namespace

TEST_F(VMBlueprintProvider, downloadsZipToExpectedLocation)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    const QFileInfo original_zip{mpt::test_data_path() + test_blueprints_zip};
    const QFileInfo downloaded_zip{cache_dir.path() + multipass_blueprints_zip};

    EXPECT_TRUE(downloaded_zip.exists());
    EXPECT_EQ(downloaded_zip.size(), original_zip.size());
}

TEST_F(VMBlueprintProvider, fetchBlueprintForUnknownBlueprintThrows)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    mp::VirtualMachineDescription vm_desc{0, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    mp::ClientLaunchData dummy_data;

    EXPECT_THROW(blueprint_provider.fetch_blueprint_for("phony", vm_desc, dummy_data), std::out_of_range);
}

TEST_F(VMBlueprintProvider, infoForUnknownBlueprintReturnsEmpty)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    EXPECT_EQ(blueprint_provider.info_for("phony"), std::nullopt);
}

TEST_F(VMBlueprintProvider, invalidImageSchemeThrows)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    mp::VirtualMachineDescription vm_desc{0, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    mp::ClientLaunchData dummy_data;

    MP_EXPECT_THROW_THAT(blueprint_provider.fetch_blueprint_for("invalid-image-blueprint", vm_desc, dummy_data),
                         mp::InvalidBlueprintException,
                         mpt::match_what(StrEq("Unsupported image scheme in Blueprint")));
}

TEST_F(VMBlueprintProvider, invalidMinCoresThrows)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    mp::VirtualMachineDescription vm_desc{0, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    mp::ClientLaunchData dummy_data;

    MP_EXPECT_THROW_THAT(blueprint_provider.fetch_blueprint_for("invalid-cpu-blueprint", vm_desc, dummy_data),
                         mp::InvalidBlueprintException,
                         mpt::match_what(StrEq("Minimum CPU value in Blueprint is invalid")));
}

TEST_F(VMBlueprintProvider, invalidMinMemorySizeThrows)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    mp::VirtualMachineDescription vm_desc{0, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    mp::ClientLaunchData dummy_data;

    MP_EXPECT_THROW_THAT(blueprint_provider.fetch_blueprint_for("invalid-memory-size-blueprint", vm_desc, dummy_data),
                         mp::InvalidBlueprintException,
                         mpt::match_what(StrEq("Minimum memory size value in Blueprint is invalid")));
}

TEST_F(VMBlueprintProvider, invalidMinDiskSpaceThrows)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    mp::VirtualMachineDescription vm_desc{0, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    mp::ClientLaunchData dummy_data;

    MP_EXPECT_THROW_THAT(blueprint_provider.fetch_blueprint_for("invalid-disk-space-blueprint", vm_desc, dummy_data),
                         mp::InvalidBlueprintException,
                         mpt::match_what(StrEq("Minimum disk space value in Blueprint is invalid")));
}

TEST_F(VMBlueprintProvider, invalidAliasDefinitionThrows)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    mp::VirtualMachineDescription vm_desc{0, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    mp::ClientLaunchData dummy_data;

    MP_EXPECT_THROW_THAT(blueprint_provider.fetch_blueprint_for("invalid-alias-blueprint", vm_desc, dummy_data),
                         mp::InvalidBlueprintException,
                         mpt::match_what(StrEq("Alias definition must be in the form instance:command")));
}

TEST_F(VMBlueprintProvider, fetchTestBlueprint1ReturnsExpectedInfo)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    mp::VirtualMachineDescription vm_desc{0, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    mp::ClientLaunchData dummy_data;

    auto query = blueprint_provider.fetch_blueprint_for("test-blueprint1", vm_desc, dummy_data);

    auto yaml_as_str = mp::utils::emit_yaml(vm_desc.vendor_data_config);

    EXPECT_EQ(query.release, "default");
    EXPECT_EQ(vm_desc.num_cores, 2);
    EXPECT_EQ(vm_desc.mem_size, mp::MemorySize("2G"));
    EXPECT_EQ(vm_desc.disk_space, mp::MemorySize("25G"));
    EXPECT_THAT(yaml_as_str, AllOf(HasSubstr("runcmd"), HasSubstr("echo \"Have fun!\"")));
}

TEST_F(VMBlueprintProvider, fetchTestBlueprint1ReturnsExpectedAliasesAndWorkspace)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    mp::VirtualMachineDescription vm_desc{0, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    mp::ClientLaunchData launch_data;

    auto query = blueprint_provider.fetch_blueprint_for("test-blueprint1", vm_desc, launch_data);

    EXPECT_EQ(launch_data.aliases_to_be_created.size(), 2);

    auto lst_alias = launch_data.aliases_to_be_created["lst"];
    EXPECT_EQ(lst_alias.instance, "test-blueprint1");
    EXPECT_EQ(lst_alias.command, "ls");
    EXPECT_EQ(lst_alias.working_directory, "map");

    auto lsp_alias = launch_data.aliases_to_be_created["lsp"];
    EXPECT_EQ(lsp_alias.instance, "test-blueprint1");
    EXPECT_EQ(lsp_alias.command, "pwd");
    EXPECT_EQ(lsp_alias.working_directory, "map");

    EXPECT_TRUE(launch_data.workspaces_to_be_created.size() == 1 &&
                launch_data.workspaces_to_be_created[0] == "test-blueprint1");
}

TEST_F(VMBlueprintProvider, fetchTestBlueprint2ReturnsExpectedInfo)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    mp::VirtualMachineDescription vm_desc{0, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    mp::ClientLaunchData launch_data;

    auto query = blueprint_provider.fetch_blueprint_for("test-blueprint2", vm_desc, launch_data);

    EXPECT_EQ(query.release, "bionic");
    EXPECT_EQ(query.remote_name, "daily");
    EXPECT_EQ(vm_desc.num_cores, 4);
    EXPECT_EQ(vm_desc.mem_size, mp::MemorySize("4G"));
    EXPECT_EQ(vm_desc.disk_space, mp::MemorySize("50G"));
    EXPECT_TRUE(vm_desc.vendor_data_config.IsNull());

    EXPECT_EQ(launch_data.workspaces_to_be_created.size(), 0);
}

TEST_F(VMBlueprintProvider, missingDescriptionThrows)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    const std::string blueprint{"missing-description-blueprint"};
    MP_EXPECT_THROW_THAT(
        blueprint_provider.info_for(blueprint), mp::InvalidBlueprintException,
        mpt::match_what(StrEq(fmt::format("The \'description\' key is required for the {} Blueprint", blueprint))));
}

TEST_F(VMBlueprintProvider, missingVersionThrows)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    const std::string blueprint{"missing-version-blueprint"};
    MP_EXPECT_THROW_THAT(
        blueprint_provider.info_for(blueprint), mp::InvalidBlueprintException,
        mpt::match_what(StrEq(fmt::format("The \'version\' key is required for the {} Blueprint", blueprint))));
}

TEST_F(VMBlueprintProvider, invalidDescriptionThrows)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    const std::string blueprint{"invalid-description-blueprint"};
    MP_EXPECT_THROW_THAT(
        blueprint_provider.info_for(blueprint), mp::InvalidBlueprintException,
        mpt::match_what(StrEq(fmt::format("Cannot convert \'description\' key for the {} Blueprint", blueprint))));
}

TEST_F(VMBlueprintProvider, invalidVersionThrows)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    const std::string blueprint{"invalid-version-blueprint"};
    MP_EXPECT_THROW_THAT(
        blueprint_provider.info_for(blueprint), mp::InvalidBlueprintException,
        mpt::match_what(StrEq(fmt::format("Cannot convert \'version\' key for the {} Blueprint", blueprint))));
}

TEST_F(VMBlueprintProvider, invalidCloudInitThrows)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    mp::VirtualMachineDescription vm_desc{0, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    const std::string blueprint{"invalid-cloud-init-blueprint"};

    mp::ClientLaunchData dummy_data;

    MP_EXPECT_THROW_THAT(
        blueprint_provider.fetch_blueprint_for(blueprint, vm_desc, dummy_data), mp::InvalidBlueprintException,
        mpt::match_what(StrEq(fmt::format("Cannot convert cloud-init data for the {} Blueprint", blueprint))));
}

TEST_F(VMBlueprintProvider, givenCoresLessThanMinimumThrows)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    mp::VirtualMachineDescription vm_desc{1, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    mp::ClientLaunchData dummy_data;

    MP_EXPECT_THROW_THAT(blueprint_provider.fetch_blueprint_for("test-blueprint1", vm_desc, dummy_data),
                         mp::BlueprintMinimumException,
                         mpt::match_what(AllOf(HasSubstr("Number of CPUs"), HasSubstr("2"))));
}

TEST_F(VMBlueprintProvider, givenMemLessThanMinimumThrows)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    mp::VirtualMachineDescription vm_desc{0, mp::MemorySize{"1G"}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    mp::ClientLaunchData dummy_data;

    MP_EXPECT_THROW_THAT(blueprint_provider.fetch_blueprint_for("test-blueprint1", vm_desc, dummy_data),
                         mp::BlueprintMinimumException,
                         mpt::match_what(AllOf(HasSubstr("Memory size"), HasSubstr("2G"))));
}

TEST_F(VMBlueprintProvider, givenDiskSpaceLessThanMinimumThrows)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    mp::VirtualMachineDescription vm_desc{0, {}, mp::MemorySize{"20G"}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    mp::ClientLaunchData dummy_data;

    MP_EXPECT_THROW_THAT(blueprint_provider.fetch_blueprint_for("test-blueprint1", vm_desc, dummy_data),
                         mp::BlueprintMinimumException,
                         mpt::match_what(AllOf(HasSubstr("Disk space"), HasSubstr("25G"))));
}

TEST_F(VMBlueprintProvider, higherOptionsIsNotOverridden)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    mp::VirtualMachineDescription vm_desc{
        4, mp::MemorySize{"4G"}, mp::MemorySize{"50G"}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    mp::ClientLaunchData dummy_data;

    blueprint_provider.fetch_blueprint_for("test-blueprint1", vm_desc, dummy_data);

    EXPECT_EQ(vm_desc.num_cores, 4);
    EXPECT_EQ(vm_desc.mem_size, mp::MemorySize("4G"));
    EXPECT_EQ(vm_desc.disk_space, mp::MemorySize("50G"));
}

TEST_F(VMBlueprintProvider, infoForReturnsExpectedInfo)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    auto blueprint = blueprint_provider.info_for("test-blueprint2");

    ASSERT_TRUE(blueprint);
    ASSERT_EQ(blueprint->aliases.size(), 1);
    EXPECT_EQ(blueprint->aliases[0], "test-blueprint2");
    EXPECT_EQ(blueprint->release_title, "Another test blueprint");
    EXPECT_EQ(blueprint->version, "0.1");
}

TEST_F(VMBlueprintProvider, allBlueprintsReturnsExpectedInfo)
{
    logger_scope.mock_logger->screen_logs(mpl::Level::error);
    logger_scope.mock_logger->expect_log(
        mpl::Level::error,
        "Invalid Blueprint: Cannot convert 'description' key for the invalid-description-blueprint Blueprint");
    logger_scope.mock_logger->expect_log(
        mpl::Level::error,
        "Invalid Blueprint: Cannot convert 'version' key for the invalid-version-blueprint Blueprint");
    logger_scope.mock_logger->expect_log(
        mpl::Level::error,
        "Invalid Blueprint: The 'description' key is required for the missing-description-blueprint Blueprint");
    logger_scope.mock_logger->expect_log(
        mpl::Level::error,
        "Invalid Blueprint: The 'version' key is required for the missing-version-blueprint Blueprint");
    logger_scope.mock_logger->expect_log(
        mpl::Level::error, "Invalid Blueprint name \'42-invalid-hostname-blueprint\': must be a valid host name");
    logger_scope.mock_logger->expect_log(mpl::Level::debug, "Not loading malformed \"invalid-arch\" v1");

    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    auto blueprints = blueprint_provider.all_blueprints();

    EXPECT_EQ(blueprints.size(), 12ul);

    EXPECT_TRUE(std::find_if(blueprints.cbegin(), blueprints.cend(), [](const mp::VMImageInfo& blueprint_info) {
                    return ((blueprint_info.aliases.size() == 1) && (blueprint_info.aliases[0] == "test-blueprint1") &&
                            (blueprint_info.release_title == "The first test blueprint"));
                }) != blueprints.cend());

    EXPECT_TRUE(std::find_if(blueprints.cbegin(), blueprints.cend(), [](const mp::VMImageInfo& blueprint_info) {
                    return ((blueprint_info.aliases.size() == 1) && (blueprint_info.aliases[0] == "test-blueprint2") &&
                            (blueprint_info.release_title == "Another test blueprint"));
                }) != blueprints.cend());
}

TEST_F(VMBlueprintProvider, doesNotUpdateBlueprintsWhenNotNeeded)
{
    mpt::MockURLDownloader mock_url_downloader;

    EXPECT_CALL(mock_url_downloader, download_to(_, _, _, _, _))
        .Times(1)
        .WillRepeatedly([](auto, const QString& file_name, auto...) {
            QFile file(file_name);
            file.open(QFile::WriteOnly);
        });

    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &mock_url_downloader, cache_dir.path(),
                                                      default_ttl};

    blueprint_provider.all_blueprints();
}

TEST_F(VMBlueprintProvider, updatesBlueprintsWhenNeeded)
{
    mpt::MockURLDownloader mock_url_downloader;
    EXPECT_CALL(mock_url_downloader, download_to(_, _, _, _, _))
        .Times(2)
        .WillRepeatedly([](auto, const QString& file_name, auto...) {
            QFile file(file_name);

            if (!file.exists())
                file.open(QFile::WriteOnly);
        });

    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &mock_url_downloader, cache_dir.path(),
                                                      std::chrono::milliseconds(0)};

    blueprint_provider.all_blueprints();
}

TEST_F(VMBlueprintProvider, downloadFailureOnStartupLogsErrorAndDoesNotThrow)
{
    const std::string error_msg{"There is a problem, Houston."};
    const std::string url{"https://fake.url"};
    mpt::MockURLDownloader mock_url_downloader;
    EXPECT_CALL(mock_url_downloader, download_to(_, _, _, _, _)).WillOnce(Throw(mp::DownloadException(url, error_msg)));

    auto logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs(mpl::Level::error);
    logger_scope.mock_logger->expect_log(
        mpl::Level::error, fmt::format("Error fetching Blueprints: failed to download from '{}': {}", url, error_msg));

    EXPECT_NO_THROW(
        mp::DefaultVMBlueprintProvider(blueprints_zip_url, &mock_url_downloader, cache_dir.path(), default_ttl));
}

TEST_F(VMBlueprintProvider, downloadFailureDuringUpdateLogsErrorAndDoesNotThrow)
{
    const std::string error_msg{"There is a problem, Houston."};
    const std::string url{"https://fake.url"};
    mpt::MockURLDownloader mock_url_downloader;
    EXPECT_CALL(mock_url_downloader, download_to(_, _, _, _, _))
        .Times(2)
        .WillOnce([](auto, const QString& file_name, auto...) {
            QFile file(file_name);
            file.open(QFile::WriteOnly);
        })
        .WillRepeatedly(Throw(mp::DownloadException(url, error_msg)));

    auto logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs(mpl::Level::error);
    logger_scope.mock_logger->expect_log(
        mpl::Level::error, fmt::format("Error fetching Blueprints: failed to download from '{}': {}", url, error_msg));

    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &mock_url_downloader, cache_dir.path(),
                                                      std::chrono::milliseconds(0)};

    EXPECT_NO_THROW(blueprint_provider.all_blueprints());
}

TEST_F(VMBlueprintProvider, zipArchivePocoExceptionLogsErrorAndDoesNotThrow)
{
    auto [mock_poco_zip_utils, guard] = mpt::MockPocoZipUtils::inject();
    const std::string error_msg{"Rubbish zip file"};

    EXPECT_CALL(*mock_poco_zip_utils, zip_archive_for(_)).WillOnce(Throw(Poco::IllegalStateException(error_msg)));

    auto logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs(mpl::Level::error);
    logger_scope.mock_logger->expect_log(
        mpl::Level::error, fmt::format("Error extracting Blueprints zip file: Illegal state: {}", error_msg));

    EXPECT_NO_THROW(mp::DefaultVMBlueprintProvider(blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                   std::chrono::milliseconds(0)));
}

TEST_F(VMBlueprintProvider, generalExceptionDuringStartupThrows)
{
    const std::string error_msg{"Bad stuff just happened"};
    mpt::MockURLDownloader mock_url_downloader;
    EXPECT_CALL(mock_url_downloader, download_to(_, _, _, _, _)).WillRepeatedly(Throw(std::runtime_error(error_msg)));

    MP_EXPECT_THROW_THAT(mp::DefaultVMBlueprintProvider blueprint_provider(
                             blueprints_zip_url, &mock_url_downloader, cache_dir.path(), std::chrono::milliseconds(0)),
                         std::runtime_error, mpt::match_what(StrEq(error_msg)));
}

TEST_F(VMBlueprintProvider, generalExceptionDuringCallThrows)
{
    const std::string error_msg{"This can't be possible"};
    mpt::MockURLDownloader mock_url_downloader;
    EXPECT_CALL(mock_url_downloader, download_to(_, _, _, _, _))
        .Times(2)
        .WillOnce([](auto, const QString& file_name, auto...) {
            QFile file(file_name);
            file.open(QFile::WriteOnly);
        })
        .WillRepeatedly(Throw(std::runtime_error(error_msg)));

    mp::DefaultVMBlueprintProvider blueprint_provider(blueprints_zip_url, &mock_url_downloader, cache_dir.path(),
                                                      std::chrono::milliseconds(0));

    MP_EXPECT_THROW_THAT(blueprint_provider.info_for("foo"), std::runtime_error, mpt::match_what(StrEq(error_msg)));
}

TEST_F(VMBlueprintProvider, validBlueprintReturnsExpectedName)
{
    const std::string blueprint_name{"test-blueprint1"};

    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    auto name = blueprint_provider.name_from_blueprint(blueprint_name);

    EXPECT_EQ(name, blueprint_name);
}

TEST_F(VMBlueprintProvider, nonexistentBlueprintReturnsEmptyName)
{
    const std::string blueprint_name{"not-a-blueprint"};

    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    auto name = blueprint_provider.name_from_blueprint(blueprint_name);

    EXPECT_TRUE(name.empty());
}

TEST_F(VMBlueprintProvider, returnsExpectedTimeout)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    EXPECT_EQ(blueprint_provider.blueprint_timeout("test-blueprint1"), 600);
}

TEST_F(VMBlueprintProvider, noTimeoutReturnsZero)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    EXPECT_EQ(blueprint_provider.blueprint_timeout("test-blueprint2"), 0);
}

TEST_F(VMBlueprintProvider, nonexistentBlueprintTimeoutReturnsZero)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    EXPECT_EQ(blueprint_provider.blueprint_timeout("not-a-blueprint"), 0);
}

TEST_F(VMBlueprintProvider, invalidTimeoutThrows)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    MP_EXPECT_THROW_THAT(blueprint_provider.blueprint_timeout("invalid-timeout-blueprint"),
                         mp::InvalidBlueprintException,
                         mpt::match_what(StrEq(fmt::format("Invalid timeout given in Blueprint"))));
}

TEST_F(VMBlueprintProvider, noImageDefinedReturnsDefault)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    mp::VirtualMachineDescription vm_desc{0, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    mp::ClientLaunchData dummy_data;

    auto query = blueprint_provider.fetch_blueprint_for("no-image-blueprint", vm_desc, dummy_data);

    EXPECT_EQ(query.release, "default");
}

TEST_F(VMBlueprintProvider, nameMismatchThrows)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    mp::VirtualMachineDescription vm_desc{0, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    mp::ClientLaunchData dummy_data;

    MP_EXPECT_THROW_THAT(
        blueprint_provider.fetch_blueprint_for("name-mismatch", vm_desc, dummy_data), mp::InvalidBlueprintException,
        mpt::match_what(StrEq("There are no instance definitions matching Blueprint name \"name-mismatch\"")));
}

TEST_F(VMBlueprintProvider, invalidRunsOnThrows)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    const std::string blueprint{"invalid-description-blueprint"};
    MP_EXPECT_THROW_THAT(
        blueprint_provider.info_for(blueprint), mp::InvalidBlueprintException,
        mpt::match_what(StrEq(fmt::format("Cannot convert \'description\' key for the {} Blueprint", blueprint))));
}

TEST_F(VMBlueprintProvider, fetchForInvalidReturnsEmpty)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    const std::string blueprint{"invalid-arch"};
    // This call fails with an std::nullopt exception because the Blueprint is invalid and was filtered out by
    // blueprints_map_for() at provider construction.
    EXPECT_EQ(blueprint_provider.info_for(blueprint), std::nullopt);
}

TEST_F(VMBlueprintProvider, infoForIncompatibleReturnsEmpty)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    const std::string blueprint{"arch-only"};
    // This call fails with an std::nullopt exception because the Blueprint is invalid and was filtered out by
    // blueprints_map_for() at provider construction.
    EXPECT_EQ(blueprint_provider.info_for(blueprint), std::nullopt);
}

TEST_F(VMBlueprintProvider, infoForCompatibleReturnsExpectedInfo)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl, "arch"};

    auto blueprint = blueprint_provider.info_for("arch-only");

    ASSERT_TRUE(blueprint);
    ASSERT_EQ(blueprint->aliases.size(), 1);
    EXPECT_EQ(blueprint->aliases[0], "arch-only");
    EXPECT_EQ(blueprint->release_title, "An arch-only blueprint");
}

TEST_F(VMBlueprintProvider, allBlueprintsReturnsExpectedInfoForArch)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl, "arch"};

    auto blueprints = blueprint_provider.all_blueprints();

    EXPECT_EQ(blueprints.size(), 13ul);
    EXPECT_TRUE(std::find_if(blueprints.cbegin(), blueprints.cend(), [](const mp::VMImageInfo& blueprint_info) {
                    return ((blueprint_info.aliases.size() == 1) && (blueprint_info.aliases[0] == "arch-only") &&
                            (blueprint_info.release_title == "An arch-only blueprint"));
                }) != blueprints.cend());
    ASSERT_EQ(blueprints[0].aliases.size(), 1);
}

//
// Blueprints v2 tests.
//

TEST_F(VMBlueprintProvider, v2WithNoInstancesKeyNotAdded)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    EXPECT_EQ(blueprint_provider.info_for("no-instances"), std::nullopt);
}

TEST_F(VMBlueprintProvider, v2WithNoBlueprintKeyNotAdded)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    EXPECT_EQ(blueprint_provider.info_for("no-blueprint"), std::nullopt);
}

TEST_F(VMBlueprintProvider, v2WithNoImagesKeyNotAdded)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    EXPECT_EQ(blueprint_provider.info_for("no-images"), std::nullopt);
}

TEST_F(VMBlueprintProvider, v2WithNoUrlKeyNotAdded)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl, "multivacs"};

    EXPECT_EQ(blueprint_provider.info_for("no-url"), std::nullopt);
}

TEST_F(VMBlueprintProvider, v2MinimalDefinitionAdded)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl, "multivacs"};

    EXPECT_NO_THROW(blueprint_provider.info_for("minimal"));
}

TEST_F(VMBlueprintProvider, v2MinimalDefinitionWithShaOnUrlAdded)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl, "multivacs"};

    EXPECT_NO_THROW(blueprint_provider.info_for("minimal-with-sha256-url"));
}

TEST_F(VMBlueprintProvider, v2MinimalDefinitionWithHardcodedShaAdded)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl, "multivacs"};

    EXPECT_NO_THROW(blueprint_provider.info_for("minimal-with-sha256-string"));
}

TEST_F(VMBlueprintProvider, v2ShaOnUrlIsCorrectlyPropagated)
{
    mpt::MockURLDownloader mock_url_downloader;

    EXPECT_CALL(mock_url_downloader, download_to(_, _, _, _, _))
        .WillOnce([this](const QUrl& url, const QString& file_name, int64_t size, const int download_type,
                         const mp::ProgressMonitor& monitor) {
            url_downloader.download_to(url, file_name, size, download_type, monitor);
        });

    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &mock_url_downloader, cache_dir.path(),
                                                      default_ttl, "multivacs"};

    mp::VirtualMachineDescription vm_desc;
    mp::ClientLaunchData launch_data;

    EXPECT_CALL(mock_url_downloader, download(_)).Times(1).WillRepeatedly([](auto) {
        char full_sha256_string[73];
        strcpy(full_sha256_string, sha256_checksum);
        strcat(full_sha256_string, " sha256\n");
        return QByteArray{full_sha256_string};
    });

    auto query = blueprint_provider.fetch_blueprint_for("minimal-with-sha256-url", vm_desc, launch_data);

    ASSERT_EQ(vm_desc.image.id, sha256_checksum);
}

TEST_F(VMBlueprintProvider, v2HardcodedShaIsCorrectlyPropagated)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl, "multivacs"};

    mp::VirtualMachineDescription vm_desc;
    mp::ClientLaunchData launch_data;

    auto query = blueprint_provider.fetch_blueprint_for("minimal-with-sha256-string", vm_desc, launch_data);

    ASSERT_EQ(vm_desc.image.id, sha256_checksum);
}

TEST_F(VMBlueprintProvider, v2WithoutShaIsCorrectlyPropagated)
{
    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl, "multivacs"};

    mp::VirtualMachineDescription vm_desc;
    mp::ClientLaunchData launch_data;

    auto query = blueprint_provider.fetch_blueprint_for("minimal", vm_desc, launch_data);

    ASSERT_EQ(vm_desc.image.id, "");
}

//
// Test loading Blueprints from file.
//

struct VMBlueprintFileLaunch : public VMBlueprintProvider
{
    mpt::MockPlatform::GuardedMock attr{mpt::MockPlatform::inject<NiceMock>()};
    mpt::MockPlatform* mock_platform = attr.first;
};

struct VMBlueprintFileLaunchFromFile : public VMBlueprintFileLaunch,
                                       public WithParamInterface<std::pair<std::string, std::string>>
{
};

TEST_P(VMBlueprintFileLaunchFromFile, loadsFile)
{
    const auto& [file, blueprint_name] = GetParam();

    mpt::MockURLDownloader mock_url_downloader;

    EXPECT_CALL(mock_url_downloader, download_to(_, _, _, _, _))
        .WillRepeatedly([](auto, const QString& file_name, auto...) {
            QFile file(file_name);
            file.open(QFile::WriteOnly);
        });

    ON_CALL(*mock_platform, is_image_url_supported()).WillByDefault(Return(true));

    auto blueprint_path = QString(mpt::test_data_path() + "/blueprints/" + QString::fromStdString(file)).toStdString();

    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &mock_url_downloader, cache_dir.path(),
                                                      default_ttl, "multivacs"};

    mp::VirtualMachineDescription vm_desc{0, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    mp::ClientLaunchData dummy_data;

    auto query = blueprint_provider.blueprint_from_file(blueprint_path, blueprint_name, vm_desc, dummy_data);

    EXPECT_EQ(vm_desc.num_cores, 2);
    EXPECT_EQ(vm_desc.mem_size, mp::MemorySize("2G"));
    EXPECT_EQ(vm_desc.disk_space, mp::MemorySize("25G"));

    auto yaml_as_str = mp::utils::emit_yaml(vm_desc.vendor_data_config);
    EXPECT_THAT(yaml_as_str, AllOf(HasSubstr("runcmd"), HasSubstr("echo \"Have fun!\"")));
}

INSTANTIATE_TEST_SUITE_P(VMBlueprintFileLaunch, VMBlueprintFileLaunchFromFile,
                         Values(std::pair{"v1/test-blueprint1.yaml", "test-blueprint1"},
                                std::pair{"v2/test-blueprint1.yaml", "test-blueprint1"}));

TEST_F(VMBlueprintFileLaunch, mergesBlueprintVendorData)
{
    ON_CALL(*mock_platform, is_image_url_supported()).WillByDefault(Return(true));

    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url,
                                                      &url_downloader,
                                                      cache_dir.path(),
                                                      default_ttl};

    YAML::Node vendor_data;
    vendor_data["runcmd"].push_back("echo 123");
    vendor_data["system_info"]["default_user"]["name"] = "ubuntu";
    vendor_data["growpart"]["devices"].push_back("/dev/vdb1");

    mp::VirtualMachineDescription vm_desc{0, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, vendor_data, {}};

    mp::ClientLaunchData dummy_data;

    const auto blueprint_path = mpt::test_data_path() + "blueprints/v1/complex-cloud-init.yaml";

    blueprint_provider.blueprint_from_file(blueprint_path.toStdString(), "complex-cloud-init", vm_desc, dummy_data);

    const auto runcmd = vm_desc.vendor_data_config["runcmd"];
    EXPECT_EQ(runcmd[0].Scalar(), "echo 123");
    EXPECT_EQ(runcmd[1].Scalar(), "echo abc");
    EXPECT_EQ(runcmd[2].Scalar(), "echo def");

    const auto default_user = vm_desc.vendor_data_config["system_info"]["default_user"];
    EXPECT_EQ(default_user["name"].Scalar(), "ubuntu");
    EXPECT_EQ(default_user["shell"].Scalar(), "/bin/zsh");

    const auto devices = vm_desc.vendor_data_config["growpart"]["devices"];
    EXPECT_EQ(devices[0].Scalar(), "/dev/vdb1");
    EXPECT_EQ(devices[1].Scalar(), "/");
}

TEST_F(VMBlueprintFileLaunch, failsMergeVmBlueprintVendorDataDifferentTypes)
{
    ON_CALL(*mock_platform, is_image_url_supported()).WillByDefault(Return(true));

    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url,
                                                      &url_downloader,
                                                      cache_dir.path(),
                                                      default_ttl};

    YAML::Node vendor_data;
    vendor_data["runcmd"] = "echo 123";

    mp::VirtualMachineDescription vm_desc{0, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, vendor_data, {}};

    mp::ClientLaunchData dummy_data;

    const auto blueprint_path = mpt::test_data_path() + "blueprints/v1/complex-cloud-init.yaml";

    MP_EXPECT_THROW_THAT(
        blueprint_provider.blueprint_from_file(blueprint_path.toStdString(), "complex-cloud-init", vm_desc, dummy_data),
        mp::InvalidBlueprintException,
        mpt::match_what(HasSubstr("Cannot merge values of runcmd")));
}

TEST_F(VMBlueprintFileLaunch, failsMergeVmBlueprintVendorDataScalarValues)
{
    ON_CALL(*mock_platform, is_image_url_supported()).WillByDefault(Return(true));

    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url,
                                                      &url_downloader,
                                                      cache_dir.path(),
                                                      default_ttl};

    YAML::Node vendor_data;
    vendor_data["system_info"]["default_user"]["shell"] = "/bin/fish";

    mp::VirtualMachineDescription vm_desc{0, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, vendor_data, {}};

    mp::ClientLaunchData dummy_data;

    const auto blueprint_path = mpt::test_data_path() + "blueprints/v1/complex-cloud-init.yaml";

    MP_EXPECT_THROW_THAT(
        blueprint_provider.blueprint_from_file(blueprint_path.toStdString(), "complex-cloud-init", vm_desc, dummy_data),
        mp::InvalidBlueprintException,
        mpt::match_what(HasSubstr("Cannot merge values of shell")));
}

TEST_F(VMBlueprintFileLaunch, failsWithNonexistentFile)
{
    ON_CALL(*mock_platform, is_image_url_supported()).WillByDefault(Return(true));

    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    mp::VirtualMachineDescription vm_desc{0, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    mp::ClientLaunchData dummy_data;

    MP_EXPECT_THROW_THAT(blueprint_provider.blueprint_from_file("/blah.yaml", "blah", vm_desc, dummy_data),
                         mp::InvalidBlueprintException, mpt::match_what(StrEq("Wrong file '/blah.yaml'")));
}

TEST_F(VMBlueprintFileLaunch, fileLoadfailsWithInvalidHostName)
{
    ON_CALL(*mock_platform, is_image_url_supported()).WillByDefault(Return(true));

    auto blueprint_path =
        QString(mpt::test_data_path() + "/blueprints/v1/42-invalid-hostname-blueprint.yaml").toStdString();

    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    mp::VirtualMachineDescription vm_desc{0, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    mp::ClientLaunchData dummy_data;

    MP_EXPECT_THROW_THAT(
        blueprint_provider.blueprint_from_file(blueprint_path, "42-invalid-hostname-blueprint", vm_desc, dummy_data),
        mp::InvalidBlueprintException,
        mpt::match_what(StrEq("Invalid Blueprint name \'42-invalid-hostname-blueprint\': must be a valid host name")));
}

TEST_F(VMBlueprintFileLaunch, failsIfFileLaunchIsUnsupported)
{
    ON_CALL(*mock_platform, is_image_url_supported()).WillByDefault(Return(false));

    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    mp::VirtualMachineDescription vm_desc{0, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    mp::ClientLaunchData dummy_data;

    MP_EXPECT_THROW_THAT(blueprint_provider.blueprint_from_file("/blah.yaml", "blah", vm_desc, dummy_data),
                         std::runtime_error,
                         mpt::match_what(StrEq("Launching a Blueprint from a file is not supported")));
}

struct NameFromBlueprintTestSuite : public VMBlueprintProvider,
                                    public WithParamInterface<std::pair<std::string, std::string>>
{
};

TEST_P(NameFromBlueprintTestSuite, nameFromBlueprintWorks)
{
    const auto& [input, output] = GetParam();

    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl};

    EXPECT_EQ(blueprint_provider.name_from_blueprint(input), output);
}

INSTANTIATE_TEST_SUITE_P(VMBlueprintProvider, NameFromBlueprintTestSuite,
                         Values(std::pair{"file:///blah/blueprint1.yaml", "blueprint1"},
                                std::pair{"file:///blah/blueprint2.yml", "blueprint2"},
                                std::pair{"nonexistent-blueprint", ""}));

TEST_F(VMBlueprintFileLaunch, fileLoadfailsWithNoUrl)
{
    auto blueprint_path = QString(mpt::test_data_path() + "/blueprints/v2/test-blueprint1.yaml").toStdString();

    ON_CALL(*mock_platform, is_image_url_supported()).WillByDefault(Return(true));

    mp::DefaultVMBlueprintProvider blueprint_provider{blueprints_zip_url, &url_downloader, cache_dir.path(),
                                                      default_ttl, "microvac"};

    mp::VirtualMachineDescription vm_desc{0, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};

    mp::ClientLaunchData dummy_data;

    MP_EXPECT_THROW_THAT(blueprint_provider.blueprint_from_file(blueprint_path, "test-blueprint1", vm_desc, dummy_data),
                         mp::InvalidBlueprintException,
                         mpt::match_what(StrEq("No image URL for architecture microvac in Blueprint")));
}
