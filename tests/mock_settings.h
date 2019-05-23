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

#include <cassert> // TODO @ricab move to cpp

namespace multipass
{
namespace test
{
class MockSettings : public Settings // This will automatically verify expectations set upon it at the end of each test
{
public:
    using Settings::get_default; // promote visibility
    using Settings::Settings;    // ctor

    static void mockit();
    static MockSettings& mock_instance();

    MOCK_CONST_METHOD1(get, QString(const QString&));
    MOCK_METHOD2(set, void(const QString&, const QString&));

private:
    class Accountant : public ::testing::EmptyTestEventListener
    {
    public:
        void OnTestEnd(const ::testing::TestInfo&) override;
    };

    class TestEnv : public ::testing::Environment // tying setup/teardown here ensures registered mock is unregistered
    {
    public:
        void SetUp() override;
        void TearDown() override;

    private:
        void register_accountant();
        void release_accountant();

        Accountant* accountant = nullptr; // non-owning ptr
    };
};
} // namespace test
} // namespace multipass

// TODO @ricab move this stuff to compilation unit

inline void multipass::test::MockSettings::mockit()
{
    ::testing::AddGlobalTestEnvironment(new TestEnv{}); // takes pointer ownership o_O
}

inline auto multipass::test::MockSettings::mock_instance() -> MockSettings&
{
    return dynamic_cast<MockSettings&>(instance());
}

inline void multipass::test::MockSettings::TestEnv::SetUp()
{
    using namespace testing;

    Settings::mock<NiceMock<MockSettings>>();

    auto& mock = MockSettings::mock_instance();
    ON_CALL(mock, get(_)).WillByDefault(Invoke(&mock, &MockSettings::get_default));
    ON_CALL(mock, set(_, _)).WillByDefault(WithArg<0>(IgnoreResult(Invoke(&mock, &MockSettings::get_default))));

    register_accountant();
}

inline void multipass::test::MockSettings::TestEnv::TearDown()
{
    release_accountant();
    Settings::reset(); /* We need to make sure this runs before gtest unwinds because this is a mock and otherwise
                            a) the mock would leak,
                            b) expectations would not be checked,
                            c) it would refer to stuff that was deleted by then.
                        */
}

inline void multipass::test::MockSettings::TestEnv::register_accountant()
{
    accountant = new Accountant{};
    ::testing::UnitTest::GetInstance()->listeners().Append(accountant); // takes ownership
}

inline void multipass::test::MockSettings::TestEnv::release_accountant()
{
    // TODO @ricab move this stuff to anonymous namespace in cpp
    auto* listener = ::testing::UnitTest::GetInstance()->listeners().Release(accountant); // releases ownership
    assert(listener == accountant);
    static_cast<void>(listener); // to avoid unused var warning

    delete accountant; // no prob if already null
    accountant = nullptr;
}

inline void multipass::test::MockSettings::Accountant::OnTestEnd(const ::testing::TestInfo& /*unused*/)
{
    ::testing::Mock::VerifyAndClearExpectations(&MockSettings::mock_instance());
}

#endif // MULTIPASS_MOCK_SETTINGS_H
