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

TEST(SimpleStreamsIndex, parses_manifest_location)
{
    auto json = mpt::load_test_file("good_index.json");
    auto index = mp::SimpleStreamsIndex::fromJson(json);

    EXPECT_THAT(index.manifest_path, Eq("multiple_versions_manifest.json"));
}

TEST(SimpleStreamsIndex, parses_update_stamp)
{
    auto json = mpt::load_test_file("good_index.json");
    auto index = mp::SimpleStreamsIndex::fromJson(json);

    EXPECT_THAT(index.updated_at, Eq("Thu, 18 May 2017 09:18:01 +0000"));
}

TEST(SimpleStreamsIndex, throws_if_invalid_data_type)
{
    auto json = mpt::load_test_file("bad_datatype_index.json");
    EXPECT_THROW(mp::SimpleStreamsIndex::fromJson(json), std::runtime_error);
}

TEST(SimpleStreamsIndex, throws_if_missing_index)
{
    auto json = mpt::load_test_file("missing_index.json");
    EXPECT_THROW(mp::SimpleStreamsIndex::fromJson(json), std::runtime_error);
}

TEST(SimpleStreamsIndex, throws_if_index_is_not_object_type)
{
    auto json = mpt::load_test_file("bad_index.json");
    EXPECT_THROW(mp::SimpleStreamsIndex::fromJson(json), std::runtime_error);
}

TEST(SimpleStreamsIndex, throws_on_invalid_json)
{
    QByteArray json;
    EXPECT_THROW(mp::SimpleStreamsIndex::fromJson(json), std::runtime_error);
}

TEST(SimpleStreamsIndex, throws_on_invalid_top_level_type)
{
    auto json = mpt::load_test_file("invalid_top_level.json");
    EXPECT_THROW(mp::SimpleStreamsIndex::fromJson(json), std::runtime_error);
}
