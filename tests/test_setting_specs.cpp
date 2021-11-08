/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#include <multipass/settings/basic_setting_spec.h>
#include <multipass/settings/bool_setting_spec.h>
#include <multipass/settings/dynamic_setting_spec.h>

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

namespace
{
template <typename T>
struct TestPlainKeyAndDefault : public Test
{
};

using PlainKeyAndDefaultTypes = Types<mp::BasicSettingSpec, mp::BoolSettingSpec, mp::DynamicSettingSpec>;
MP_TYPED_TEST_SUITE(TestPlainKeyAndDefault, PlainKeyAndDefaultTypes);

TYPED_TEST(TestPlainKeyAndDefault, basicSettingSpecReturnsProvidedKeyAndDefault)
{
    const auto key = "foo", default_ = "bar";
    const auto setting = [key, default_]() {
        if constexpr (std::is_same_v<TypeParam, mp::DynamicSettingSpec>)
            return TypeParam{key, default_, {}};
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

TEST(TestSettingSpec, dynamicSettingSpecCallsGivenInterpreter)
{
    bool called = false;
    const auto val = "yak";
    mp::DynamicSettingSpec setting{"a", "b", [&called](const QString& v) {
                                       called = true;
                                       return v;
                                   }};

    EXPECT_EQ(setting.interpret(val), val);
    EXPECT_TRUE(called);
}

} // namespace
