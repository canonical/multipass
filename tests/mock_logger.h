/*
 * Copyright (C) 2020 Canonical, Ltd.
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

#ifndef MULTIPASS_MOCK_LOGGER_H
#define MULTIPASS_MOCK_LOGGER_H

#include <multipass/logging/log.h>
#include <multipass/logging/logger.h>

#include <scope_guard.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace multipass
{
namespace test
{
class MockLogger : public multipass::logging::Logger
{
public:
    MockLogger() = default;
    MOCK_CONST_METHOD3(log, void(multipass::logging::Level level, multipass::logging::CString category,
                                 multipass::logging::CString message));

    template <typename Matcher>
    static auto make_cstring_matcher(const Matcher& matcher)
    {
        return Property(&multipass::logging::CString::c_str, matcher);
    }
};

inline auto guarded_mock_logger()
{
    auto guard = sg::make_scope_guard([] { multipass::logging::set_logger(nullptr); });
    auto mock_logger = std::make_shared<testing::StrictMock<MockLogger>>();
    multipass::logging::set_logger(mock_logger);

    return std::make_pair(mock_logger, std::move(guard)); // needs to be moved into the pair first (NRVO does not apply)
}

inline auto expect_log(multipass::logging::Level lvl, const std::string& substr)
{
    auto [mock_logger, guard] = guarded_mock_logger();

    EXPECT_CALL(*mock_logger, log(lvl, testing::_, MockLogger::make_cstring_matcher(testing::HasSubstr(substr))));

    return std::move(guard); // needs to be moved because it's only part of an implicit local var (NRVO does not apply)
}

} // namespace test
} // namespace multipass

#endif // MULTIPASS_MOCK_LOGGER_H
