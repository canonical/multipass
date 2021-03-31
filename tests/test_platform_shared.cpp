/*
 * Copyright (C) 2020-2021 Canonical, Ltd.
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

#include "extra_assertions.h"

#include <multipass/constants.h>
#include <multipass/platform.h>

#include <src/platform/platform_shared.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QKeySequence>
#include <multipass/exceptions/settings_exceptions.h>

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

namespace
{

TEST(PlatformShared, hotkey_interpretation_throws_on_invalid_hotkey)
{
    const auto bad_sequences =
        std::vector<QString>{"abcd",  "uU", "f42", "f0", "d3", "Fn+x", "Ctrl+a,Shift+b", "Alt+u,Ctrl+y,Alt+t",
                             "alt+,x"}; // multiple not allowed either
    for (const auto& bad_sequence : bad_sequences)
    {
        MP_EXPECT_THROW_THAT(mp::platform::interpret_hotkey(bad_sequence), mp::InvalidSettingsException,
                             mpt::match_what(AllOf(HasSubstr(mp::hotkey_key), HasSubstr(bad_sequence.toStdString()))));
    }
}

TEST(PlatformShared, hotkey_interpretation_of_acceptable_hotkey)
{
    const auto good_sequences = std::vector<QString>{
        "u",          "U",     "shift+U", "Space",     "alt+space", "backspace",    "alt+meta+l",
        "alt+,",      "RIGHT", "-",       "shift+-",   "shift+_",   "ctrl+shift+-", "ctrl+_",
        "Media Play", "Home",  "Pause",   "shift+end", "tab",       "alt+shift+3",
    };

    for (const auto& good_sequence : good_sequences)
    {
        EXPECT_EQ(QKeySequence{mp::platform::interpret_hotkey(good_sequence)}, QKeySequence{good_sequence});
    }
}

} // namespace
