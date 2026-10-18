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
#include "mock_logger.h"

#include <multipass/logging/level.h>
#include <multipass/name_generator.h>

#include <rslogger/src/lib.rs.h>

#include <boost/algorithm/string/split.hpp>

#include <string>
#include <unordered_set>
#include <vector>

namespace mp = multipass;
namespace mpt = mp::test;
namespace mpl = mp::logging;
namespace rxt = rxx::test;

using namespace testing;

namespace
{
std::vector<std::string> split(const std::string& string, char delimiter)
{
    std::vector<std::string> split_result;
    boost::split(
        split_result,
        string,
        [delimiter](char a) { return a == delimiter; },
        boost::token_compress_on);
    return split_result;
}
} // namespace

// Tests are intended to test the Rust module integration, not the module itself

TEST(PetnameIntegration, usesDefaultSeparator)
{
    char expected_separator{'-'};
    mp::NameGenerator::UPtr name_generator{multipass::petname::make_petname_provider()};
    auto name = name_generator->make_name();
    auto tokens = split(name, expected_separator);
    EXPECT_THAT(tokens.size(), Eq(2u));
}

TEST(PetnameIntegration, generatesTwoTokensByDefault)
{
    constexpr char separator{'-'};
    mp::NameGenerator::UPtr name_generator{multipass::petname::make_petname_provider<separator>()};
    auto name = name_generator->make_name();
    auto tokens = split(name, separator);
    EXPECT_THAT(tokens.size(), Eq(2u));

    // Each token should be unique
    std::unordered_set<std::string> set(tokens.begin(), tokens.end());
    EXPECT_THAT(set.size(), Eq(tokens.size()));
}

TEST(RustLoggerIntegration, LogsGetThrough)
{
    auto [mock_logger] = mpt::MockLogger::inject();

    mock_logger->screen_logs(mpl::Level::warning);
    mock_logger->expect_log(mpl::Level::warning, "test_log");
    rxx::test::logging::test_log(mpl::Level::warning, "category", "test_log");
}

TEST(RustLoggerIntegration, ExceptionPropagates)
{
    auto [mock_logger] = mpt::MockLogger::inject();

    EXPECT_CALL(*mock_logger, log).WillOnce(Throw(std::runtime_error{"Exception"}));

    EXPECT_THROW(rxt::logging::test_log(mpl::Level::warning, "category", "test_log"), rust::Error);
}
