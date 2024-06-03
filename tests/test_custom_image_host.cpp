
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
#include "mock_platform.h"
#include "mock_url_downloader.h"
#include "path.h"

#include <src/daemon/custom_image_host.h>

#include <multipass/exceptions/download_exception.h>
#include <multipass/exceptions/unsupported_alias_exception.h>
#include <multipass/exceptions/unsupported_remote_exception.h>
#include <multipass/format.h>
#include <multipass/query.h>

#include <QUrl>

#include <cstddef>
#include <unordered_set>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace multipass::platform;
using namespace testing;
using namespace std::literals::chrono_literals;

namespace
{

constexpr auto sha256_sums =
    "934d52e4251537ee3bd8c500f212ae4c34992447e7d40d94f00bc7c21f72ceb7 *ubuntu-core-16-amd64.img.xz\n"
    "1ffea8a9caf5a4dcba4f73f9144cb4afe1e4fc1987f4ab43bed4c02fad9f087f *ubuntu-core-18-amd64.img.xz\n"
    "52a4606b0b3b28e4cb64e2c2595ef8fdbb4170bfd3596f4e0b84f4d84511b614 *ubuntu-core-20-amd64.img.xz\n"
    "6378b1fa3db76cdf18c905c8282ebc97401951a9338722486f653dbf16eb7915 *ubuntu-core-22-amd64.img.xz\n"
    "192c40c8f3361f4f9da2757d87e409ac5abb2df393145983d3696e21f486b552 *ubuntu-core-24-amd64.img.xz\n";

struct CustomImageHost : public Test
{
    CustomImageHost()
    {
        EXPECT_CALL(*mock_platform, is_remote_supported(_)).WillRepeatedly(Return(true));
        EXPECT_CALL(*mock_platform, is_alias_supported(_, _)).WillRepeatedly(Return(true));

        ON_CALL(mock_url_downloader, last_modified(_)).WillByDefault(Return(QDateTime::currentDateTime()));
        ON_CALL(mock_url_downloader, download(_)).WillByDefault(Return(sha256_sums));
        ON_CALL(mock_url_downloader, download(_, _)).WillByDefault(Return(sha256_sums));
    }

    mp::Query make_query(std::string release, std::string remote)
    {
        return {"", std::move(release), false, std::move(remote), mp::Query::Type::Alias};
    }

    NiceMock<mpt::MockURLDownloader> mock_url_downloader;

    mpt::MockPlatform::GuardedMock attr{mpt::MockPlatform::inject()};
    mpt::MockPlatform* mock_platform = attr.first;
};
} // namespace

typedef std::tuple<std::vector<std::string> /* aliases */, std::string /* remote */, QUrl /* url */, QString /* id */,
                   QString /* release */, QString /* release_title */>
    CustomData;

struct ExpectedDataSuite : CustomImageHost, WithParamInterface<CustomData>
{
};

TEST_P(ExpectedDataSuite, returns_expected_data)
{
    const auto [aliases, remote, url, id, release, release_title] = GetParam();

    mp::CustomVMImageHost host{"x86_64", &mock_url_downloader};
    host.update_manifests(false);
    for (const auto& alias : aliases)
    {
        auto info = host.info_for(make_query(alias, remote));

        ASSERT_TRUE(info);
        EXPECT_EQ(info->image_location, url.toDisplayString());
        EXPECT_EQ(info->id, id);
        EXPECT_EQ(info->release, release);
        EXPECT_EQ(info->release_title, release_title);
        EXPECT_TRUE(info->supported);
        EXPECT_EQ(info->version, QLocale::c().toString(QDateTime::currentDateTime(), "yyyyMMdd"));
    }
}

INSTANTIATE_TEST_SUITE_P(
    CustomImageHost, ExpectedDataSuite,
    Values(CustomData{std::vector<std::string>{"core", "core16"}, "",
                      "https://cdimage.ubuntu.com/ubuntu-core/16/stable/current/ubuntu-core-16-amd64.img.xz",
                      "934d52e4251537ee3bd8c500f212ae4c34992447e7d40d94f00bc7c21f72ceb7", "core-16", "Core 16"},
           CustomData{std::vector<std::string>{"core18"}, "",
                      "https://cdimage.ubuntu.com/ubuntu-core/18/stable/current/ubuntu-core-18-amd64.img.xz",
                      "1ffea8a9caf5a4dcba4f73f9144cb4afe1e4fc1987f4ab43bed4c02fad9f087f", "core-18", "Core 18"}));

TEST_F(CustomImageHost, iterates_over_all_entries)
{
    mp::CustomVMImageHost host{"x86_64", &mock_url_downloader};
    host.update_manifests(false);
    std::unordered_set<std::string> ids;
    auto action = [&ids](const std::string& remote, const mp::VMImageInfo& info) { ids.insert(info.id.toStdString()); };
    host.for_each_entry_do(action);

    EXPECT_THAT(ids,
                UnorderedElementsAre("934d52e4251537ee3bd8c500f212ae4c34992447e7d40d94f00bc7c21f72ceb7",
                                     "1ffea8a9caf5a4dcba4f73f9144cb4afe1e4fc1987f4ab43bed4c02fad9f087f",
                                     "52a4606b0b3b28e4cb64e2c2595ef8fdbb4170bfd3596f4e0b84f4d84511b614",
                                     "6378b1fa3db76cdf18c905c8282ebc97401951a9338722486f653dbf16eb7915",
                                     "192c40c8f3361f4f9da2757d87e409ac5abb2df393145983d3696e21f486b552"));
}

TEST_F(CustomImageHost, unsupported_alias_iterates_over_expected_entries)
{
    std::unordered_set<std::string> ids;
    EXPECT_CALL(*mock_platform, is_alias_supported("core18", _)).WillRepeatedly(Return(false));

    auto action = [&ids](const std::string& remote, const mp::VMImageInfo& info) { ids.insert(info.id.toStdString()); };
    mp::CustomVMImageHost host{"x86_64", &mock_url_downloader};
    host.update_manifests(false);
    host.for_each_entry_do(action);

    const size_t expected_entries{4};
    EXPECT_EQ(ids.size(), expected_entries);
}

TEST_F(CustomImageHost, unsupported_remote_iterates_over_expected_entries)
{
    const std::string unsupported_remote{""};
    EXPECT_CALL(*mock_platform, is_remote_supported(unsupported_remote)).WillRepeatedly(Return(false));

    mp::CustomVMImageHost host{"x86_64", &mock_url_downloader};
    host.update_manifests(false);

    std::unordered_set<std::string> ids;
    auto action = [&ids](const std::string& remote, const mp::VMImageInfo& info) { ids.insert(info.id.toStdString()); };
    host.for_each_entry_do(action);

    const size_t expected_entries{0};
    EXPECT_EQ(ids.size(), expected_entries);
}

TEST_F(CustomImageHost, all_images_for_no_remote_returns_appropriate_matches)
{
    mp::CustomVMImageHost host{"x86_64", &mock_url_downloader};
    host.update_manifests(false);
    auto images = host.all_images_for("", false);

    const size_t expected_matches{5};
    EXPECT_THAT(images.size(), Eq(expected_matches));
}

TEST_F(CustomImageHost, all_images_for_no_remote_unsupported_alias_returns_appropriate_matches)
{
    mp::CustomVMImageHost host{"x86_64", &mock_url_downloader};
    host.update_manifests(false);

    const std::string unsupported_alias{"core18"};
    EXPECT_CALL(*mock_platform, is_alias_supported(unsupported_alias, _)).WillOnce(Return(false));

    auto images = host.all_images_for("", false);

    const size_t expected_matches{4};
    EXPECT_EQ(images.size(), expected_matches);
}

TEST_F(CustomImageHost, all_info_for_no_remote_returns_one_alias_match)
{
    mp::CustomVMImageHost host{"x86_64", &mock_url_downloader};
    host.update_manifests(false);
    auto images_info = host.all_info_for(make_query("core20", ""));

    const size_t expected_matches{1};
    EXPECT_THAT(images_info.size(), Eq(expected_matches));
}

TEST_F(CustomImageHost, all_info_for_unsupported_alias_throws)
{
    mp::CustomVMImageHost host{"x86_64", &mock_url_downloader};
    host.update_manifests(false);

    const std::string unsupported_alias{"core20"};
    EXPECT_CALL(*mock_platform, is_alias_supported(unsupported_alias, _)).WillOnce(Return(false));

    MP_EXPECT_THROW_THAT(
        host.all_info_for(make_query(unsupported_alias, "")), mp::UnsupportedAliasException,
        mpt::match_what(HasSubstr(fmt::format("\'{}\' is not a supported alias.", unsupported_alias))));
}

TEST_F(CustomImageHost, supported_remotes_returns_expected_values)
{
    mp::CustomVMImageHost host{"x86_64", &mock_url_downloader};
    host.update_manifests(false);

    auto supported_remotes = host.supported_remotes();

    const size_t expected_size{1};
    EXPECT_THAT(supported_remotes.size(), Eq(expected_size));

    EXPECT_TRUE(std::find(supported_remotes.begin(), supported_remotes.end(), "") != supported_remotes.end());
}

TEST_F(CustomImageHost, invalid_image_returns_false)
{
    mp::CustomVMImageHost host{"x86_64", &mock_url_downloader};
    host.update_manifests(false);

    EXPECT_FALSE(host.info_for(make_query("foo", "")));
}

TEST_F(CustomImageHost, invalid_remote_throws_error)
{
    mp::CustomVMImageHost host{"x86_64", &mock_url_downloader};
    host.update_manifests(false);

    EXPECT_THROW(host.info_for(make_query("core", "foo")), std::runtime_error);
}

TEST_F(CustomImageHost, handles_and_recovers_from_initial_network_failure)
{
    EXPECT_CALL(mock_url_downloader, last_modified(_))
        .WillOnce(Throw(mp::DownloadException{"", ""}))
        .WillRepeatedly(DoDefault());

    mp::CustomVMImageHost host{"x86_64", &mock_url_downloader};

    const auto query = make_query("core20", "");
    EXPECT_THROW(host.update_manifests(false), mp::DownloadException);
    EXPECT_THROW(host.info_for(query), std::runtime_error);

    host.update_manifests(false);
    EXPECT_TRUE(host.info_for(query));
}

TEST_F(CustomImageHost, handles_and_recovers_from_later_network_failure)
{
    mp::CustomVMImageHost host{"x86_64", &mock_url_downloader};

    const auto query = make_query("core20", "");
    host.update_manifests(false);
    EXPECT_TRUE(host.info_for(query));

    EXPECT_CALL(mock_url_downloader, last_modified(_))
        .WillOnce(Throw(mp::DownloadException{"", ""}))
        .WillRepeatedly(DoDefault());
    EXPECT_THROW(host.update_manifests(false), mp::DownloadException);
    EXPECT_THROW(host.info_for(query), std::runtime_error);

    host.update_manifests(false);
    EXPECT_TRUE(host.info_for(query));
}

TEST_F(CustomImageHost, handles_and_recovers_from_independent_server_failures)
{
    mp::CustomVMImageHost host{"x86_64", &mock_url_downloader};
    host.update_manifests(false);

    const auto num_remotes = mpt::count_remotes(host);
    EXPECT_GT(num_remotes, 0u);

    for (size_t i = 0; i < num_remotes; ++i)
    {
        Sequence seq;
        if (i > 0)
        {
            EXPECT_CALL(mock_url_downloader, last_modified(_))
                .Times(i)
                .InSequence(seq)
                .WillRepeatedly(Throw(mp::DownloadException{"", ""}));
        }
        EXPECT_CALL(mock_url_downloader, last_modified(_)).Times(AnyNumber()).InSequence(seq);

        EXPECT_EQ(mpt::count_remotes(host), num_remotes - i);
        EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_url_downloader));
    }
}

TEST_F(CustomImageHost, info_for_unsupported_remote_throws)
{
    mp::CustomVMImageHost host{"x86_64", &mock_url_downloader};
    host.update_manifests(false);

    const std::string unsupported_remote{"foo"};
    EXPECT_CALL(*mock_platform, is_remote_supported(unsupported_remote)).WillRepeatedly(Return(false));

    MP_EXPECT_THROW_THAT(host.info_for(make_query("xenial", unsupported_remote)), mp::UnsupportedRemoteException,
                         mpt::match_what(HasSubstr(fmt::format(
                             "Remote \'{}\' is not a supported remote for this platform.", unsupported_remote))));
}

TEST_F(CustomImageHost, info_for_full_hash_returns_empty_image_info)
{
    mp::CustomVMImageHost host{"x86_64", &mock_url_downloader};
    host.update_manifests(false);

    const auto info = host.info_for_full_hash("934d52e4251537ee3bd8c500f212ae4c34992447e7d40d94f00bc7c21f72ceb7");

    EXPECT_EQ(info, mp::VMImageInfo{});
}

struct EmptyArchSuite : CustomImageHost, WithParamInterface<QString>
{
};

TEST_P(EmptyArchSuite, empty_for_other_arches)
{
    auto arch = GetParam();
    mp::CustomVMImageHost host{arch, &mock_url_downloader};
    host.update_manifests(false);

    EXPECT_CALL(mock_url_downloader, last_modified(_)).Times(0);
    EXPECT_CALL(mock_url_downloader, download(_)).Times(0);

    std::unordered_set<std::string> ids;
    host.for_each_entry_do(
        [&ids](const std::string& remote, const mp::VMImageInfo& info) { ids.insert(info.id.toStdString()); });

    const size_t expected_entries{0};
    EXPECT_THAT(ids.size(), Eq(expected_entries));
}

INSTANTIATE_TEST_SUITE_P(CustomImageHost, EmptyArchSuite, Values("arm", "arm64", "i386", "power", "power64", "s390x"));
