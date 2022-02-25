/*
 * Copyright (C) 2019-2022 Canonical, Ltd.
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

#include <QKeySequence>
#include <QString>

#include <algorithm>
#include <memory>

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
        SettingsResetter::reset();
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

TEST_F(TestSettings, returnsNoKeysWhenNoHandler)
{
    EXPECT_THAT(MP_SETTINGS.keys(), IsEmpty());
}

TEST_F(TestSettings, returnsKeysFromSingleHandler)
{
    auto some_keys = {QStringLiteral("a.b"), QStringLiteral("c.d.e"), QStringLiteral("f")};
    auto mock_handler = std::make_unique<MockSettingsHandler>();
    EXPECT_CALL(*mock_handler, keys).WillOnce(Return(some_keys));

    MP_SETTINGS.register_handler(std::move(mock_handler));
    EXPECT_THAT(MP_SETTINGS.keys(), UnorderedElementsAreArray(some_keys));
}

TEST_F(TestSettings, returnsKeysFromMultipleHandlers)
{
    std::array<std::unique_ptr<MockSettingsHandler>, 3> mock_handlers;
    std::generate(std::begin(mock_handlers), std::end(mock_handlers), &std::make_unique<MockSettingsHandler>);

    auto some_keychains =
        std::array{std::set({QStringLiteral("asdf.fdsa"), QStringLiteral("blah.bleh")}),
                   std::set({QStringLiteral("qwerty.ytrewq"), QStringLiteral("foo"), QStringLiteral("bar")}),
                   std::set({QStringLiteral("a.b.c.d")})};

    static_assert(mock_handlers.size() == some_keychains.size());
    for (auto i = 0u; i < some_keychains.size(); ++i)
    {
        EXPECT_CALL(*mock_handlers[i], keys).WillOnce(Return(some_keychains[i])); // copies, so ok to modify below
        MP_SETTINGS.register_handler(std::move(mock_handlers[i]));
    }

    auto all_keys = std::reduce(std::begin(some_keychains), std::end(some_keychains), // hands-off clang-format
                                std::set<QString>{}, [](auto&& a, auto&& b) {
                                    a.merge(std::forward<decltype(b)>(b));
                                    return std::forward<decltype(a)>(a);
                                });
    EXPECT_THAT(MP_SETTINGS.keys(), UnorderedElementsAreArray(all_keys));
}

TEST_F(TestSettings, throwsUnrecognizedWhenNoHandler)
{
    auto key = "qwer";
    MP_EXPECT_THROW_THAT(MP_SETTINGS.get(key), mp::UnrecognizedSettingException, mpt::match_what(HasSubstr(key)));
}

TEST_F(TestSettings, throwsUnrecognizedFromSingleHandler)
{
    auto key = "asdf";
    auto mock_handler = std::make_unique<MockSettingsHandler>();
    EXPECT_CALL(*mock_handler, get(Eq(key))).WillOnce(Throw(mp::UnrecognizedSettingException{key}));

    MP_SETTINGS.register_handler(std::move(mock_handler));
    MP_EXPECT_THROW_THAT(MP_SETTINGS.get(key), mp::UnrecognizedSettingException, mpt::match_what(HasSubstr(key)));
}

TEST_F(TestSettings, throwsUnrecognizedAfterTryingAllHandlers)
{
    auto key = "zxcv";
    std::array<std::unique_ptr<MockSettingsHandler>, 10> mock_handlers;
    std::generate(std::begin(mock_handlers), std::end(mock_handlers), &std::make_unique<MockSettingsHandler>);

    for (auto& mock_handler : mock_handlers)
    {
        EXPECT_CALL(*mock_handler, get(Eq(key))).WillOnce(Throw(mp::UnrecognizedSettingException{key}));
        MP_SETTINGS.register_handler(std::move(mock_handler));
    }

    MP_EXPECT_THROW_THAT(MP_SETTINGS.get(key), mp::UnrecognizedSettingException, mpt::match_what(HasSubstr(key)));
}

TEST_F(TestSettings, returnsSettingFromSingleHandler)
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

TEST_P(TestSettingsGetMultipleHandlers, returnsSettingFromFirstHandlerHit)
{
    auto key = "τ", val = "2π";
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

TEST_F(TestSettings, returnsSettingsFromDifferentHandlers)
{
    std::array keys = {"k1", "k2", "k3"}, vals = {"v1", "v2", "v3"}; // @TODO@ricab pairs
    std::array<std::unique_ptr<MockSettingsHandler>, 5> mock_handlers;
    std::generate(std::begin(mock_handlers), std::end(mock_handlers), &std::make_unique<MockSettingsHandler>);

    for (auto i = 0u; i < mock_handlers.size(); ++i)
    {
        auto kv_index = std::div(i, 2); // TODO@ricab structured bindings
        for (auto j = 0u; j < keys.size(); ++j)
        {
            auto key = keys[j], val = vals[j];
            if (j == static_cast<unsigned>(kv_index.quot) && !kv_index.rem) // TODO@ricab cast only once
            {
                EXPECT_CALL(*mock_handlers[i], get(Eq(key))).WillOnce(Return(val)); // TODO@ricab index only once
            }
            else if (j <= static_cast<unsigned>(kv_index.quot))
            {
                EXPECT_CALL(*mock_handlers[i], get(Eq(key))).Times(0);
            }
            else
            {
                EXPECT_CALL(*mock_handlers[i], get(Eq(key))).WillOnce(Throw(mp::UnrecognizedSettingException{key}));
            }
        }
        MP_SETTINGS.register_handler(std::move(mock_handlers[i]));
    }

    for (auto i = 0u; i < keys.size(); ++i)
    {
        EXPECT_EQ(MP_SETTINGS.get(keys[i]), vals[i]);
    }
}

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

template <>
std::vector<SettingValueRepresentation<QKeySequence>> setting_val_reprs()
{
    return {{QKeySequence{"Ctrl+Alt+O", QKeySequence::NativeText}, {"Ctrl+Alt+O", "ctrl+alt+o"}},
            {QKeySequence{"shift+e", QKeySequence::NativeText}, {"Shift+E", "shift+e"}}}; /*
                                  only a couple here, don't wanna go into platform details */
}

template <typename T>
struct TestSuccessfulSettingsGetAs : public TestSettingsGetAs
{
};

using GetAsTestTypes = ::testing::Types<bool, int, QKeySequence>; // to add more, specialize setting_val_reprs above
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
