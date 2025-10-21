
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
#include "file_operations.h"
#include "image_host_remote_count.h"
#include "mock_logger.h"
#include "mock_url_downloader.h"
#include "path.h"

#include <multipass/exceptions/download_exception.h>
#include <multipass/exceptions/image_not_found_exception.h>
#include <multipass/image_host/custom_image_host.h>
#include <multipass/query.h>

#include <QUrl>

#include <unordered_set>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct CustomImageHost : public Test
{
    CustomImageHost()
    {
    }

    mp::Query make_query(std::string release, std::string remote)
    {
        return {"", std::move(release), false, std::move(remote), mp::Query::Type::Alias};
    }

    QByteArray payload = mpt::load_test_file("custom_image_host/good_manifest.json");
    NiceMock<mpt::MockURLDownloader> mock_url_downloader;
};
} // namespace

TEST_F(CustomImageHost, iteratesOverAllEntries)
{
    EXPECT_CALL(mock_url_downloader, download(_, _)).WillRepeatedly(Return(payload));

    mp::CustomVMImageHost host{&mock_url_downloader};
    host.update_manifests(false);

    std::unordered_set<std::string> ids;
    auto action = [&ids](const std::string& remote, const mp::VMImageInfo& info) {
        ids.insert(info.id.toStdString());
    };
    host.for_each_entry_do(action);

    EXPECT_THAT(ids, UnorderedElementsAre("debian-12-hash", "fedora-42-hash"));
}

TEST_F(CustomImageHost, allImagesForNoRemoteReturnsAppropriateMatches)
{
    EXPECT_CALL(mock_url_downloader, download(_, _)).WillOnce(Return(payload));
    mp::CustomVMImageHost host{&mock_url_downloader};

    host.update_manifests(false);

    auto images = host.all_images_for("", false);

    EXPECT_EQ(images.size(), 2);
}

TEST_F(CustomImageHost, allInfoForNoRemoteReturnsOneAliasMatch)
{
    EXPECT_CALL(mock_url_downloader, download(_, _)).WillOnce(Return(payload));
    mp::CustomVMImageHost host{&mock_url_downloader};

    host.update_manifests(false);

    auto images_info = host.all_info_for(make_query("debian", ""));

    EXPECT_EQ(images_info.size(), 1);
}

TEST_F(CustomImageHost, supportedRemotesReturnsExpectedValues)
{
    EXPECT_CALL(mock_url_downloader, download(_, _)).WillOnce(Return(payload));
    mp::CustomVMImageHost host{&mock_url_downloader};

    host.update_manifests(false);

    auto supported_remotes = host.supported_remotes();

    EXPECT_EQ(supported_remotes.size(), 1);

    EXPECT_TRUE(std::find(supported_remotes.begin(), supported_remotes.end(), "") !=
                supported_remotes.end());
}

TEST_F(CustomImageHost, invalidImageReturnsFalse)
{
    EXPECT_CALL(mock_url_downloader, download(_, _)).WillOnce(Return(payload));
    mp::CustomVMImageHost host{&mock_url_downloader};

    host.update_manifests(false);

    EXPECT_FALSE(host.info_for(make_query("foo", "")));
}

TEST_F(CustomImageHost, invalidRemoteThrowsError)
{
    EXPECT_CALL(mock_url_downloader, download(_, _)).WillOnce(Return(payload));
    mp::CustomVMImageHost host{&mock_url_downloader};

    host.update_manifests(false);

    EXPECT_THROW(host.info_for(make_query("core", "foo")), std::runtime_error);
}

TEST_F(CustomImageHost, handlesAndRecoversFromInitialNetworkFailure)
{
    EXPECT_CALL(mock_url_downloader, download(_, _))
        .WillOnce(Throw(mp::DownloadException{"", ""}))
        .WillRepeatedly(Return(payload));

    mp::CustomVMImageHost host{&mock_url_downloader};

    const auto query = make_query("debian", "");
    host.update_manifests(false);
    auto images_info = host.all_info_for(make_query("debian", ""));
    EXPECT_EQ(images_info.size(), 0);

    host.update_manifests(false);
    images_info = host.all_info_for(make_query("debian", ""));
    EXPECT_EQ(images_info.size(), 1);
}

TEST_F(CustomImageHost, handlesAndRecoversFromLaterNetworkFailure)
{
    EXPECT_CALL(mock_url_downloader, download(_, _))
        .WillOnce(Return(payload))
        .WillOnce(Throw(mp::DownloadException{"", ""}))
        .WillOnce(Return(payload));

    mp::CustomVMImageHost host{&mock_url_downloader};

    const auto query = make_query("debian", "");
    host.update_manifests(false);
    EXPECT_TRUE(host.info_for(query));

    host.update_manifests(false);
    EXPECT_FALSE(host.info_for(query));

    host.update_manifests(false);
    EXPECT_TRUE(host.info_for(query));
}

TEST_F(CustomImageHost, infoForFullHashReturnsEmptyImageInfo)
{
    EXPECT_CALL(mock_url_downloader, download(_, _)).WillOnce(Return(payload));
    mp::CustomVMImageHost host{&mock_url_downloader};

    host.update_manifests(false);

    EXPECT_THROW(host.info_for_full_hash("invalid-hash"), mp::ImageNotFoundException);
}

TEST_F(CustomImageHost, infoForFullHashFindsImageInfo)
{
    EXPECT_CALL(mock_url_downloader, download(_, _)).WillOnce(Return(payload));
    mp::CustomVMImageHost host{&mock_url_downloader};

    host.update_manifests(false);

    const auto image = host.info_for_full_hash("debian-12-HASH");

    EXPECT_THAT(image.release, Eq("bookworm"));
}

TEST_F(CustomImageHost, badJsonLogsAndReturnsEmptyImages)
{
    const auto bad_json = mpt::load_test_file("custom_image_host/malformed_manifest.json");

    auto logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs(multipass::logging::Level::warning);
    logger_scope.mock_logger->expect_log(
        multipass::logging::Level::warning,
        "Failed to parse manifest: file does not contain a valid JSON object");

    EXPECT_CALL(mock_url_downloader, download(_, _)).WillOnce(Return(bad_json));
    mp::CustomVMImageHost host{&mock_url_downloader};

    host.update_manifests(false);

    auto images = host.all_images_for("", false);

    EXPECT_EQ(images.size(), 0);
}
