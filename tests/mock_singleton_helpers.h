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

#ifndef MULTIPASS_MOCK_SINGLETON_HELPERS_H
#define MULTIPASS_MOCK_SINGLETON_HELPERS_H

#include "common.h"

#include <scope_guard.hpp>

#include <cassert>
#include <utility>

#define MP_MOCK_SINGLETON_INSTANCE(mock_class)                                                                         \
public:                                                                                                                \
    static mock_class& mock_instance()                                                                                 \
    {                                                                                                                  \
        return dynamic_cast<mock_class&>(instance());                                                                  \
    }

#define MP_MOCK_SINGLETON_HELPER_TYPES(mock_class, parent_class)                                                       \
private:                                                                                                               \
    static constexpr auto academy = [] {                                                                               \
        return sg::make_scope_guard([]() noexcept { parent_class::reset(); });                                         \
    }; /* Produces "guards" :) Use a lambda so that the type gets deduced before decltype below */                     \
                                                                                                                       \
public:                                                                                                                \
    using Guard = decltype(academy());                                                                                 \
    using GuardedMock = std::pair<mock_class*, Guard>;

#define MP_MOCK_SINGLETON_INJECT(mock_class, parent_class)                                                             \
public:                                                                                                                \
    template <template <typename /*MockClass*/> typename MockCharacter = ::testing::NaggyMock>                         \
    [[nodiscard]] static GuardedMock inject()                                                                          \
    {                                                                                                                  \
        parent_class::reset();                                                                                         \
        parent_class::mock<MockCharacter<mock_class>>();                                                               \
        return std::make_pair(&mock_instance(), academy());                                                            \
    } // one at a time, please!

#define MP_MOCK_SINGLETON_BOILERPLATE(mock_class, parent_class)                                                        \
public:                                                                                                                \
    MP_MOCK_SINGLETON_HELPER_TYPES(mock_class, parent_class)                                                           \
    MP_MOCK_SINGLETON_INSTANCE(mock_class)                                                                             \
    MP_MOCK_SINGLETON_INJECT(mock_class, parent_class)                                                                 \
    void please_dont_call_this_undefined_method_that_gobbles_semicolons() // see https://godbolt.org/z/7ac1zaGxr

namespace multipass::test
{
template <typename ConcreteMock, template <typename /*MockClass*/> typename MockCharacter = ::testing::NaggyMock>
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

template <typename ConcreteMock, template <typename /*MockClass*/> typename MockCharacter>
void multipass::test::MockSingletonHelper<ConcreteMock, MockCharacter>::mockit()
{
    ::testing::AddGlobalTestEnvironment(
        new MockSingletonHelper<ConcreteMock, MockCharacter>{}); // takes pointer ownership o_O
}

template <typename ConcreteMock, template <typename /*MockClass*/> typename MockCharacter>
void multipass::test::MockSingletonHelper<ConcreteMock, MockCharacter>::SetUp()
{
    ConcreteMock::template mock<MockCharacter<ConcreteMock>>(); // Register mock as the singleton instance

    auto& mock = ConcreteMock::mock_instance();
    mock.setup_mock_defaults(); // setup any custom actions for calls on the mock

    register_accountant(); // register a test observer to verify and clear mock expectations
}

template <typename ConcreteMock, template <typename /*MockClass*/> typename MockCharacter>
void multipass::test::MockSingletonHelper<ConcreteMock, MockCharacter>::TearDown()
{
    release_accountant();  // release this mock's test observer
    ConcreteMock::reset(); /* Make sure this runs before gtest unwinds, so that:
                              - the mock doesn't leak
                              - expectations are checked
                              - it doesn't refer to stuff that was already deleted */
}

template <typename ConcreteMock, template <typename /*MockClass*/> typename MockCharacter>
void multipass::test::MockSingletonHelper<ConcreteMock, MockCharacter>::register_accountant()
{
    accountant = new Accountant{};
    ::testing::UnitTest::GetInstance()->listeners().Append(accountant); // takes ownership
}

template <typename ConcreteMock, template <typename /*MockClass*/> typename MockCharacter>
void multipass::test::MockSingletonHelper<ConcreteMock, MockCharacter>::release_accountant()
{
    [[maybe_unused]] auto* listener =
        ::testing::UnitTest::GetInstance()->listeners().Release(accountant); // releases ownership
    assert(listener == accountant);

    delete accountant; // no prob if already null
    accountant = nullptr;
}

template <typename ConcreteMock, template <typename /*MockClass*/> typename MockCharacter>
void multipass::test::MockSingletonHelper<ConcreteMock, MockCharacter>::Accountant::OnTestEnd(
    const ::testing::TestInfo& /*unused*/)
{
    ::testing::Mock::VerifyAndClearExpectations(&ConcreteMock::mock_instance());
}

#endif // MULTIPASS_MOCK_SINGLETON_HELPERS_H
