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
        EXPECT_CALL(mock_settings, get(Eq(mp::driver_key))).WillRepeatedly(Return("emu"));
    }

    mpt::MockSettings::GuardedMock mock_settings_injection =
        mpt::MockSettings::inject<StrictMock>();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;
};

TEST_F(TestSimpleStreamsManifest, canParseImageInfo)
{
    auto json = mpt::load_test_file("good_manifest.json");
    auto manifest = mp::SimpleStreamsManifest::fromJson(json, std::nullopt, "");

    EXPECT_THAT(manifest->updated_at, Eq("Wed, 20 May 2020 16:47:50 +0000"));
    EXPECT_THAT(manifest->products.size(), Eq(2u));

    const auto info = manifest->image_records.at("default");
    ASSERT_THAT(info, NotNull());
    EXPECT_FALSE(info->image_location.isEmpty());
}

TEST_F(TestSimpleStreamsManifest, canFindInfoByAlias)
{
    auto json = mpt::load_test_file("good_manifest.json");
    const auto host_url{"http://stream/url"};
    auto manifest = mp::SimpleStreamsManifest::fromJson(json, std::nullopt, host_url);

    const QString expected_id{"1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac"};
    const QString expected_location =
        QString("http://stream/urlserver/releases/xenial/release-20170516/"
                "ubuntu-16.04-server-cloudimg-%1-disk1.img")
            .arg(MANIFEST_ARCH);

    const auto info = manifest->image_records.at(expected_id);
    ASSERT_THAT(info, NotNull());
    EXPECT_THAT(info->image_location, Eq(expected_location));
    EXPECT_THAT(info->id, Eq(expected_id));
    EXPECT_THAT(info->stream_location, Eq(host_url));
}

TEST_F(TestSimpleStreamsManifest, throwsOnInvalidJson)
{
    QByteArray json;
    EXPECT_THROW(mp::SimpleStreamsManifest::fromJson(json, std::nullopt, ""),
                 mp::GenericManifestException);
}

TEST_F(TestSimpleStreamsManifest, throwsOnInvalidTopLevelType)
{
    auto json = mpt::load_test_file("invalid_top_level.json");
    EXPECT_THROW(mp::SimpleStreamsManifest::fromJson(json, std::nullopt, ""),
                 mp::GenericManifestException);
}

TEST_F(TestSimpleStreamsManifest, throwsWhenMissingProducts)
{
    auto json = mpt::load_test_file("missing_products_manifest.json");
    EXPECT_THROW(mp::SimpleStreamsManifest::fromJson(json, std::nullopt, ""),
                 mp::GenericManifestException);
}

TEST_F(TestSimpleStreamsManifest, throwsWhenFailedToParseAnyProducts)
{
    auto json = mpt::load_test_file("missing_versions_manifest.json");
    EXPECT_THROW(mp::SimpleStreamsManifest::fromJson(json, std::nullopt, ""),
                 mp::EmptyManifestException);

    json = mpt::load_test_file("missing_versions_manifest.json");
    EXPECT_THROW(mp::SimpleStreamsManifest::fromJson(json, std::nullopt, ""),
                 mp::EmptyManifestException);
}

TEST_F(TestSimpleStreamsManifest, choosesNewestVersion)
{
    auto json = mpt::load_test_file("releases/multiple_versions_manifest.json");
    auto manifest = mp::SimpleStreamsManifest::fromJson(json, std::nullopt, "");

    const QString expected_id{"8842e7a8adb01c7a30cc702b01a5330a1951b12042816e87efd24b61c5e2239f"};
    const QString expected_location{"newest_image.img"};

    const auto info = manifest->image_records.at("default");
    ASSERT_THAT(info, NotNull());
    EXPECT_THAT(info->image_location, Eq(expected_location));
    EXPECT_THAT(info->id, Eq(expected_id));
}

TEST_F(TestSimpleStreamsManifest, canQueryAllVersions)
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
        const auto info = manifest->image_records.at(hash);
        EXPECT_THAT(info, NotNull());
    }
}

} // namespace
