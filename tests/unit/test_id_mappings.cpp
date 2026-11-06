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

struct UniqueIdMappingsTestSuite
    : public Test,
      public WithParamInterface<std::pair<mp::id_mappings, mp::id_mappings>>
{
};

TEST_P(UniqueIdMappingsTestSuite, uniqueIdMappingsWorks)
{
    auto [input_mappings, expected_mappings] = GetParam();

    mp::unique_id_mappings(input_mappings);
    ASSERT_EQ(input_mappings, expected_mappings);
}

TEST(UniqueIdMappings, retainsConflictsFromDroppedMappings)
{
    mp::id_mappings mappings{{1, 10}, {1, 11}, {2, 11}, {3, 30}, {4, 40}, {3, 31}};

    const auto [duplicate_ids, duplicate_reverse_ids] = mp::unique_id_mappings(mappings);

    EXPECT_THAT(mappings, ElementsAre(Pair(1, 10), Pair(3, 30), Pair(4, 40)));
    EXPECT_THAT(duplicate_ids,
                UnorderedElementsAre(Pair(1, UnorderedElementsAre(10, 11)),
                                     Pair(3, UnorderedElementsAre(30, 31))));
    EXPECT_THAT(duplicate_reverse_ids, UnorderedElementsAre(Pair(11, UnorderedElementsAre(1, 2))));
}

INSTANTIATE_TEST_SUITE_P(IdMappings,
                         UniqueIdMappingsTestSuite,
                         Values(std::make_pair(mp::id_mappings{{1, 1}, {2, 1}, {1, 1}, {1, 2}},
                                               mp::id_mappings{{1, 1}}),
                                std::make_pair(mp::id_mappings{{3, 4}}, mp::id_mappings{{3, 4}}),
                                std::make_pair(mp::id_mappings{}, mp::id_mappings{})));
