/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#ifndef MULTIPASS_MOCK_SETTINGS_H
#define MULTIPASS_MOCK_SETTINGS_H

#include <multipass/settings.h>

#include <gmock/gmock.h>

namespace multipass
{
namespace test
{
class MockSettings : public Settings
{
public:
    using Settings::get_default; // promote visibility
    using Settings::Settings;    // ctor

    static testing::Environment* mocking_environment(); // transfers ownership (as gtest expects)!
    static MockSettings& mock_instance();

    MOCK_CONST_METHOD1(get, QString(const QString&));
    MOCK_METHOD2(set, void(const QString&, const QString&));

private:
    class TestEnv : public ::testing::Environment // tying setup/teardown here ensures registered mock is unregistered
    {
    public:
        void SetUp() override;
        void TearDown() override;
    };
};
} // namespace test
} // namespace multipass

inline ::testing::Environment* multipass::test::MockSettings::mocking_environment()
{
    return new TestEnv;
}

inline auto multipass::test::MockSettings::mock_instance() -> MockSettings&
{
    return dynamic_cast<MockSettings&>(instance());
}

inline void multipass::test::MockSettings::TestEnv::SetUp()
{
    using namespace testing;
    Settings::mock<testing::NiceMock<MockSettings>>();
    auto& mock = MockSettings::mock_instance();
    ON_CALL(mock, get(_)).WillByDefault(Invoke(&mock, &MockSettings::get_default));
    ON_CALL(mock, set(_, _)).WillByDefault(WithArg<0>(IgnoreResult(Invoke(&mock, &MockSettings::get_default))));
}

inline void multipass::test::MockSettings::TestEnv::TearDown()
{
    Settings::reset(); /* We need to make sure this runs before gtest unwinds because this is a mock and otherwise
                            a) the mock would leak,
                            b) expectations would not be checked,
                            c) it would refer to stuff that was deleted by then.
                        */
}

#endif // MULTIPASS_MOCK_SETTINGS_H
