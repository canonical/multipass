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
#include "mock_settings.h"

#include <multipass/settings/settings_handler.h>

#include <QString>

#include <algorithm>
#include <array>
#include <memory>
#include <variant>

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

namespace
{
class TestSettings : public Test
{
public:
    class SettingsResetter : public mp::Settings
    {
    public:
        static void reset()
        {
            mp::Settings::reset();
        }
    };

    void TearDown() override
    {
        SettingsResetter::reset(); // expectations on MockHandlers verified here (unless manually unregistered earlier)
    }

    std::pair<QString, QString> make_setting(unsigned index)
    {
        return std::pair(QString("k%1").arg(index), QString("v%1").arg(index));
    };

    std::pair<unsigned, bool> half_and_is_odd(unsigned i)
    {
        auto half_i_div = std::div(i, 2); // can't use structured bindings directly: quot/rem order unspecified
        return std::pair(static_cast<unsigned>(half_i_div.quot), static_cast<bool>(half_i_div.rem));
    }
};

class MockSettingsHandler : public mp::SettingsHandler
{
public:
    using SettingsHandler::SettingsHandler;

    MOCK_METHOD(std::set<QString>, keys, (), (const, override));
    MOCK_METHOD(QString, get, (const QString&), (const, override));
    MOCK_METHOD(void, set, (const QString& key, const QString& val), (override));
};

TEST_F(TestSettings, keysReturnsNoKeysWhenNoHandler)
{
    EXPECT_THAT(MP_SETTINGS.keys(), IsEmpty());
}

TEST_F(TestSettings, keysReturnsKeysFromSingleHandler)
{
    auto some_keys = {QStringLiteral("a.b"), QStringLiteral("c.d.e"), QStringLiteral("f")};
    auto mock_handler = std::make_unique<MockSettingsHandler>();
    EXPECT_CALL(*mock_handler, keys).WillOnce(Return(some_keys));

    MP_SETTINGS.register_handler(std::move(mock_handler));
    EXPECT_THAT(MP_SETTINGS.keys(), UnorderedElementsAreArray(some_keys));
}

TEST_F(TestSettings, keysReturnsKeysFromMultipleHandlers)
{
    auto some_keychains =
        std::array{std::set({QStringLiteral("asdf.fdsa"), QStringLiteral("blah.bleh")}),
                   std::set({QStringLiteral("qwerty.ytrewq"), QStringLiteral("foo"), QStringLiteral("bar")}),
                   std::set({QStringLiteral("a.b.c.d")})};

    for (auto i = 0u; i < some_keychains.size(); ++i)
    {
        auto mock_handler = std::make_unique<MockSettingsHandler>();
        EXPECT_CALL(*mock_handler, keys).WillOnce(Return(some_keychains[i])); // copies, so ok to modify below
        MP_SETTINGS.register_handler(std::move(mock_handler));
    }

    auto all_keys = std::reduce(std::begin(some_keychains), std::end(some_keychains), // hands-off clang-format
                                std::set<QString>{}, [](auto&& a, auto&& b) {
                                    a.merge(std::forward<decltype(b)>(b));
                                    return std::forward<decltype(a)>(a);
                                });
    EXPECT_THAT(MP_SETTINGS.keys(), UnorderedElementsAreArray(all_keys));
}

TEST_F(TestSettings, getThrowsUnrecognizedWhenNoHandler)
{
    auto key = "qwer";
    MP_EXPECT_THROW_THAT(MP_SETTINGS.get(key), mp::UnrecognizedSettingException, mpt::match_what(HasSubstr(key)));
}

TEST_F(TestSettings, getThrowsUnrecognizedFromSingleHandler)
{
    auto key = "asdf";
    auto mock_handler = std::make_unique<MockSettingsHandler>();
    EXPECT_CALL(*mock_handler, get(Eq(key))).WillOnce(Throw(mp::UnrecognizedSettingException{key}));

    MP_SETTINGS.register_handler(std::move(mock_handler));
    MP_EXPECT_THROW_THAT(MP_SETTINGS.get(key), mp::UnrecognizedSettingException, mpt::match_what(HasSubstr(key)));
}

TEST_F(TestSettings, getThrowsUnrecognizedAfterTryingAllHandlers)
{
    auto key = "zxcv";
    for (auto i = 0; i < 10; ++i)
    {
        auto mock_handler = std::make_unique<MockSettingsHandler>();
        EXPECT_CALL(*mock_handler, get(Eq(key))).WillOnce(Throw(mp::UnrecognizedSettingException{key}));
        MP_SETTINGS.register_handler(std::move(mock_handler));
    }

    MP_EXPECT_THROW_THAT(MP_SETTINGS.get(key), mp::UnrecognizedSettingException, mpt::match_what(HasSubstr(key)));
}

TEST_F(TestSettings, getReturnsSettingFromSingleHandler)
{
    auto key = "asdf", val = "vvv";
    auto mock_handler = std::make_unique<MockSettingsHandler>();
    EXPECT_CALL(*mock_handler, get(Eq(key))).WillOnce(Return(val));

    MP_SETTINGS.register_handler(std::move(mock_handler));
    EXPECT_EQ(MP_SETTINGS.get(key), val);
}

using NumHandlersAndHitIndex = std::tuple<unsigned, unsigned>;
class TestSettingsGetMultipleHandlers : public TestSettings, public WithParamInterface<NumHandlersAndHitIndex>
{
};

TEST_P(TestSettingsGetMultipleHandlers, getReturnsSettingFromFirstHandlerHit)
{
    constexpr auto key = "τ", val = "2π";
    auto [num_handlers, hit_index] = GetParam();
    ASSERT_GT(num_handlers, 0u);
    ASSERT_GT(num_handlers, hit_index);

    for (auto i = 0u; i < num_handlers; ++i)
    {
        auto mock_handler = std::make_unique<MockSettingsHandler>();
        if (i < hit_index)
        {
            EXPECT_CALL(*mock_handler, get(Eq(key))).WillOnce(Throw(mp::UnrecognizedSettingException{key}));
        }
        else if (i == hit_index)
        {
            EXPECT_CALL(*mock_handler, get(Eq(key))).WillOnce(Return(val));
        }
        else
        {
            EXPECT_CALL(*mock_handler, get).Times(0);
        }

        MP_SETTINGS.register_handler(std::move(mock_handler));
    }

    EXPECT_EQ(MP_SETTINGS.get(key), val);
}

INSTANTIATE_TEST_SUITE_P(TestSettings, TestSettingsGetMultipleHandlers, Combine(Values(30u), Range(0u, 30u, 3u)));

TEST_F(TestSettings, getReturnsSettingsFromDifferentHandlers)
{
    constexpr auto num_settings = 3u;
    constexpr auto num_handlers = num_settings * 2u;
    constexpr auto unknown_key = "?";

    for (auto i = 0u; i < num_handlers; ++i)
    {
        auto mock_handler = std::make_unique<MockSettingsHandler>();

        auto [half_i, odd_i] = half_and_is_odd(i);
        for (auto j = 0u; j < num_settings; ++j)
        {
            auto [key, val] = make_setting(j);
            auto& expectation = EXPECT_CALL(*mock_handler, get(Eq(key)));

            if (j == half_i && !odd_i)
                expectation.WillOnce(Return(val));
            else if (j <= half_i)
                expectation.Times(0);
            else
                expectation.WillOnce(Throw(mp::UnrecognizedSettingException{key}));
        }

        EXPECT_CALL(*mock_handler, get(Eq(unknown_key))).WillOnce(Throw(mp::UnrecognizedSettingException{unknown_key}));
        MP_SETTINGS.register_handler(std::move(mock_handler));
    }

    for (auto i = 0u; i < num_settings; ++i)
    {
        auto [key, val] = make_setting(i);
        EXPECT_EQ(MP_SETTINGS.get(key), val);
    }

    MP_EXPECT_THROW_THAT(MP_SETTINGS.get(unknown_key), mp::UnrecognizedSettingException,
                         mpt::match_what(HasSubstr(unknown_key)));
}

TEST_F(TestSettings, setThrowsUnrecognizedWhenNoHandler)
{
    auto key = "poiu";
    MP_EXPECT_THROW_THAT(MP_SETTINGS.set(key, "qwer"), mp::UnrecognizedSettingException,
                         mpt::match_what(HasSubstr(key)));
}

TEST_F(TestSettings, setThrowsUnrecognizedFromSingleHandler)
{
    auto key = "lkjh", val = "asdf";
    auto mock_handler = std::make_unique<MockSettingsHandler>();
    EXPECT_CALL(*mock_handler, set(Eq(key), Eq(val))).WillOnce(Throw(mp::UnrecognizedSettingException{key}));

    MP_SETTINGS.register_handler(std::move(mock_handler));
    MP_EXPECT_THROW_THAT(MP_SETTINGS.set(key, val), mp::UnrecognizedSettingException, mpt::match_what(HasSubstr(key)));
}

TEST_F(TestSettings, setThrowsUnrecognizedAfterTryingAllHandlers)
{
    auto key = "mnbv", val = "zxcv";

    for (auto i = 0u; i < 10; ++i)
    {
        auto mock_handler = std::make_unique<MockSettingsHandler>();
        EXPECT_CALL(*mock_handler, set(Eq(key), Eq(val))).WillRepeatedly(Throw(mp::UnrecognizedSettingException{key}));
        MP_SETTINGS.register_handler(std::move(mock_handler));
    }

    MP_EXPECT_THROW_THAT(MP_SETTINGS.set(key, val), mp::UnrecognizedSettingException, mpt::match_what(HasSubstr(key)));
}

TEST_F(TestSettings, setDelegatesOnSingleHandler)
{
    auto key = "xyz", val = "zyx";
    auto mock_handler = std::make_unique<MockSettingsHandler>();
    EXPECT_CALL(*mock_handler, set(Eq(key), Eq(val)));

    MP_SETTINGS.register_handler(std::move(mock_handler));
    EXPECT_NO_THROW(MP_SETTINGS.set(key, val));
}

TEST_F(TestSettings, setDelegatesOnAllHandlers)
{
    auto key = "boo", val = "far";

    for (auto i = 0; i < 5; ++i)
    {
        auto mock_handler = std::make_unique<MockSettingsHandler>();
        EXPECT_CALL(*mock_handler, set(Eq(key), Eq(val)));
        MP_SETTINGS.register_handler(std::move(mock_handler));
    }

    EXPECT_NO_THROW(MP_SETTINGS.set(key, val));
}

using Indices = std::initializer_list<unsigned>;
using NumHandlersAndHitIndices = std::tuple<unsigned, Indices>;
class TestSettingsSetMultipleHandlers : public TestSettings, public WithParamInterface<NumHandlersAndHitIndices>
{
};

TEST_P(TestSettingsSetMultipleHandlers, setDelegatesOnMultipleHandlers)
{
    auto key = "boo", val = "far";
    auto [num_handlers, hit_indices] = GetParam();
    ASSERT_GT(num_handlers, 0u);
    ASSERT_GT(hit_indices.size(), 0u);
    ASSERT_THAT(hit_indices, Each(Lt(num_handlers)));

    for (auto i = 0u; i < num_handlers; ++i)
    {
        auto mock_handler = std::make_unique<MockSettingsHandler>();
        auto& expectation = EXPECT_CALL(*mock_handler, set(Eq(key), Eq(val)));

        if (std::find(hit_indices.begin(), hit_indices.end(), i) == hit_indices.end())
            expectation.WillOnce(Throw(mp::UnrecognizedSettingException{key}));

        MP_SETTINGS.register_handler(std::move(mock_handler));
    }

    EXPECT_NO_THROW(MP_SETTINGS.set(key, val));
}

const auto test_indices = std::initializer_list<Indices>{{0u},
                                                         {9u},
                                                         {7u},
                                                         {0u, 4u},
                                                         {0u, 4u, 9u},
                                                         {1u, 2u, 3u},
                                                         {5u, 6u, 7u, 8u},
                                                         {1u, 3u, 5u, 7u},
                                                         {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u}};
INSTANTIATE_TEST_SUITE_P(TestSettings, TestSettingsSetMultipleHandlers, Combine(Values(10u), ValuesIn(test_indices)));

TEST_F(TestSettings, setDelegatesOnDifferentHandlers)
{
    constexpr auto num_settings = 5u;
    constexpr auto num_handlers = num_settings * 2u;
    constexpr auto unknown_key = "hhhh";

    for (auto i = 0u; i < num_handlers; ++i)
    {
        auto mock_handler = std::make_unique<MockSettingsHandler>();

        auto [half_i, odd_i] = half_and_is_odd(i);
        for (auto j = 0u; j < num_settings; ++j)
        {
            auto [key, val] = make_setting(j);
            auto& expectation = EXPECT_CALL(*mock_handler, set(Eq(key), Eq(val)));
            if (j < half_i || odd_i)
            {
                expectation.WillOnce(Throw(mp::UnrecognizedSettingException{key}));
            }
        }

        EXPECT_CALL(*mock_handler, set(Eq(unknown_key), _))
            .WillOnce(Throw(mp::UnrecognizedSettingException{unknown_key}));
        MP_SETTINGS.register_handler(std::move(mock_handler));
    }

    for (auto i = 0u; i < num_settings; ++i)
    {
        auto [key, val] = make_setting(i);
        EXPECT_NO_THROW(MP_SETTINGS.set(key, val));
    }

    MP_EXPECT_THROW_THAT(MP_SETTINGS.set(unknown_key, "asdf"), mp::UnrecognizedSettingException,
                         mpt::match_what(HasSubstr(unknown_key)));
}

using SetException = std::variant<mp::UnrecognizedSettingException, mp::InvalidSettingException, std::runtime_error>; /*
  We need to throw an exception of the ultimate type whose handling we're trying to test (to avoid slicing and enter the
  right catch block). Therefore, we can't just use a base exception type. Parameterized and typed tests don't mix in
  gtest, so we use a variant parameter. */
using SetExceptNumHandlersAndThrowerIdx = std::tuple<SetException, unsigned, unsigned>;
class TestSettingSetOtherExceptions : public TestSettings, public WithParamInterface<SetExceptNumHandlersAndThrowerIdx>
{
};

TEST_P(TestSettingSetOtherExceptions, setThrowsOtherExceptionsFromAnyHandler)
{
    constexpr auto key = "F", val = "X";
    constexpr auto thrower = [](const auto& e) { throw e; };

    auto [except, num_handlers, thrower_idx] = GetParam();
    ASSERT_GT(num_handlers, 0u);
    ASSERT_GT(num_handlers, thrower_idx);

    for (auto i = 0u; i < num_handlers; ++i)
    {
        auto mock_handler = std::make_unique<MockSettingsHandler>();
        auto& expectation = EXPECT_CALL(*mock_handler, set(Eq(key), Eq(val)));
        if (i == thrower_idx)
            expectation.WillOnce(WithoutArgs([&thrower, e = &except] { std::visit(thrower, *e); })); /* lambda capture
                                                with initializer works around forbidden capture of structured binding */
        else if (i > thrower_idx)
            expectation.Times(0);

        MP_SETTINGS.register_handler(std::move(mock_handler));
    }

    auto get_what = [](const auto& e) { return e.what(); };
    MP_EXPECT_THROW_THAT(MP_SETTINGS.set(key, val), std::exception,
                         mpt::match_what(StrEq(std::visit(get_what, except))));
}

INSTANTIATE_TEST_SUITE_P(TestSettings, TestSettingSetOtherExceptions,
                         Combine(Values(mp::InvalidSettingException{"foo", "bar", "err"},
                                        std::runtime_error{"something else"}),
                                 Values(8u), Range(0u, 8u, 2u)));

struct TestSettingsGetAs : public Test
{
    mpt::MockSettings::GuardedMock mock_settings_injection = mpt::MockSettings::inject();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;
};

template <typename T>
struct SettingValueRepresentation
{
    T val;
    QStringList reprs;
};

template <typename T>
std::vector<SettingValueRepresentation<T>> setting_val_reprs();

template <>
std::vector<SettingValueRepresentation<bool>> setting_val_reprs()
{
    return {{false, {"False", "false", "0", ""}}, {true, {"True", "true", "1", "no", "off", "anything else"}}};
}

template <>
std::vector<SettingValueRepresentation<int>> setting_val_reprs()
{
    return {{0, {"0", "+0", "-0000"}}, {42, {"42", "+42"}}, {-2, {"-2"}}, {23, {"023"}}}; // no hex or octal
}

template <typename T>
struct TestSuccessfulSettingsGetAs : public TestSettingsGetAs
{
};

using GetAsTestTypes = ::testing::Types<bool, int>; // to add more, specialize setting_val_reprs above
MP_TYPED_TEST_SUITE(TestSuccessfulSettingsGetAs, GetAsTestTypes);

TYPED_TEST(TestSuccessfulSettingsGetAs, getAsConvertsValues)
{
    InSequence seq;
    const auto key = "whatever";
    for (const auto& [val, reprs] : setting_val_reprs<TypeParam>())
    {
        for (const auto& repr : reprs)
        {
            EXPECT_CALL(this->mock_settings, get(Eq(key))).WillOnce(Return(repr)); // needs `this` ¯\_(ツ)_/¯
            EXPECT_EQ(MP_SETTINGS.get_as<TypeParam>(key), val);
        }
    }
}

TEST_F(TestSettingsGetAs, getAsThrowsOnUnsupportedTypeConversion)
{
    const auto key = "the.key";
    const auto bad_repr = "#$%!@";
    EXPECT_CALL(mock_settings, get(Eq(key))).WillOnce(Return(bad_repr));
    MP_ASSERT_THROW_THAT(MP_SETTINGS.get_as<QVariant>(key), mp::UnsupportedSettingValueType<QVariant>,
                         mpt::match_what(HasSubstr(key)));
}

TEST_F(TestSettingsGetAs, getAsReturnsDefaultOnBadValue)
{
    const auto key = "a.key";
    const auto bad_int = "ceci n'est pas une int";
    EXPECT_CALL(mock_settings, get(Eq(key))).WillOnce(Return(bad_int));
    EXPECT_EQ(MP_SETTINGS.get_as<int>(key), 0);
}

} // namespace
