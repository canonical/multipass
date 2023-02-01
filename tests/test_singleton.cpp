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

#include <multipass/singleton.h>

#include <string>

namespace mp = multipass;
using namespace testing;

namespace
{
class TestSingleton : public mp::Singleton<TestSingleton>
{
public:
    using Singleton<TestSingleton>::Singleton; // ctor

    virtual std::string foo() const
    {
        return "Hi from singleton";
    }
};

class MockTestSingleton : public TestSingleton
{
public:
    using TestSingleton::TestSingleton; // ctor

    static void mock()
    {
        TestSingleton::mock<MockTestSingleton>();
    }

    static void reset() // not thread-safe, make sure no other threads using this singleton
    {
        TestSingleton::reset();
    }

    std::string foo() const override
    {
        return "Hi from mock";
    }
};

TEST(Singleton, singleton_can_be_mocked_and_reset)
{
    const auto mock_matcher = HasSubstr("mock");
    ASSERT_THAT(TestSingleton::instance().foo(), Not(mock_matcher));

    MockTestSingleton::reset();
    MockTestSingleton::mock();
    ASSERT_THAT(TestSingleton::instance().foo(), mock_matcher);

    MockTestSingleton::reset();
    ASSERT_THAT(TestSingleton::instance().foo(), Not(mock_matcher));
}

// safety demo
class TryMultipleton : public mp::Singleton<TryMultipleton>
{
public:
    // TryMultipleton() = default; // compilation fails when called

    // TryMultipleton()
    //     : Singleton<TryMultipleton>{Singleton<TryMultipleton>::pass} // error: private
    // {}

    struct lockpick
    {
    } pick{};
    // TryMultipleton()
    //     : Singleton<TryMultipleton>{reinterpret_cast<Singleton<TryMultipleton>::PrivatePass>(pick)} // bad cast
    //     : Singleton<TryMultipleton>{(Singleton<TryMultipleton>::PrivatePass)(pick)} // no way to convert
    // {}
};

} // namespace
