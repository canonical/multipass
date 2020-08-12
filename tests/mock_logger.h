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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// TODO@ricab move implementations to cpp

namespace multipass
{
namespace test
{
class MockLogger : public multipass::logging::Logger
{
public:
    MockLogger() = default; // TODO@ricab disable except through inject
    MOCK_CONST_METHOD3(log, void(multipass::logging::Level level, multipass::logging::CString category,
                                 multipass::logging::CString message));

    class Scope
    {
    public:
        ~Scope()
        {
            multipass::logging::set_logger(nullptr);
        }

        std::shared_ptr<testing::NiceMock<MockLogger>> mock_logger = std::make_shared<testing::NiceMock<MockLogger>>();

    private:
        Scope()
        {
            multipass::logging::set_logger(mock_logger);
        }

        friend class MockLogger;
    };

    [[nodiscard]] static Scope inject()
    {
        return Scope{};
    }

    template <typename Matcher>
    static auto make_cstring_matcher(const Matcher& matcher)
    {
        return Property(&multipass::logging::CString::c_str, matcher);
    }

    void expect_log(multipass::logging::Level lvl, const std::string& substr)
    {
        EXPECT_CALL(*this, log(lvl, testing::_, make_cstring_matcher(testing::HasSubstr(substr))));
    }
};
} // namespace test
} // namespace multipass

#endif // MULTIPASS_MOCK_LOGGER_H
