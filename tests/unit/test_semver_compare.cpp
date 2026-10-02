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

#include <multipass/utils/semver_compare.h>

#include <stdexcept>
#include <utility>

namespace mp = multipass;
using namespace mp::literals;

struct SemverPrecedence : testing::TestWithParam<std::pair<const char*, const char*>>
{
};

TEST_P(SemverPrecedence, ordersVersions)
{
    const mp::opaque_semver lhs{GetParam().first};
    const mp::opaque_semver rhs{GetParam().second};

    EXPECT_EQ(lhs <=> rhs, std::weak_ordering::less);
    EXPECT_EQ(rhs <=> lhs, std::weak_ordering::greater);
    EXPECT_LT(lhs, rhs);
    EXPECT_GT(rhs, lhs);
}

INSTANTIATE_TEST_SUITE_P(SemverCompare,
                         SemverPrecedence,
                         testing::Values(std::pair{"1.0.0", "2.0.0"},
                                         std::pair{"1.0.0", "1.1.0"},
                                         std::pair{"1.0.0", "1.0.1"},
                                         std::pair{"1.0.0-alpha", "1.0.0-alpha.1"},
                                         std::pair{"1.0.0-alpha.1", "1.0.0-alpha.beta"},
                                         std::pair{"1.0.0-alpha.beta", "1.0.0-beta"},
                                         std::pair{"1.0.0-beta", "1.0.0-beta.2"},
                                         std::pair{"1.0.0-beta.2", "1.0.0-beta.11"},
                                         std::pair{"1.0.0-beta.11", "1.0.0-rc.1"},
                                         std::pair{"1.0.0-rc.1", "1.0.0"}));

TEST(SemverCompare, identicalVersionsAreEquivalent)
{
    EXPECT_EQ("1.2.3"_semver <=> "1.2.3"_semver, std::weak_ordering::equivalent);
}

TEST(SemverCompare, buildMetadataDoesNotAffectPrecedence)
{
    const auto lhs = "1.2.3+build.1"_semver;
    const auto rhs = "1.2.3+build.2"_semver;

    EXPECT_EQ(lhs <=> rhs, std::weak_ordering::equivalent);
    EXPECT_EQ(lhs <=> "1.2.3"_semver, std::weak_ordering::equivalent);
    EXPECT_LE(lhs, rhs);
    EXPECT_GE(lhs, rhs);
}

struct InvalidSemver : testing::TestWithParam<const char*>
{
};

TEST_P(InvalidSemver, throwsForEitherOperand)
{
    const mp::opaque_semver invalid{GetParam()};

    EXPECT_THROW((void)(invalid <=> "1.2.3"_semver), std::invalid_argument);
    EXPECT_THROW((void)("1.2.3"_semver <=> invalid), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(SemverCompare,
                         InvalidSemver,
                         testing::Values("", "1.2", "01.2.3", "1.2.3-", "1.2.3+"));
