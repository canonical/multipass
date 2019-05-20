/*
 * Copyright (C) 2018-2019 Canonical, Ltd.
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

#include <multipass/cli/csv_formatter.h>
#include <multipass/cli/json_formatter.h>
#include <multipass/cli/table_formatter.h>
#include <multipass/cli/yaml_formatter.h>
#include <multipass/constants.h>
#include <multipass/rpc/multipass.grpc.pb.h>

#include <fmt/format.h>

#include <gmock/gmock.h>

#include <locale>

namespace mp = multipass;
using namespace testing;

namespace
{
auto construct_single_instance_list_reply()
{
    mp::ListReply list_reply;

    auto list_entry = list_reply.add_instances();
    list_entry->set_name("foo");
    list_entry->mutable_instance_status()->set_status(mp::InstanceStatus::RUNNING);
    list_entry->set_current_release("16.04 LTS");
    list_entry->set_ipv4("10.168.32.2");

    return list_reply;
}

auto construct_multiple_instances_list_reply()
{
    mp::ListReply list_reply;

    auto list_entry = list_reply.add_instances();
    list_entry->set_name("bogus-instance");
    list_entry->mutable_instance_status()->set_status(mp::InstanceStatus::RUNNING);
    list_entry->set_current_release("16.04 LTS");
    list_entry->set_ipv4("10.21.124.56");

    list_entry = list_reply.add_instances();
    list_entry->set_name("bombastic");
    list_entry->mutable_instance_status()->set_status(mp::InstanceStatus::STOPPED);
    list_entry->set_current_release("18.04 LTS");

    return list_reply;
}

auto construct_multiple_instances_including_petenv_list_reply()
{
    auto reply = construct_multiple_instances_list_reply();

    auto instance = reply.add_instances();
    instance->set_name(mp::petenv_name);
    instance->mutable_instance_status()->set_status(mp::InstanceStatus::DELETED);
    instance->set_current_release("Not Available");

    return reply;
}

auto construct_single_instance_info_reply()
{
    mp::InfoReply info_reply;

    auto info_entry = info_reply.add_info();
    info_entry->set_name("foo");
    info_entry->mutable_instance_status()->set_status(mp::InstanceStatus::RUNNING);
    info_entry->set_image_release("16.04 LTS");
    info_entry->set_id("1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac");

    auto mount_info = info_entry->mutable_mount_info();
    mount_info->set_longest_path_len(19);

    auto mount_entry = mount_info->add_mount_paths();
    mount_entry->set_source_path("/home/user/foo");
    mount_entry->set_target_path("foo");
    (*mount_entry->mutable_mount_maps()->mutable_uid_map())[1000] = 1000;
    (*mount_entry->mutable_mount_maps()->mutable_gid_map())[1000] = 1000;

    mount_entry = mount_info->add_mount_paths();
    mount_entry->set_source_path("/home/user/test_dir");
    mount_entry->set_target_path("test_dir");
    (*mount_entry->mutable_mount_maps()->mutable_uid_map())[1000] = 1000;
    (*mount_entry->mutable_mount_maps()->mutable_gid_map())[1000] = 1000;

    info_entry->set_load("0.45 0.51 0.15");
    info_entry->set_memory_usage("60817408");
    info_entry->set_memory_total("1503238554");
    info_entry->set_disk_usage("1288490188");
    info_entry->set_disk_total("5153960756");
    info_entry->set_current_release("Ubuntu 16.04.3 LTS");
    info_entry->set_ipv4("10.168.32.2");

    return info_reply;
}

auto construct_multiple_instances_info_reply()
{
    mp::InfoReply info_reply;

    auto info_entry = info_reply.add_info();
    info_entry->set_name("bogus-instance");
    info_entry->mutable_instance_status()->set_status(mp::InstanceStatus::RUNNING);
    info_entry->set_image_release("16.04 LTS");
    info_entry->set_id("1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac");

    auto mount_info = info_entry->mutable_mount_info();
    mount_info->set_longest_path_len(17);

    auto mount_entry = mount_info->add_mount_paths();
    mount_entry->set_source_path("/home/user/source");
    mount_entry->set_target_path("source");
    (*mount_entry->mutable_mount_maps()->mutable_uid_map())[1000] = 501;
    (*mount_entry->mutable_mount_maps()->mutable_gid_map())[1000] = 501;

    info_entry->set_load("0.03 0.10 0.15");
    info_entry->set_memory_usage("38797312");
    info_entry->set_memory_total("1610612736");
    info_entry->set_disk_usage("1932735284");
    info_entry->set_disk_total("6764573492");
    info_entry->set_current_release("Ubuntu 16.04.3 LTS");
    info_entry->set_ipv4("10.21.124.56");

    info_entry = info_reply.add_info();
    info_entry->set_name("bombastic");
    info_entry->mutable_instance_status()->set_status(mp::InstanceStatus::STOPPED);
    info_entry->set_image_release("18.04 LTS");
    info_entry->set_id("ab5191cc172564e7cc0eafd397312a32598823e645279c820f0935393aead509");

    return info_reply;
}

auto construct_multiple_instances_including_petenv_info_reply()
{
    auto reply = construct_multiple_instances_info_reply();

    auto entry = reply.add_info();
    entry->set_name(mp::petenv_name);
    entry->mutable_instance_status()->set_status(mp::InstanceStatus::SUSPENDED);
    entry->set_image_release("18.10");
    entry->set_id("1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd");

    return reply;
}

auto construct_find_one_reply()
{
    auto reply = mp::FindReply();

    auto image_entry = reply.add_images_info();
    image_entry->set_os("Ubuntu");
    image_entry->set_release("18.04 LTS");
    image_entry->set_version("20190516");

    auto alias_entry = image_entry->add_aliases_info();
    alias_entry->set_alias("ubuntu");

    return reply;
}

auto construct_find_one_reply_no_os()
{
    auto reply = mp::FindReply();

    auto image_entry = reply.add_images_info();
    image_entry->set_release("Snapcraft builder for core18");
    image_entry->set_version("20190520");

    auto alias_entry = image_entry->add_aliases_info();
    alias_entry->set_alias("core18");
    alias_entry->set_remote_name("snapcraft");

    return reply;
}

auto construct_find_multiple_replies()
{
    auto reply = mp::FindReply();

    auto image_entry = reply.add_images_info();
    image_entry->set_os("Ubuntu");
    image_entry->set_release("18.04 LTS");
    image_entry->set_version("20190516");

    auto alias_entry = image_entry->add_aliases_info();
    alias_entry->set_alias("ubuntu");
    alias_entry = image_entry->add_aliases_info();
    alias_entry->set_alias("lts");

    image_entry = reply.add_images_info();
    image_entry->set_os("Ubuntu");
    image_entry->set_release("19.10");
    image_entry->set_version("20190516");

    alias_entry = image_entry->add_aliases_info();
    alias_entry->set_alias("19.10");
    alias_entry->set_remote_name("daily");
    alias_entry = image_entry->add_aliases_info();
    alias_entry->set_alias("eoan");
    alias_entry->set_remote_name("daily");
    alias_entry = image_entry->add_aliases_info();
    alias_entry->set_alias("devel");
    alias_entry->set_remote_name("daily");

    return reply;
}

auto construct_find_multiple_replies_duplicate_image()
{
    auto reply = mp::FindReply();

    auto image_entry = reply.add_images_info();
    image_entry->set_os("Ubuntu");
    image_entry->set_release("Core 18");
    image_entry->set_version("20190520");

    auto alias_entry = image_entry->add_aliases_info();
    alias_entry->set_alias("core18");

    image_entry = reply.add_images_info();
    image_entry->set_release("Snapcraft builder for core18");
    image_entry->set_version("20190520");

    alias_entry = image_entry->add_aliases_info();
    alias_entry->set_alias("core18");
    alias_entry->set_remote_name("snapcraft");

    return reply;
}
class LocaleSettingTest : public testing::Test
{
public:
    LocaleSettingTest() : saved_locale{std::locale()}
    {
        // The tests expected output are for the default C locale
        std::locale::global(std::locale("C"));
    }

    ~LocaleSettingTest()
    {
        std::locale::global(saved_locale);
    }

private:
    std::locale saved_locale;
};

class TableFormatter : public LocaleSettingTest
{
};

class JsonFormatter : public LocaleSettingTest
{
};

class CSVFormatter : public LocaleSettingTest
{
};

class YamlFormatter : public LocaleSettingTest
{
};
}

TEST_F(TableFormatter, single_instance_list_output)
{
    auto list_reply = construct_single_instance_list_reply();

    auto expected_table_output = "Name                    State             IPv4             Image\n"
                                 "foo                     Running           10.168.32.2      Ubuntu 16.04 LTS\n";

    mp::TableFormatter table_formatter;
    auto output = table_formatter.format(list_reply);

    EXPECT_THAT(output, Eq(expected_table_output));
}

TEST_F(TableFormatter, multiple_instance_list_output)
{
    auto list_reply = construct_multiple_instances_list_reply();

    auto expected_table_output = "Name                    State             IPv4             Image\n"
                                 "bogus-instance          Running           10.21.124.56     Ubuntu 16.04 LTS\n"
                                 "bombastic               Stopped           --               Ubuntu 18.04 LTS\n";

    mp::TableFormatter table_formatter;
    auto output = table_formatter.format(list_reply);

    EXPECT_THAT(output, Eq(expected_table_output));
}

#if GTEST_HAS_POSIX_RE
TEST_F(TableFormatter, pet_env_first_in_list_output)
{
    const mp::TableFormatter formatter;
    const auto reply = construct_multiple_instances_including_petenv_list_reply();
    const auto regex = fmt::format("Name[[:print:]]*\n{}[[:space:]]+.*", mp::petenv_name);

    const auto output = formatter.format(reply);
    EXPECT_THAT(output, MatchesRegex(regex));
}
#endif

TEST_F(TableFormatter, no_instances_list_output)
{
    mp::ListReply list_reply;

    auto expected_table_output = "No instances found.\n";

    mp::TableFormatter table_formatter;
    auto output = table_formatter.format(list_reply);

    EXPECT_THAT(output, Eq(expected_table_output));
}

TEST_F(TableFormatter, single_instance_info_output)
{
    auto info_reply = construct_single_instance_info_reply();

    auto expected_table_output = "Name:           foo\n"
                                 "State:          Running\n"
                                 "IPv4:           10.168.32.2\n"
                                 "Release:        Ubuntu 16.04.3 LTS\n"
                                 "Image hash:     1797c5c82016 (Ubuntu 16.04 LTS)\n"
                                 "Load:           0.45 0.51 0.15\n"
                                 "Disk usage:     1.2G out of 4.8G\n"
                                 "Memory usage:   58.0M out of 1.4G\n"
                                 "Mounts:         /home/user/foo      => foo\n"
                                 "                    UID map: 1000:1000\n"
                                 "                    GID map: 1000:1000\n"
                                 "                /home/user/test_dir => test_dir\n"
                                 "                    UID map: 1000:1000\n"
                                 "                    GID map: 1000:1000\n";

    mp::TableFormatter table_formatter;
    auto output = table_formatter.format(info_reply);

    EXPECT_THAT(output, Eq(expected_table_output));
}

TEST_F(TableFormatter, multiple_instances_info_output)
{
    auto info_reply = construct_multiple_instances_info_reply();

    auto expected_table_output = "Name:           bogus-instance\n"
                                 "State:          Running\n"
                                 "IPv4:           10.21.124.56\n"
                                 "Release:        Ubuntu 16.04.3 LTS\n"
                                 "Image hash:     1797c5c82016 (Ubuntu 16.04 LTS)\n"
                                 "Load:           0.03 0.10 0.15\n"
                                 "Disk usage:     1.8G out of 6.3G\n"
                                 "Memory usage:   37.0M out of 1.5G\n"
                                 "Mounts:         /home/user/source => source\n"
                                 "                    UID map: 1000:501\n"
                                 "                    GID map: 1000:501\n\n"
                                 "Name:           bombastic\n"
                                 "State:          Stopped\n"
                                 "IPv4:           --\n"
                                 "Release:        --\n"
                                 "Image hash:     ab5191cc1725 (Ubuntu 18.04 LTS)\n"
                                 "Load:           --\n"
                                 "Disk usage:     --\n"
                                 "Memory usage:   --\n";

    mp::TableFormatter table_formatter;
    auto output = table_formatter.format(info_reply);

    EXPECT_THAT(output, Eq(expected_table_output));
}

#if GTEST_HAS_POSIX_RE
TEST_F(TableFormatter, pet_env_first_in_info_output)
{
    const mp::TableFormatter formatter;
    const auto reply = construct_multiple_instances_including_petenv_info_reply();
    const auto regex = fmt::format("Name:[[:space:]]+{}.+", mp::petenv_name);

    const auto output = formatter.format(reply);
    EXPECT_THAT(output, MatchesRegex(regex));
}
#endif

TEST_F(TableFormatter, no_instances_info_output)
{
    mp::InfoReply info_reply;

    auto expected_table_output = "\n";

    mp::TableFormatter table_formatter;
    auto output = table_formatter.format(info_reply);

    EXPECT_THAT(output, Eq(expected_table_output));
}

TEST_F(TableFormatter, at_least_one_alias_in_find_output)
{
    mp::TableFormatter formatter;
    const auto reply = construct_find_one_reply();

    auto expected_output = "Image                   Aliases           Version          Description\n"
                           "ubuntu                                    20190516         Ubuntu 18.04 LTS\n";

    auto output = formatter.format(reply);

    EXPECT_EQ(output, expected_output);
}

TEST_F(TableFormatter, filtered_aliases_in_find_output)
{
    mp::TableFormatter formatter;
    const auto reply = construct_find_multiple_replies();

    auto expected_output = "Image                   Aliases           Version          Description\n"
                           "lts                                       20190516         Ubuntu 18.04 LTS\n"
                           "daily:19.10             eoan,devel        20190516         Ubuntu 19.10\n";

    auto output = formatter.format(reply);

    EXPECT_EQ(output, expected_output);
}

TEST_F(TableFormatter, well_formatted_empty_os_find_output)
{
    mp::TableFormatter formatter;
    const auto reply = construct_find_one_reply_no_os();

    auto expected_output = "Image                   Aliases           Version          Description\n"
                           "snapcraft:core18                          20190520         Snapcraft builder for core18\n";

    auto output = formatter.format(reply);

    EXPECT_EQ(output, expected_output);
}

TEST_F(TableFormatter, duplicate_images_in_find_output)
{
    mp::TableFormatter formatter;
    const auto reply = construct_find_multiple_replies_duplicate_image();

    auto expected_output = "Image                   Aliases           Version          Description\n"
                           "core18                                    20190520         Ubuntu Core 18\n"
                           "snapcraft:core18                          20190520         Snapcraft builder for core18\n";

    auto output = formatter.format(reply);

    EXPECT_EQ(output, expected_output);
}

TEST_F(TableFormatter, no_images_find_output)
{
    mp::FindReply find_reply;

    auto expected_table_output = "No images found.\n";

    mp::TableFormatter table_formatter;
    auto output = table_formatter.format(find_reply);

    EXPECT_EQ(output, expected_table_output);
}

TEST_F(JsonFormatter, single_instance_list_output)
{
    auto list_reply = construct_single_instance_list_reply();

    auto expected_json_output = "{\n"
                                "    \"list\": [\n"
                                "        {\n"
                                "            \"ipv4\": [\n"
                                "                \"10.168.32.2\"\n"
                                "            ],\n"
                                "            \"name\": \"foo\",\n"
                                "            \"release\": \"16.04 LTS\",\n"
                                "            \"state\": \"Running\"\n"
                                "        }\n"
                                "    ]\n"
                                "}\n";

    mp::JsonFormatter json_formatter;
    auto output = json_formatter.format(list_reply);

    EXPECT_THAT(output, Eq(expected_json_output));
}

TEST_F(JsonFormatter, multiple_instances_list_output)
{
    auto list_reply = construct_multiple_instances_list_reply();

    auto expected_json_output = "{\n"
                                "    \"list\": [\n"
                                "        {\n"
                                "            \"ipv4\": [\n"
                                "                \"10.21.124.56\"\n"
                                "            ],\n"
                                "            \"name\": \"bogus-instance\",\n"
                                "            \"release\": \"16.04 LTS\",\n"
                                "            \"state\": \"Running\"\n"
                                "        },\n"
                                "        {\n"
                                "            \"ipv4\": [\n"
                                "            ],\n"
                                "            \"name\": \"bombastic\",\n"
                                "            \"release\": \"18.04 LTS\",\n"
                                "            \"state\": \"Stopped\"\n"
                                "        }\n"
                                "    ]\n"
                                "}\n";

    mp::JsonFormatter json_formatter;
    auto output = json_formatter.format(list_reply);

    EXPECT_THAT(output, Eq(expected_json_output));
}

TEST_F(JsonFormatter, no_instances_list_output)
{
    mp::ListReply list_reply;

    auto expected_json_output = "{\n"
                                "    \"list\": [\n"
                                "    ]\n"
                                "}\n";

    mp::JsonFormatter json_formatter;
    auto output = json_formatter.format(list_reply);

    EXPECT_THAT(output, Eq(expected_json_output));
}

TEST_F(JsonFormatter, single_instance_info_output)
{
    auto info_reply = construct_single_instance_info_reply();

    auto expected_json_output =
        "{\n"
        "    \"errors\": [\n"
        "    ],\n"
        "    \"info\": {\n"
        "        \"foo\": {\n"
        "            \"disks\": {\n"
        "                \"sda1\": {\n"
        "                    \"total\": \"5153960756\",\n"
        "                    \"used\": \"1288490188\"\n"
        "                }\n"
        "            },\n"
        "            \"image_hash\": \"1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac\",\n"
        "            \"image_release\": \"16.04 LTS\",\n"
        "            \"ipv4\": [\n"
        "                \"10.168.32.2\"\n"
        "            ],\n"
        "            \"load\": [\n"
        "                0.45,\n"
        "                0.51,\n"
        "                0.15\n"
        "            ],\n"
        "            \"memory\": {\n"
        "                \"total\": 1503238554,\n"
        "                \"used\": 60817408\n"
        "            },\n"
        "            \"mounts\": {\n"
        "                \"foo\": {\n"
        "                    \"gid_mappings\": [\n"
        "                        \"1000:1000\"\n"
        "                    ],\n"
        "                    \"source_path\": \"/home/user/foo\",\n"
        "                    \"uid_mappings\": [\n"
        "                        \"1000:1000\"\n"
        "                    ]\n"
        "                },\n"
        "                \"test_dir\": {\n"
        "                    \"gid_mappings\": [\n"
        "                        \"1000:1000\"\n"
        "                    ],\n"
        "                    \"source_path\": \"/home/user/test_dir\",\n"
        "                    \"uid_mappings\": [\n"
        "                        \"1000:1000\"\n"
        "                    ]\n"
        "                }\n"
        "            },\n"
        "            \"release\": \"Ubuntu 16.04.3 LTS\",\n"
        "            \"state\": \"Running\"\n"
        "        }\n"
        "    }\n"
        "}\n";

    mp::JsonFormatter json_formatter;
    auto output = json_formatter.format(info_reply);

    EXPECT_THAT(output, Eq(expected_json_output));
}

TEST_F(JsonFormatter, multiple_instances_info_output)
{
    auto info_reply = construct_multiple_instances_info_reply();

    auto expected_json_output =
        "{\n"
        "    \"errors\": [\n"
        "    ],\n"
        "    \"info\": {\n"
        "        \"bogus-instance\": {\n"
        "            \"disks\": {\n"
        "                \"sda1\": {\n"
        "                    \"total\": \"6764573492\",\n"
        "                    \"used\": \"1932735284\"\n"
        "                }\n"
        "            },\n"
        "            \"image_hash\": \"1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac\",\n"
        "            \"image_release\": \"16.04 LTS\",\n"
        "            \"ipv4\": [\n"
        "                \"10.21.124.56\"\n"
        "            ],\n"
        "            \"load\": [\n"
        "                0.03,\n"
        "                0.1,\n"
        "                0.15\n"
        "            ],\n"
        "            \"memory\": {\n"
        "                \"total\": 1610612736,\n"
        "                \"used\": 38797312\n"
        "            },\n"
        "            \"mounts\": {\n"
        "                \"source\": {\n"
        "                    \"gid_mappings\": [\n"
        "                        \"1000:501\"\n"
        "                    ],\n"
        "                    \"source_path\": \"/home/user/source\",\n"
        "                    \"uid_mappings\": [\n"
        "                        \"1000:501\"\n"
        "                    ]\n"
        "                }\n"
        "            },\n"
        "            \"release\": \"Ubuntu 16.04.3 LTS\",\n"
        "            \"state\": \"Running\"\n"
        "        },\n"
        "        \"bombastic\": {\n"
        "            \"disks\": {\n"
        "                \"sda1\": {\n"
        "                }\n"
        "            },\n"
        "            \"image_hash\": \"ab5191cc172564e7cc0eafd397312a32598823e645279c820f0935393aead509\",\n"
        "            \"image_release\": \"18.04 LTS\",\n"
        "            \"ipv4\": [\n"
        "            ],\n"
        "            \"load\": [\n"
        "            ],\n"
        "            \"memory\": {\n"
        "            },\n"
        "            \"mounts\": {\n"
        "            },\n"
        "            \"release\": \"\",\n"
        "            \"state\": \"Stopped\"\n"
        "        }\n"
        "    }\n"
        "}\n";

    mp::JsonFormatter json_formatter;
    auto output = json_formatter.format(info_reply);

    EXPECT_THAT(output, Eq(expected_json_output));
}

TEST_F(JsonFormatter, no_instances_info_output)
{
    mp::InfoReply info_reply;

    auto expected_json_output = "{\n"
                                "    \"errors\": [\n"
                                "    ],\n"
                                "    \"info\": {\n"
                                "    }\n"
                                "}\n";

    mp::JsonFormatter json_formatter;
    auto output = json_formatter.format(info_reply);

    EXPECT_THAT(output, Eq(expected_json_output));
}

TEST_F(JsonFormatter, at_least_one_alias_in_find_output)
{
    mp::JsonFormatter formatter;
    const auto reply = construct_find_one_reply();

    auto expected_output = "{\n"
                           "    \"errors\": [\n"
                           "    ],\n"
                           "    \"images\": {\n"
                           "        \"ubuntu\": {\n"
                           "            \"aliases\": [\n"
                           "            ],\n"
                           "            \"os\": \"Ubuntu\",\n"
                           "            \"release\": \"18.04 LTS\",\n"
                           "            \"remote\": \"\",\n"
                           "            \"version\": \"20190516\"\n"
                           "        }\n"
                           "    }\n"
                           "}\n";

    auto output = formatter.format(reply);

    EXPECT_EQ(output, expected_output);
}

TEST_F(JsonFormatter, filtered_aliases_in_find_output)
{
    mp::JsonFormatter formatter;
    const auto reply = construct_find_multiple_replies();

    auto expected_output = "{\n"
                           "    \"errors\": [\n"
                           "    ],\n"
                           "    \"images\": {\n"
                           "        \"daily:19.10\": {\n"
                           "            \"aliases\": [\n"
                           "                \"eoan\",\n"
                           "                \"devel\"\n"
                           "            ],\n"
                           "            \"os\": \"Ubuntu\",\n"
                           "            \"release\": \"19.10\",\n"
                           "            \"remote\": \"daily\",\n"
                           "            \"version\": \"20190516\"\n"
                           "        },\n"
                           "        \"lts\": {\n"
                           "            \"aliases\": [\n"
                           "            ],\n"
                           "            \"os\": \"Ubuntu\",\n"
                           "            \"release\": \"18.04 LTS\",\n"
                           "            \"remote\": \"\",\n"
                           "            \"version\": \"20190516\"\n"
                           "        }\n"
                           "    }\n"
                           "}\n";

    auto output = formatter.format(reply);

    EXPECT_EQ(output, expected_output);
}

TEST_F(JsonFormatter, no_images_find_output)
{
    mp::FindReply find_reply;

    auto expected_output = "{\n"
                           "    \"errors\": [\n"
                           "    ],\n"
                           "    \"images\": {\n"
                           "    }\n"
                           "}\n";

    mp::JsonFormatter json_formatter;
    auto output = json_formatter.format(find_reply);

    EXPECT_EQ(output, expected_output);
}

TEST_F(JsonFormatter, duplicate_images_in_find_output)
{
    mp::JsonFormatter formatter;
    const auto reply = construct_find_multiple_replies_duplicate_image();

    auto expected_output = "{\n"
                           "    \"errors\": [\n"
                           "    ],\n"
                           "    \"images\": {\n"
                           "        \"core18\": {\n"
                           "            \"aliases\": [\n"
                           "            ],\n"
                           "            \"os\": \"Ubuntu\",\n"
                           "            \"release\": \"Core 18\",\n"
                           "            \"remote\": \"\",\n"
                           "            \"version\": \"20190520\"\n"
                           "        },\n"
                           "        \"snapcraft:core18\": {\n"
                           "            \"aliases\": [\n"
                           "            ],\n"
                           "            \"os\": \"\",\n"
                           "            \"release\": \"Snapcraft builder for core18\",\n"
                           "            \"remote\": \"snapcraft\",\n"
                           "            \"version\": \"20190520\"\n"
                           "        }\n"
                           "    }\n"
                           "}\n";

    auto output = formatter.format(reply);

    EXPECT_EQ(output, expected_output);
}

TEST_F(CSVFormatter, single_instance_list_output)
{
    auto list_reply = construct_single_instance_list_reply();

    auto expected_output = "Name,State,IPv4,IPv6,Release\n"
                           "foo,Running,10.168.32.2,,16.04 LTS\n";

    mp::CSVFormatter csv_formatter;
    auto output = csv_formatter.format(list_reply);

    EXPECT_THAT(output, Eq(expected_output));
}

TEST_F(CSVFormatter, multiple_instance_list_output)
{
    auto list_reply = construct_multiple_instances_list_reply();

    auto expected_output = "Name,State,IPv4,IPv6,Release\n"
                           "bogus-instance,Running,10.21.124.56,,16.04 LTS\n"
                           "bombastic,Stopped,,,18.04 LTS\n";

    mp::CSVFormatter csv_formatter;
    auto output = csv_formatter.format(list_reply);

    EXPECT_THAT(output, Eq(expected_output));
}

#if GTEST_HAS_POSIX_RE
TEST_F(CSVFormatter, pet_env_first_in_list_output)
{
    const mp::CSVFormatter formatter;
    const auto reply = construct_multiple_instances_including_petenv_list_reply();
    const auto regex = fmt::format("Name[[:print:]]*\n{},.*", mp::petenv_name);

    const auto output = formatter.format(reply);
    EXPECT_THAT(output, MatchesRegex(regex));
}
#endif

TEST_F(CSVFormatter, no_instances_list_output)
{
    mp::ListReply list_reply;

    auto expected_output = "Name,State,IPv4,IPv6,Release\n";

    mp::CSVFormatter csv_formatter;
    auto output = csv_formatter.format(list_reply);

    EXPECT_THAT(output, Eq(expected_output));
}

TEST_F(CSVFormatter, single_instance_info_output)
{
    auto info_reply = construct_single_instance_info_reply();

    auto expected_output = "Name,State,Ipv4,Ipv6,Release,Image hash,Image release,Load,Disk usage,Disk total,Memory "
                           "usage,Memory total,Mounts\nfoo,Running,10.168.32.2,,Ubuntu 16.04.3 "
                           "LTS,1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac,16.04 LTS,0.45 0.51 "
                           "0.15,1288490188,5153960756,60817408,1503238554,/home/user/foo => foo;/home/user/test_dir "
                           "=> test_dir;\n";

    mp::CSVFormatter csv_formatter;
    auto output = csv_formatter.format(info_reply);

    EXPECT_THAT(output, Eq(expected_output));
}

TEST_F(CSVFormatter, multiple_instances_info_output)
{
    auto info_reply = construct_multiple_instances_info_reply();

    auto expected_output = "Name,State,Ipv4,Ipv6,Release,Image hash,Image release,Load,Disk usage,Disk total,Memory "
                           "usage,Memory total,Mounts\nbogus-instance,Running,10.21.124.56,,Ubuntu 16.04.3 "
                           "LTS,1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac,16.04 LTS,0.03 0.10 "
                           "0.15,1932735284,6764573492,38797312,1610612736,/home/user/source => "
                           "source;\nbombastic,Stopped,,,,"
                           "ab5191cc172564e7cc0eafd397312a32598823e645279c820f0935393aead509,18.04 LTS,,,,,,\n";

    mp::CSVFormatter csv_formatter;
    auto output = csv_formatter.format(info_reply);

    EXPECT_THAT(output, Eq(expected_output));
}

#if GTEST_HAS_POSIX_RE
TEST_F(CSVFormatter, pet_env_first_in_info_output)
{
    const mp::CSVFormatter formatter;
    const auto reply = construct_multiple_instances_including_petenv_info_reply();
    const auto regex = fmt::format("Name[[:print:]]*\n{},.*", mp::petenv_name);

    const auto output = formatter.format(reply);
    EXPECT_THAT(output, MatchesRegex(regex));
}
#endif

TEST_F(CSVFormatter, no_instances_info_output)
{
    mp::InfoReply info_reply;

    auto expected_output = "Name,State,Ipv4,Ipv6,Release,Image hash,Image release,Load,Disk usage,Disk total,Memory "
                           "usage,Memory total,Mounts\n";

    mp::CSVFormatter csv_formatter;
    auto output = csv_formatter.format(info_reply);

    EXPECT_THAT(output, Eq(expected_output));
}

TEST_F(CSVFormatter, at_least_one_alias_in_find_output)
{
    mp::CSVFormatter formatter;
    const auto reply = construct_find_one_reply();

    auto expected_output = "Image,Remote,Aliases,OS,Release,Version\n"
                           "ubuntu,,,Ubuntu,18.04 LTS,20190516\n";
    auto output = formatter.format(reply);

    EXPECT_EQ(output, expected_output);
}

TEST_F(CSVFormatter, filtered_aliases_in_find_output)
{
    mp::CSVFormatter formatter;
    const auto reply = construct_find_multiple_replies();

    auto expected_output = "Image,Remote,Aliases,OS,Release,Version\n"
                           "lts,,,Ubuntu,18.04 LTS,20190516\n"
                           "daily:19.10,daily,eoan;devel,Ubuntu,19.10,20190516\n";

    auto output = formatter.format(reply);

    EXPECT_EQ(output, expected_output);
}

TEST_F(CSVFormatter, duplicate_images_in_find_output)
{
    mp::CSVFormatter formatter;
    const auto reply = construct_find_multiple_replies_duplicate_image();

    auto expected_output = "Image,Remote,Aliases,OS,Release,Version\n"
                           "core18,,,Ubuntu,Core 18,20190520\n"
                           "snapcraft:core18,snapcraft,,,Snapcraft builder for core18,20190520\n";

    auto output = formatter.format(reply);

    EXPECT_EQ(output, expected_output);
}

TEST_F(CSVFormatter, no_images_find_output)
{
    mp::FindReply find_reply;

    auto expected_output = "Image,Remote,Aliases,OS,Release,Version\n";

    mp::CSVFormatter csv_formatter;
    auto output = csv_formatter.format(find_reply);

    EXPECT_EQ(output, expected_output);
}

TEST_F(YamlFormatter, single_instance_list_output)
{
    auto list_reply = construct_single_instance_list_reply();

    auto expected_output = "foo:\n"
                           "  - state: Running\n"
                           "    ipv4:\n"
                           "      - 10.168.32.2\n"
                           "    release: 16.04 LTS\n";

    mp::YamlFormatter formatter;
    auto output = formatter.format(list_reply);

    EXPECT_THAT(output, Eq(expected_output));
}

TEST_F(YamlFormatter, multiple_instance_list_output)
{
    auto list_reply = construct_multiple_instances_list_reply();

    auto expected_output = "bogus-instance:\n"
                           "  - state: Running\n"
                           "    ipv4:\n"
                           "      - 10.21.124.56\n"
                           "    release: 16.04 LTS\n"
                           "bombastic:\n"
                           "  - state: Stopped\n"
                           "    ipv4:\n"
                           "      - \"\"\n"
                           "    release: 18.04 LTS\n";

    mp::YamlFormatter formatter;
    auto output = formatter.format(list_reply);

    EXPECT_THAT(output, Eq(expected_output));
}

TEST_F(YamlFormatter, pet_env_first_in_list_output)
{
    const mp::YamlFormatter formatter;
    const auto reply = construct_multiple_instances_including_petenv_list_reply();

    const auto output = formatter.format(reply);
    EXPECT_THAT(output, StartsWith(mp::petenv_name));
}

TEST_F(YamlFormatter, no_instances_list_output)
{
    mp::ListReply list_reply;

    auto expected_output = "\n";

    mp::YamlFormatter formatter;
    auto output = formatter.format(list_reply);

    EXPECT_THAT(output, Eq(expected_output));
}

TEST_F(YamlFormatter, single_instance_info_output)
{
    auto info_reply = construct_single_instance_info_reply();

    auto expected_output = "errors:\n"
                           "  - ~\n"
                           "foo:\n"
                           "  - state: Running\n"
                           "    image_hash: 1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac\n"
                           "    image_release: 16.04 LTS\n"
                           "    release: Ubuntu 16.04.3 LTS\n"
                           "    load:\n"
                           "      - 0.45\n"
                           "      - 0.51\n"
                           "      - 0.15\n"
                           "    disks:\n"
                           "      - sda1:\n"
                           "          used: 1288490188\n"
                           "          total: 5153960756\n"
                           "    memory:\n"
                           "      usage: 60817408\n"
                           "      total: 1503238554\n"
                           "    ipv4:\n"
                           "      - 10.168.32.2\n"
                           "    mounts:\n"
                           "      foo:\n"
                           "        uid_mappings:\n"
                           "          - 1000:1000\n"
                           "        gid_mappings:\n"
                           "          - 1000:1000\n"
                           "        source_path: /home/user/foo\n"
                           "      test_dir:\n"
                           "        uid_mappings:\n"
                           "          - 1000:1000\n"
                           "        gid_mappings:\n"
                           "          - 1000:1000\n"
                           "        source_path: /home/user/test_dir\n";

    mp::YamlFormatter formatter;
    auto output = formatter.format(info_reply);

    EXPECT_THAT(output, Eq(expected_output));
}

TEST_F(YamlFormatter, multiple_instances_info_output)
{
    auto info_reply = construct_multiple_instances_info_reply();

    auto expected_output = "errors:\n"
                           "  - ~\n"
                           "bogus-instance:\n"
                           "  - state: Running\n"
                           "    image_hash: 1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac\n"
                           "    image_release: 16.04 LTS\n"
                           "    release: Ubuntu 16.04.3 LTS\n"
                           "    load:\n"
                           "      - 0.03\n"
                           "      - 0.1\n"
                           "      - 0.15\n"
                           "    disks:\n"
                           "      - sda1:\n"
                           "          used: 1932735284\n"
                           "          total: 6764573492\n"
                           "    memory:\n"
                           "      usage: 38797312\n"
                           "      total: 1610612736\n"
                           "    ipv4:\n"
                           "      - 10.21.124.56\n"
                           "    mounts:\n"
                           "      source:\n"
                           "        uid_mappings:\n"
                           "          - 1000:501\n"
                           "        gid_mappings:\n"
                           "          - 1000:501\n"
                           "        source_path: /home/user/source\n"
                           "bombastic:\n"
                           "  - state: Stopped\n"
                           "    image_hash: ab5191cc172564e7cc0eafd397312a32598823e645279c820f0935393aead509\n"
                           "    image_release: 18.04 LTS\n"
                           "    release: ~\n"
                           "    disks:\n"
                           "      - sda1:\n"
                           "          used: ~\n"
                           "          total: ~\n"
                           "    memory:\n"
                           "      usage: ~\n"
                           "      total: ~\n"
                           "    mounts: ~\n";

    mp::YamlFormatter formatter;
    auto output = formatter.format(info_reply);

    EXPECT_THAT(output, Eq(expected_output));
}

#if GTEST_HAS_POSIX_RE
TEST_F(YamlFormatter, pet_env_first_in_info_output)
{
    const mp::YamlFormatter formatter;
    const auto reply = construct_multiple_instances_including_petenv_info_reply();
    const auto regex = fmt::format("(errors:[[:space:]]+-[[:space:]]+~[[:space:]]+)?{}:.*", mp::petenv_name);

    const auto output = formatter.format(reply);
    EXPECT_THAT(output, MatchesRegex(regex));
}
#endif

TEST_F(YamlFormatter, no_instances_info_output)
{
    mp::InfoReply info_reply;

    auto expected_output = "errors:\n  - ~\n";

    mp::YamlFormatter formatter;
    auto output = formatter.format(info_reply);

    EXPECT_THAT(output, Eq(expected_output));
}

TEST_F(YamlFormatter, at_least_one_alias_in_find_output)
{
    mp::YamlFormatter formatter;
    const auto reply = construct_find_one_reply();

    auto expected_output = "errors:\n"
                           "  []\n"
                           "images:\n"
                           "  ubuntu:\n"
                           "    aliases:\n"
                           "      []\n"
                           "    os: Ubuntu\n"
                           "    release: 18.04 LTS\n"
                           "    version: 20190516\n"
                           "    remote: \"\"\n";

    auto output = formatter.format(reply);

    EXPECT_EQ(output, expected_output);
}

TEST_F(YamlFormatter, filtered_aliases_in_find_output)
{
    mp::YamlFormatter formatter;
    const auto reply = construct_find_multiple_replies();

    auto expected_output = "errors:\n"
                           "  []\n"
                           "images:\n"
                           "  lts:\n"
                           "    aliases:\n"
                           "      []\n"
                           "    os: Ubuntu\n"
                           "    release: 18.04 LTS\n"
                           "    version: 20190516\n"
                           "    remote: \"\"\n"
                           "  daily:19.10:\n"
                           "    aliases:\n"
                           "      - eoan\n"
                           "      - devel\n"
                           "    os: Ubuntu\n"
                           "    release: 19.10\n"
                           "    version: 20190516\n"
                           "    remote: daily\n";

    auto output = formatter.format(reply);

    EXPECT_EQ(output, expected_output);
}

TEST_F(YamlFormatter, duplicate_images_in_find_output)
{
    mp::YamlFormatter formatter;
    const auto reply = construct_find_multiple_replies_duplicate_image();

    auto expected_output = "errors:\n"
                           "  []\n"
                           "images:\n"
                           "  core18:\n"
                           "    aliases:\n"
                           "      []\n"
                           "    os: Ubuntu\n"
                           "    release: Core 18\n"
                           "    version: 20190520\n"
                           "    remote: \"\"\n"
                           "  snapcraft:core18:\n"
                           "    aliases:\n"
                           "      []\n"
                           "    os: \"\"\n"
                           "    release: Snapcraft builder for core18\n"
                           "    version: 20190520\n"
                           "    remote: snapcraft\n";

    auto output = formatter.format(reply);

    EXPECT_EQ(output, expected_output);
}

TEST_F(YamlFormatter, no_images_find_output)
{
    mp::FindReply find_reply;

    auto expected_output = "errors:\n"
                           "  []\n"
                           "images:\n"
                           "  {}\n";

    mp::YamlFormatter yaml_formatter;
    auto output = yaml_formatter.format(find_reply);

    EXPECT_EQ(output, expected_output);
}
