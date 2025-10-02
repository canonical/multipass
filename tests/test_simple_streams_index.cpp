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

#include <multipass/simple_streams_index.h>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

TEST(SimpleStreamsIndex, parsesManifestLocation)
{
    auto json = mpt::load_test_file("simple_streams_index/good_index.json");
    auto index = mp::SimpleStreamsIndex::fromJson(json);

    EXPECT_THAT(index.manifest_path, Eq("multiple_versions_manifest.json"));
}

TEST(SimpleStreamsIndex, parsesUpdateStamp)
{
    auto json = mpt::load_test_file("simple_streams_index/good_index.json");
    auto index = mp::SimpleStreamsIndex::fromJson(json);

    EXPECT_THAT(index.updated_at, Eq("Thu, 18 May 2017 09:18:01 +0000"));
}

TEST(SimpleStreamsIndex, throwsIfInvalidDataType)
{
    auto json = mpt::load_test_file("simple_streams_index/bad_datatype_index.json");
    EXPECT_THROW(mp::SimpleStreamsIndex::fromJson(json), std::runtime_error);
}

TEST(SimpleStreamsIndex, throwsIfMissingIndex)
{
    auto json = mpt::load_test_file("simple_streams_index/missing_index.json");
    EXPECT_THROW(mp::SimpleStreamsIndex::fromJson(json), std::runtime_error);
}

TEST(SimpleStreamsIndex, throwsIfIndexIsNotObjectType)
{
    auto json = mpt::load_test_file("simple_streams_index/bad_index.json");
    EXPECT_THROW(mp::SimpleStreamsIndex::fromJson(json), std::runtime_error);
}

TEST(SimpleStreamsIndex, throwsOnInvalidJson)
{
    QByteArray json;
    EXPECT_THROW(mp::SimpleStreamsIndex::fromJson(json), std::runtime_error);
}

TEST(SimpleStreamsIndex, throwsOnInvalidTopLevelType)
{
    auto json = mpt::load_test_file("simple_streams_manifest/invalid_top_level.json");
    EXPECT_THROW(mp::SimpleStreamsIndex::fromJson(json), std::runtime_error);
}

TEST(SimpleStreamsIndex, throws_on_no_image_with_image_downloads)
{
    auto json = mpt::load_test_file("simple_streams_index/no_image_downloads_in_datatype.json");
    EXPECT_THROW(mp::SimpleStreamsIndex::fromJson(json), std::runtime_error);
}
