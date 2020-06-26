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

#include <multipass/constants.h>
#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/platform.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QKeySequence>

namespace mp = multipass;
using namespace testing;

namespace
{

TEST(PlatformOSX, test_no_extra_settings)
{
    EXPECT_THAT(mp::platform::extra_settings_defaults(), IsEmpty());
}

TEST(PlatformOSX, test_interpretation_of_winterm_setting_not_supported)
{
    for (const auto x : {"no", "matter", "what"})
        EXPECT_THROW(mp::platform::interpret_setting(mp::winterm_key, x), mp::InvalidSettingsException);
}

TEST(PlatformOSX, test_interpretation_of_unknown_settings_not_supported)
{
    for (const auto k : {"unimaginable", "katxama", "katxatxa"})
        for (const auto v : {"no", "matter", "what"})
            EXPECT_THROW(mp::platform::interpret_setting(k, v), mp::InvalidSettingsException);
}

TEST(PlatformOSX, test_empty_sync_winterm_profiles)
{
    EXPECT_NO_THROW(mp::platform::sync_winterm_profiles());
}

template <typename Matcher>
void check_interpreted_hotkey(const QString& hotkey, Matcher&& matcher)
{
    auto interpreted_hotkey = mp::platform::interpret_setting(mp::hotkey_key, hotkey);
    EXPECT_THAT(QKeySequence{interpreted_hotkey}.toString(QKeySequence::PortableText).toLower(),
                Property(&QString::toStdString, std::forward<Matcher>(matcher)));
}

TEST(PlatformOSX, test_hotkey_interpretation_replaces_meta_and_opt)
{
    for (QString sequence : {"shift+opt+u", "Option+3", "meta+Opt+.", "Meta+Shift+Space"})
        check_interpreted_hotkey(sequence, AllOf(Not(HasSubstr("opt")), Not(HasSubstr("meta")), HasSubstr("alt")));
}

TEST(PlatformOSX, test_hotkey_interpretation_replaces_ctrl)
{
    for (QString sequence : {"ctrl+m", "Alt+Ctrl+/", "Control+opt+-"})
        check_interpreted_hotkey(sequence, AllOf(Not(HasSubstr("ctrl")), Not(HasSubstr("control")), HasSubstr("meta")));
}

TEST(PlatformOSX, test_hotkey_interpretation_replaces_cmd)
{
    for (QString sequence : {"cmd+t", "ctrl+cmd+u", "Alt+Command+i", "Command+=", "Command+shift+]"})
        check_interpreted_hotkey(sequence, AllOf(Not(HasSubstr("cmd")), Not(HasSubstr("command")), HasSubstr("ctrl")));
}

TEST(PlatformOSX, test_hotkey_interpretation_replaces_mix)
{
    const auto check_cmd = AllOf(Not(HasSubstr("cmd")), Not(HasSubstr("command")), HasSubstr("ctrl"));
    const auto check_opt = AllOf(Not(HasSubstr("opt")), HasSubstr("alt"), Not(HasSubstr("ion"))); // option - opt = ion
    const auto check_ctrl = HasSubstr("meta");
    const auto check_dot = HasSubstr(".");
    const auto check_all = AllOf(check_cmd, check_opt, check_ctrl, check_dot);

    for (QString sequence : {"cmd+meta+ctrl+.", "Control+Command+Option+."})
        check_interpreted_hotkey(sequence, check_all);
}
} // namespace
