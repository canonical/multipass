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
#include <multipass/private_pass_provider.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// TODO@ricab move implementations to cpp

namespace multipass
{
namespace test
{
class MockLogger : public multipass::logging::Logger, public PrivatePassProvider<MockLogger>
{
public:
    MockLogger(const PrivatePass&)
    {
    }

    MockLogger() = delete;
    MockLogger(const MockLogger&) = delete;
    MockLogger& operator=(const MockLogger&) = delete;

    MOCK_CONST_METHOD3(log, void(multipass::logging::Level level, multipass::logging::CString category,
                                 multipass::logging::CString message));

    class Scope
    {
    public:
        ~Scope()
        {
            multipass::logging::set_logger(nullptr);
        }

        std::shared_ptr<testing::NiceMock<MockLogger>> mock_logger =
            std::make_shared<testing::NiceMock<MockLogger>>(pass);

    private:
        Scope()
        {
            multipass::logging::set_logger(mock_logger);
        }

        friend class MockLogger;
    };

    // only one at a time, please
    [[nodiscard]] static Scope inject()
    {
        return Scope{};
    }

    // TODO@ricab check what can be made private
    template <typename Matcher>
    static auto make_cstring_matcher(const Matcher& matcher)
    {
        return Property(&multipass::logging::CString::c_str, matcher);
    }

    void expect_log(multipass::logging::Level lvl, const std::string& substr)
    {
        EXPECT_CALL(*this, log(lvl, testing::_, make_cstring_matcher(testing::HasSubstr(substr))));
    }

    // Reject logs with severity `lvl` or higher (lower integer), accept the rest
    // By default, all logs are rejected. Pass error level to accept everything but errors (expect those explicitly)
    void screen_logs(multipass::logging::Level lvl = multipass::logging::Level::trace)
    {
        namespace mpl = multipass::logging;
        for (auto i = 0; i <= mpl::enum_type(mpl::Level::trace); ++i)
        {
            auto times = i < mpl::enum_type(lvl) ? testing::Exactly(0) : testing::AnyNumber();
            EXPECT_CALL(*this, log(mpl::level_from(i), testing::_, testing::_)).Times(times);
        }
    }
};
} // namespace test
} // namespace multipass

#endif // MULTIPASS_MOCK_LOGGER_H
