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
#include "image_host_remote_count.h"
#include "mischievous_url_downloader.h"
#include "mock_platform.h"
#include "mock_settings.h"
#include "path.h"
#include "stub_url_downloader.h"

#include <src/daemon/ubuntu_image_host.h>

#include <multipass/constants.h>
#include <multipass/exceptions/download_exception.h>
#include <multipass/exceptions/unsupported_alias_exception.h>
#include <multipass/exceptions/unsupported_image_exception.h>
#include <multipass/exceptions/unsupported_remote_exception.h>
#include <multipass/query.h>

#include <QUrl>

#include <cstddef>
#include <optional>
#include <unordered_set>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace multipass::platform;
using namespace testing;
using namespace std::literals::chrono_literals;

namespace
{
struct UbuntuImageHost : public testing::Test
{
    UbuntuImageHost()
    {
        EXPECT_CALL(mock_platform, is_remote_supported(_)).WillRepeatedly(Return(true));
        EXPECT_CALL(mock_platform, is_alias_supported(_, _)).WillRepeatedly(Return(true));

        EXPECT_CALL(mock_settings, get(Eq(mp::driver_key))).WillRepeatedly(Return("emu")); /* TODO parameterize driver
                                                                                              (code branches for lxd) */
        EXPECT_CALL(mock_settings, get(Eq(mp::mirror_key))).WillRepeatedly(Return(""));
    }

    mp::Query make_query(std::string release, std::string remote)
    {
        return {"", std::move(release), false, std::move(remote), mp::Query::Type::Alias};
    }

    QString test_host = QUrl::fromLocalFile(mpt::test_data_path()).toString();
    QString test_valid_mirror_host = QUrl::fromLocalFile(mpt::test_data_sub_dir_path("valid_image_mirror")).toString();
    QString test_valid_outdated_mirror_host =
        QUrl::fromLocalFile(mpt::test_data_sub_dir_path("valid_outdated_image_mirror")).toString();
    QString test_invalid_mirror_host =
        QUrl::fromLocalFile(mpt::test_data_sub_dir_path("invalid_image_mirror")).toString();

    std::string mock_image_host = test_host.toStdString();
    QString host_url{test_host + "releases/"};
    QString daily_url{test_host + "daily/"};
    std::pair<std::string, mp::UbuntuVMImageRemote> release_remote_spec = {
        "release", mp::UbuntuVMImageRemote{mock_image_host, "releases/"}};
    std::pair<std::string, mp::UbuntuVMImageRemote> release_remote_spec_with_mirror_allowed = {
        "release", mp::UbuntuVMImageRemote{mock_image_host, "releases/", std::make_optional<QString>(mp::mirror_key)}};
    std::pair<std::string, mp::UbuntuVMImageRemote> daily_remote_spec = {
        "daily", mp::UbuntuVMImageRemote{mock_image_host, "daily/"}};
    std::vector<std::pair<std::string, mp::UbuntuVMImageRemote>> all_remote_specs = {release_remote_spec,
                                                                                     daily_remote_spec};
    mpt::MischievousURLDownloader url_downloader{std::chrono::seconds{10}};
    QString expected_location{host_url + "newest_image.img"};
    QString expected_id{"8842e7a8adb01c7a30cc702b01a5330a1951b12042816e87efd24b61c5e2239f"};

    mpt::MockPlatform::GuardedMock mock_platform_injection{mpt::MockPlatform::inject()};
    mpt::MockPlatform& mock_platform = *mock_platform_injection.first;

    mpt::MockSettings::GuardedMock mock_settings_injection = mpt::MockSettings::inject<StrictMock>();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;
};
} // namespace

TEST_F(UbuntuImageHost, returns_expected_info)
{
    mp::UbuntuVMImageHost host{{release_remote_spec}, &url_downloader};
    host.update_manifests(false);
    auto info = host.info_for(make_query("xenial", release_remote_spec.first));

    ASSERT_TRUE(info);
    EXPECT_THAT(info->image_location, Eq(expected_location));
    EXPECT_THAT(info->id, Eq(expected_id));
}

TEST_F(UbuntuImageHost, returns_expected_mirror_info)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::mirror_key))).WillRepeatedly(Return(test_valid_mirror_host));

    mp::UbuntuVMImageHost host{{release_remote_spec_with_mirror_allowed}, &url_downloader};
    host.update_manifests(false);

    auto info = host.info_for(make_query("xenial", release_remote_spec.first));
    QString expected_location{test_valid_mirror_host + "releases/" + "newest_image.img"};

    ASSERT_TRUE(info);
    EXPECT_THAT(info->image_location, Eq(expected_location));
    EXPECT_THAT(info->id, Eq(expected_id));
}

TEST_F(UbuntuImageHost, returns_expected_mirror_info_with_most_recent_image)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::mirror_key))).WillRepeatedly(Return(test_valid_outdated_mirror_host));

    mp::UbuntuVMImageHost host{{release_remote_spec_with_mirror_allowed}, &url_downloader};
    host.update_manifests(false);

    auto info = host.info_for(make_query("xenial", release_remote_spec.first));
    QString expected_location{test_valid_outdated_mirror_host + "releases/" + "test_image.img"};
    QString expected_id{"1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac"};

    ASSERT_TRUE(info);
    EXPECT_THAT(info->image_location, Eq(expected_location));
    EXPECT_THAT(info->id, Eq(expected_id));
}

TEST_F(UbuntuImageHost, throw_if_mirror_is_invalid)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::mirror_key))).WillRepeatedly(Return(test_invalid_mirror_host));

    mp::UbuntuVMImageHost host{{release_remote_spec_with_mirror_allowed}, &url_downloader};
    host.update_manifests(false);

    EXPECT_THROW(host.info_for(make_query("xenial", release_remote_spec.first)), std::runtime_error);
}

TEST_F(UbuntuImageHost, uses_default_on_unspecified_release)
{
    mp::UbuntuVMImageHost host{{release_remote_spec}, &url_downloader};
    host.update_manifests(false);

    auto info = host.info_for(make_query("", release_remote_spec.first));

    ASSERT_TRUE(info);
    EXPECT_THAT(info->image_location, Eq(expected_location));
    EXPECT_THAT(info->id, Eq(expected_id));
}

TEST_F(UbuntuImageHost, iterates_over_all_entries)
{
    mp::UbuntuVMImageHost host{{release_remote_spec}, &url_downloader};
    host.update_manifests(false);

    std::unordered_set<std::string> ids;
    auto action = [&ids](const std::string& remote, const mp::VMImageInfo& info) { ids.insert(info.id.toStdString()); };
    host.for_each_entry_do(action);

    const size_t expected_entries{5};
    EXPECT_THAT(ids.size(), Eq(expected_entries));

    EXPECT_THAT(ids.count("1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac"), Eq(1u));
    EXPECT_THAT(ids.count("8842e7a8adb01c7a30cc702b01a5330a1951b12042816e87efd24b61c5e2239f"), Eq(1u));
    EXPECT_THAT(ids.count("1507bd2b3288ef4bacd3e699fe71b827b7ccf321ec4487e168a30d7089d3c8e4"), Eq(1u));
    EXPECT_THAT(ids.count("ab115b83e7a8bebf3d3a02bf55ad0cb75a0ed515fcbc65fb0c9abe76c752921c"), Eq(1u));
    EXPECT_THAT(ids.count("520224efaaf49b15a976b49c7ce7f2bd2e5b161470d684b37a838933595c0520"), Eq(1u));
}

TEST_F(UbuntuImageHost, unsupported_alias_iterates_over_expected_entries)
{
    mp::UbuntuVMImageHost host{{release_remote_spec}, &url_downloader};
    host.update_manifests(false);

    std::unordered_set<std::string> ids;
    auto action = [&ids](const std::string& remote, const mp::VMImageInfo& info) { ids.insert(info.id.toStdString()); };

    EXPECT_CALL(mock_platform, is_alias_supported(AnyOf("zesty", "17.04", "z"), _)).WillRepeatedly(Return(false));

    host.for_each_entry_do(action);

    const size_t expected_entries{4};
    EXPECT_THAT(ids.size(), Eq(expected_entries));
}

TEST_F(UbuntuImageHost, can_query_by_hash)
{
    mp::UbuntuVMImageHost host{{release_remote_spec}, &url_downloader};
    host.update_manifests(false);
    const auto expected_id = "1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac";
    auto info = host.info_for(make_query(expected_id, release_remote_spec.first));

    ASSERT_TRUE(info);
    EXPECT_THAT(info->id, Eq(expected_id));
}

TEST_F(UbuntuImageHost, can_query_by_partial_hash)
{
    mp::UbuntuVMImageHost host{{release_remote_spec}, &url_downloader};
    host.update_manifests(false);
    const auto expected_id = "1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac";

    QStringList short_hashes;
    short_hashes << "1797"
                 << "1797c5"
                 << "1797c5c";

    for (const auto& hash : short_hashes)
    {
        auto info = host.info_for(make_query(hash.toStdString(), release_remote_spec.first));

        ASSERT_TRUE(info);
        EXPECT_THAT(info->id, Eq(expected_id));
    }

    EXPECT_FALSE(host.info_for(make_query("abcde", release_remote_spec.first)));
}

TEST_F(UbuntuImageHost, supports_multiple_manifests)
{
    mp::UbuntuVMImageHost host{all_remote_specs, &url_downloader};
    host.update_manifests(false);

    QString daily_expected_location{daily_url + "newest-artful.img"};
    QString daily_expected_id{"c09f123b9589c504fe39ec6e9ebe5188c67be7d1fc4fb80c969bf877f5a8333a"};

    auto info = host.info_for(make_query("artful", daily_remote_spec.first));

    ASSERT_TRUE(info);
    EXPECT_THAT(info->image_location, Eq(daily_expected_location));
    EXPECT_THAT(info->id, Eq(daily_expected_id));

    auto xenial_info = host.info_for(make_query("xenial", release_remote_spec.first));

    ASSERT_TRUE(xenial_info);
    EXPECT_THAT(xenial_info->image_location, Eq(expected_location));
    EXPECT_THAT(xenial_info->id, Eq(expected_id));
}

TEST_F(UbuntuImageHost, looks_for_aliases_before_hashes)
{
    mp::UbuntuVMImageHost host{all_remote_specs, &url_downloader};
    host.update_manifests(false);

    QString daily_expected_location{daily_url + "newest-artful.img"};
    QString daily_expected_id{"c09f123b9589c504fe39ec6e9ebe5188c67be7d1fc4fb80c969bf877f5a8333a"};

    auto info = host.info_for(make_query("a", daily_remote_spec.first));

    ASSERT_TRUE(info);
    EXPECT_THAT(info->image_location, Eq(daily_expected_location));
    EXPECT_THAT(info->id, Eq(daily_expected_id));
}

TEST_F(UbuntuImageHost, all_info_release_returns_multiple_hash_matches)
{
    mp::UbuntuVMImageHost host{all_remote_specs, &url_downloader};
    host.update_manifests(false);

    auto images_info = host.all_info_for(make_query("1", release_remote_spec.first));

    const size_t expected_matches{2};
    EXPECT_THAT(images_info.size(), Eq(expected_matches));
}

TEST_F(UbuntuImageHost, all_info_daily_no_matches_returns_empty_vector)
{
    mp::UbuntuVMImageHost host{all_remote_specs, &url_downloader};
    host.update_manifests(false);

    auto images = host.all_info_for(make_query("1", daily_remote_spec.first));

    EXPECT_TRUE(images.empty());
}

TEST_F(UbuntuImageHost, all_info_release_returns_one_alias_match)
{
    mp::UbuntuVMImageHost host{all_remote_specs, &url_downloader};
    host.update_manifests(false);

    auto images_info = host.all_info_for(make_query("xenial", release_remote_spec.first));

    const size_t expected_matches{1};
    EXPECT_THAT(images_info.size(), Eq(expected_matches));
}

TEST_F(UbuntuImageHost, all_images_for_release_returns_four_matches)
{
    mp::UbuntuVMImageHost host{all_remote_specs, &url_downloader};
    host.update_manifests(false);

    auto images = host.all_images_for(release_remote_spec.first, false);

    const size_t expected_matches{4};
    EXPECT_THAT(images.size(), Eq(expected_matches));
}

TEST_F(UbuntuImageHost, all_images_for_release_unsupported_returns_five_matches)
{
    mp::UbuntuVMImageHost host{all_remote_specs, &url_downloader};
    host.update_manifests(false);

    auto images = host.all_images_for(release_remote_spec.first, true);

    const size_t expected_matches{5};
    EXPECT_THAT(images.size(), Eq(expected_matches));
}

TEST_F(UbuntuImageHost, all_images_for_daily_returns_all_matches)
{
    mp::UbuntuVMImageHost host{all_remote_specs, &url_downloader};
    host.update_manifests(false);

    auto images = host.all_images_for(daily_remote_spec.first, false);

    const size_t expected_matches{3};
    EXPECT_THAT(images.size(), Eq(expected_matches));
}

TEST_F(UbuntuImageHost, all_images_for_release_unsupported_alias_returns_three_matches)
{
    mp::UbuntuVMImageHost host{all_remote_specs, &url_downloader};
    host.update_manifests(false);

    EXPECT_CALL(mock_platform, is_alias_supported(AnyOf("zesty", "17.04", "z"), _)).WillRepeatedly(Return(false));

    auto images = host.all_images_for(release_remote_spec.first, false);

    const size_t expected_matches{3};
    EXPECT_EQ(images.size(), expected_matches);
}

TEST_F(UbuntuImageHost, supported_remotes_returns_expected_values)
{
    mp::UbuntuVMImageHost host{all_remote_specs, &url_downloader};
    host.update_manifests(false);

    auto supported_remotes = host.supported_remotes();

    const size_t expected_size{2};
    EXPECT_THAT(supported_remotes.size(), Eq(expected_size));

    EXPECT_TRUE(std::find(supported_remotes.begin(), supported_remotes.end(), release_remote_spec.first) !=
                supported_remotes.end());
    EXPECT_TRUE(std::find(supported_remotes.begin(), supported_remotes.end(), daily_remote_spec.first) !=
                supported_remotes.end());
}

TEST_F(UbuntuImageHost, invalid_remote_throws_error)
{
    mp::UbuntuVMImageHost host{all_remote_specs, &url_downloader};
    host.update_manifests(false);

    EXPECT_THROW(host.info_for(make_query("xenial", "foo")), std::runtime_error);
}

TEST_F(UbuntuImageHost, handles_and_recovers_from_initial_network_failure)
{
    url_downloader.mischiefs = 1000;
    mp::UbuntuVMImageHost host{all_remote_specs, &url_downloader};
    EXPECT_THROW(host.update_manifests(false), mp::DownloadException);

    const auto query = make_query("xenial", release_remote_spec.first);
    EXPECT_THROW(host.info_for(query), std::runtime_error);

    url_downloader.mischiefs = 0;
    host.update_manifests(false);
    EXPECT_TRUE(host.info_for(query));
}

TEST_F(UbuntuImageHost, handles_and_recovers_from_later_network_failure)
{
    mp::UbuntuVMImageHost host{all_remote_specs, &url_downloader};

    const auto query = make_query("xenial", release_remote_spec.first);
    host.update_manifests(false);
    EXPECT_TRUE(host.info_for(query));

    url_downloader.mischiefs = 1000;
    EXPECT_THROW(host.update_manifests(false), mp::DownloadException);
    EXPECT_THROW(host.info_for(query), std::runtime_error);

    url_downloader.mischiefs = 0;
    host.update_manifests(false);
    EXPECT_TRUE(host.info_for(query));
}

TEST_F(UbuntuImageHost, handles_and_recovers_from_independent_server_failures)
{
    mp::UbuntuVMImageHost host{all_remote_specs, &url_downloader};
    host.update_manifests(false);

    const auto num_remotes = mpt::count_remotes(host);
    EXPECT_GT(num_remotes, 0u);

    url_downloader.mischiefs = 0;
    EXPECT_EQ(mpt::count_remotes(host), num_remotes);

    for (size_t i = 1; i < num_remotes; ++i)
    {
        url_downloader.mischiefs = i;
        EXPECT_THROW(mpt::count_remotes(host), mp::DownloadException);
    }
}

TEST_F(UbuntuImageHost, throws_unsupported_image_when_image_not_supported)
{
    mp::UbuntuVMImageHost host{all_remote_specs, &url_downloader};
    host.update_manifests(false);

    EXPECT_THROW(host.info_for(make_query("artful", release_remote_spec.first)), mp::UnsupportedImageException);
}

TEST_F(UbuntuImageHost, devel_request_with_no_remote_returns_expected_info)
{
    mp::UbuntuVMImageHost host{all_remote_specs, &url_downloader};
    host.update_manifests(false);

    QString daily_expected_location{daily_url + "newest-artful.img"};
    QString daily_expected_id{"c09f123b9589c504fe39ec6e9ebe5188c67be7d1fc4fb80c969bf877f5a8333a"};

    auto info = host.info_for(make_query("devel", ""));

    ASSERT_TRUE(info);
    EXPECT_EQ(info->image_location, daily_expected_location);
    EXPECT_EQ(info->id, daily_expected_id);
}

TEST_F(UbuntuImageHost, info_for_too_many_hash_matches_throws)
{
    mp::UbuntuVMImageHost host{{release_remote_spec}, &url_downloader};
    host.update_manifests(false);

    const std::string release{"1"};

    MP_EXPECT_THROW_THAT(host.info_for(make_query(release, release_remote_spec.first)), std::runtime_error,
                         mpt::match_what(StrEq(fmt::format("Too many images matching \"{}\"", release))));
}

TEST_F(UbuntuImageHost, info_for_same_full_hash_in_both_remotes_does_not_throw)
{
    mp::UbuntuVMImageHost host{all_remote_specs, &url_downloader};
    host.update_manifests(false);

    const auto hash_query{"ab115b83e7a8bebf3d3a02bf55ad0cb75a0ed515fcbc65fb0c9abe76c752921c"};

    EXPECT_NO_THROW(host.info_for(make_query(hash_query, "")));
}

TEST_F(UbuntuImageHost, info_for_partial_hash_in_both_remotes_throws)
{
    mp::UbuntuVMImageHost host{all_remote_specs, &url_downloader};
    host.update_manifests(false);

    const auto hash_query{"ab115"};

    MP_EXPECT_THROW_THAT(host.info_for(make_query(hash_query, "")), std::runtime_error,
                         mpt::match_what(StrEq(fmt::format("Too many images matching \"{}\"", hash_query))));
}

TEST_F(UbuntuImageHost, all_info_for_no_remote_query_defaults_to_release)
{
    mp::UbuntuVMImageHost host{all_remote_specs, &url_downloader};
    host.update_manifests(false);

    auto images_info = host.all_info_for(make_query("1", ""));

    const size_t expected_matches{2};
    EXPECT_EQ(images_info.size(), expected_matches);
}

TEST_F(UbuntuImageHost, all_info_for_unsupported_image_throw)
{
    mp::UbuntuVMImageHost host{all_remote_specs, &url_downloader};
    host.update_manifests(false);

    const std::string release{"artful"};

    MP_EXPECT_THROW_THAT(host.all_info_for(make_query(release, release_remote_spec.first)),
                         mp::UnsupportedImageException,
                         mpt::match_what(StrEq(fmt::format("The {} release is no longer supported.", release))));
}

TEST_F(UbuntuImageHost, all_info_for_unsupported_alias_throws)
{
    mp::UbuntuVMImageHost host{all_remote_specs, &url_downloader};
    host.update_manifests(false);

    const std::string unsupported_alias{"daily"};
    EXPECT_CALL(mock_platform, is_alias_supported(unsupported_alias, _)).WillOnce(Return(false));

    MP_EXPECT_THROW_THAT(
        host.all_info_for(make_query(unsupported_alias, release_remote_spec.first)), mp::UnsupportedAliasException,
        mpt::match_what(HasSubstr(fmt::format("\'{}\' is not a supported alias.", unsupported_alias))));
}

TEST_F(UbuntuImageHost, info_for_unsupported_remote_throws)
{
    mp::UbuntuVMImageHost host{all_remote_specs, &url_downloader};
    host.update_manifests(false);

    const std::string unsupported_remote{"bar"};
    EXPECT_CALL(mock_platform, is_remote_supported(unsupported_remote)).WillRepeatedly(Return(false));

    MP_EXPECT_THROW_THAT(host.info_for(make_query("xenial", unsupported_remote)), mp::UnsupportedRemoteException,
                         mpt::match_what(HasSubstr(fmt::format(
                             "Remote \'{}\' is not a supported remote for this platform.", unsupported_remote))));
}

TEST_F(UbuntuImageHost, info_for_no_remote_first_unsupported_returns_expected_info)
{
    mp::UbuntuVMImageHost host{all_remote_specs, &url_downloader};
    host.update_manifests(false);

    EXPECT_CALL(mock_platform, is_remote_supported("release")).Times(AtLeast(1)).WillRepeatedly(Return(false));

    auto info = host.info_for(make_query("artful", ""));

    EXPECT_EQ(info->id, "c09f123b9589c504fe39ec6e9ebe5188c67be7d1fc4fb80c969bf877f5a8333a");
}
