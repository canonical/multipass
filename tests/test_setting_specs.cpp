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

#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/settings/basic_setting_spec.h>
#include <multipass/settings/bool_setting_spec.h>
#include <multipass/settings/custom_setting_spec.h>

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

namespace
{
template <typename T>
struct TestPlainKeyAndDefault : public Test
{
};

using PlainKeyAndDefaultTypes = Types<mp::BasicSettingSpec, mp::BoolSettingSpec, mp::CustomSettingSpec>;
MP_TYPED_TEST_SUITE(TestPlainKeyAndDefault, PlainKeyAndDefaultTypes);

TYPED_TEST(TestPlainKeyAndDefault, basicSettingSpecReturnsProvidedKeyAndDefault)
{
    const auto key = "foo", default_ = "true";
    const auto setting = [key, default_]() {
        if constexpr (std::is_same_v<TypeParam, mp::CustomSettingSpec>)
            return TypeParam{key, default_, [](const auto& v) { return v; }};
        else
            return TypeParam{key, default_};
    }();

    EXPECT_EQ(setting.get_key(), key);
    EXPECT_EQ(setting.get_default(), default_);
}

TEST(TestSettingSpec, basicSettingSpecImplementsInterpretAsIdentity)
{
    mp::BasicSettingSpec setting{"a", "b"};

    const auto val = "an arbitrary value";
    EXPECT_EQ(setting.interpret(val), val);
}

TEST(TestSettingSpec, customSettingSpecCallsGivenInterpreter)
{
    bool called = false;
    const auto val = "yak";
    mp::CustomSettingSpec setting{"a", "b", [&called](QString v) {
                                      called = true;
                                      return v;
                                  }};

    EXPECT_EQ(setting.interpret(val), val);
    EXPECT_TRUE(called);
}

TEST(TestSettingSpec, customSettingSpecInterpretsGivenDefault)
{
    const auto interpreted = "real";
    mp::CustomSettingSpec setting{"poiu", "lkjh", [interpreted](auto) { return interpreted; }};
    EXPECT_EQ(setting.get_default(), interpreted);
}

struct TestBadBoolSettingSpec : public TestWithParam<const char*>
{
};

TEST_P(TestBadBoolSettingSpec, boolSettingSpecRejectsBadDefaults)
{
    const auto key = "asdf";
    const auto bad = GetParam();
    MP_ASSERT_THROW_THAT((mp::BoolSettingSpec{key, bad}), mp::InvalidSettingException,
                         mpt::match_what(AllOf(HasSubstr(key), HasSubstr(bad))));
}

TEST_P(TestBadBoolSettingSpec, boolSettingSpecRejectsOtherValues)
{
    const auto key = "key";
    const auto bad = GetParam();
    mp::BoolSettingSpec setting{key, "true"};
    MP_ASSERT_THROW_THAT(setting.interpret(bad), mp::InvalidSettingException,
                         mpt::match_what(AllOf(HasSubstr(key), HasSubstr(bad))));
}

INSTANTIATE_TEST_SUITE_P(TestBadBoolSettingSpec, TestBadBoolSettingSpec,
                         Values("nonsense", "invalid", "", "bool", "representations", "-", "null", "4"));

using ReprVal = std::tuple<QString, QString>;
struct TestGoodBoolSettingSpec : public TestWithParam<ReprVal>
{
};

TEST_P(TestGoodBoolSettingSpec, interpretsBools)
{
    const auto& [repr, val] = GetParam();
    mp::BoolSettingSpec setting{"key", "false"};

    EXPECT_EQ(setting.interpret(repr), val);
}

INSTANTIATE_TEST_SUITE_P(TestTrueBoolSettingSpec, TestGoodBoolSettingSpec,
                         Combine(Values("yes", "on", "1", "true"), Values("true")));
INSTANTIATE_TEST_SUITE_P(TestFalseBoolSettingSpec, TestGoodBoolSettingSpec,
                         Combine(Values("no", "off", "0", "false"), Values("false")));
} // namespace
