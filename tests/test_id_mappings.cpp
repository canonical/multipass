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

#include <multipass/id_mappings.h>

namespace mp = multipass;

using namespace testing;

struct UniqueIdMappingsTestSuite : public Test, public WithParamInterface<std::pair<mp::id_mappings, mp::id_mappings>>
{
};

TEST_P(UniqueIdMappingsTestSuite, UniqueIdMappingsWorks)
{
    auto [input_mappings, expected_mappings] = GetParam();

    mp::unique_id_mappings(input_mappings);
    ASSERT_EQ(input_mappings, expected_mappings);
}

INSTANTIATE_TEST_SUITE_P(IdMappings,
                         UniqueIdMappingsTestSuite,
                         Values(std::make_pair(mp::id_mappings{{1, 1}, {2, 1}, {1, 1}, {1, 2}},
                                               mp::id_mappings{{1, 1}}),
                                std::make_pair(mp::id_mappings{{3, 4}}, mp::id_mappings{{3, 4}}),
                                std::make_pair(mp::id_mappings{}, mp::id_mappings{})));
