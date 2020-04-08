/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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

#include "file_operations.h"
#include <multipass/simple_streams_manifest.h>

#include <gmock/gmock.h>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

TEST(SimpleStreamsManifest, can_parse_image_info)
{
    auto json = mpt::load_test_file("good_manifest.json");
    auto manifest = mp::SimpleStreamsManifest::fromJson(json, "");

    EXPECT_THAT(manifest->updated_at, Eq("Thu, 18 May 2017 09:18:01 +0000"));
    EXPECT_THAT(manifest->products.size(), Eq(1u));

    const auto info = manifest->image_records["default"];
    ASSERT_THAT(info, NotNull());
    EXPECT_FALSE(info->image_location.isEmpty());
}

TEST(SimpleStreamsManifest, can_find_info_by_alias)
{
    auto json = mpt::load_test_file("good_manifest.json");
    auto manifest = mp::SimpleStreamsManifest::fromJson(json, "");

    const QString expected_id{"1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac"};
    const QString expected_location{
        "server/releases/xenial/release-20170516/ubuntu-16.04-server-cloudimg-amd64-disk1.img"};

    const auto info = manifest->image_records[expected_id];
    ASSERT_THAT(info, NotNull());
    EXPECT_THAT(info->image_location, Eq(expected_location));
    EXPECT_THAT(info->id, Eq(expected_id));
}

TEST(SimpleStreamsManifest, throws_on_invalid_json)
{
    QByteArray json;
    EXPECT_THROW(mp::SimpleStreamsManifest::fromJson(json, ""), std::runtime_error);
}

TEST(SimpleStreamsManifest, throws_on_invalid_top_level_type)
{
    auto json = mpt::load_test_file("invalid_top_level.json");
    EXPECT_THROW(mp::SimpleStreamsManifest::fromJson(json, ""), std::runtime_error);
}

TEST(SimpleStreamsManifest, throws_when_missing_products)
{
    auto json = mpt::load_test_file("missing_products_manifest.json");
    EXPECT_THROW(mp::SimpleStreamsManifest::fromJson(json, ""), std::runtime_error);
}

TEST(SimpleStreamsManifest, throws_when_failed_to_parse_any_products)
{
    auto json = mpt::load_test_file("missing_versions_manifest.json");
    EXPECT_THROW(mp::SimpleStreamsManifest::fromJson(json, ""), std::runtime_error);

    json = mpt::load_test_file("missing_versions_manifest.json");
    EXPECT_THROW(mp::SimpleStreamsManifest::fromJson(json, ""), std::runtime_error);
}

TEST(SimpleStreamsManifest, chooses_newest_version)
{
    auto json = mpt::load_test_file("releases/multiple_versions_manifest.json");
    auto manifest = mp::SimpleStreamsManifest::fromJson(json, "");

    const QString expected_id{"8842e7a8adb01c7a30cc702b01a5330a1951b12042816e87efd24b61c5e2239f"};
    const QString expected_location{"newest_image.img"};

    const auto info = manifest->image_records["default"];
    ASSERT_THAT(info, NotNull());
    EXPECT_THAT(info->image_location, Eq(expected_location));
    EXPECT_THAT(info->id, Eq(expected_id));
}

TEST(SimpleStreamsManifest, can_query_all_versions)
{
    auto json = mpt::load_test_file("releases/multiple_versions_manifest.json");
    auto manifest = mp::SimpleStreamsManifest::fromJson(json, "");

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

TEST(SimpleStreamsManifest, info_has_kernel_and_initrd_paths)
{
    auto json = mpt::load_test_file("good_manifest.json");
    auto manifest = mp::SimpleStreamsManifest::fromJson(json, "");

    const auto info = manifest->image_records["default"];
    ASSERT_THAT(info, NotNull());

    EXPECT_FALSE(info->kernel_location.isEmpty());
    EXPECT_FALSE(info->initrd_location.isEmpty());
}
