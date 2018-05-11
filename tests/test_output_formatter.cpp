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

#include <multipass/cli/csv_formatter.h>
#include <multipass/cli/json_formatter.h>
#include <multipass/cli/table_formatter.h>
#include <multipass/cli/yaml_formatter.h>
#include <multipass/rpc/multipass.grpc.pb.h>

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

    auto expected_table_output = "Name                    State       IPv4             Release\n"
                                 "foo                     RUNNING     10.168.32.2      Ubuntu 16.04 LTS\n";

    mp::TableFormatter table_formatter;
    auto output = table_formatter.format(list_reply);

    EXPECT_THAT(output, Eq(expected_table_output));
}

TEST_F(TableFormatter, multiple_instance_list_output)
{
    auto list_reply = construct_multiple_instances_list_reply();

    auto expected_table_output = "Name                    State       IPv4             Release\n"
                                 "bogus-instance          RUNNING     10.21.124.56     Ubuntu 16.04 LTS\n"
                                 "bombastic               STOPPED     --               Ubuntu 18.04 LTS\n";

    mp::TableFormatter table_formatter;
    auto output = table_formatter.format(list_reply);

    EXPECT_THAT(output, Eq(expected_table_output));
}

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
                                 "State:          RUNNING\n"
                                 "IPv4:           10.168.32.2\n"
                                 "Release:        Ubuntu 16.04.3 LTS\n"
                                 "Image hash:     1797c5c82016 (Ubuntu 16.04 LTS)\n"
                                 "Load:           0.45 0.51 0.15\n"
                                 "Disk usage:     1.2G out of 4.8G\n"
                                 "Memory usage:   58.0M out of 1.4G\n"
                                 "Mounts:         /home/user/foo      => foo\n"
                                 "                /home/user/test_dir => test_dir\n";

    mp::TableFormatter table_formatter;
    auto output = table_formatter.format(info_reply);

    EXPECT_THAT(output, Eq(expected_table_output));
}

TEST_F(TableFormatter, multiple_instances_info_output)
{
    auto info_reply = construct_multiple_instances_info_reply();

    auto expected_table_output = "Name:           bogus-instance\n"
                                 "State:          RUNNING\n"
                                 "IPv4:           10.21.124.56\n"
                                 "Release:        Ubuntu 16.04.3 LTS\n"
                                 "Image hash:     1797c5c82016 (Ubuntu 16.04 LTS)\n"
                                 "Load:           0.03 0.10 0.15\n"
                                 "Disk usage:     1.8G out of 6.3G\n"
                                 "Memory usage:   37.0M out of 1.5G\n"
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

TEST_F(TableFormatter, no_instances_info_output)
{
    mp::InfoReply info_reply;

    auto expected_table_output = "\n";

    mp::TableFormatter table_formatter;
    auto output = table_formatter.format(info_reply);

    EXPECT_THAT(output, Eq(expected_table_output));
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
                                "            \"state\": \"RUNNING\"\n"
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
                                "            \"state\": \"RUNNING\"\n"
                                "        },\n"
                                "        {\n"
                                "            \"ipv4\": [\n"
                                "            ],\n"
                                "            \"name\": \"bombastic\",\n"
                                "            \"release\": \"18.04 LTS\",\n"
                                "            \"state\": \"STOPPED\"\n"
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
        "            \"release\": \"Ubuntu 16.04.3 LTS\",\n"
        "            \"state\": \"RUNNING\"\n"
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
        "                    ],\n"
        "                    \"source_path\": \"/home/user/source\",\n"
        "                    \"uid_mappings\": [\n"
        "                    ]\n"
        "                }\n"
        "            },\n"
        "            \"release\": \"Ubuntu 16.04.3 LTS\",\n"
        "            \"state\": \"RUNNING\"\n"
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
        "            \"state\": \"STOPPED\"\n"
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

TEST_F(CSVFormatter, single_instance_list_output)
{
    auto list_reply = construct_single_instance_list_reply();

    auto expected_output = "Name,State,IPv4,IPv6,Release\n"
                           "foo,RUNNING,10.168.32.2,,16.04 LTS\n";

    mp::CSVFormatter csv_formatter;
    auto output = csv_formatter.format(list_reply);

    EXPECT_THAT(output, Eq(expected_output));
}

TEST_F(CSVFormatter, multiple_instance_list_output)
{
    auto list_reply = construct_multiple_instances_list_reply();

    auto expected_output = "Name,State,IPv4,IPv6,Release\n"
                           "bogus-instance,RUNNING,10.21.124.56,,16.04 LTS\n"
                           "bombastic,STOPPED,,,18.04 LTS\n";

    mp::CSVFormatter csv_formatter;
    auto output = csv_formatter.format(list_reply);

    EXPECT_THAT(output, Eq(expected_output));
}

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
                           "usage,Memory total,Mounts\nfoo,RUNNING,10.168.32.2,,Ubuntu 16.04.3 "
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
                           "usage,Memory total,Mounts\nbogus-instance,RUNNING,10.21.124.56,,Ubuntu 16.04.3 "
                           "LTS,1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac,16.04 LTS,0.03 0.10 "
                           "0.15,1932735284,6764573492,38797312,1610612736,/home/user/source => "
                           "source;\nbombastic,STOPPED,,,,"
                           "ab5191cc172564e7cc0eafd397312a32598823e645279c820f0935393aead509,18.04 LTS,,,,,,\n";

    mp::CSVFormatter csv_formatter;
    auto output = csv_formatter.format(info_reply);

    EXPECT_THAT(output, Eq(expected_output));
}

TEST_F(CSVFormatter, no_instances_info_output)
{
    mp::InfoReply info_reply;

    auto expected_output = "Name,State,Ipv4,Ipv6,Release,Image hash,Image release,Load,Disk usage,Disk total,Memory "
                           "usage,Memory total,Mounts\n";

    mp::CSVFormatter csv_formatter;
    auto output = csv_formatter.format(info_reply);

    EXPECT_THAT(output, Eq(expected_output));
}

TEST_F(YamlFormatter, single_instance_list_output)
{
    auto list_reply = construct_single_instance_list_reply();

    auto expected_output = "foo:\n"
                           "  - state: RUNNING\n"
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
                           "  - state: RUNNING\n"
                           "    ipv4:\n"
                           "      - 10.21.124.56\n"
                           "    release: 16.04 LTS\n"
                           "bombastic:\n"
                           "  - state: STOPPED\n"
                           "    ipv4:\n"
                           "      - \"\"\n"
                           "    release: 18.04 LTS\n";

    mp::YamlFormatter formatter;
    auto output = formatter.format(list_reply);

    EXPECT_THAT(output, Eq(expected_output));
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
                           "  - state: RUNNING\n"
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
                           "        - gid_mappings:\n"
                           "            - ~\n"
                           "          uid_mappings:\n"
                           "            - ~\n"
                           "          source_path: /home/user/foo\n"
                           "      test_dir:\n"
                           "        - gid_mappings:\n"
                           "            - ~\n"
                           "          uid_mappings:\n"
                           "            - ~\n"
                           "          source_path: /home/user/test_dir\n";

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
                           "  - state: RUNNING\n"
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
                           "        - gid_mappings:\n"
                           "            - ~\n"
                           "          uid_mappings:\n"
                           "            - ~\n"
                           "          source_path: /home/user/source\n"
                           "bombastic:\n"
                           "  - state: STOPPED\n"
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

TEST_F(YamlFormatter, no_instances_info_output)
{
    mp::InfoReply info_reply;

    auto expected_output = "errors:\n  - ~\n";

    mp::YamlFormatter formatter;
    auto output = formatter.format(info_reply);

    EXPECT_THAT(output, Eq(expected_output));
}
