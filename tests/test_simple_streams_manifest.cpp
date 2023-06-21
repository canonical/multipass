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
#include "mock_settings.h"

#include <multipass/constants.h>
#include <multipass/exceptions/manifest_exceptions.h>
#include <multipass/simple_streams_manifest.h>

#include <optional>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{

struct TestSimpleStreamsManifest : public Test
{
    void SetUp() override
    {
        EXPECT_CALL(mock_settings, get(Eq(mp::driver_key))).WillRepeatedly(Return("emu")); /* TODO parameterize driver
                                                                                              (code branches for lxd) */
    }

    mpt::MockSettings::GuardedMock mock_settings_injection = mpt::MockSettings::inject<StrictMock>();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;
};

TEST_F(TestSimpleStreamsManifest, can_parse_image_info)
{
    auto json = mpt::load_test_file("good_manifest.json");
    auto manifest = mp::SimpleStreamsManifest::fromJson(json, std::nullopt, "");

    EXPECT_THAT(manifest->updated_at, Eq("Wed, 20 May 2020 16:47:50 +0000"));
    EXPECT_THAT(manifest->products.size(), Eq(2u));

    const auto info = manifest->image_records["default"];
    ASSERT_THAT(info, NotNull());
    EXPECT_FALSE(info->image_location.isEmpty());
}

TEST_F(TestSimpleStreamsManifest, can_find_info_by_alias)
{
    auto json = mpt::load_test_file("good_manifest.json");
    const auto host_url{"http://stream/url"};
    auto manifest = mp::SimpleStreamsManifest::fromJson(json, std::nullopt, host_url);

    const QString expected_id{"1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac"};
    const QString expected_location =
        QString("server/releases/xenial/release-20170516/ubuntu-16.04-server-cloudimg-%1-disk1.img").arg(MANIFEST_ARCH);

    const auto info = manifest->image_records[expected_id];
    ASSERT_THAT(info, NotNull());
    EXPECT_THAT(info->image_location, Eq(expected_location));
    EXPECT_THAT(info->id, Eq(expected_id));
    EXPECT_THAT(info->stream_location, Eq(host_url));
}

TEST_F(TestSimpleStreamsManifest, throws_on_invalid_json)
{
    QByteArray json;
    EXPECT_THROW(mp::SimpleStreamsManifest::fromJson(json, std::nullopt, ""), mp::GenericManifestException);
}

TEST_F(TestSimpleStreamsManifest, throws_on_invalid_top_level_type)
{
    auto json = mpt::load_test_file("invalid_top_level.json");
    EXPECT_THROW(mp::SimpleStreamsManifest::fromJson(json, std::nullopt, ""), mp::GenericManifestException);
}

TEST_F(TestSimpleStreamsManifest, throws_when_missing_products)
{
    auto json = mpt::load_test_file("missing_products_manifest.json");
    EXPECT_THROW(mp::SimpleStreamsManifest::fromJson(json, std::nullopt, ""), mp::GenericManifestException);
}

TEST_F(TestSimpleStreamsManifest, throws_when_failed_to_parse_any_products)
{
    auto json = mpt::load_test_file("missing_versions_manifest.json");
    EXPECT_THROW(mp::SimpleStreamsManifest::fromJson(json, std::nullopt, ""), mp::EmptyManifestException);

    json = mpt::load_test_file("missing_versions_manifest.json");
    EXPECT_THROW(mp::SimpleStreamsManifest::fromJson(json, std::nullopt, ""), mp::EmptyManifestException);
}

TEST_F(TestSimpleStreamsManifest, chooses_newest_version)
{
    auto json = mpt::load_test_file("releases/multiple_versions_manifest.json");
    auto manifest = mp::SimpleStreamsManifest::fromJson(json, std::nullopt, "");

    const QString expected_id{"8842e7a8adb01c7a30cc702b01a5330a1951b12042816e87efd24b61c5e2239f"};
    const QString expected_location{"newest_image.img"};

    const auto info = manifest->image_records["default"];
    ASSERT_THAT(info, NotNull());
    EXPECT_THAT(info->image_location, Eq(expected_location));
    EXPECT_THAT(info->id, Eq(expected_id));
}

TEST_F(TestSimpleStreamsManifest, can_query_all_versions)
{
    auto json = mpt::load_test_file("releases/multiple_versions_manifest.json");
    auto manifest = mp::SimpleStreamsManifest::fromJson(json, std::nullopt, "");

    QStringList all_known_hashes;
    all_known_hashes << "1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac"
                     << "8842e7a8adb01c7a30cc702b01a5330a1951b12042816e87efd24b61c5e2239f"
                     << "1507bd2b3288ef4bacd3e699fe71b827b7ccf321ec4487e168a30d7089d3c8e4"
                     << "ab115b83e7a8bebf3d3a02bf55ad0cb75a0ed515fcbc65fb0c9abe76c752921c";

    for (const auto& hash : all_known_hashes)
    {
        const auto info = manifest->image_records[hash];
        EXPECT_THAT(info, NotNull());
    }
}

TEST_F(TestSimpleStreamsManifest, LXDDriverReturnsExpectedData)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::driver_key))).WillRepeatedly(Return("lxd"));

    auto json = mpt::load_test_file("lxd_test_manifest.json");
    auto manifest = mp::SimpleStreamsManifest::fromJson(json, std::nullopt, "");

    EXPECT_EQ(manifest->products.size(), 2u);

    const auto xenial_info = manifest->image_records["xenial"];

    // combined_disk1-img_sha256 for xenial product
    const QString expected_xenial_id{"09d24fab15c6e1c86a47d3de2e7fb6d01a10f9ff2655a43f0959a672e03e7674"};
    EXPECT_EQ(xenial_info->id, expected_xenial_id);

    // combined_disk-img_sha256 despite -kvm being available (canonical/multipass#2491)
    const auto bionic_info = manifest->image_records["bionic"];

    const QString expected_bionic_id{"09d24fab15c6e1c86a47d3de2e83d0d01a10f9ff2655a43f0959a672e03e7674"};
    EXPECT_EQ(bionic_info->id, expected_bionic_id);
}

} // namespace
