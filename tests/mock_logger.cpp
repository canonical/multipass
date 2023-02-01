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

#include "mock_logger.h"

#include <type_traits>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;

static_assert(!std::is_copy_assignable_v<mpt::MockLogger>);
static_assert(!std::is_copy_constructible_v<mpt::MockLogger>);

mpt::MockLogger::MockLogger(const PrivatePass&, const mpl::Level logging_level) : Logger{logging_level}
{
}

auto mpt::MockLogger::inject(const mpl::Level logging_level) -> Scope
{
    return Scope{logging_level};
}

mpt::MockLogger::Scope::Scope(const mpl::Level logging_level)
    : mock_logger{std::make_shared<testing::NiceMock<MockLogger>>(pass, logging_level)}
{
    mpl::set_logger(mock_logger);
}

mpt::MockLogger::Scope::~Scope()
{
    if (mpl::get_logger() == mock_logger.get() && mock_logger.use_count() == 2)
        mpl::set_logger(nullptr); // only reset if we are the last scope with the registered logger
}

void mpt::MockLogger::expect_log(mpl::Level lvl, const std::string& substr, const Cardinality& times)
{
    EXPECT_CALL(*this, log(lvl, _, make_cstring_matcher(HasSubstr(substr)))).Times(times);
}

void mpt::MockLogger::screen_logs(mpl::Level lvl)
{
    for (auto i = 0; i <= mpl::enum_type(mpl::Level::trace); ++i)
    {
        auto times = i <= mpl::enum_type(lvl) ? Exactly(0) : AnyNumber();
        EXPECT_CALL(*this, log(mpl::level_from(i), _, _)).Times(times);
    }
}
