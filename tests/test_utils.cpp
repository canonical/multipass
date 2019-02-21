/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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

#include <multipass/utils.h>

#include "file_operations.h"
#include "temp_dir.h"

#include <QRegExp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <sstream>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

TEST(Utils, KB_is_valid_memory_value)
{
    EXPECT_TRUE(mp::utils::valid_memory_value(QString("1024KB")));
}

TEST(Utils, K_is_valid_memory_value)
{
    EXPECT_TRUE(mp::utils::valid_memory_value(QString("1024K")));
}

TEST(Utils, MB_is_valid_memory_value)
{
    EXPECT_TRUE(mp::utils::valid_memory_value(QString("1024MB")));
}

TEST(Utils, M_is_valid_memory_value)
{
    EXPECT_TRUE(mp::utils::valid_memory_value(QString("1024M")));
}

TEST(Utils, GB_is_valid_memory_value)
{
    EXPECT_TRUE(mp::utils::valid_memory_value(QString("1024GB")));
}

TEST(Utils, G_is_valid_memory_value)
{
    EXPECT_TRUE(mp::utils::valid_memory_value(QString("1024G")));
}

TEST(Utils, no_unit_is_valid_memory_value)
{
    EXPECT_TRUE(mp::utils::valid_memory_value(QString("1024")));
}

TEST(Utils, B_is_valid_memory_value)
{
    EXPECT_TRUE(mp::utils::valid_memory_value(QString("123B")));
}

TEST(Utils, memory_value_units_may_be_smallcase)
{
    EXPECT_TRUE(mp::utils::valid_memory_value(QString("42b")));
    EXPECT_TRUE(mp::utils::valid_memory_value(QString("42mb")));
    EXPECT_TRUE(mp::utils::valid_memory_value(QString("42kB")));
    EXPECT_TRUE(mp::utils::valid_memory_value(QString("42g")));
}

TEST(Utils, BB_is_invalid_memory_value)
{
    EXPECT_FALSE(mp::utils::valid_memory_value(QString("321BB")));
}

TEST(Utils, BK_is_invalid_memory_value)
{
    EXPECT_FALSE(mp::utils::valid_memory_value(QString("321BK")));
}

TEST(Utils, MM_unit_is_invalid_memory_value)
{
    EXPECT_FALSE(mp::utils::valid_memory_value(QString("1024MM")));
}

TEST(Utils, KM_unit_is_invalid_memory_value)
{
    EXPECT_FALSE(mp::utils::valid_memory_value(QString("1024KM")));
}

TEST(Utils, GK_unit_is_invalid_memory_value)
{
    EXPECT_FALSE(mp::utils::valid_memory_value(QString("1024GK")));
}

TEST(Utils, only_unit_is_invalid_memory_value)
{
    EXPECT_FALSE(mp::utils::valid_memory_value(QString("K")));
}

TEST(Utils, empty_string_is_invalid_memory_value)
{
    EXPECT_FALSE(mp::utils::valid_memory_value(QString("")));
}

TEST(Utils, decimal_is_invalid_memory_value)
{
    EXPECT_FALSE(mp::utils::valid_memory_value(QString("123.321K")));
}

TEST(Utils, hostname_begins_with_letter_is_valid)
{
    EXPECT_TRUE(mp::utils::valid_hostname(QString("foo")));
}

TEST(Utils, hostname_single_letter_is_valid)
{
    EXPECT_TRUE(mp::utils::valid_hostname(QString("f")));
}

TEST(Utils, hostname_contains_digit_is_valid)
{
    EXPECT_TRUE(mp::utils::valid_hostname(QString("foo1")));
}

TEST(Utils, hostname_contains_hyphen_is_valid)
{
    EXPECT_TRUE(mp::utils::valid_hostname(QString("foo-bar")));
}

TEST(Utils, hostname_begins_with_digit_is_invalid)
{
    EXPECT_FALSE(mp::utils::valid_hostname(QString("1foo")));
}

TEST(Utils, hostname_single_digit_is_invalid)
{
    EXPECT_FALSE(mp::utils::valid_hostname(QString("1")));
}

TEST(Utils, hostname_contains_underscore_is_invalid)
{
    EXPECT_FALSE(mp::utils::valid_hostname(QString("foo_bar")));
}

TEST(Utils, hostname_contains_special_character_is_invalid)
{
    EXPECT_FALSE(mp::utils::valid_hostname(QString("foo!")));
}

TEST(Utils, path_root_invalid)
{
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//")));
}

TEST(Utils, path_root_foo_valid)
{
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/foo")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/foo/")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("//foo")));
}

TEST(Utils, path_dev_invalid)
{
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/dev")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/dev/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//dev/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/dev//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//dev//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/dev/foo")));
}

TEST(Utils, path_devpath_valid)
{
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/devpath")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/devpath/")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/devpath/foo")));
}

TEST(Utils, path_proc_invalid)
{
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/proc")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/proc/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//proc/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/proc//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//proc//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/proc/foo")));
}

TEST(Utils, path_sys_invalid)
{
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/sys")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/sys/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//sys/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/sys//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//sys//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/sys/foo")));
}

TEST(Utils, path_home_proper_invalid)
{
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//home/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//home//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home/foo/..")));
}

TEST(Utils, path_home_ubuntu_invalid)
{
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home/ubuntu")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home/ubuntu/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//home/ubuntu/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home//ubuntu/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home/ubuntu//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//home//ubuntu//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home/ubuntu/foo/..")));
}

TEST(Utils, path_home_foo_valid)
{
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/home/foo")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/home/foo/")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("//home/foo/")));
}

TEST(Utils, path_home_ubuntu_foo_valid)
{
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/home/ubuntu/foo")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/home/ubuntu/foo/")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("//home/ubuntu/foo")));
}

TEST(Utils, to_cmd_output_has_no_quotes)
{
    std::vector<std::string> args{"hello", "world"};
    auto output = mp::utils::to_cmd(args, mp::utils::QuoteType::no_quotes);
    EXPECT_THAT(output, ::testing::StrEq("hello world"));
}

TEST(Utils, to_cmd_arguments_are_single_quoted)
{
    std::vector<std::string> args{"hello", "world"};
    auto output = mp::utils::to_cmd(args, mp::utils::QuoteType::quote_every_arg);
    EXPECT_THAT(output, ::testing::StrEq("'hello' 'world'"));
}

TEST(Utils, to_cmd_arguments_are_double_quoted_when_needed)
{
    std::vector<std::string> args{"it's", "me"};
    auto output = mp::utils::to_cmd(args, mp::utils::QuoteType::quote_every_arg);
    EXPECT_THAT(output, ::testing::StrEq("\"it's\" 'me'"));
}

TEST(Utils, to_cmd_arguments_are_single_quoted_when_needed)
{
    std::vector<std::string> args{"they", "said", "\"please\""};
    auto output = mp::utils::to_cmd(args, mp::utils::QuoteType::quote_every_arg);
    EXPECT_THAT(output, ::testing::StrEq("'they' 'said' '\"please\"'"));
}

TEST(Utils, trim_end_actually_trims_end)
{
    std::string s{"I'm a great\n\t string \n \f \n \r \t   \v"};
    mp::utils::trim_end(s);

    EXPECT_THAT(s, ::testing::StrEq("I'm a great\n\t string"));
}

TEST(Utils, escape_char_actually_escapes)
{
    std::string s{"I've got \"quotes\""};
    auto res = mp::utils::escape_char(s, '"');
    EXPECT_THAT(res, ::testing::StrEq("I've got \\\"quotes\\\""));
}

TEST(Utils, try_action_actually_times_out)
{
    bool on_timeout_called{false};
    auto on_timeout = [&on_timeout_called] { on_timeout_called = true; };
    auto retry_action = [] { return mp::utils::TimeoutAction::retry; };
    mp::utils::try_action_for(on_timeout, std::chrono::milliseconds(1), retry_action);

    EXPECT_TRUE(on_timeout_called);
}

TEST(Utils, try_action_does_not_timeout)
{
    bool on_timeout_called{false};
    auto on_timeout = [&on_timeout_called] { on_timeout_called = true; };

    bool action_called{false};
    auto successful_action = [&action_called] {
        action_called = true;
        return mp::utils::TimeoutAction::done;
    };
    mp::utils::try_action_for(on_timeout, std::chrono::seconds(1), successful_action);

    EXPECT_FALSE(on_timeout_called);
    EXPECT_TRUE(action_called);
}

TEST(Utils, uuid_has_no_curly_brackets)
{
    auto uuid = mp::utils::make_uuid();
    EXPECT_FALSE(uuid.contains(QRegExp("[{}]")));
}

TEST(Utils, contents_of_actually_reads_contents)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    std::string expected_content{"just a bit of test content here"};
    mpt::make_file_with_content(file_name, expected_content);

    auto content = mp::utils::contents_of(file_name);
    EXPECT_THAT(content, StrEq(expected_content));
}

TEST(Utils, contents_of_throws_on_missing_file)
{
    EXPECT_THROW(mp::utils::contents_of("this-file-does-not-exist"), std::runtime_error);
}

TEST(Utils, contents_of_empty_contents_on_empty_file)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/empty_test_file";
    mpt::make_file_with_content(file_name, "");

    auto content = mp::utils::contents_of(file_name);
    EXPECT_TRUE(content.empty());
}

TEST(Utils, split_returns_token_list)
{
    std::vector<std::string> expected_tokens;
    expected_tokens.push_back("Hello");
    expected_tokens.push_back("World");
    expected_tokens.push_back("Bye!");

    const std::string delimiter{":"};

    std::stringstream content;
    for (const auto& token : expected_tokens)
    {
        content << token;
        content << delimiter;
    }

    const auto tokens = mp::utils::split(content.str(), delimiter);
    EXPECT_THAT(tokens, ContainerEq(expected_tokens));
}

TEST(Utils, split_returns_one_token_if_no_delimiter)
{
    const std::string content{"no delimiter here"};
    const std::string delimiter{":"};

    const auto tokens = mp::utils::split(content, delimiter);
    ASSERT_THAT(tokens.size(), Eq(1u));

    EXPECT_THAT(tokens[0], StrEq(content));
}

TEST(Utils, has_only_digits_works)
{
    EXPECT_FALSE(mp::utils::has_only_digits("124ft:,"));
    EXPECT_TRUE(mp::utils::has_only_digits("0123456789"));
    EXPECT_FALSE(mp::utils::has_only_digits("0123456789:'`'"));
}

TEST(Utils, validate_server_address_throws_on_invalid_address)
{
    EXPECT_THROW(mp::utils::validate_server_address("unix"), std::runtime_error);
    EXPECT_THROW(mp::utils::validate_server_address("unix:"), std::runtime_error);
    EXPECT_THROW(mp::utils::validate_server_address("test:test"), std::runtime_error);
    EXPECT_THROW(mp::utils::validate_server_address(""), std::runtime_error);
}

TEST(Utils, validate_server_address_does_not_throw_on_good_address)
{
    EXPECT_NO_THROW(mp::utils::validate_server_address("unix:/tmp/a_socket"));
    EXPECT_NO_THROW(mp::utils::validate_server_address("test-server.net:123"));
}

TEST(Utils, dir_is_a_dir)
{
    mpt::TempDir temp_dir;
    EXPECT_TRUE(mp::utils::is_dir(temp_dir.path().toStdString()));
}

TEST(Utils, file_is_not_a_dir)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/empty_test_file";
    mpt::make_file_with_content(file_name, "");

    EXPECT_FALSE(mp::utils::is_dir(file_name.toStdString()));
}

TEST(Utils, filename_only_is_returned)
{
    std::string file_name{"my_file"};
    std::string full_path{"/tmp/foo/" + file_name};

    EXPECT_THAT(mp::utils::filename_for(full_path), Eq(file_name));
}

TEST(Utils, vm_running_returns_true)
{
    mp::VirtualMachine::State state = mp::VirtualMachine::State::running;

    EXPECT_TRUE(mp::utils::is_running(state));
}

TEST(Utils, vm_delayed_shutdown_returns_true)
{
    mp::VirtualMachine::State state = mp::VirtualMachine::State::delayed_shutdown;

    EXPECT_TRUE(mp::utils::is_running(state));
}

TEST(Utils, vm_stopped_returns_false)
{
    mp::VirtualMachine::State state = mp::VirtualMachine::State::stopped;

    EXPECT_FALSE(mp::utils::is_running(state));
}
