/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include <multipass/cli/json_formatter.h>
#include <multipass/cli/table_formatter.h>
#include <multipass/rpc/multipass.grpc.pb.h>

#include <gmock/gmock.h>

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
    list_entry->set_current_release("Ubuntu 16.04 LTS");
    list_entry->set_ipv4("10.168.32.2");

    return list_reply;
}

auto construct_multiple_instances_list_reply()
{
    mp::ListReply list_reply;

    auto list_entry = list_reply.add_instances();
    list_entry->set_name("bogus-instance");
    list_entry->mutable_instance_status()->set_status(mp::InstanceStatus::RUNNING);
    list_entry->set_current_release("Ubuntu 16.04 LTS");
    list_entry->set_ipv4("10.21.124.56");

    list_entry = list_reply.add_instances();
    list_entry->set_name("bombastic");
    list_entry->mutable_instance_status()->set_status(mp::InstanceStatus::STOPPED);
    list_entry->set_current_release("Ubuntu 18.04 LTS");

    return list_reply;
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

    mount_entry = mount_info->add_mount_paths();
    mount_entry->set_source_path("/home/user/test_dir");
    mount_entry->set_target_path("test_dir");

    info_entry->set_load("0.00 0.00 0.00");
    info_entry->set_memory_usage("58M out of 1.4G");
    info_entry->set_disk_usage("1.2G out of 4.8G");
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

    info_entry->set_load("0.03 0.10 0.15");
    info_entry->set_memory_usage("37M out of 1.5G");
    info_entry->set_disk_usage("1.8G out of 6.3G");
    info_entry->set_current_release("Ubuntu 16.04.3 LTS");
    info_entry->set_ipv4("10.21.124.56");

    info_entry = info_reply.add_info();
    info_entry->set_name("bombastic");
    info_entry->mutable_instance_status()->set_status(mp::InstanceStatus::STOPPED);
    info_entry->set_image_release("18.04 LTS");
    info_entry->set_id("ab5191cc172564e7cc0eafd397312a32598823e645279c820f0935393aead509");

    return info_reply;
}
}

TEST(TableFormatter, single_instance_list_output)
{
    auto list_reply = construct_single_instance_list_reply();

    auto expected_table_output = "Name                    State    IPv4             Release\n"
                                 "foo                     RUNNING  10.168.32.2      Ubuntu 16.04 LTS\n";

    mp::TableFormatter table_formatter;
    auto output = table_formatter.format(list_reply);

    EXPECT_THAT(output, Eq(expected_table_output));
}

TEST(TableFormatter, multiple_instance_list_output)
{
    auto list_reply = construct_multiple_instances_list_reply();

    auto expected_table_output = "Name                    State    IPv4             Release\n"
                                 "bogus-instance          RUNNING  10.21.124.56     Ubuntu 16.04 LTS\n"
                                 "bombastic               STOPPED  --               Ubuntu 18.04 LTS\n";

    mp::TableFormatter table_formatter;
    auto output = table_formatter.format(list_reply);

    EXPECT_THAT(output, Eq(expected_table_output));
}

TEST(TableFormatter, no_instances_list_output)
{
    mp::ListReply list_reply;

    auto expected_table_output = "No instances found.\n";

    mp::TableFormatter table_formatter;
    auto output = table_formatter.format(list_reply);

    EXPECT_THAT(output, Eq(expected_table_output));
}

TEST(TableFormatter, single_instance_info_output)
{
    auto info_reply = construct_single_instance_info_reply();

    auto expected_table_output = "Name:           foo\n"
                                 "State:          RUNNING\n"
                                 "IPv4:           10.168.32.2\n"
                                 "Release:        Ubuntu 16.04.3 LTS\n"
                                 "Image hash:     1797c5c82016 (Ubuntu 16.04 LTS)\n"
                                 "Load:           0.00 0.00 0.00\n"
                                 "Disk usage:     1.2G out of 4.8G\n"
                                 "Memory usage:   58M out of 1.4G\n"
                                 "Mounts:         /home/user/foo      => foo\n"
                                 "                /home/user/test_dir => test_dir\n";

    mp::TableFormatter table_formatter;
    auto output = table_formatter.format(info_reply);

    EXPECT_THAT(output, Eq(expected_table_output));
}

TEST(TableFormatter, multiple_instances_info_output)
{
    auto info_reply = construct_multiple_instances_info_reply();

    auto expected_table_output = "Name:           bogus-instance\n"
                                 "State:          RUNNING\n"
                                 "IPv4:           10.21.124.56\n"
                                 "Release:        Ubuntu 16.04.3 LTS\n"
                                 "Image hash:     1797c5c82016 (Ubuntu 16.04 LTS)\n"
                                 "Load:           0.03 0.10 0.15\n"
                                 "Disk usage:     1.8G out of 6.3G\n"
                                 "Memory usage:   37M out of 1.5G\n"
                                 "Mounts:         /home/user/source => source\n\n"
                                 "Name:           bombastic\n"
                                 "State:          STOPPED\n"
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

TEST(TableFormatter, no_instances_info_output)
{
    mp::InfoReply info_reply;

    auto expected_table_output = "\n";

    mp::TableFormatter table_formatter;
    auto output = table_formatter.format(info_reply);

    EXPECT_THAT(output, Eq(expected_table_output));
}

TEST(JsonFormatter, single_instance_list_output)
{
    auto list_reply = construct_single_instance_list_reply();

    auto expected_json_output = "{\n"
                                "    \"list\": [\n"
                                "        {\n"
                                "            \"ipv4\": [\n"
                                "                \"10.168.32.2\"\n"
                                "            ],\n"
                                "            \"name\": \"foo\",\n"
                                "            \"state\": \"RUNNING\"\n"
                                "        }\n"
                                "    ]\n"
                                "}\n";

    mp::JsonFormatter json_formatter;
    auto output = json_formatter.format(list_reply);

    EXPECT_THAT(output, Eq(expected_json_output));
}

TEST(JsonFormatter, multiple_instances_list_output)
{
    auto list_reply = construct_multiple_instances_list_reply();

    auto expected_json_output = "{\n"
                                "    \"list\": [\n"
                                "        {\n"
                                "            \"ipv4\": [\n"
                                "                \"10.21.124.56\"\n"
                                "            ],\n"
                                "            \"name\": \"bogus-instance\",\n"
                                "            \"state\": \"RUNNING\"\n"
                                "        },\n"
                                "        {\n"
                                "            \"ipv4\": [\n"
                                "            ],\n"
                                "            \"name\": \"bombastic\",\n"
                                "            \"state\": \"STOPPED\"\n"
                                "        }\n"
                                "    ]\n"
                                "}\n";

    mp::JsonFormatter json_formatter;
    auto output = json_formatter.format(list_reply);

    EXPECT_THAT(output, Eq(expected_json_output));
}

TEST(JsonFormatter, no_instances_list_output)
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

TEST(JsonFormatter, single_instance_info_output)
{
    auto info_reply = construct_single_instance_info_reply();

    auto expected_json_output = "{\n"
                                "    \"errors\": [\n"
                                "    ],\n"
                                "    \"info\": {\n"
                                "        \"foo\": {\n"
                                "            \"image_hash\": \"1797c5c82016\",\n"
                                "            \"ipv4\": [\n"
                                "                \"10.168.32.2\"\n"
                                "            ],\n"
                                "            \"mounts\": {\n"
                                "                \"foo\": {\n"
                                "                    \"gid_mappings\": [\n"
                                "                    ],\n"
                                "                    \"source_path\": \"/home/user/foo\",\n"
                                "                    \"uid_mappings\": [\n"
                                "                    ]\n"
                                "                },\n"
                                "                \"test_dir\": {\n"
                                "                    \"gid_mappings\": [\n"
                                "                    ],\n"
                                "                    \"source_path\": \"/home/user/test_dir\",\n"
                                "                    \"uid_mappings\": [\n"
                                "                    ]\n"
                                "                }\n"
                                "            },\n"
                                "            \"state\": \"RUNNING\"\n"
                                "        }\n"
                                "    }\n"
                                "}\n";

    mp::JsonFormatter json_formatter;
    auto output = json_formatter.format(info_reply);

    EXPECT_THAT(output, Eq(expected_json_output));
}

TEST(JsonFormatter, multiple_instances_info_output)
{
    auto info_reply = construct_multiple_instances_info_reply();

    auto expected_json_output = "{\n"
                                "    \"errors\": [\n"
                                "    ],\n"
                                "    \"info\": {\n"
                                "        \"bogus-instance\": {\n"
                                "            \"image_hash\": \"1797c5c82016\",\n"
                                "            \"ipv4\": [\n"
                                "                \"10.21.124.56\"\n"
                                "            ],\n"
                                "            \"mounts\": {\n"
                                "                \"source\": {\n"
                                "                    \"gid_mappings\": [\n"
                                "                    ],\n"
                                "                    \"source_path\": \"/home/user/source\",\n"
                                "                    \"uid_mappings\": [\n"
                                "                    ]\n"
                                "                }\n"
                                "            },\n"
                                "            \"state\": \"RUNNING\"\n"
                                "        },\n"
                                "        \"bombastic\": {\n"
                                "            \"image_hash\": \"ab5191cc1725\",\n"
                                "            \"ipv4\": [\n"
                                "            ],\n"
                                "            \"mounts\": {\n"
                                "            },\n"
                                "            \"state\": \"STOPPED\"\n"
                                "        }\n"
                                "    }\n"
                                "}\n";

    mp::JsonFormatter json_formatter;
    auto output = json_formatter.format(info_reply);

    EXPECT_THAT(output, Eq(expected_json_output));
}

TEST(JsonFormatter, no_instances_info_output)
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
