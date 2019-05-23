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

#include "mock_settings.h"

#include <cassert>

using namespace testing;

void multipass::test::MockSettings::mockit()
{
    AddGlobalTestEnvironment(new TestEnv{}); // takes pointer ownership o_O
}

auto multipass::test::MockSettings::mock_instance() -> MockSettings&
{
    return dynamic_cast<MockSettings&>(instance());
}

void multipass::test::MockSettings::TestEnv::SetUp()
{
    Settings::mock<NiceMock<MockSettings>>();

    auto& mock = MockSettings::mock_instance();
    ON_CALL(mock, get(_)).WillByDefault(Invoke(&mock, &MockSettings::get_default));
    ON_CALL(mock, set(_, _)).WillByDefault(WithArg<0>(IgnoreResult(Invoke(&mock, &MockSettings::get_default))));

    register_accountant();
}

void multipass::test::MockSettings::TestEnv::TearDown()
{
    release_accountant();
    Settings::reset(); /* We need to make sure this runs before gtest unwinds because this is a mock and otherwise
                            a) the mock would leak,
                            b) expectations would not be checked,
                            c) it would refer to stuff that was deleted by then.
                        */
}

void multipass::test::MockSettings::TestEnv::register_accountant()
{
    accountant = new Accountant{};
    UnitTest::GetInstance()->listeners().Append(accountant); // takes ownership
}

void multipass::test::MockSettings::TestEnv::release_accountant()
{
    auto* listener = UnitTest::GetInstance()->listeners().Release(accountant); // releases ownership
    assert(listener == accountant);
    static_cast<void>(listener); // to avoid unused var warning

    delete accountant; // no prob if already null
    accountant = nullptr;
}

void multipass::test::MockSettings::Accountant::OnTestEnd(const TestInfo& /*unused*/)
{
    Mock::VerifyAndClearExpectations(&MockSettings::mock_instance());
}
