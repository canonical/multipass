/*
 * Copyright (C) 2018-2021 Canonical, Ltd.
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

#include "extra_assertions.h"
#include "image_host_remote_count.h"
#include "mischievous_url_downloader.h"
#include "mock_platform.h"
#include "path.h"

#include <multipass/exceptions/unsupported_alias_exception.h>
#include <multipass/exceptions/unsupported_remote_exception.h>
#include <multipass/format.h>
#include <multipass/query.h>

#include <QUrl>

#include <gmock/gmock.h>

#include <cstddef>
#include <unordered_set>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace multipass::platform;
using namespace testing;
using namespace std::literals::chrono_literals;

namespace
{
struct CustomImageHost : public Test
{
    CustomImageHost()
    {
        EXPECT_CALL(*mock_platform, is_remote_supported(_)).WillRepeatedly(Return(true));
        EXPECT_CALL(*mock_platform, is_alias_supported(_, _)).WillRepeatedly(Return(true));
    }

    mp::Query make_query(std::string release, std::string remote)
    {
        return {"", std::move(release), false, std::move(remote), mp::Query::Type::Alias};
    }

    std::chrono::seconds timeout{10};
    mpt::MischievousURLDownloader url_downloader{timeout};
    std::chrono::seconds default_ttl{1};
    const QString test_path{mpt::test_data_path() + "custom/"};

    mpt::MockPlatform::GuardedMock attr{mpt::MockPlatform::inject()};
    mpt::MockPlatform* mock_platform = attr.first;
};
} // namespace

typedef typename std::tuple<std::vector<std::string>, std::string, bool, QString, QString, QString, QString>
    CustomImageHostQueryParam;

struct CustomImageHostQuerySuite : public CustomImageHost, public WithParamInterface<CustomImageHostQueryParam>
{
};

TEST_P(CustomImageHostQuerySuite, returns_expected_data)
{
    const auto& [aliases, remote, valid, image, hash, release, title] = GetParam();

    mp::CustomVMImageHost host{&url_downloader, default_ttl, test_path};

    for (const auto& alias : aliases)
    {

        auto info = host.info_for(make_query(alias, remote));

        if (valid)
        {
            ASSERT_TRUE(info);
            EXPECT_THAT(info->image_location, Eq(QUrl::fromLocalFile(test_path + image).toString()));
            EXPECT_THAT(info->id, Eq(hash));
            EXPECT_THAT(info->release, Eq(release));
            EXPECT_THAT(info->release_title, Eq(title));
            EXPECT_TRUE(info->supported);
            EXPECT_FALSE(info->version.isEmpty());
        }
        else
        {
            EXPECT_FALSE(info);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
    CustomImageHost, CustomImageHostQuerySuite,
    Values(CustomImageHostQueryParam{{"core", "core16"},
                                     "",
                                     true,
                                     "ubuntu-core-16-amd64.img.xz",
                                     "934d52e4251537ee3bd8c500f212ae4c34992447e7d40d94f00bc7c21f72ceb7",
                                     "core-16",
                                     "Core 16"},
           CustomImageHostQueryParam{{"core18"},
                                     "",
                                     true,
                                     "ubuntu-core-18-amd64.img.xz",
                                     "1ffea8a9caf5a4dcba4f73f9144cb4afe1e4fc1987f4ab43bed4c02fad9f087f",
                                     "core-18",
                                     "Core 18"},
           CustomImageHostQueryParam{{"core", "16.04"},
                                     "snapcraft",
                                     true,
                                     "ubuntu-16.04-minimal-cloudimg-amd64-disk1.img",
                                     "a6e6db185f53763d9d6607b186f1e6ae2dc02f8da8ea25e58d92c0c0c6dc4e48",
                                     "snapcraft-core16",
                                     "Snapcraft builder for Core 16"},
           CustomImageHostQueryParam{{"core16"}, "snapcraft", false, "", "", "", ""},
           CustomImageHostQueryParam{{"core18", "18.04"},
                                     "snapcraft",
                                     true,
                                     "bionic-server-cloudimg-amd64-disk.img",
                                     "96107afaa1673577c91dfbe2905a823043face65be6e8a0edc82f6b932d8380c",
                                     "snapcraft-core18",
                                     "Snapcraft builder for Core 18"},
           CustomImageHostQueryParam{{"core20", "20.04"},
                                     "snapcraft",
                                     true,
                                     "focal-server-cloudimg-amd64-disk.img",
                                     "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
                                     "snapcraft-core20",
                                     "Snapcraft builder for Core 20"}));

TEST_F(CustomImageHost, iterates_over_all_entries)
{
    mp::CustomVMImageHost host{&url_downloader, default_ttl, test_path};

    std::unordered_set<std::string> ids;
    auto action = [&ids](const std::string& remote, const mp::VMImageInfo& info) { ids.insert(info.id.toStdString()); };
    host.for_each_entry_do(action);

    const size_t expected_entries{5};
    EXPECT_THAT(ids.size(), Eq(expected_entries));

    EXPECT_THAT(ids.count("934d52e4251537ee3bd8c500f212ae4c34992447e7d40d94f00bc7c21f72ceb7"), Eq(1u));
    EXPECT_THAT(ids.count("1ffea8a9caf5a4dcba4f73f9144cb4afe1e4fc1987f4ab43bed4c02fad9f087f"), Eq(1u));
    EXPECT_THAT(ids.count("a6e6db185f53763d9d6607b186f1e6ae2dc02f8da8ea25e58d92c0c0c6dc4e48"), Eq(1u));
    EXPECT_THAT(ids.count("96107afaa1673577c91dfbe2905a823043face65be6e8a0edc82f6b932d8380c"), Eq(1u));
    EXPECT_THAT(ids.count("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"), Eq(1u));
}

TEST_F(CustomImageHost, unsupported_alias_iterates_over_expected_entries)
{
    mp::CustomVMImageHost host{&url_downloader, default_ttl, test_path};

    std::unordered_set<std::string> ids;
    auto action = [&ids](const std::string& remote, const mp::VMImageInfo& info) { ids.insert(info.id.toStdString()); };

    EXPECT_CALL(*mock_platform, is_alias_supported("core18", _)).WillRepeatedly(Return(false));

    host.for_each_entry_do(action);

    const size_t expected_entries{3};
    EXPECT_EQ(ids.size(), expected_entries);
}

TEST_F(CustomImageHost, unsupported_remote_iterates_over_expected_entries)
{
    mp::CustomVMImageHost host{&url_downloader, default_ttl, test_path};

    std::unordered_set<std::string> ids;
    auto action = [&ids](const std::string& remote, const mp::VMImageInfo& info) { ids.insert(info.id.toStdString()); };

    const std::string unsupported_remote{"snapcraft"};
    EXPECT_CALL(*mock_platform, is_remote_supported(unsupported_remote)).WillRepeatedly(Return(false));

    host.for_each_entry_do(action);

    const size_t expected_entries{2};
    EXPECT_EQ(ids.size(), expected_entries);
}

TEST_F(CustomImageHost, all_images_for_snapcraft_returns_three_matches)
{
    mp::CustomVMImageHost host{&url_downloader, default_ttl, test_path};

    auto images = host.all_images_for("snapcraft", false);

    const size_t expected_matches{3};
    EXPECT_THAT(images.size(), Eq(expected_matches));
}

TEST_F(CustomImageHost, all_images_for_snapcraft_unsupported_alias_returns_two_matches)
{
    mp::CustomVMImageHost host{&url_downloader, default_ttl, test_path};
    const std::string unsupported_alias{"core18"};

    EXPECT_CALL(*mock_platform, is_alias_supported(unsupported_alias, _)).WillOnce(Return(false));

    auto images = host.all_images_for("snapcraft", false);

    const size_t expected_matches{2};
    EXPECT_EQ(images.size(), expected_matches);
}

TEST_F(CustomImageHost, all_info_for_snapcraft_returns_one_alias_match)
{
    mp::CustomVMImageHost host{&url_downloader, default_ttl, test_path};

    auto images_info = host.all_info_for(make_query("core", "snapcraft"));

    const size_t expected_matches{1};
    EXPECT_THAT(images_info.size(), Eq(expected_matches));
}

TEST_F(CustomImageHost, all_info_for_unsupported_alias_throws)
{
    mp::CustomVMImageHost host{&url_downloader, default_ttl, test_path};

    const std::string unsupported_alias{"core"};
    EXPECT_CALL(*mock_platform, is_alias_supported(unsupported_alias, _)).WillOnce(Return(false));

    MP_EXPECT_THROW_THAT(
        host.all_info_for(make_query(unsupported_alias, "snapcraft")), mp::UnsupportedAliasException,
        mpt::match_what(HasSubstr(fmt::format("\'{}\' is not a supported alias.", unsupported_alias))));
}

TEST_F(CustomImageHost, supported_remotes_returns_expected_values)
{
    mp::CustomVMImageHost host{&url_downloader, default_ttl, test_path};

    auto supported_remotes = host.supported_remotes();

    const size_t expected_size{2};
    EXPECT_THAT(supported_remotes.size(), Eq(expected_size));

    EXPECT_TRUE(std::find(supported_remotes.begin(), supported_remotes.end(), "") != supported_remotes.end());
    EXPECT_TRUE(std::find(supported_remotes.begin(), supported_remotes.end(), "snapcraft") != supported_remotes.end());
}

TEST_F(CustomImageHost, invalid_image_returns_false)
{
    mp::CustomVMImageHost host{&url_downloader, default_ttl, test_path};

    EXPECT_FALSE(host.info_for(make_query("foo", "")));
}

TEST_F(CustomImageHost, invalid_remote_throws_error)
{
    mp::CustomVMImageHost host{&url_downloader, default_ttl, test_path};

    EXPECT_THROW(host.info_for(make_query("core", "foo")), std::runtime_error);
}

TEST_F(CustomImageHost, handles_and_recovers_from_initial_network_failure)
{
    const auto ttl = 1h; // so that updates are only retried when unsuccessful
    url_downloader.mischiefs = 1000;
    mp::CustomVMImageHost host{&url_downloader, ttl, test_path};

    const auto query = make_query("core", "snapcraft");
    EXPECT_THROW(host.info_for(query), std::runtime_error);

    url_downloader.mischiefs = 0;
    EXPECT_TRUE(host.info_for(query));
}

TEST_F(CustomImageHost, handles_and_recovers_from_later_network_failure)
{
    const auto ttl = 0s; // to ensure updates are always retried
    mp::CustomVMImageHost host{&url_downloader, ttl, test_path};

    const auto query = make_query("core", "snapcraft");
    EXPECT_TRUE(host.info_for(query));

    url_downloader.mischiefs = 1000;
    EXPECT_THROW(host.info_for(query), std::runtime_error);

    url_downloader.mischiefs = 0;
    EXPECT_TRUE(host.info_for(query));
}

TEST_F(CustomImageHost, handles_and_recovers_from_independent_server_failures)
{
    const auto ttl = 0h;
    mp::CustomVMImageHost host{&url_downloader, ttl, test_path};

    const auto num_remotes = mpt::count_remotes(host);
    EXPECT_GT(num_remotes, 0u);

    for (size_t i = 0; i < num_remotes; ++i)
    {
        url_downloader.mischiefs = i;
        EXPECT_EQ(mpt::count_remotes(host), num_remotes - i);
    }
}

TEST_F(CustomImageHost, info_for_unsupported_remote_throws)
{
    mp::CustomVMImageHost host{&url_downloader, default_ttl, test_path};

    const std::string unsupported_remote{"snapcraft"};
    EXPECT_CALL(*mock_platform, is_remote_supported(unsupported_remote)).WillRepeatedly(Return(false));

    MP_EXPECT_THROW_THAT(host.info_for(make_query("xenial", unsupported_remote)), mp::UnsupportedRemoteException,
                         mpt::match_what(HasSubstr(fmt::format(
                             "Remote \'{}\' is not a supported remote for this platform.", unsupported_remote))));
}
