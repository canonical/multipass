/*
 * Copyright (C) 2017 Canonical, Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include "src/daemon/ubuntu_image_host.h"

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
struct UbuntuImageHost : public testing::Test
{
    mp::Query make_query(std::string release, std::string remote)
    {
        return {"", release, false, remote, mp::Query::Type::SimpleStreams};
    }

    QString host_url{QUrl::fromLocalFile(mpt::test_data_path()).toString() + "releases/"};
    QString daily_url{QUrl::fromLocalFile(mpt::test_data_path()).toString() + "daily/"};
    mp::URLDownloader url_downloader;
    std::chrono::seconds default_ttl{1};
    QString expected_location{host_url + "newest_image.img"};
    QString expected_id{"8842e7a8adb01c7a30cc702b01a5330a1951b12042816e87efd24b61c5e2239f"};
};
}

TEST_F(UbuntuImageHost, returns_expected_info)
{
    mp::UbuntuVMImageHost host{{{"release", host_url.toStdString()}}, &url_downloader, default_ttl};

    auto info = host.info_for(make_query("xenial", "release"));

    EXPECT_THAT(info.image_location, Eq(expected_location));
    EXPECT_THAT(info.id, Eq(expected_id));
}

TEST_F(UbuntuImageHost, uses_default_on_unspecified_release)
{
    mp::UbuntuVMImageHost host{{{"release", host_url.toStdString()}}, &url_downloader, default_ttl};

    auto info = host.info_for(make_query("", "release"));

    EXPECT_THAT(info.image_location, Eq(expected_location));
    EXPECT_THAT(info.id, Eq(expected_id));
}

TEST_F(UbuntuImageHost, iterates_over_all_entries)
{
    mp::UbuntuVMImageHost host{{{"release", host_url.toStdString()}}, &url_downloader, default_ttl};

    std::unordered_set<std::string> ids;
    auto action = [&ids](const std::string& remote, const mp::VMImageInfo& info) { ids.insert(info.id.toStdString()); };
    host.for_each_entry_do(action);

    const size_t expected_entries{4};
    EXPECT_THAT(ids.size(), Eq(expected_entries));

    EXPECT_THAT(ids.count("1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac"), Eq(1u));
    EXPECT_THAT(ids.count("8842e7a8adb01c7a30cc702b01a5330a1951b12042816e87efd24b61c5e2239f"), Eq(1u));
    EXPECT_THAT(ids.count("1507bd2b3288ef4bacd3e699fe71b827b7ccf321ec4487e168a30d7089d3c8e4"), Eq(1u));
    EXPECT_THAT(ids.count("ab115b83e7a8bebf3d3a02bf55ad0cb75a0ed515fcbc65fb0c9abe76c752921c"), Eq(1u));
}

TEST_F(UbuntuImageHost, can_query_by_hash)
{
    mp::UbuntuVMImageHost host{{{"release", host_url.toStdString()}}, &url_downloader, default_ttl};
    const auto expected_id = "1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac";
    auto info = host.info_for(make_query(expected_id, "release"));
    EXPECT_THAT(info.id, Eq(expected_id));
}

TEST_F(UbuntuImageHost, can_query_by_partial_hash)
{
    mp::UbuntuVMImageHost host{{{"release", host_url.toStdString()}}, &url_downloader, default_ttl};
    const auto expected_id = "1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac";

    QStringList short_hashes;
    short_hashes << "1797"
                 << "1797c5"
                 << "1797c5c";

    for (const auto& hash : short_hashes)
    {
        auto info = host.info_for(make_query(hash.toStdString(), "release"));
        EXPECT_THAT(info.id, Eq(expected_id));
    }

    EXPECT_THROW(host.info_for(make_query("abcde", "release")), std::runtime_error);
}

TEST_F(UbuntuImageHost, supports_multiple_manifests)
{
    mp::UbuntuVMImageHost host{
        {{"release", host_url.toStdString()}, {"daily", daily_url.toStdString()}}, &url_downloader, default_ttl};

    QString daily_expected_location{daily_url + "newest-artful.img"};
    QString daily_expected_id{"c09f123b9589c504fe39ec6e9ebe5188c67be7d1fc4fb80c969bf877f5a8333a"};

    auto info = host.info_for(make_query("artful", "daily"));

    EXPECT_THAT(info.image_location, Eq(daily_expected_location));
    EXPECT_THAT(info.id, Eq(daily_expected_id));

    auto xenial_info = host.info_for(make_query("xenial", "release"));

    EXPECT_THAT(xenial_info.image_location, Eq(expected_location));
    EXPECT_THAT(xenial_info.id, Eq(expected_id));
}

TEST_F(UbuntuImageHost, looks_for_aliases_before_hashes)
{
    mp::UbuntuVMImageHost host{
        {{"release", host_url.toStdString()}, {"daily", daily_url.toStdString()}}, &url_downloader, default_ttl};

    QString daily_expected_location{daily_url + "newest-artful.img"};
    QString daily_expected_id{"c09f123b9589c504fe39ec6e9ebe5188c67be7d1fc4fb80c969bf877f5a8333a"};

    auto info = host.info_for(make_query("a", "daily"));

    EXPECT_THAT(info.image_location, Eq(daily_expected_location));
    EXPECT_THAT(info.id, Eq(daily_expected_id));
}

TEST_F(UbuntuImageHost, all_info_release_returns_multiple_hash_matches)
{
    mp::UbuntuVMImageHost host{
        {{"release", host_url.toStdString()}, {"daily", daily_url.toStdString()}}, &url_downloader, default_ttl};

    auto images_info = host.all_info_for(make_query("1", "release"));

    const size_t expected_matches{2};
    EXPECT_THAT(images_info.size(), Eq(expected_matches));
}

TEST_F(UbuntuImageHost, all_info_daily_no_matches_throws_error)
{
    mp::UbuntuVMImageHost host{
        {{"release", host_url.toStdString()}, {"daily", daily_url.toStdString()}}, &url_downloader, default_ttl};

    EXPECT_THROW(host.all_info_for(make_query("1", "daily")), std::runtime_error);
}

TEST_F(UbuntuImageHost, all_info_release_returns_one_alias_match)
{
    mp::UbuntuVMImageHost host{
        {{"release", host_url.toStdString()}, {"daily", daily_url.toStdString()}}, &url_downloader, default_ttl};

    auto images_info = host.all_info_for(make_query("xenial", "release"));

    const size_t expected_matches{1};
    EXPECT_THAT(images_info.size(), Eq(expected_matches));
}

TEST_F(UbuntuImageHost, invalid_remote_throws_error)
{
    mp::UbuntuVMImageHost host{
        {{"release", host_url.toStdString()}, {"daily", daily_url.toStdString()}}, &url_downloader, default_ttl};

    EXPECT_THROW(host.info_for(make_query("xenial", "foo")), std::runtime_error);
}
