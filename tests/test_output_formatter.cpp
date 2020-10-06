/*
 * Copyright (C) 2018-2020 Canonical, Ltd.
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

#include "mock_settings.h"

#include <multipass/cli/csv_formatter.h>
#include <multipass/cli/json_formatter.h>
#include <multipass/cli/table_formatter.h>
#include <multipass/cli/yaml_formatter.h>
#include <multipass/constants.h>
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/settings.h>

#include <multipass/format.h>

#include <gmock/gmock.h>

#include <locale>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
auto petenv_name()
{
    return MP_SETTINGS.get(mp::petenv_key).toStdString();
}

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

auto construct_unsorted_list_reply()
{
    mp::ListReply list_reply;

    auto list_entry = list_reply.add_instances();
    list_entry->set_name("trusty-190611-1542");
    list_entry->mutable_instance_status()->set_status(mp::InstanceStatus::RUNNING);
    list_entry->set_current_release("N/A");

    list_entry = list_reply.add_instances();
    list_entry->set_name("trusty-190611-1535");
    list_entry->mutable_instance_status()->set_status(mp::InstanceStatus::STOPPED);
    list_entry->set_current_release("N/A");

    list_entry = list_reply.add_instances();
    list_entry->set_name("trusty-190611-1539");
    list_entry->mutable_instance_status()->set_status(mp::InstanceStatus::SUSPENDED);
    list_entry->set_current_release("N/A");

    list_entry = list_reply.add_instances();
    list_entry->set_name("trusty-190611-1529");
    list_entry->mutable_instance_status()->set_status(mp::InstanceStatus::DELETED);
    list_entry->set_current_release("N/A");

    return list_reply;
}

auto add_petenv_to_reply(mp::ListReply& reply)
{
    auto instance = reply.add_instances();
    instance->set_name(petenv_name());
    instance->mutable_instance_status()->set_status(mp::InstanceStatus::DELETED);
    instance->set_current_release("Not Available");
}

auto construct_one_short_line_list_networks_reply()
{
    mp::ListNetworksReply list_networks_reply;

    // This reply will have strings shorter than the column headers, to test formatting.
    auto list_networks_entry = list_networks_reply.add_interfaces();
    list_networks_entry->set_name("en0");
    list_networks_entry->set_type("eth");
    list_networks_entry->set_description("Ether");

    return list_networks_reply;
}

auto construct_one_long_line_list_networks_reply()
{
    mp::ListNetworksReply list_networks_reply;

    // This reply will have strings shorter than the column headers, to test formatting.
    auto list_networks_entry = list_networks_reply.add_interfaces();
    list_networks_entry->set_name("enp3s0");
    list_networks_entry->set_type("ethernet");
    list_networks_entry->set_description("Amazingly fast and robust ethernet adapter");

    return list_networks_reply;
}

auto construct_multiple_lines_list_networks_reply()
{
    mp::ListNetworksReply list_networks_reply = construct_one_short_line_list_networks_reply();

    // This reply will have strings shorter than the column headers, to test formatting.
    auto list_networks_entry = list_networks_reply.add_interfaces();
    list_networks_entry->set_name("wlx0123456789ab");
    list_networks_entry->set_type("wifi");
    list_networks_entry->set_description("Wireless");

    return list_networks_reply;
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

auto add_petenv_to_reply(mp::InfoReply& reply)
{
    auto entry = reply.add_info();
    entry->set_name(petenv_name());
    entry->mutable_instance_status()->set_status(mp::InstanceStatus::SUSPENDED);
    entry->set_image_release("18.10");
    entry->set_id("1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd");
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

auto construct_find_multiple_reply()
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

auto construct_find_multiple_reply_duplicate_image()
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

protected:
    mpt::MockSettings& mock_settings = mpt::MockSettings::mock_instance(); /* although this is shared, expectations are
                                                                              reset at the end of each test */

private:
    std::locale saved_locale;
};

typedef std::tuple<const mp::Formatter*, const ::google::protobuf::Message*, std::string /* output */,
                   std::string /* test name */>
    FormatterParamType;

struct FormatterSuite : public LocaleSettingTest, public WithParamInterface<FormatterParamType>
{
};

auto print_param_name(const testing::TestParamInfo<FormatterSuite::ParamType>& info)
{
    return std::get<3>(info.param);
}

struct PetenvFormatterSuite : public LocaleSettingTest,
                              public WithParamInterface<std::tuple<QString, bool, FormatterParamType>>
{
};

auto print_petenv_param_name(const testing::TestParamInfo<PetenvFormatterSuite::ParamType>& info)
{
    const auto param_name = std::get<3>(std::get<2>(info.param));
    const auto petenv_name = std::get<0>(info.param);
    const auto prepend = std::get<1>(info.param);
    return fmt::format("{}_{}_{}", param_name, petenv_name.isEmpty() ? "default" : petenv_name,
                       prepend ? "prepend" : "append");
}

const mp::TableFormatter table_formatter;
const mp::JsonFormatter json_formatter;
const mp::CSVFormatter csv_formatter;
const mp::YamlFormatter yaml_formatter;

const auto empty_list_reply = mp::ListReply();
const auto single_instance_list_reply = construct_single_instance_list_reply();
const auto multiple_instances_list_reply = construct_multiple_instances_list_reply();
const auto unsorted_list_reply = construct_unsorted_list_reply();

const auto empty_list_networks_reply = mp::ListNetworksReply();
const auto one_short_line_list_networks_reply = construct_one_short_line_list_networks_reply();
const auto one_long_line_list_networks_reply = construct_one_long_line_list_networks_reply();
const auto multiple_lines_list_networks_reply = construct_multiple_lines_list_networks_reply();

const auto empty_info_reply = mp::InfoReply();
const auto single_instance_info_reply = construct_single_instance_info_reply();
const auto multiple_instances_info_reply = construct_multiple_instances_info_reply();

const std::vector<FormatterParamType> orderable_list_info_formatter_outputs{
    {&table_formatter, &empty_list_reply, "No instances found.\n", "table_list_empty"},
    {&table_formatter, &single_instance_list_reply,
     "Name                    State             IPv4             Image\n"
     "foo                     Running           10.168.32.2      Ubuntu 16.04 LTS\n",
     "table_list_single"},

    {&table_formatter, &multiple_instances_list_reply,
     "Name                    State             IPv4             Image\n"
     "bogus-instance          Running           10.21.124.56     Ubuntu 16.04 LTS\n"
     "bombastic               Stopped           --               Ubuntu 18.04 LTS\n",
     "table_list_multiple"},

    {&table_formatter, &unsorted_list_reply,
     "Name                    State             IPv4             Image\n"
     "trusty-190611-1529      Deleted           --               Ubuntu N/A\n"
     "trusty-190611-1535      Stopped           --               Ubuntu N/A\n"
     "trusty-190611-1539      Suspended         --               Ubuntu N/A\n"
     "trusty-190611-1542      Running           --               Ubuntu N/A\n",
     "table_list_unsorted"},

    {&table_formatter, &empty_info_reply, "\n", "table_info_empty"},
    {&table_formatter, &single_instance_info_reply,
     "Name:           foo\n"
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
     "                    GID map: 1000:1000\n",
     "table_info_single"},
    {&table_formatter, &multiple_instances_info_reply,
     "Name:           bogus-instance\n"
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
     "Memory usage:   --\n",
     "table_info_multiple"},

    {&csv_formatter, &empty_list_reply, "Name,State,IPv4,IPv6,Release\n", "csv_list_empty"},
    {&csv_formatter, &single_instance_list_reply,
     "Name,State,IPv4,IPv6,Release\n"
     "foo,Running,10.168.32.2,,16.04 LTS\n",
     "csv_list_single"},
    {&csv_formatter, &multiple_instances_list_reply,
     "Name,State,IPv4,IPv6,Release\n"
     "bogus-instance,Running,10.21.124.56,,16.04 LTS\n"
     "bombastic,Stopped,,,18.04 LTS\n",
     "csv_list_multiple"},
    {&csv_formatter, &unsorted_list_reply,
     "Name,State,IPv4,IPv6,Release\n"
     "trusty-190611-1529,Deleted,,,N/A\n"
     "trusty-190611-1535,Stopped,,,N/A\n"
     "trusty-190611-1539,Suspended,,,N/A\n"
     "trusty-190611-1542,Running,,,N/A\n",
     "csv_list_unsorted"},

    {&csv_formatter, &empty_info_reply,
     "Name,State,Ipv4,Ipv6,Release,Image hash,Image release,Load,Disk usage,Disk total,Memory "
     "usage,Memory total,Mounts\n",
     "csv_info_empty"},
    {&csv_formatter, &single_instance_info_reply,
     "Name,State,Ipv4,Ipv6,Release,Image hash,Image release,Load,Disk usage,Disk total,Memory "
     "usage,Memory total,Mounts\nfoo,Running,10.168.32.2,,Ubuntu 16.04.3 "
     "LTS,1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac,16.04 LTS,0.45 0.51 "
     "0.15,1288490188,5153960756,60817408,1503238554,/home/user/foo => foo;/home/user/test_dir "
     "=> test_dir;\n",
     "csv_info_single"},
    {&csv_formatter, &multiple_instances_info_reply,
     "Name,State,Ipv4,Ipv6,Release,Image hash,Image release,Load,Disk usage,Disk total,Memory "
     "usage,Memory total,Mounts\nbogus-instance,Running,10.21.124.56,,Ubuntu 16.04.3 "
     "LTS,1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac,16.04 LTS,0.03 0.10 "
     "0.15,1932735284,6764573492,38797312,1610612736,/home/user/source => "
     "source;\nbombastic,Stopped,,,,"
     "ab5191cc172564e7cc0eafd397312a32598823e645279c820f0935393aead509,18.04 LTS,,,,,,\n",
     "csv_info_multiple"},

    {&yaml_formatter, &empty_list_reply, "\n", "yaml_list_empty"},
    {&yaml_formatter, &single_instance_list_reply,
     "foo:\n"
     "  - state: Running\n"
     "    ipv4:\n"
     "      - 10.168.32.2\n"
     "    release: 16.04 LTS\n",
     "yaml_list_single"},
    {&yaml_formatter, &multiple_instances_list_reply,
     "bogus-instance:\n"
     "  - state: Running\n"
     "    ipv4:\n"
     "      - 10.21.124.56\n"
     "    release: 16.04 LTS\n"
     "bombastic:\n"
     "  - state: Stopped\n"
     "    ipv4:\n"
     "      - \"\"\n"
     "    release: 18.04 LTS\n",
     "yaml_list_multiple"},
    {&yaml_formatter, &unsorted_list_reply,
     "trusty-190611-1529:\n"
     "  - state: Deleted\n"
     "    ipv4:\n"
     "      - \"\"\n"
     "    release: N/A\n"
     "trusty-190611-1535:\n"
     "  - state: Stopped\n"
     "    ipv4:\n"
     "      - \"\"\n"
     "    release: N/A\n"
     "trusty-190611-1539:\n"
     "  - state: Suspended\n"
     "    ipv4:\n"
     "      - \"\"\n"
     "    release: N/A\n"
     "trusty-190611-1542:\n"
     "  - state: Running\n"
     "    ipv4:\n"
     "      - \"\"\n"
     "    release: N/A\n",
     "yaml_list_unsorted"},
    {&yaml_formatter, &empty_info_reply, "errors:\n  - ~\n", "yaml_info_empty"},

    {&yaml_formatter, &single_instance_info_reply,
     "errors:\n"
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
     "        source_path: /home/user/test_dir\n",
     "yaml_info_single"},
    {&yaml_formatter, &multiple_instances_info_reply,
     "errors:\n"
     "  - ~\n"
     "bogus-instance:\n"
     "  - state: Running\n"
     "    image_hash: 1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac\n"
     "    image_release: 16.04 LTS\n"
     "    release: Ubuntu 16.04.3 LTS\n"
     "    load:\n"
     "      - 0.03\n"
     "      - 0.10\n"
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
     "    mounts: ~\n",
     "yaml_info_multiple"}};

const std::vector<FormatterParamType> non_orderable_list_info_formatter_outputs{
    {&json_formatter, &empty_list_reply,
     "{\n"
     "    \"list\": [\n"
     "    ]\n"
     "}\n",
     "json_list_empty"},
    {&json_formatter, &single_instance_list_reply,
     "{\n"
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
     "}\n",
     "json_list_single"},
    {&json_formatter, &multiple_instances_list_reply,
     "{\n"
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
     "}\n",
     "json_list_multiple"},
    {&json_formatter, &empty_info_reply,
     "{\n"
     "    \"errors\": [\n"
     "    ],\n"
     "    \"info\": {\n"
     "    }\n"
     "}\n",
     "json_info_empty"},
    {&json_formatter, &single_instance_info_reply,
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
     "}\n",
     "json_info_single"},
    {&json_formatter, &multiple_instances_info_reply,
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
     "}\n",
     "json_info_multiple"}};

const std::vector<FormatterParamType> non_orderable_list_networks_formatter_outputs{
    {&table_formatter, &empty_list_networks_reply, "No network interfaces found.\n", "table_list_networks_empty"},
    {&table_formatter, &one_short_line_list_networks_reply,
     "Name Type Description\n"
     "en0  eth  Ether\n",
     "table_list_networks_one_short_line"},
    {&table_formatter, &one_long_line_list_networks_reply,
     "Name   Type     Description\n"
     "enp3s0 ethernet Amazingly fast and robust ethernet adapter\n",
     "table_list_networks_one_long_line"},
    {&table_formatter, &multiple_lines_list_networks_reply,
     "Name            Type Description\n"
     "en0             eth  Ether\n"
     "wlx0123456789ab wifi Wireless\n",
     "table_list_networks_multiple_lines"},

    {&csv_formatter, &empty_list_networks_reply, "Name,Type,Description\n", "csv_list_networks_empty"},
    {&csv_formatter, &one_short_line_list_networks_reply,
     "Name,Type,Description\n"
     "en0,eth,\"Ether\"\n",
     "csv_list_networks_one_short_line"},
    {&csv_formatter, &one_long_line_list_networks_reply,
     "Name,Type,Description\n"
     "enp3s0,ethernet,\"Amazingly fast and robust ethernet adapter\"\n",
     "csv_list_networks_one_long_line"},
    {&csv_formatter, &multiple_lines_list_networks_reply,
     "Name,Type,Description\n"
     "en0,eth,\"Ether\"\n"
     "wlx0123456789ab,wifi,\"Wireless\"\n",
     "csv_list_networks_multiple_lines"},

    {&yaml_formatter, &empty_list_networks_reply, "\n", "yaml_list_networks_empty"},
    {&yaml_formatter, &one_short_line_list_networks_reply,
     "en0:\n"
     "  - type: eth\n"
     "    description: Ether\n",
     "yaml_list_networks_one_short_line"},
    {&yaml_formatter, &one_long_line_list_networks_reply,
     "enp3s0:\n"
     "  - type: ethernet\n"
     "    description: Amazingly fast and robust ethernet adapter\n",
     "yaml_list_networks_one_long_line"},
    {&yaml_formatter, &multiple_lines_list_networks_reply,
     "en0:\n"
     "  - type: eth\n"
     "    description: Ether\n"
     "wlx0123456789ab:\n"
     "  - type: wifi\n"
     "    description: Wireless\n",
     "yaml_list_networks_multiple_lines"},

    {&json_formatter, &empty_list_networks_reply,
     "{\n"
     "    \"list\": [\n"
     "    ]\n"
     "}\n",
     "json_list_networks_empty"},
    {&json_formatter, &one_short_line_list_networks_reply,
     "{\n"
     "    \"list\": [\n"
     "        {\n"
     "            \"description\": \"Ether\",\n"
     "            \"name\": \"en0\",\n"
     "            \"type\": \"eth\"\n"
     "        }\n"
     "    ]\n"
     "}\n",
     "json_list_networks_one_short_line"},
    {&json_formatter, &one_long_line_list_networks_reply,
     "{\n"
     "    \"list\": [\n"
     "        {\n"
     "            \"description\": \"Amazingly fast and robust ethernet adapter\",\n"
     "            \"name\": \"enp3s0\",\n"
     "            \"type\": \"ethernet\"\n"
     "        }\n"
     "    ]\n"
     "}\n",
     "json_list_networks_one_long_line"},
    {&json_formatter, &multiple_lines_list_networks_reply,
     "{\n"
     "    \"list\": [\n"
     "        {\n"
     "            \"description\": \"Ether\",\n"
     "            \"name\": \"en0\",\n"
     "            \"type\": \"eth\"\n"
     "        },\n"
     "        {\n"
     "            \"description\": \"Wireless\",\n"
     "            \"name\": \"wlx0123456789ab\",\n"
     "            \"type\": \"wifi\"\n"
     "        }\n"
     "    ]\n"
     "}\n",
     "json_list_networks_multiple_lines"}};

const auto empty_find_reply = mp::FindReply();
const auto find_one_reply = construct_find_one_reply();
const auto find_multiple_reply = construct_find_multiple_reply();
const auto find_one_reply_no_os = construct_find_one_reply_no_os();
const auto find_multiple_reply_duplicate_image = construct_find_multiple_reply_duplicate_image();

const std::vector<FormatterParamType> find_formatter_outputs{
    {&table_formatter, &empty_find_reply, "No images found.\n", "table_find_empty"},
    {&table_formatter, &find_one_reply,
     "Image                       Aliases           Version          Description\n"
     "ubuntu                                        20190516         Ubuntu 18.04 LTS\n",
     "table_find_one"},
    {&table_formatter, &find_multiple_reply,
     "Image                       Aliases           Version          Description\n"
     "lts                                           20190516         Ubuntu 18.04 LTS\n"
     "daily:19.10                 eoan,devel        20190516         Ubuntu 19.10\n",
     "table_find_multiple"},
    {&table_formatter, &find_one_reply_no_os,
     "Image                       Aliases           Version          Description\n"
     "snapcraft:core18                              20190520         Snapcraft builder for core18\n",
     "table_find_no_os"},
    {&table_formatter, &find_multiple_reply_duplicate_image,
     "Image                       Aliases           Version          Description\n"
     "core18                                        20190520         Ubuntu Core 18\n"
     "snapcraft:core18                              20190520         Snapcraft builder for core18\n",
     "table_find_multiple_duplicate_image"},
    {&json_formatter, &empty_find_reply,
     "{\n"
     "    \"errors\": [\n"
     "    ],\n"
     "    \"images\": {\n"
     "    }\n"
     "}\n",
     "json_find_empty"},
    {&json_formatter, &find_one_reply,
     "{\n"
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
     "}\n",
     "json_find_one"},
    {&json_formatter, &find_multiple_reply,
     "{\n"
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
     "}\n",
     "json_find_multiple"},
    {&json_formatter, &find_multiple_reply_duplicate_image,
     "{\n"
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
     "}\n",
     "json_find_multiple_duplicate_image"},
    {&csv_formatter, &empty_find_reply, "Image,Remote,Aliases,OS,Release,Version\n", "csv_find_empty"},
    {&csv_formatter, &find_one_reply,
     "Image,Remote,Aliases,OS,Release,Version\n"
     "ubuntu,,,Ubuntu,18.04 LTS,20190516\n",
     "csv_find_one"},
    {&csv_formatter, &find_multiple_reply,
     "Image,Remote,Aliases,OS,Release,Version\n"
     "lts,,,Ubuntu,18.04 LTS,20190516\n"
     "daily:19.10,daily,eoan;devel,Ubuntu,19.10,20190516\n",
     "csv_find_multiple"},
    {&csv_formatter, &find_multiple_reply_duplicate_image,
     "Image,Remote,Aliases,OS,Release,Version\n"
     "core18,,,Ubuntu,Core 18,20190520\n"
     "snapcraft:core18,snapcraft,,,Snapcraft builder for core18,20190520\n",
     "csv_find_multiple_duplicate_image"},
    {&yaml_formatter, &empty_find_reply,
     "errors:\n"
     "  []\n"
     "images:\n"
     "  {}\n",
     "yaml_find_empty"},
    {&yaml_formatter, &find_one_reply,
     "errors:\n"
     "  []\n"
     "images:\n"
     "  ubuntu:\n"
     "    aliases:\n"
     "      []\n"
     "    os: Ubuntu\n"
     "    release: 18.04 LTS\n"
     "    version: 20190516\n"
     "    remote: \"\"\n",
     "yaml_find_one"},
    {&yaml_formatter, &find_multiple_reply,
     "errors:\n"
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
     "    remote: daily\n",
     "yaml_find_multiple"},
    {&yaml_formatter, &find_multiple_reply_duplicate_image,
     "errors:\n"
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
     "    remote: snapcraft\n",
     "yaml_find_multiple_duplicate_image"}};
} // namespace

TEST_P(FormatterSuite, properly_formats_output)
{
    const auto& [formatter, reply, expected_output, test_name] = GetParam();
    Q_UNUSED(test_name); // gcc 7.4 can't do [[maybe_unused]] for structured bindings

    std::string output;

    if (auto input = dynamic_cast<const mp::ListReply*>(reply))
        output = formatter->format(*input);
    else if (auto input = dynamic_cast<const mp::ListNetworksReply*>(reply))
        output = formatter->format(*input);
    else if (auto input = dynamic_cast<const mp::InfoReply*>(reply))
        output = formatter->format(*input);
    else if (auto input = dynamic_cast<const mp::FindReply*>(reply))
        output = formatter->format(*input);
    else
        FAIL() << "Not a supported reply type.";

    EXPECT_EQ(output, expected_output);
}

INSTANTIATE_TEST_SUITE_P(OrderableListInfoOutputFormatter, FormatterSuite,
                         ValuesIn(orderable_list_info_formatter_outputs), print_param_name);
INSTANTIATE_TEST_SUITE_P(NonOrderableListInfoOutputFormatter, FormatterSuite,
                         ValuesIn(non_orderable_list_info_formatter_outputs), print_param_name);
INSTANTIATE_TEST_SUITE_P(FindOutputFormatter, FormatterSuite, ValuesIn(find_formatter_outputs), print_param_name);
INSTANTIATE_TEST_SUITE_P(NonOrderableListNetworksOutputFormatter, FormatterSuite,
                         ValuesIn(non_orderable_list_networks_formatter_outputs), print_param_name);

#if GTEST_HAS_POSIX_RE
TEST_P(PetenvFormatterSuite, pet_env_first_in_output)
{
    const auto& [petenv_nname, prepend, param] = GetParam();
    const auto& [formatter, reply, expected_output, test_name] = param;
    Q_UNUSED(expected_output);
    Q_UNUSED(test_name); // gcc 7.4 can't do [[maybe_unused]] for structured bindings

    if (!petenv_nname.isEmpty())
    {
        EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(petenv_nname));
    }

    std::string output, regex;

    if (auto input = dynamic_cast<const mp::ListReply*>(reply))
    {
        mp::ListReply reply_copy;
        if (prepend)
        {
            add_petenv_to_reply(reply_copy);
            reply_copy.MergeFrom(*input);
        }
        else
        {
            reply_copy.CopyFrom(*input);
            add_petenv_to_reply(reply_copy);
        }
        output = formatter->format(reply_copy);

        if (dynamic_cast<const mp::TableFormatter*>(formatter))
            regex = fmt::format("Name[[:print:]]*\n{}[[:space:]]+.*", petenv_name());
        else if (dynamic_cast<const mp::CSVFormatter*>(formatter))
            regex = fmt::format("Name[[:print:]]*\n{},.*", petenv_name());
        else if (dynamic_cast<const mp::YamlFormatter*>(formatter))
            regex = fmt::format("{}:.*", petenv_name());
        else
            FAIL() << "Not a supported formatter.";
    }
    else if (auto input = dynamic_cast<const mp::InfoReply*>(reply))
    {
        mp::InfoReply reply_copy;
        if (prepend)
        {
            add_petenv_to_reply(reply_copy);
            reply_copy.MergeFrom(*input);
        }
        else
        {
            reply_copy.CopyFrom(*input);
            add_petenv_to_reply(reply_copy);
        }
        output = formatter->format(reply_copy);

        if (dynamic_cast<const mp::TableFormatter*>(formatter))
            regex = fmt::format("Name:[[:space:]]+{}.+", petenv_name());
        else if (dynamic_cast<const mp::CSVFormatter*>(formatter))
            regex = fmt::format("Name[[:print:]]*\n{},.*", petenv_name());
        else if (dynamic_cast<const mp::YamlFormatter*>(formatter))
            regex = fmt::format("(errors:[[:space:]]+-[[:space:]]+~[[:space:]]+)?{}:.*", petenv_name());
        else
            FAIL() << "Not a supported formatter.";
    }
    else
        FAIL() << "Not a supported reply type.";

    EXPECT_THAT(output, MatchesRegex(regex));
}

INSTANTIATE_TEST_SUITE_P(PetenvOutputFormatter, PetenvFormatterSuite,
                         Combine(Values(QStringLiteral(), QStringLiteral("aaa"),
                                        QStringLiteral("zzz")) /* primary name */,
                                 Bool() /* prepend or append */, ValuesIn(orderable_list_info_formatter_outputs)),
                         print_petenv_param_name);
#endif
