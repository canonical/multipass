/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include "src/daemon/custom_image_host.h"

#include "path.h"

#include <multipass/query.h>
#include <multipass/url_downloader.h>

#include <QUrl>

#include <gmock/gmock.h>

#include <unordered_set>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct CustomImageHost : public Test
{
    mp::Query make_query(std::string release, std::string remote)
    {
        return {"", std::move(release), false, std::move(remote), mp::Query::Type::Alias};
    }

    mp::URLDownloader url_downloader{std::chrono::seconds{10}};
    const QString test_path{mpt::test_data_path() + "custom/"};
};
} // namespace

TEST_F(CustomImageHost, returns_expected_data_for_core)
{
    mp::CustomVMImageHost host{&url_downloader, test_path};

    auto info = *host.info_for(make_query("core", ""));

    EXPECT_THAT(info.image_location, Eq(QUrl::fromLocalFile(test_path + "ubuntu-core-16-amd64.img.xz").toString()));
    EXPECT_THAT(info.id, Eq(QString("934d52e4251537ee3bd8c500f212ae4c34992447e7d40d94f00bc7c21f72ceb7")));
    EXPECT_THAT(info.release, Eq(QString("core-16")));
    EXPECT_THAT(info.release_title, Eq(QString("Core 16")));
    EXPECT_TRUE(info.supported);
    EXPECT_FALSE(info.version.isEmpty());
}

TEST_F(CustomImageHost, returns_expected_data_for_snapcraft_core)
{
    mp::CustomVMImageHost host{&url_downloader, test_path};

    auto info = *host.info_for(make_query("core", "snapcraft"));

    EXPECT_THAT(info.image_location,
                Eq(QUrl::fromLocalFile(test_path + "ubuntu-16.04-server-cloudimg-amd64-disk1.img").toString()));
    EXPECT_THAT(info.id, Eq(QString("7bb741d90f56a42bbde01633d1f6e9204693c6ae357edd42a0b363945a76812f")));
    EXPECT_THAT(info.release, Eq(QString("snapcraft-core16")));
    EXPECT_THAT(info.release_title, Eq(QString("Snapcraft builder for Core 16")));
    EXPECT_TRUE(info.supported);
    EXPECT_FALSE(info.version.isEmpty());
}

TEST_F(CustomImageHost, returns_expected_data_for_snapcraft_core18)
{
    mp::CustomVMImageHost host{&url_downloader, test_path};

    auto info = *host.info_for(make_query("core18", "snapcraft"));

    EXPECT_THAT(info.image_location,
                Eq(QUrl::fromLocalFile(test_path + "ubuntu-18.04-server-cloudimg-amd64.img").toString()));
    EXPECT_THAT(info.id, Eq(QString("ba081faaf5128d2855803128bbdce0c137e310d1725c6bcc3d5aaa2daa96f224")));
    EXPECT_THAT(info.release, Eq(QString("snapcraft-core18")));
    EXPECT_THAT(info.release_title, Eq(QString("Snapcraft builder for Core 18")));
    EXPECT_TRUE(info.supported);
    EXPECT_FALSE(info.version.isEmpty());
}

TEST_F(CustomImageHost, iterates_over_all_entries)
{
    mp::CustomVMImageHost host{&url_downloader, test_path};

    std::unordered_set<std::string> ids;
    auto action = [&ids](const std::string& remote, const mp::VMImageInfo& info) { ids.insert(info.id.toStdString()); };
    host.for_each_entry_do(action);

    const size_t expected_entries{3};
    EXPECT_THAT(ids.size(), Eq(expected_entries));

    EXPECT_THAT(ids.count("934d52e4251537ee3bd8c500f212ae4c34992447e7d40d94f00bc7c21f72ceb7"), Eq(1u));
    EXPECT_THAT(ids.count("7bb741d90f56a42bbde01633d1f6e9204693c6ae357edd42a0b363945a76812f"), Eq(1u));
    EXPECT_THAT(ids.count("ba081faaf5128d2855803128bbdce0c137e310d1725c6bcc3d5aaa2daa96f224"), Eq(1u));
}

TEST_F(CustomImageHost, all_images_for_snapcraft_returns_two_matches)
{
    mp::CustomVMImageHost host{&url_downloader, test_path};

    auto images = host.all_images_for("snapcraft");

    const size_t expected_matches{2};
    EXPECT_THAT(images.size(), Eq(expected_matches));
}

TEST_F(CustomImageHost, all_info_for_snapcraft_returns_one_alias_match)
{
    mp::CustomVMImageHost host{&url_downloader, test_path};

    auto images_info = host.all_info_for(make_query("core16", "snapcraft"));

    const size_t expected_matches{1};
    EXPECT_THAT(images_info.size(), Eq(expected_matches));
}

TEST_F(CustomImageHost, invalid_image_throws_error)
{
    mp::CustomVMImageHost host{&url_downloader, test_path};

    EXPECT_THROW(*host.info_for(make_query("foo", "")), std::runtime_error);
}

TEST_F(CustomImageHost, invalid_remote_throws_error)
{
    mp::CustomVMImageHost host{&url_downloader, test_path};

    EXPECT_THROW(*host.info_for(make_query("core", "foo")), std::runtime_error);
}
