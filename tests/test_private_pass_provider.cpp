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

#include <multipass/private_pass_provider.h>

#include <string>

namespace mp = multipass;

namespace
{
struct FriendExample;

struct PassExample : public mp::PrivatePassProvider<PassExample>
{
public:
    static std::string speak_friend_and_enter(const PrivatePass&, const std::string& ret)
    {
        return ret;
    }

private:
    static constexpr const PrivatePass& mellon = pass;
    friend struct FriendExample;
};

struct FriendExample
{
public:
    std::string enter(const std::string& ret) const
    {
        return PassExample::speak_friend_and_enter(PassExample::mellon, ret);
    }
};

TEST(PrivatePass, friend_can_call_function_requiring_pass)
{
    const auto str = "proof";
    const FriendExample fex{};
    EXPECT_EQ(fex.enter(str), str);
}

// safety demo
struct TryBreakInExample : public mp::PrivatePassProvider<TryBreakInExample>,
                           public mp::PrivatePassProvider<PassExample>
{
    static void trybreakin()
    {
        // PassExample::speak_friend_and_enter(pass, "asdf"); // error: which one?
        // PassExample::speak_friend_and_enter(PrivatePassProvider<TryBreakInExample>::pass, "fdsa"); // error: wrong
        // type PassExample::speak_friend_and_enter(PrivatePassProvider<PassExample>::pass, "x"); // error: not friends
    }
};

} // namespace
