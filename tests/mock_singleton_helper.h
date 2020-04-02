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

#ifndef MULTIPASS_MOCK_SINGLETON_HELPER_H
#define MULTIPASS_MOCK_SINGLETON_HELPER_H

#include <gmock/gmock.h>

#include <cassert>

namespace multipass::test
{

template <typename ConcreteSingleton, typename ConcreteMock>
class MockSingletonHelper : public ::testing::Environment
{
public:
    static void mockit();

    void SetUp() override;
    void TearDown() override;

private:
    class Accountant : public ::testing::EmptyTestEventListener
    {
    public:
        void OnTestEnd(const ::testing::TestInfo&) override;
    };

    void register_accountant();
    void release_accountant();

    Accountant* accountant = nullptr; // non-owning ptr
};
} // namespace multipass::test

template <typename ConcreteSingleton, typename ConcreteMock>
void multipass::test::MockSingletonHelper<ConcreteSingleton, ConcreteMock>::mockit()
{
    testing::AddGlobalTestEnvironment(
        new MockSingletonHelper<ConcreteSingleton, ConcreteMock>{}); // takes pointer ownership o_O
}

template <typename ConcreteSingleton, typename ConcreteMock>
void multipass::test::MockSingletonHelper<ConcreteSingleton, ConcreteMock>::SetUp()
{
    ConcreteSingleton::template mock<testing::NiceMock<ConcreteMock>>(); // Register mock as the singleton instance

    auto& mock = ConcreteMock::mock_instance();
    mock.setup_mock_defaults(); // setup any custom actions for calls on the mock

    register_accountant(); // register a test observer to verify and clear mock expectations
}

template <typename ConcreteSingleton, typename ConcreteMock>
void multipass::test::MockSingletonHelper<ConcreteSingleton, ConcreteMock>::TearDown()
{
    release_accountant();       // release this mock's test observer
    ConcreteSingleton::reset(); /*
    We need to make sure this runs before gtest unwinds because this is a mock and otherwise
      a) the mock would leak,
      b) expectations would not be checked,
      c) it would refer to stuff that was deleted by then.
    */
}

template <typename ConcreteSingleton, typename ConcreteMock>
void multipass::test::MockSingletonHelper<ConcreteSingleton, ConcreteMock>::register_accountant()
{
    accountant = new Accountant{};
    testing::UnitTest::GetInstance()->listeners().Append(accountant); // takes ownership
}

template <typename ConcreteSingleton, typename ConcreteMock>
void multipass::test::MockSingletonHelper<ConcreteSingleton, ConcreteMock>::release_accountant()
{
    [[maybe_unused]] auto* listener =
        testing::UnitTest::GetInstance()->listeners().Release(accountant); // releases ownership
    assert(listener == accountant);

    delete accountant; // no prob if already null
    accountant = nullptr;
}

template <typename ConcreteSingleton, typename ConcreteMock>
void multipass::test::MockSingletonHelper<ConcreteSingleton, ConcreteMock>::Accountant::OnTestEnd(
    const testing::TestInfo& /*unused*/)
{
    testing::Mock::VerifyAndClearExpectations(&ConcreteMock::mock_instance());
}

#endif // MULTIPASS_MOCK_SINGLETON_HELPER_H
