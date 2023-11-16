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
#include "mock_format_utils.h"
#include "mock_settings.h"

#include <multipass/cli/csv_formatter.h>
#include <multipass/cli/json_formatter.h>
#include <multipass/cli/table_formatter.h>
#include <multipass/cli/yaml_formatter.h>
#include <multipass/constants.h>
#include <multipass/format.h>
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/settings/settings.h>

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

auto construct_empty_list_reply()
{
    mp::ListReply list_reply;
    list_reply.mutable_instance_list();
    return list_reply;
}

auto construct_empty_list_snapshot_reply()
{
    mp::ListReply list_reply;
    list_reply.mutable_snapshot_list();
    return list_reply;
}

auto construct_single_instance_list_reply()
{
    mp::ListReply list_reply;

    auto list_entry = list_reply.mutable_instance_list()->add_instances();
    list_entry->set_name("foo");
    list_entry->mutable_instance_status()->set_status(mp::InstanceStatus::RUNNING);
    list_entry->set_current_release("16.04 LTS");
    list_entry->add_ipv4("10.168.32.2");
    list_entry->add_ipv4("200.3.123.30");
    list_entry->add_ipv6("fdde:2681:7a2::4ca");
    list_entry->add_ipv6("fe80::1c3c:b703:d561:a00");

    return list_reply;
}

auto construct_multiple_instances_list_reply()
{
    mp::ListReply list_reply;

    auto list_entry = list_reply.mutable_instance_list()->add_instances();
    list_entry->set_name("bogus-instance");
    list_entry->mutable_instance_status()->set_status(mp::InstanceStatus::RUNNING);
    list_entry->set_current_release("16.04 LTS");
    list_entry->add_ipv4("10.21.124.56");

    list_entry = list_reply.mutable_instance_list()->add_instances();
    list_entry->set_name("bombastic");
    list_entry->mutable_instance_status()->set_status(mp::InstanceStatus::STOPPED);
    list_entry->set_current_release("18.04 LTS");

    return list_reply;
}

auto construct_unsorted_list_reply()
{
    mp::ListReply list_reply;

    auto list_entry = list_reply.mutable_instance_list()->add_instances();
    list_entry->set_name("trusty-190611-1542");
    list_entry->mutable_instance_status()->set_status(mp::InstanceStatus::RUNNING);
    list_entry->set_current_release("N/A");

    list_entry = list_reply.mutable_instance_list()->add_instances();
    list_entry->set_name("trusty-190611-1535");
    list_entry->mutable_instance_status()->set_status(mp::InstanceStatus::STOPPED);
    list_entry->set_current_release("N/A");

    list_entry = list_reply.mutable_instance_list()->add_instances();
    list_entry->set_name("trusty-190611-1539");
    list_entry->mutable_instance_status()->set_status(mp::InstanceStatus::SUSPENDED);
    list_entry->set_current_release("");

    list_entry = list_reply.mutable_instance_list()->add_instances();
    list_entry->set_name("trusty-190611-1529");
    list_entry->mutable_instance_status()->set_status(mp::InstanceStatus::DELETED);
    list_entry->set_current_release("");

    return list_reply;
}

auto construct_single_snapshot_list_reply()
{
    mp::ListReply list_reply;

    auto list_entry = list_reply.mutable_snapshot_list()->add_snapshots();
    list_entry->set_name("foo");

    auto fundamentals = list_entry->mutable_fundamentals();
    fundamentals->set_snapshot_name("snapshot1");
    fundamentals->set_comment("This is a sample comment");

    google::protobuf::Timestamp timestamp;
    timestamp.set_seconds(time(nullptr));
    timestamp.set_nanos(0);
    fundamentals->mutable_creation_timestamp()->CopyFrom(timestamp);

    return list_reply;
}

auto construct_multiple_snapshots_list_reply()
{
    mp::ListReply list_reply;

    auto list_entry = list_reply.mutable_snapshot_list()->add_snapshots();
    auto fundamentals = list_entry->mutable_fundamentals();
    google::protobuf::Timestamp timestamp;

    list_entry->set_name("prosperous-spadefish");
    fundamentals->set_snapshot_name("snapshot10");
    fundamentals->set_parent("snapshot2");
    timestamp.set_seconds(1672531200);
    fundamentals->mutable_creation_timestamp()->CopyFrom(timestamp);

    list_entry = list_reply.mutable_snapshot_list()->add_snapshots();
    fundamentals = list_entry->mutable_fundamentals();
    list_entry->set_name("hale-roller");
    fundamentals->set_snapshot_name("rolling");
    fundamentals->set_parent("pristine");
    fundamentals->set_comment("Loaded with stuff");
    timestamp.set_seconds(25425952800);
    fundamentals->mutable_creation_timestamp()->CopyFrom(timestamp);

    list_entry = list_reply.mutable_snapshot_list()->add_snapshots();
    fundamentals = list_entry->mutable_fundamentals();
    list_entry->set_name("hale-roller");
    fundamentals->set_snapshot_name("rocking");
    fundamentals->set_parent("pristine");
    fundamentals->set_comment("A very long comment that should be truncated by the table formatter");
    timestamp.set_seconds(2209234259);
    fundamentals->mutable_creation_timestamp()->CopyFrom(timestamp);

    list_entry = list_reply.mutable_snapshot_list()->add_snapshots();
    fundamentals = list_entry->mutable_fundamentals();
    list_entry->set_name("hale-roller");
    fundamentals->set_snapshot_name("pristine");
    fundamentals->set_comment("A first snapshot");
    timestamp.set_seconds(409298914);
    fundamentals->mutable_creation_timestamp()->CopyFrom(timestamp);

    list_entry = list_reply.mutable_snapshot_list()->add_snapshots();
    fundamentals = list_entry->mutable_fundamentals();
    list_entry->set_name("prosperous-spadefish");
    fundamentals->set_snapshot_name("snapshot2");
    fundamentals->set_comment("Before restoring snap1\nContains a newline that\r\nshould be truncated");
    timestamp.set_seconds(1671840000);
    fundamentals->mutable_creation_timestamp()->CopyFrom(timestamp);

    return list_reply;
}

auto add_petenv_to_reply(mp::ListReply& reply)
{
    if (reply.has_instance_list())
    {
        auto instance = reply.mutable_instance_list()->add_instances();
        instance->set_name(petenv_name());
        instance->mutable_instance_status()->set_status(mp::InstanceStatus::DELETED);
        instance->set_current_release("Not Available");
    }
    else
    {
        auto snapshot = reply.mutable_snapshot_list()->add_snapshots();
        snapshot->set_name(petenv_name());
        snapshot->mutable_fundamentals()->set_snapshot_name("snapshot1");
        snapshot->mutable_fundamentals()->set_comment("An exemplary comment");
    }
}

auto construct_one_short_line_networks_reply()
{
    mp::NetworksReply networks_reply;

    // This reply will have strings shorter than the column headers, to test formatting.
    auto networks_entry = networks_reply.add_interfaces();
    networks_entry->set_name("en0");
    networks_entry->set_type("eth");
    networks_entry->set_description("Ether");

    return networks_reply;
}

auto construct_one_long_line_networks_reply()
{
    mp::NetworksReply networks_reply;

    // This reply will have strings shorter than the column headers, to test formatting.
    auto networks_entry = networks_reply.add_interfaces();
    networks_entry->set_name("enp3s0");
    networks_entry->set_type("ethernet");
    networks_entry->set_description("Amazingly fast and robust ethernet adapter");

    return networks_reply;
}

auto construct_multiple_lines_networks_reply()
{
    mp::NetworksReply networks_reply = construct_one_short_line_networks_reply();

    // This reply will have strings shorter than the column headers, to test formatting.
    auto networks_entry = networks_reply.add_interfaces();
    networks_entry->set_name("wlx0123456789ab");
    networks_entry->set_type("wifi");
    networks_entry->set_description("Wireless");

    return networks_reply;
}

auto construct_empty_info_snapshot_reply()
{
    mp::InfoReply info_reply;
    info_reply.set_snapshots(true);
    return info_reply;
}

auto construct_single_instance_info_reply()
{
    mp::InfoReply info_reply;

    auto info_entry = info_reply.add_details();
    info_entry->set_name("foo");
    info_entry->mutable_instance_status()->set_status(mp::InstanceStatus::RUNNING);
    info_entry->mutable_instance_info()->set_image_release("16.04 LTS");
    info_entry->mutable_instance_info()->set_id("1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac");

    auto mount_info = info_entry->mutable_mount_info();
    mount_info->set_longest_path_len(19);

    auto mount_entry = mount_info->add_mount_paths();
    mount_entry->set_source_path("/home/user/foo");
    mount_entry->set_target_path("foo");

    auto uid_map_pair = mount_entry->mutable_mount_maps()->add_uid_mappings();
    uid_map_pair->set_host_id(1000);
    uid_map_pair->set_instance_id(1000);

    auto gid_map_pair = mount_entry->mutable_mount_maps()->add_gid_mappings();
    gid_map_pair->set_host_id(1000);
    gid_map_pair->set_instance_id(1000);

    mount_entry = mount_info->add_mount_paths();
    mount_entry->set_source_path("/home/user/test_dir");
    mount_entry->set_target_path("test_dir");

    uid_map_pair = mount_entry->mutable_mount_maps()->add_uid_mappings();
    uid_map_pair->set_host_id(1000);
    uid_map_pair->set_instance_id(1000);

    gid_map_pair = mount_entry->mutable_mount_maps()->add_gid_mappings();
    gid_map_pair->set_host_id(1000);
    gid_map_pair->set_instance_id(1000);

    info_entry->set_cpu_count("1");
    info_entry->mutable_instance_info()->set_load("0.45 0.51 0.15");
    info_entry->mutable_instance_info()->set_memory_usage("60817408");
    info_entry->set_memory_total("1503238554");
    info_entry->mutable_instance_info()->set_disk_usage("1288490188");
    info_entry->set_disk_total("5153960756");
    info_entry->mutable_instance_info()->set_current_release("Ubuntu 16.04.3 LTS");
    info_entry->mutable_instance_info()->add_ipv4("10.168.32.2");
    info_entry->mutable_instance_info()->add_ipv4("200.3.123.29");
    info_entry->mutable_instance_info()->add_ipv6("2001:67c:1562:8007::aac:423a");
    info_entry->mutable_instance_info()->add_ipv6("fd52:2ccf:f758:0:a342:79b5:e2ba:e05e");
    info_entry->mutable_instance_info()->set_num_snapshots(0);

    return info_reply;
}

auto construct_multiple_instances_info_reply()
{
    mp::InfoReply info_reply;

    auto info_entry = info_reply.add_details();
    info_entry->set_name("bogus-instance");
    info_entry->mutable_instance_status()->set_status(mp::InstanceStatus::RUNNING);
    info_entry->mutable_instance_info()->set_image_release("16.04 LTS");
    info_entry->mutable_instance_info()->set_id("1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac");

    auto mount_info = info_entry->mutable_mount_info();
    mount_info->set_longest_path_len(17);

    auto mount_entry = mount_info->add_mount_paths();
    mount_entry->set_source_path("/home/user/source");
    mount_entry->set_target_path("source");

    auto uid_map_pair = mount_entry->mutable_mount_maps()->add_uid_mappings();
    uid_map_pair->set_host_id(1000);
    uid_map_pair->set_instance_id(501);

    auto gid_map_pair = mount_entry->mutable_mount_maps()->add_gid_mappings();
    gid_map_pair->set_host_id(1000);
    gid_map_pair->set_instance_id(501);

    info_entry->set_cpu_count("4");
    info_entry->mutable_instance_info()->set_load("0.03 0.10 0.15");
    info_entry->mutable_instance_info()->set_memory_usage("38797312");
    info_entry->set_memory_total("1610612736");
    info_entry->mutable_instance_info()->set_disk_usage("1932735284");
    info_entry->set_disk_total("6764573492");
    info_entry->mutable_instance_info()->set_current_release("Ubuntu 16.04.3 LTS");
    info_entry->mutable_instance_info()->add_ipv4("10.21.124.56");
    info_entry->mutable_instance_info()->set_num_snapshots(1);

    info_entry = info_reply.add_details();
    info_entry->set_name("bombastic");
    info_entry->mutable_instance_status()->set_status(mp::InstanceStatus::STOPPED);
    info_entry->mutable_instance_info()->set_image_release("18.04 LTS");
    info_entry->mutable_instance_info()->set_id("ab5191cc172564e7cc0eafd397312a32598823e645279c820f0935393aead509");
    info_entry->mutable_instance_info()->set_num_snapshots(3);

    return info_reply;
}

auto construct_single_snapshot_info_reply()
{
    mp::InfoReply info_reply;

    auto info_entry = info_reply.add_details();
    auto fundamentals = info_entry->mutable_snapshot_info()->mutable_fundamentals();

    info_entry->set_name("bogus-instance");
    info_entry->set_cpu_count("2");
    info_entry->set_disk_total("4.9GiB");
    info_entry->set_memory_total("0.9GiB");
    fundamentals->set_snapshot_name("snapshot2");
    fundamentals->set_parent("snapshot1");
    fundamentals->set_comment("This is a comment with some\nnew\r\nlines.");
    info_entry->mutable_snapshot_info()->set_size("128MiB");
    info_entry->mutable_snapshot_info()->add_children("snapshot3");
    info_entry->mutable_snapshot_info()->add_children("snapshot4");

    auto mount_entry = info_entry->mutable_mount_info()->add_mount_paths();
    mount_entry->set_source_path("/home/user/source");
    mount_entry->set_target_path("source");
    mount_entry = info_entry->mutable_mount_info()->add_mount_paths();
    mount_entry->set_source_path("/home/user");
    mount_entry->set_target_path("Home");

    google::protobuf::Timestamp timestamp;
    timestamp.set_seconds(63108020);
    timestamp.set_nanos(21000000);
    fundamentals->mutable_creation_timestamp()->CopyFrom(timestamp);

    return info_reply;
}

auto construct_multiple_snapshots_info_reply()
{
    mp::InfoReply info_reply;

    auto info_entry = info_reply.add_details();
    auto fundamentals = info_entry->mutable_snapshot_info()->mutable_fundamentals();

    info_entry->set_name("messier-87");
    info_entry->set_cpu_count("1");
    info_entry->set_disk_total("1024GiB");
    info_entry->set_memory_total("128GiB");
    fundamentals->set_snapshot_name("black-hole");
    fundamentals->set_comment("Captured by EHT");

    google::protobuf::Timestamp timestamp;
    timestamp.set_seconds(1554897599);
    fundamentals->mutable_creation_timestamp()->CopyFrom(timestamp);

    info_entry = info_reply.add_details();
    fundamentals = info_entry->mutable_snapshot_info()->mutable_fundamentals();

    info_entry->set_name("bogus-instance");
    info_entry->set_cpu_count("2");
    info_entry->set_disk_total("4.9GiB");
    info_entry->set_memory_total("0.9GiB");
    fundamentals->set_snapshot_name("snapshot2");
    fundamentals->set_parent("snapshot1");
    info_entry->mutable_snapshot_info()->add_children("snapshot3");
    info_entry->mutable_snapshot_info()->add_children("snapshot4");

    auto mount_entry = info_entry->mutable_mount_info()->add_mount_paths();
    mount_entry->set_source_path("/home/user/source");
    mount_entry->set_target_path("source");
    mount_entry = info_entry->mutable_mount_info()->add_mount_paths();
    mount_entry->set_source_path("/home/user");
    mount_entry->set_target_path("Home");

    timestamp.set_seconds(63108020);
    timestamp.set_nanos(21000000);
    fundamentals->mutable_creation_timestamp()->CopyFrom(timestamp);

    return info_reply;
}

auto construct_mixed_instance_and_snapshot_info_reply()
{
    mp::InfoReply info_reply;

    auto info_entry = info_reply.add_details();
    auto fundamentals = info_entry->mutable_snapshot_info()->mutable_fundamentals();

    info_entry->set_name("bogus-instance");
    info_entry->set_cpu_count("2");
    info_entry->set_disk_total("4.9GiB");
    info_entry->set_memory_total("0.9GiB");
    fundamentals->set_snapshot_name("snapshot2");
    fundamentals->set_parent("snapshot1");
    info_entry->mutable_snapshot_info()->add_children("snapshot3");
    info_entry->mutable_snapshot_info()->add_children("snapshot4");

    auto mount_entry = info_entry->mutable_mount_info()->add_mount_paths();
    mount_entry->set_source_path("/home/user/source");
    mount_entry->set_target_path("source");
    mount_entry = info_entry->mutable_mount_info()->add_mount_paths();
    mount_entry->set_source_path("/home/user");
    mount_entry->set_target_path("Home");

    google::protobuf::Timestamp timestamp;
    timestamp.set_seconds(63108020);
    timestamp.set_nanos(21000000);
    fundamentals->mutable_creation_timestamp()->CopyFrom(timestamp);

    info_entry = info_reply.add_details();
    info_entry->set_name("bombastic");
    info_entry->mutable_instance_status()->set_status(mp::InstanceStatus::STOPPED);
    info_entry->mutable_instance_info()->set_image_release("18.04 LTS");
    info_entry->mutable_instance_info()->set_id("ab5191cc172564e7cc0eafd397312a32598823e645279c820f0935393aead509");
    info_entry->mutable_instance_info()->set_num_snapshots(3);

    return info_reply;
}

auto construct_multiple_mixed_instances_and_snapshots_info_reply()
{
    mp::InfoReply info_reply;

    auto info_entry = info_reply.add_details();
    info_entry->set_name("bogus-instance");
    info_entry->mutable_instance_status()->set_status(mp::InstanceStatus::RUNNING);
    info_entry->mutable_instance_info()->set_image_release("16.04 LTS");
    info_entry->mutable_instance_info()->set_id("1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac");

    auto mount_info = info_entry->mutable_mount_info();
    mount_info->set_longest_path_len(17);

    auto mount_entry = mount_info->add_mount_paths();
    mount_entry->set_source_path("/home/user/source");
    mount_entry->set_target_path("source");

    auto uid_map_pair = mount_entry->mutable_mount_maps()->add_uid_mappings();
    uid_map_pair->set_host_id(1000);
    uid_map_pair->set_instance_id(501);

    auto gid_map_pair = mount_entry->mutable_mount_maps()->add_gid_mappings();
    gid_map_pair->set_host_id(1000);
    gid_map_pair->set_instance_id(501);

    info_entry->set_cpu_count("4");
    info_entry->mutable_instance_info()->set_load("0.03 0.10 0.15");
    info_entry->mutable_instance_info()->set_memory_usage("38797312");
    info_entry->set_memory_total("1610612736");
    info_entry->mutable_instance_info()->set_disk_usage("1932735284");
    info_entry->set_disk_total("6764573492");
    info_entry->mutable_instance_info()->set_current_release("Ubuntu 16.04.3 LTS");
    info_entry->mutable_instance_info()->add_ipv4("10.21.124.56");
    info_entry->mutable_instance_info()->set_num_snapshots(2);

    info_entry = info_reply.add_details();
    auto fundamentals = info_entry->mutable_snapshot_info()->mutable_fundamentals();

    info_entry->set_name("bogus-instance");
    info_entry->set_cpu_count("2");
    info_entry->set_disk_total("4.9GiB");
    info_entry->set_memory_total("0.9GiB");
    fundamentals->set_snapshot_name("snapshot2");
    fundamentals->set_parent("snapshot1");
    info_entry->mutable_snapshot_info()->add_children("snapshot3");
    info_entry->mutable_snapshot_info()->add_children("snapshot4");

    mount_entry = info_entry->mutable_mount_info()->add_mount_paths();
    mount_entry->set_source_path("/home/user/source");
    mount_entry->set_target_path("source");
    mount_entry = info_entry->mutable_mount_info()->add_mount_paths();
    mount_entry->set_source_path("/home/user");
    mount_entry->set_target_path("Home");

    google::protobuf::Timestamp timestamp;
    timestamp.set_seconds(63108020);
    timestamp.set_nanos(21000000);
    fundamentals->mutable_creation_timestamp()->CopyFrom(timestamp);

    info_entry = info_reply.add_details();
    fundamentals = info_entry->mutable_snapshot_info()->mutable_fundamentals();

    info_entry->set_name("bogus-instance");
    info_entry->set_cpu_count("2");
    info_entry->set_disk_total("4.9GiB");
    info_entry->set_memory_total("0.9GiB");
    fundamentals->set_snapshot_name("snapshot1");

    timestamp.set_seconds(63107999);
    timestamp.set_nanos(21000000);
    fundamentals->mutable_creation_timestamp()->CopyFrom(timestamp);

    info_entry = info_reply.add_details();
    info_entry->set_name("bombastic");
    info_entry->mutable_instance_status()->set_status(mp::InstanceStatus::STOPPED);
    info_entry->mutable_instance_info()->set_image_release("18.04 LTS");
    info_entry->mutable_instance_info()->set_id("ab5191cc172564e7cc0eafd397312a32598823e645279c820f0935393aead509");
    info_entry->mutable_instance_info()->set_num_snapshots(3);

    info_entry = info_reply.add_details();
    fundamentals = info_entry->mutable_snapshot_info()->mutable_fundamentals();

    info_entry->set_name("messier-87");
    info_entry->set_cpu_count("1");
    info_entry->set_disk_total("1024GiB");
    info_entry->set_memory_total("128GiB");
    fundamentals->set_snapshot_name("black-hole");
    fundamentals->set_comment("Captured by EHT");

    timestamp.set_seconds(1554897599);
    timestamp.set_nanos(0);
    fundamentals->mutable_creation_timestamp()->CopyFrom(timestamp);

    return info_reply;
}

auto add_petenv_to_reply(mp::InfoReply& reply, bool csv_format, bool snapshots)
{
    if ((csv_format && !snapshots) || !csv_format)
    {
        auto entry = reply.add_details();
        entry->set_name(petenv_name());
        entry->mutable_instance_status()->set_status(mp::InstanceStatus::SUSPENDED);
        entry->mutable_instance_info()->set_image_release("18.10");
        entry->mutable_instance_info()->set_id("1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd");
    }

    if ((csv_format && snapshots) || !csv_format)
    {
        auto entry = reply.add_details();
        entry->set_name(petenv_name());
        entry->mutable_snapshot_info()->mutable_fundamentals()->set_snapshot_name("snapshot1");
    }
}

auto construct_empty_reply()
{
    auto reply = mp::FindReply();
    reply.set_show_blueprints(true);
    reply.set_show_images(true);
    return reply;
}

auto construct_empty_reply_only_images()
{
    auto reply = mp::FindReply();
    reply.set_show_images(true);
    return reply;
}

auto construct_empty_reply_only_blueprints()
{
    auto reply = mp::FindReply();
    reply.set_show_blueprints(true);
    return reply;
}

auto construct_find_one_reply()
{
    auto reply = mp::FindReply();

    reply.set_show_images(true);

    auto image_entry = reply.add_images_info();
    image_entry->set_os("Ubuntu");
    image_entry->set_release("18.04 LTS");
    image_entry->set_version("20190516");

    auto alias_entry = image_entry->add_aliases_info();
    alias_entry->set_alias("ubuntu");

    return reply;
}

auto construct_find_one_blueprint_reply()
{
    auto reply = mp::FindReply();

    reply.set_show_blueprints(true);
    reply.set_show_images(true);

    auto blueprint_entry = reply.add_blueprints_info();
    blueprint_entry->set_release("Anbox Cloud Appliance");
    blueprint_entry->set_version("latest");

    auto alias_entry = blueprint_entry->add_aliases_info();
    alias_entry->set_alias("anbox-cloud-appliance");

    return reply;
}

auto construct_find_one_reply_no_os()
{
    auto reply = mp::FindReply();

    reply.set_show_blueprints(true);
    reply.set_show_images(true);

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

    reply.set_show_blueprints(true);
    reply.set_show_images(true);

    auto blueprint_entry = reply.add_blueprints_info();
    blueprint_entry->set_release("Anbox Cloud Appliance");
    blueprint_entry->set_version("latest");

    auto alias_entry = blueprint_entry->add_aliases_info();
    alias_entry->set_alias("anbox-cloud-appliance");

    auto image_entry = reply.add_images_info();
    image_entry->set_os("Ubuntu");
    image_entry->set_release("18.04 LTS");
    image_entry->set_version("20190516");

    alias_entry = image_entry->add_aliases_info();
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

    reply.set_show_blueprints(true);
    reply.set_show_images(true);

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

auto construct_version_info_multipassd_update_available()
{
    auto reply = mp::VersionReply();
    reply.set_version("Daemon version");

    reply.mutable_update_info()->set_version("update version number");
    reply.mutable_update_info()->set_title("update title information");
    reply.mutable_update_info()->set_description("update description information");
    reply.mutable_update_info()->set_url("http://multipass.web");

    return reply;
}

auto construct_version_info_multipassd_up_to_date()
{
    auto reply = mp::VersionReply();
    reply.set_version("Daemon version");

    return reply;
}

class BaseFormatterSuite : public testing::Test
{
public:
    BaseFormatterSuite() : saved_locale{std::locale()}
    {
        // The tests expected output are for the default C locale
        std::locale::global(std::locale("C"));
        EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return("pet"));

        // Timestamps in tests need to be in a consistent locale
        EXPECT_CALL(mock_format_utils, convert_to_user_locale(_)).WillRepeatedly([](const auto timestamp) {
            return google::protobuf::util::TimeUtil::ToString(timestamp);
        });
    }

    ~BaseFormatterSuite()
    {
        std::locale::global(saved_locale);
    }

protected:
    mpt::MockSettings::GuardedMock mock_settings_injection = mpt::MockSettings::inject<StrictMock>();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;

    mpt::MockFormatUtils::GuardedMock mock_format_utils_injection = mpt::MockFormatUtils::inject<NiceMock>();
    mpt::MockFormatUtils& mock_format_utils = *mock_format_utils_injection.first;

private:
    std::locale saved_locale;
};

typedef std::tuple<const mp::Formatter*, const ::google::protobuf::Message*, std::string /* output */,
                   std::string /* test name */>
    FormatterParamType;

struct FormatterSuite : public BaseFormatterSuite, public WithParamInterface<FormatterParamType>
{
};

auto print_param_name(const testing::TestParamInfo<FormatterSuite::ParamType>& info)
{
    return std::get<3>(info.param);
}

struct PetenvFormatterSuite : public BaseFormatterSuite,
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

const auto empty_list_reply = construct_empty_list_reply();
const auto empty_list_snapshot_reply = construct_empty_list_snapshot_reply();
const auto single_instance_list_reply = construct_single_instance_list_reply();
const auto multiple_instances_list_reply = construct_multiple_instances_list_reply();
const auto unsorted_list_reply = construct_unsorted_list_reply();
const auto single_snapshot_list_reply = construct_single_snapshot_list_reply();
const auto multiple_snapshots_list_reply = construct_multiple_snapshots_list_reply();

const auto empty_networks_reply = mp::NetworksReply();
const auto one_short_line_networks_reply = construct_one_short_line_networks_reply();
const auto one_long_line_networks_reply = construct_one_long_line_networks_reply();
const auto multiple_lines_networks_reply = construct_multiple_lines_networks_reply();

const auto empty_info_reply = mp::InfoReply();
const auto empty_info_snapshot_reply = construct_empty_info_snapshot_reply();
const auto single_instance_info_reply = construct_single_instance_info_reply();
const auto multiple_instances_info_reply = construct_multiple_instances_info_reply();
const auto single_snapshot_info_reply = construct_single_snapshot_info_reply();
const auto multiple_snapshots_info_reply = construct_multiple_snapshots_info_reply();
const auto mixed_instance_and_snapshot_info_reply = construct_mixed_instance_and_snapshot_info_reply();
const auto multiple_mixed_instances_and_snapshots_info_reply =
    construct_multiple_mixed_instances_and_snapshots_info_reply();

const std::vector<FormatterParamType> orderable_list_info_formatter_outputs{
    {&table_formatter, &empty_list_reply, "No instances found.\n", "table_list_empty"},
    {&table_formatter, &empty_list_snapshot_reply, "No snapshots found.\n", "table_list_snapshot_empty"},
    {&table_formatter,
     &single_instance_list_reply,
     "Name                    State             IPv4             Image\n"
     "foo                     Running           10.168.32.2      Ubuntu 16.04 LTS\n"
     "                                          200.3.123.30\n",
     "table_list_single"},

    {&table_formatter,
     &multiple_instances_list_reply,
     "Name                    State             IPv4             Image\n"
     "bogus-instance          Running           10.21.124.56     Ubuntu 16.04 LTS\n"
     "bombastic               Stopped           --               Ubuntu 18.04 LTS\n",
     "table_list_multiple"},

    {&table_formatter,
     &unsorted_list_reply,
     "Name                    State             IPv4             Image\n"
     "trusty-190611-1529      Deleted           --               Not Available\n"
     "trusty-190611-1535      Stopped           --               Ubuntu N/A\n"
     "trusty-190611-1539      Suspended         --               Not Available\n"
     "trusty-190611-1542      Running           --               Ubuntu N/A\n",
     "table_list_unsorted"},
    {&table_formatter,
     &single_snapshot_list_reply,
     "Instance   Snapshot    Parent   Comment\n"
     "foo        snapshot1   --       This is a sample comment\n",
     "table_list_single_snapshot"},
    {&table_formatter,
     &multiple_snapshots_list_reply,
     "Instance               Snapshot     Parent      Comment\n"
     "hale-roller            pristine     --          A first snapshot\n"
     "hale-roller            rocking      pristine    A very long comment that should be truncated by t…\n"
     "hale-roller            rolling      pristine    Loaded with stuff\n"
     "prosperous-spadefish   snapshot2    --          Before restoring snap1…\n"
     "prosperous-spadefish   snapshot10   snapshot2   --\n",
     "table_list_multiple_snapshots"},

    {&table_formatter, &empty_info_reply, "No instances found.\n", "table_info_empty"},
    {&table_formatter, &empty_info_snapshot_reply, "No snapshots found.\n", "table_info_snapshot_empty"},
    {&table_formatter,
     &single_instance_info_reply,
     "Name:           foo\n"
     "State:          Running\n"
     "Snapshots:      0\n"
     "IPv4:           10.168.32.2\n"
     "                200.3.123.29\n"
     "IPv6:           2001:67c:1562:8007::aac:423a\n"
     "                fd52:2ccf:f758:0:a342:79b5:e2ba:e05e\n"
     "Release:        Ubuntu 16.04.3 LTS\n"
     "Image hash:     1797c5c82016 (Ubuntu 16.04 LTS)\n"
     "CPU(s):         1\n"
     "Load:           0.45 0.51 0.15\n"
     "Disk usage:     1.2GiB out of 4.8GiB\n"
     "Memory usage:   58.0MiB out of 1.4GiB\n"
     "Mounts:         /home/user/foo      => foo\n"
     "                    UID map: 1000:1000\n"
     "                    GID map: 1000:1000\n"
     "                /home/user/test_dir => test_dir\n"
     "                    UID map: 1000:1000\n"
     "                    GID map: 1000:1000\n",
     "table_info_single_instance"},
    {&table_formatter,
     &multiple_instances_info_reply,
     "Name:           bogus-instance\n"
     "State:          Running\n"
     "Snapshots:      1\n"
     "IPv4:           10.21.124.56\n"
     "Release:        Ubuntu 16.04.3 LTS\n"
     "Image hash:     1797c5c82016 (Ubuntu 16.04 LTS)\n"
     "CPU(s):         4\n"
     "Load:           0.03 0.10 0.15\n"
     "Disk usage:     1.8GiB out of 6.3GiB\n"
     "Memory usage:   37.0MiB out of 1.5GiB\n"
     "Mounts:         /home/user/source => source\n"
     "                    UID map: 1000:501\n"
     "                    GID map: 1000:501\n\n"
     "Name:           bombastic\n"
     "State:          Stopped\n"
     "Snapshots:      3\n"
     "IPv4:           --\n"
     "Release:        --\n"
     "Image hash:     ab5191cc1725 (Ubuntu 18.04 LTS)\n"
     "CPU(s):         --\n"
     "Load:           --\n"
     "Disk usage:     --\n"
     "Memory usage:   --\n"
     "Mounts:         --\n",
     "table_info_multiple_instances"},
    {&table_formatter,
     &single_snapshot_info_reply,
     "Snapshot:       snapshot2\n"
     "Instance:       bogus-instance\n"
     "Size:           128MiB\n"
     "CPU(s):         2\n"
     "Disk space:     4.9GiB\n"
     "Memory size:    0.9GiB\n"
     "Mounts:         /home/user/source => source\n"
     "                /home/user => Home\n"
     "Created:        1972-01-01T10:00:20.021Z\n"
     "Parent:         snapshot1\n"
     "Children:       snapshot3\n"
     "                snapshot4\n"
     "Comment:        This is a comment with some\n"
     "                new\r\n"
     "                lines.\n",
     "table_info_single_snapshot"},
    {&table_formatter,
     &multiple_snapshots_info_reply,
     "Snapshot:       snapshot2\n"
     "Instance:       bogus-instance\n"
     "CPU(s):         2\n"
     "Disk space:     4.9GiB\n"
     "Memory size:    0.9GiB\n"
     "Mounts:         /home/user/source => source\n"
     "                /home/user => Home\n"
     "Created:        1972-01-01T10:00:20.021Z\n"
     "Parent:         snapshot1\n"
     "Children:       snapshot3\n"
     "                snapshot4\n"
     "Comment:        --\n\n"
     "Snapshot:       black-hole\n"
     "Instance:       messier-87\n"
     "CPU(s):         1\n"
     "Disk space:     1024GiB\n"
     "Memory size:    128GiB\n"
     "Mounts:         --\n"
     "Created:        2019-04-10T11:59:59Z\n"
     "Parent:         --\n"
     "Children:       --\n"
     "Comment:        Captured by EHT\n",
     "table_info_multiple_snapshots"},
    {&table_formatter,
     &mixed_instance_and_snapshot_info_reply,
     "Name:           bombastic\n"
     "State:          Stopped\n"
     "Snapshots:      3\n"
     "IPv4:           --\n"
     "Release:        --\n"
     "Image hash:     ab5191cc1725 (Ubuntu 18.04 LTS)\n"
     "CPU(s):         --\n"
     "Load:           --\n"
     "Disk usage:     --\n"
     "Memory usage:   --\n"
     "Mounts:         --\n\n"
     "Snapshot:       snapshot2\n"
     "Instance:       bogus-instance\n"
     "CPU(s):         2\n"
     "Disk space:     4.9GiB\n"
     "Memory size:    0.9GiB\n"
     "Mounts:         /home/user/source => source\n"
     "                /home/user => Home\n"
     "Created:        1972-01-01T10:00:20.021Z\n"
     "Parent:         snapshot1\n"
     "Children:       snapshot3\n"
     "                snapshot4\n"
     "Comment:        --\n",
     "table_info_mixed_instance_and_snapshot"},
    {&table_formatter,
     &multiple_mixed_instances_and_snapshots_info_reply,
     "Name:           bogus-instance\n"
     "State:          Running\n"
     "Snapshots:      2\n"
     "IPv4:           10.21.124.56\n"
     "Release:        Ubuntu 16.04.3 LTS\n"
     "Image hash:     1797c5c82016 (Ubuntu 16.04 LTS)\n"
     "CPU(s):         4\n"
     "Load:           0.03 0.10 0.15\n"
     "Disk usage:     1.8GiB out of 6.3GiB\n"
     "Memory usage:   37.0MiB out of 1.5GiB\n"
     "Mounts:         /home/user/source => source\n"
     "                    UID map: 1000:501\n"
     "                    GID map: 1000:501\n\n"
     "Name:           bombastic\n"
     "State:          Stopped\n"
     "Snapshots:      3\n"
     "IPv4:           --\n"
     "Release:        --\n"
     "Image hash:     ab5191cc1725 (Ubuntu 18.04 LTS)\n"
     "CPU(s):         --\n"
     "Load:           --\n"
     "Disk usage:     --\n"
     "Memory usage:   --\n"
     "Mounts:         --\n\n"
     "Snapshot:       snapshot1\n"
     "Instance:       bogus-instance\n"
     "CPU(s):         2\n"
     "Disk space:     4.9GiB\n"
     "Memory size:    0.9GiB\n"
     "Mounts:         --\n"
     "Created:        1972-01-01T09:59:59.021Z\n"
     "Parent:         --\n"
     "Children:       --\n"
     "Comment:        --\n\n"
     "Snapshot:       snapshot2\n"
     "Instance:       bogus-instance\n"
     "CPU(s):         2\n"
     "Disk space:     4.9GiB\n"
     "Memory size:    0.9GiB\n"
     "Mounts:         /home/user/source => source\n"
     "                /home/user => Home\n"
     "Created:        1972-01-01T10:00:20.021Z\n"
     "Parent:         snapshot1\n"
     "Children:       snapshot3\n"
     "                snapshot4\n"
     "Comment:        --\n\n"
     "Snapshot:       black-hole\n"
     "Instance:       messier-87\n"
     "CPU(s):         1\n"
     "Disk space:     1024GiB\n"
     "Memory size:    128GiB\n"
     "Mounts:         --\n"
     "Created:        2019-04-10T11:59:59Z\n"
     "Parent:         --\n"
     "Children:       --\n"
     "Comment:        Captured by EHT\n",
     "table_info_multiple_mixed_instances_and_snapshots"},

    {&csv_formatter, &empty_list_reply, "Name,State,IPv4,IPv6,Release,AllIPv4\n", "csv_list_empty"},
    {&csv_formatter,
     &single_instance_list_reply,
     "Name,State,IPv4,IPv6,Release,AllIPv4\n"
     "foo,Running,10.168.32.2,fdde:2681:7a2::4ca,Ubuntu 16.04 LTS,\"10.168.32.2,200.3.123.30\"\n",
     "csv_list_single"},
    {&csv_formatter,
     &multiple_instances_list_reply,
     "Name,State,IPv4,IPv6,Release,AllIPv4\n"
     "bogus-instance,Running,10.21.124.56,,Ubuntu 16.04 LTS,\"10.21.124.56\"\n"
     "bombastic,Stopped,,,Ubuntu 18.04 LTS,\"\"\n",
     "csv_list_multiple"},
    {&csv_formatter,
     &unsorted_list_reply,
     "Name,State,IPv4,IPv6,Release,AllIPv4\n"
     "trusty-190611-1529,Deleted,,,Not Available,\"\"\n"
     "trusty-190611-1535,Stopped,,,Ubuntu N/A,\"\"\n"
     "trusty-190611-1539,Suspended,,,Not Available,\"\"\n"
     "trusty-190611-1542,Running,,,Ubuntu N/A,\"\"\n",
     "csv_list_unsorted"},
    {&csv_formatter, &empty_list_snapshot_reply, "Instance,Snapshot,Parent,Comment\n", "csv_list_snapshot_empty"},
    {&csv_formatter,
     &single_snapshot_list_reply,
     "Instance,Snapshot,Parent,Comment\nfoo,snapshot1,,\"This is a sample comment\"\n",
     "csv_list_single_snapshot"},
    {&csv_formatter,
     &multiple_snapshots_list_reply,
     "Instance,Snapshot,Parent,Comment\nhale-roller,pristine,,\"A first "
     "snapshot\"\nhale-roller,rocking,pristine,\"A very long comment that should be truncated by the table "
     "formatter\"\nhale-roller,rolling,pristine,\"Loaded with stuff\"\nprosperous-spadefish,snapshot2,,\"Before "
     "restoring snap1\nContains a newline that\r\nshould be "
     "truncated\"\nprosperous-spadefish,snapshot10,snapshot2,\"\"\n",
     "csv_list_multiple_snapshots"},

    {&csv_formatter, &empty_info_reply, "", "csv_info_empty"},
    {&csv_formatter,
     &single_instance_info_reply,
     "Name,State,Ipv4,Ipv6,Release,Image hash,Image release,Load,Disk usage,Disk total,Memory "
     "usage,Memory total,Mounts,AllIPv4,CPU(s),Snapshots\nfoo,Running,10.168.32.2,2001:67c:1562:8007::aac:423a,Ubuntu "
     "16.04.3 "
     "LTS,1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac,16.04 LTS,0.45 0.51 "
     "0.15,1288490188,5153960756,60817408,1503238554,/home/user/foo => foo;/home/user/test_dir "
     "=> test_dir,10.168.32.2;200.3.123.29,1,0\n",
     "csv_info_single_instance"},
    {&csv_formatter,
     &single_snapshot_info_reply,
     "Snapshot,Instance,CPU(s),Disk space,Memory "
     "size,Mounts,Created,Parent,Children,Comment\nsnapshot2,bogus-instance,2,4.9GiB,0.9GiB,/home/user/source "
     "=> "
     "source;/home/user => Home,1972-01-01T10:00:20.021Z,snapshot1,snapshot3;snapshot4,\"This is a comment with "
     "some\nnew\r\nlines.\"\n",
     "csv_info_single_snapshot_info_reply"},
    {&csv_formatter,
     &multiple_snapshots_info_reply,
     "Snapshot,Instance,CPU(s),Disk space,Memory "
     "size,Mounts,Created,Parent,Children,Comment\nsnapshot2,bogus-instance,2,4.9GiB,0.9GiB,/home/user/source => "
     "source;/home/user => "
     "Home,1972-01-01T10:00:20.021Z,snapshot1,snapshot3;snapshot4,\"\"\nblack-hole,messier-87,1,1024GiB,128GiB,,"
     "2019-04-10T11:59:59Z,,,\"Captured by EHT\"\n",
     "csv_info_multiple_snapshot_info_reply"},
    {&csv_formatter,
     &multiple_instances_info_reply,
     "Name,State,Ipv4,Ipv6,Release,Image hash,Image release,Load,Disk usage,Disk total,Memory "
     "usage,Memory total,Mounts,AllIPv4,CPU(s),Snapshots\nbogus-instance,Running,10.21.124.56,,Ubuntu 16.04.3 "
     "LTS,1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac,16.04 LTS,0.03 0.10 "
     "0.15,1932735284,6764573492,38797312,1610612736,/home/user/source => "
     "source,10.21.124.56,4,1\nbombastic,Stopped,,,,"
     "ab5191cc172564e7cc0eafd397312a32598823e645279c820f0935393aead509,18.04 LTS,,,,,,,,,3\n",
     "csv_info_multiple_instances"},

    {&yaml_formatter, &empty_list_reply, "\n", "yaml_list_empty"},
    {&yaml_formatter,
     &single_instance_list_reply,
     "foo:\n"
     "  - state: Running\n"
     "    ipv4:\n"
     "      - 10.168.32.2\n"
     "      - 200.3.123.30\n"
     "    release: Ubuntu 16.04 LTS\n",
     "yaml_list_single"},
    {&yaml_formatter,
     &multiple_instances_list_reply,
     "bogus-instance:\n"
     "  - state: Running\n"
     "    ipv4:\n"
     "      - 10.21.124.56\n"
     "    release: Ubuntu 16.04 LTS\n"
     "bombastic:\n"
     "  - state: Stopped\n"
     "    ipv4:\n"
     "      []\n"
     "    release: Ubuntu 18.04 LTS\n",
     "yaml_list_multiple"},
    {&yaml_formatter,
     &unsorted_list_reply,
     "trusty-190611-1529:\n"
     "  - state: Deleted\n"
     "    ipv4:\n"
     "      []\n"
     "    release: Not Available\n"
     "trusty-190611-1535:\n"
     "  - state: Stopped\n"
     "    ipv4:\n"
     "      []\n"
     "    release: Ubuntu N/A\n"
     "trusty-190611-1539:\n"
     "  - state: Suspended\n"
     "    ipv4:\n"
     "      []\n"
     "    release: Not Available\n"
     "trusty-190611-1542:\n"
     "  - state: Running\n"
     "    ipv4:\n"
     "      []\n"
     "    release: Ubuntu N/A\n",
     "yaml_list_unsorted"},
    {&yaml_formatter, &empty_list_snapshot_reply, "\n", "yaml_list_snapshot_empty"},
    {&yaml_formatter,
     &single_snapshot_list_reply,
     "foo:\n"
     "  - snapshot1:\n"
     "      - parent: ~\n"
     "        comment: This is a sample comment\n",
     "yaml_list_single_snapshot"},
    {&yaml_formatter,
     &multiple_snapshots_list_reply,
     "hale-roller:\n"
     "  - pristine:\n"
     "      - parent: ~\n"
     "        comment: A first snapshot\n"
     "  - rocking:\n"
     "      - parent: pristine\n"
     "        comment: A very long comment that should be truncated by the table formatter\n"
     "  - rolling:\n"
     "      - parent: pristine\n"
     "        comment: Loaded with stuff\n"
     "prosperous-spadefish:\n"
     "  - snapshot2:\n"
     "      - parent: ~\n"
     "        comment: \"Before restoring snap1\\nContains a newline that\\r\\nshould be truncated\"\n"
     "  - snapshot10:\n"
     "      - parent: snapshot2\n"
     "        comment: ~\n",
     "yaml_list_multiple_snapshots"},

    {&yaml_formatter, &empty_info_reply, "errors:\n  - ~\n", "yaml_info_empty"},
    {&yaml_formatter,
     &single_instance_info_reply,
     "errors:\n"
     "  - ~\n"
     "foo:\n"
     "  - state: Running\n"
     "    snapshot_count: 0\n"
     "    image_hash: 1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac\n"
     "    image_release: 16.04 LTS\n"
     "    release: Ubuntu 16.04.3 LTS\n"
     "    cpu_count: 1\n"
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
     "      - 200.3.123.29\n"
     "    mounts:\n"
     "      foo:\n"
     "        uid_mappings:\n"
     "          - \"1000:1000\"\n"
     "        gid_mappings:\n"
     "          - \"1000:1000\"\n"
     "        source_path: /home/user/foo\n"
     "      test_dir:\n"
     "        uid_mappings:\n"
     "          - \"1000:1000\"\n"
     "        gid_mappings:\n"
     "          - \"1000:1000\"\n"
     "        source_path: /home/user/test_dir\n",
     "yaml_info_single_instance"},
    {&yaml_formatter,
     &multiple_instances_info_reply,
     "errors:\n"
     "  - ~\n"
     "bogus-instance:\n"
     "  - state: Running\n"
     "    snapshot_count: 1\n"
     "    image_hash: 1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac\n"
     "    image_release: 16.04 LTS\n"
     "    release: Ubuntu 16.04.3 LTS\n"
     "    cpu_count: 4\n"
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
     "          - \"1000:501\"\n"
     "        gid_mappings:\n"
     "          - \"1000:501\"\n"
     "        source_path: /home/user/source\n"
     "bombastic:\n"
     "  - state: Stopped\n"
     "    snapshot_count: 3\n"
     "    image_hash: ab5191cc172564e7cc0eafd397312a32598823e645279c820f0935393aead509\n"
     "    image_release: 18.04 LTS\n"
     "    release: ~\n"
     "    cpu_count: ~\n"
     "    disks:\n"
     "      - sda1:\n"
     "          used: ~\n"
     "          total: ~\n"
     "    memory:\n"
     "      usage: ~\n"
     "      total: ~\n"
     "    ipv4:\n"
     "      []\n"
     "    mounts: ~\n",
     "yaml_info_multiple_instances"},
    {&yaml_formatter,
     &single_snapshot_info_reply,
     "errors:\n"
     "  - ~\n"
     "bogus-instance:\n"
     "  - snapshots:\n"
     "      - snapshot2:\n"
     "          size: 128MiB\n"
     "          cpu_count: 2\n"
     "          disk_space: 4.9GiB\n"
     "          memory_size: 0.9GiB\n"
     "          mounts:\n"
     "            source:\n"
     "              source_path: /home/user/source\n"
     "            Home:\n"
     "              source_path: /home/user\n"
     "          created: \"1972-01-01T10:00:20.021Z\"\n"
     "          parent: snapshot1\n"
     "          children:\n"
     "            - snapshot3\n"
     "            - snapshot4\n"
     "          comment: \"This is a comment with some\\nnew\\r\\nlines.\"\n",
     "yaml_info_single_snapshot_info_reply"},
    {&yaml_formatter,
     &multiple_snapshots_info_reply,
     "errors:\n"
     "  - ~\n"
     "bogus-instance:\n"
     "  - snapshots:\n"
     "      - snapshot2:\n"
     "          size: ~\n"
     "          cpu_count: 2\n"
     "          disk_space: 4.9GiB\n"
     "          memory_size: 0.9GiB\n"
     "          mounts:\n"
     "            source:\n"
     "              source_path: /home/user/source\n"
     "            Home:\n"
     "              source_path: /home/user\n"
     "          created: \"1972-01-01T10:00:20.021Z\"\n"
     "          parent: snapshot1\n"
     "          children:\n"
     "            - snapshot3\n"
     "            - snapshot4\n"
     "          comment: ~\n"
     "messier-87:\n"
     "  - snapshots:\n"
     "      - black-hole:\n"
     "          size: ~\n"
     "          cpu_count: 1\n"
     "          disk_space: 1024GiB\n"
     "          memory_size: 128GiB\n"
     "          mounts: ~\n"
     "          created: \"2019-04-10T11:59:59Z\"\n"
     "          parent: ~\n"
     "          children:\n"
     "            []\n"
     "          comment: Captured by EHT\n",
     "yaml_info_multiple_snapshots_info_reply"},
    {&yaml_formatter,
     &mixed_instance_and_snapshot_info_reply,
     "errors:\n"
     "  - ~\n"
     "bombastic:\n"
     "  - state: Stopped\n"
     "    snapshot_count: 3\n"
     "    image_hash: ab5191cc172564e7cc0eafd397312a32598823e645279c820f0935393aead509\n"
     "    image_release: 18.04 LTS\n"
     "    release: ~\n"
     "    cpu_count: ~\n"
     "    disks:\n"
     "      - sda1:\n"
     "          used: ~\n"
     "          total: ~\n"
     "    memory:\n"
     "      usage: ~\n"
     "      total: ~\n"
     "    ipv4:\n"
     "      []\n"
     "    mounts: ~\n"
     "bogus-instance:\n"
     "  - snapshots:\n"
     "      - snapshot2:\n"
     "          size: ~\n"
     "          cpu_count: 2\n"
     "          disk_space: 4.9GiB\n"
     "          memory_size: 0.9GiB\n"
     "          mounts:\n"
     "            source:\n"
     "              source_path: /home/user/source\n"
     "            Home:\n"
     "              source_path: /home/user\n"
     "          created: \"1972-01-01T10:00:20.021Z\"\n"
     "          parent: snapshot1\n"
     "          children:\n"
     "            - snapshot3\n"
     "            - snapshot4\n"
     "          comment: ~\n",
     "yaml_info_mixed_instance_and_snapshot_info_reply"},
    {&yaml_formatter,
     &multiple_mixed_instances_and_snapshots_info_reply,
     "errors:\n"
     "  - ~\n"
     "bogus-instance:\n"
     "  - state: Running\n"
     "    snapshot_count: 2\n"
     "    image_hash: 1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac\n"
     "    image_release: 16.04 LTS\n"
     "    release: Ubuntu 16.04.3 LTS\n"
     "    cpu_count: 4\n"
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
     "          - \"1000:501\"\n"
     "        gid_mappings:\n"
     "          - \"1000:501\"\n"
     "        source_path: /home/user/source\n"
     "    snapshots:\n"
     "      - snapshot1:\n"
     "          size: ~\n"
     "          cpu_count: 2\n"
     "          disk_space: 4.9GiB\n"
     "          memory_size: 0.9GiB\n"
     "          mounts: ~\n"
     "          created: \"1972-01-01T09:59:59.021Z\"\n"
     "          parent: ~\n"
     "          children:\n"
     "            []\n"
     "          comment: ~\n"
     "      - snapshot2:\n"
     "          size: ~\n"
     "          cpu_count: 2\n"
     "          disk_space: 4.9GiB\n"
     "          memory_size: 0.9GiB\n"
     "          mounts:\n"
     "            source:\n"
     "              source_path: /home/user/source\n"
     "            Home:\n"
     "              source_path: /home/user\n"
     "          created: \"1972-01-01T10:00:20.021Z\"\n"
     "          parent: snapshot1\n"
     "          children:\n"
     "            - snapshot3\n"
     "            - snapshot4\n"
     "          comment: ~\n"
     "bombastic:\n"
     "  - state: Stopped\n"
     "    snapshot_count: 3\n"
     "    image_hash: ab5191cc172564e7cc0eafd397312a32598823e645279c820f0935393aead509\n"
     "    image_release: 18.04 LTS\n"
     "    release: ~\n"
     "    cpu_count: ~\n"
     "    disks:\n"
     "      - sda1:\n"
     "          used: ~\n"
     "          total: ~\n"
     "    memory:\n"
     "      usage: ~\n"
     "      total: ~\n"
     "    ipv4:\n"
     "      []\n"
     "    mounts: ~\n"
     "messier-87:\n"
     "  - snapshots:\n"
     "      - black-hole:\n"
     "          size: ~\n"
     "          cpu_count: 1\n"
     "          disk_space: 1024GiB\n"
     "          memory_size: 128GiB\n"
     "          mounts: ~\n"
     "          created: \"2019-04-10T11:59:59Z\"\n"
     "          parent: ~\n"
     "          children:\n"
     "            []\n"
     "          comment: Captured by EHT\n",
     "yaml_info_multiple_mixed_instances_and_snapshots"}};

const std::vector<FormatterParamType> non_orderable_list_info_formatter_outputs{
    {&json_formatter,
     &empty_list_reply,
     "{\n"
     "    \"list\": [\n"
     "    ]\n"
     "}\n",
     "json_list_empty"},
    {&json_formatter,
     &single_instance_list_reply,
     "{\n"
     "    \"list\": [\n"
     "        {\n"
     "            \"ipv4\": [\n"
     "                \"10.168.32.2\",\n"
     "                \"200.3.123.30\"\n"
     "            ],\n"
     "            \"name\": \"foo\",\n"
     "            \"release\": \"Ubuntu 16.04 LTS\",\n"
     "            \"state\": \"Running\"\n"
     "        }\n"
     "    ]\n"
     "}\n",
     "json_list_single"},
    {&json_formatter,
     &multiple_instances_list_reply,
     "{\n"
     "    \"list\": [\n"
     "        {\n"
     "            \"ipv4\": [\n"
     "                \"10.21.124.56\"\n"
     "            ],\n"
     "            \"name\": \"bogus-instance\",\n"
     "            \"release\": \"Ubuntu 16.04 LTS\",\n"
     "            \"state\": \"Running\"\n"
     "        },\n"
     "        {\n"
     "            \"ipv4\": [\n"
     "            ],\n"
     "            \"name\": \"bombastic\",\n"
     "            \"release\": \"Ubuntu 18.04 LTS\",\n"
     "            \"state\": \"Stopped\"\n"
     "        }\n"
     "    ]\n"
     "}\n",
     "json_list_multiple"},
    {&json_formatter,
     &single_snapshot_list_reply,
     "{\n"
     "    \"errors\": [\n"
     "    ],\n"
     "    \"info\": {\n"
     "        \"foo\": {\n"
     "            \"snapshot1\": {\n"
     "                \"comment\": \"This is a sample comment\",\n"
     "                \"parent\": \"\"\n"
     "            }\n"
     "        }\n"
     "    }\n"
     "}\n",
     "json_list_single_snapshot"},
    {&json_formatter,
     &multiple_snapshots_list_reply,
     "{\n"
     "    \"errors\": [\n"
     "    ],\n"
     "    \"info\": {\n"
     "        \"hale-roller\": {\n"
     "            \"pristine\": {\n"
     "                \"comment\": \"A first snapshot\",\n"
     "                \"parent\": \"\"\n"
     "            },\n"
     "            \"rocking\": {\n"
     "                \"comment\": \"A very long comment that should be truncated by the table formatter\",\n"
     "                \"parent\": \"pristine\"\n"
     "            },\n"
     "            \"rolling\": {\n"
     "                \"comment\": \"Loaded with stuff\",\n"
     "                \"parent\": \"pristine\"\n"
     "            }\n"
     "        },\n"
     "        \"prosperous-spadefish\": {\n"
     "            \"snapshot10\": {\n"
     "                \"comment\": \"\",\n"
     "                \"parent\": \"snapshot2\"\n"
     "            },\n"
     "            \"snapshot2\": {\n"
     "                \"comment\": \"Before restoring snap1\\nContains a newline that\\r\\nshould be truncated\",\n"
     "                \"parent\": \"\"\n"
     "            }\n"
     "        }\n"
     "    }\n"
     "}\n",
     "json_list_multiple_snapshots"},
    {&json_formatter,
     &empty_info_reply,
     "{\n"
     "    \"errors\": [\n"
     "    ],\n"
     "    \"info\": {\n"
     "    }\n"
     "}\n",
     "json_info_empty"},
    {&json_formatter,
     &single_instance_info_reply,
     "{\n"
     "    \"errors\": [\n"
     "    ],\n"
     "    \"info\": {\n"
     "        \"foo\": {\n"
     "            \"cpu_count\": \"1\",\n"
     "            \"disks\": {\n"
     "                \"sda1\": {\n"
     "                    \"total\": \"5153960756\",\n"
     "                    \"used\": \"1288490188\"\n"
     "                }\n"
     "            },\n"
     "            \"image_hash\": \"1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac\",\n"
     "            \"image_release\": \"16.04 LTS\",\n"
     "            \"ipv4\": [\n"
     "                \"10.168.32.2\",\n"
     "                \"200.3.123.29\"\n"
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
     "            \"snapshot_count\": \"0\",\n"
     "            \"state\": \"Running\"\n"
     "        }\n"
     "    }\n"
     "}\n",
     "json_info_single_instance"},
    {&json_formatter,
     &multiple_instances_info_reply,
     "{\n"
     "    \"errors\": [\n"
     "    ],\n"
     "    \"info\": {\n"
     "        \"bogus-instance\": {\n"
     "            \"cpu_count\": \"4\",\n"
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
     "            \"snapshot_count\": \"1\",\n"
     "            \"state\": \"Running\"\n"
     "        },\n"
     "        \"bombastic\": {\n"
     "            \"cpu_count\": \"\",\n"
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
     "            \"snapshot_count\": \"3\",\n"
     "            \"state\": \"Stopped\"\n"
     "        }\n"
     "    }\n"
     "}\n",
     "json_info_multiple_instances"},
    {&json_formatter,
     &single_snapshot_info_reply,
     "{\n"
     "    \"errors\": [\n"
     "    ],\n"
     "    \"info\": {\n"
     "        \"bogus-instance\": {\n"
     "            \"snapshots\": {\n"
     "                \"snapshot2\": {\n"
     "                    \"children\": [\n"
     "                        \"snapshot3\",\n"
     "                        \"snapshot4\"\n"
     "                    ],\n"
     "                    \"comment\": \"This is a comment with some\\nnew\\r\\nlines.\",\n"
     "                    \"cpu_count\": \"2\",\n"
     "                    \"created\": \"1972-01-01T10:00:20.021Z\",\n"
     "                    \"disk_space\": \"4.9GiB\",\n"
     "                    \"memory_size\": \"0.9GiB\",\n"
     "                    \"mounts\": {\n"
     "                        \"Home\": {\n"
     "                            \"source_path\": \"/home/user\"\n"
     "                        },\n"
     "                        \"source\": {\n"
     "                            \"source_path\": \"/home/user/source\"\n"
     "                        }\n"
     "                    },\n"
     "                    \"parent\": \"snapshot1\",\n"
     "                    \"size\": \"128MiB\"\n"
     "                }\n"
     "            }\n"
     "        }\n"
     "    }\n"
     "}\n",
     "json_info_single_snapshot_info_reply"},
    {&json_formatter,
     &multiple_snapshots_info_reply,
     "{\n"
     "    \"errors\": [\n"
     "    ],\n"
     "    \"info\": {\n"
     "        \"bogus-instance\": {\n"
     "            \"snapshots\": {\n"
     "                \"snapshot2\": {\n"
     "                    \"children\": [\n"
     "                        \"snapshot3\",\n"
     "                        \"snapshot4\"\n"
     "                    ],\n"
     "                    \"comment\": \"\",\n"
     "                    \"cpu_count\": \"2\",\n"
     "                    \"created\": \"1972-01-01T10:00:20.021Z\",\n"
     "                    \"disk_space\": \"4.9GiB\",\n"
     "                    \"memory_size\": \"0.9GiB\",\n"
     "                    \"mounts\": {\n"
     "                        \"Home\": {\n"
     "                            \"source_path\": \"/home/user\"\n"
     "                        },\n"
     "                        \"source\": {\n"
     "                            \"source_path\": \"/home/user/source\"\n"
     "                        }\n"
     "                    },\n"
     "                    \"parent\": \"snapshot1\",\n"
     "                    \"size\": \"\"\n"
     "                }\n"
     "            }\n"
     "        },\n"
     "        \"messier-87\": {\n"
     "            \"snapshots\": {\n"
     "                \"black-hole\": {\n"
     "                    \"children\": [\n"
     "                    ],\n"
     "                    \"comment\": \"Captured by EHT\",\n"
     "                    \"cpu_count\": \"1\",\n"
     "                    \"created\": \"2019-04-10T11:59:59Z\",\n"
     "                    \"disk_space\": \"1024GiB\",\n"
     "                    \"memory_size\": \"128GiB\",\n"
     "                    \"mounts\": {\n"
     "                    },\n"
     "                    \"parent\": \"\",\n"
     "                    \"size\": \"\"\n"
     "                }\n"
     "            }\n"
     "        }\n"
     "    }\n"
     "}\n",
     "json_info_multiple_snapshots_info_reply"},
    {&json_formatter,
     &mixed_instance_and_snapshot_info_reply,
     "{\n"
     "    \"errors\": [\n"
     "    ],\n"
     "    \"info\": {\n"
     "        \"bogus-instance\": {\n"
     "            \"snapshots\": {\n"
     "                \"snapshot2\": {\n"
     "                    \"children\": [\n"
     "                        \"snapshot3\",\n"
     "                        \"snapshot4\"\n"
     "                    ],\n"
     "                    \"comment\": \"\",\n"
     "                    \"cpu_count\": \"2\",\n"
     "                    \"created\": \"1972-01-01T10:00:20.021Z\",\n"
     "                    \"disk_space\": \"4.9GiB\",\n"
     "                    \"memory_size\": \"0.9GiB\",\n"
     "                    \"mounts\": {\n"
     "                        \"Home\": {\n"
     "                            \"source_path\": \"/home/user\"\n"
     "                        },\n"
     "                        \"source\": {\n"
     "                            \"source_path\": \"/home/user/source\"\n"
     "                        }\n"
     "                    },\n"
     "                    \"parent\": \"snapshot1\",\n"
     "                    \"size\": \"\"\n"
     "                }\n"
     "            }\n"
     "        },\n"
     "        \"bombastic\": {\n"
     "            \"cpu_count\": \"\",\n"
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
     "            \"snapshot_count\": \"3\",\n"
     "            \"state\": \"Stopped\"\n"
     "        }\n"
     "    }\n"
     "}\n",
     "json_info_mixed_instance_and_snapshot_info_reply"},
    {&json_formatter,
     &multiple_mixed_instances_and_snapshots_info_reply,
     "{\n"
     "    \"errors\": [\n"
     "    ],\n"
     "    \"info\": {\n"
     "        \"bogus-instance\": {\n"
     "            \"cpu_count\": \"4\",\n"
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
     "            \"snapshot_count\": \"2\",\n"
     "            \"snapshots\": {\n"
     "                \"snapshot1\": {\n"
     "                    \"children\": [\n"
     "                    ],\n"
     "                    \"comment\": \"\",\n"
     "                    \"cpu_count\": \"2\",\n"
     "                    \"created\": \"1972-01-01T09:59:59.021Z\",\n"
     "                    \"disk_space\": \"4.9GiB\",\n"
     "                    \"memory_size\": \"0.9GiB\",\n"
     "                    \"mounts\": {\n"
     "                    },\n"
     "                    \"parent\": \"\",\n"
     "                    \"size\": \"\"\n"
     "                },\n"
     "                \"snapshot2\": {\n"
     "                    \"children\": [\n"
     "                        \"snapshot3\",\n"
     "                        \"snapshot4\"\n"
     "                    ],\n"
     "                    \"comment\": \"\",\n"
     "                    \"cpu_count\": \"2\",\n"
     "                    \"created\": \"1972-01-01T10:00:20.021Z\",\n"
     "                    \"disk_space\": \"4.9GiB\",\n"
     "                    \"memory_size\": \"0.9GiB\",\n"
     "                    \"mounts\": {\n"
     "                        \"Home\": {\n"
     "                            \"source_path\": \"/home/user\"\n"
     "                        },\n"
     "                        \"source\": {\n"
     "                            \"source_path\": \"/home/user/source\"\n"
     "                        }\n"
     "                    },\n"
     "                    \"parent\": \"snapshot1\",\n"
     "                    \"size\": \"\"\n"
     "                }\n"
     "            },\n"
     "            \"state\": \"Running\"\n"
     "        },\n"
     "        \"bombastic\": {\n"
     "            \"cpu_count\": \"\",\n"
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
     "            \"snapshot_count\": \"3\",\n"
     "            \"state\": \"Stopped\"\n"
     "        },\n"
     "        \"messier-87\": {\n"
     "            \"snapshots\": {\n"
     "                \"black-hole\": {\n"
     "                    \"children\": [\n"
     "                    ],\n"
     "                    \"comment\": \"Captured by EHT\",\n"
     "                    \"cpu_count\": \"1\",\n"
     "                    \"created\": \"2019-04-10T11:59:59Z\",\n"
     "                    \"disk_space\": \"1024GiB\",\n"
     "                    \"memory_size\": \"128GiB\",\n"
     "                    \"mounts\": {\n"
     "                    },\n"
     "                    \"parent\": \"\",\n"
     "                    \"size\": \"\"\n"
     "                }\n"
     "            }\n"
     "        }\n"
     "    }\n"
     "}\n",
     "json_info_multiple_mixed_instances_and_snapshots"}};

const std::vector<FormatterParamType> non_orderable_networks_formatter_outputs{
    {&table_formatter, &empty_networks_reply, "No network interfaces found.\n", "table_networks_empty"},
    {&table_formatter,
     &one_short_line_networks_reply,
     "Name   Type   Description\n"
     "en0    eth    Ether\n",
     "table_networks_one_short_line"},
    {&table_formatter,
     &one_long_line_networks_reply,
     "Name     Type       Description\n"
     "enp3s0   ethernet   Amazingly fast and robust ethernet adapter\n",
     "table_networks_one_long_line"},
    {&table_formatter,
     &multiple_lines_networks_reply,
     "Name              Type   Description\n"
     "en0               eth    Ether\n"
     "wlx0123456789ab   wifi   Wireless\n",
     "table_networks_multiple_lines"},

    {&csv_formatter, &empty_networks_reply, "Name,Type,Description\n", "csv_networks_empty"},
    {&csv_formatter,
     &one_short_line_networks_reply,
     "Name,Type,Description\n"
     "en0,eth,\"Ether\"\n",
     "csv_networks_one_short_line"},
    {&csv_formatter,
     &one_long_line_networks_reply,
     "Name,Type,Description\n"
     "enp3s0,ethernet,\"Amazingly fast and robust ethernet adapter\"\n",
     "csv_networks_one_long_line"},
    {&csv_formatter,
     &multiple_lines_networks_reply,
     "Name,Type,Description\n"
     "en0,eth,\"Ether\"\n"
     "wlx0123456789ab,wifi,\"Wireless\"\n",
     "csv_networks_multiple_lines"},

    {&yaml_formatter, &empty_networks_reply, "\n", "yaml_networks_empty"},
    {&yaml_formatter,
     &one_short_line_networks_reply,
     "en0:\n"
     "  - type: eth\n"
     "    description: Ether\n",
     "yaml_networks_one_short_line"},
    {&yaml_formatter,
     &one_long_line_networks_reply,
     "enp3s0:\n"
     "  - type: ethernet\n"
     "    description: Amazingly fast and robust ethernet adapter\n",
     "yaml_networks_one_long_line"},
    {&yaml_formatter,
     &multiple_lines_networks_reply,
     "en0:\n"
     "  - type: eth\n"
     "    description: Ether\n"
     "wlx0123456789ab:\n"
     "  - type: wifi\n"
     "    description: Wireless\n",
     "yaml_networks_multiple_lines"},

    {&json_formatter,
     &empty_networks_reply,
     "{\n"
     "    \"list\": [\n"
     "    ]\n"
     "}\n",
     "json_networks_empty"},
    {&json_formatter,
     &one_short_line_networks_reply,
     "{\n"
     "    \"list\": [\n"
     "        {\n"
     "            \"description\": \"Ether\",\n"
     "            \"name\": \"en0\",\n"
     "            \"type\": \"eth\"\n"
     "        }\n"
     "    ]\n"
     "}\n",
     "json_networks_one_short_line"},
    {&json_formatter,
     &one_long_line_networks_reply,
     "{\n"
     "    \"list\": [\n"
     "        {\n"
     "            \"description\": \"Amazingly fast and robust ethernet adapter\",\n"
     "            \"name\": \"enp3s0\",\n"
     "            \"type\": \"ethernet\"\n"
     "        }\n"
     "    ]\n"
     "}\n",
     "json_networks_one_long_line"},
    {&json_formatter,
     &multiple_lines_networks_reply,
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
     "json_networks_multiple_lines"}};

const auto empty_find_reply = construct_empty_reply();
const auto empty_find_reply_only_images = construct_empty_reply_only_images();
const auto empty_find_reply_only_blueprints = construct_empty_reply_only_blueprints();
const auto find_one_reply = construct_find_one_reply();
const auto find_one_blueprint_reply = construct_find_one_blueprint_reply();
const auto find_multiple_reply = construct_find_multiple_reply();
const auto find_one_reply_no_os = construct_find_one_reply_no_os();
const auto find_multiple_reply_duplicate_image = construct_find_multiple_reply_duplicate_image();

const auto json_empty_find_reply = "{\n"
                                   "    \"blueprints\": {\n"
                                   "    },\n"
                                   "    \"errors\": [\n"
                                   "    ],\n"
                                   "    \"images\": {\n"
                                   "    }\n"
                                   "}\n";
const auto csv_empty_find_reply = "Image,Remote,Aliases,OS,Release,Version,Type\n";
const auto yaml_empty_find_reply = "errors:\n"
                                   "  []\n"
                                   "blueprints:\n"
                                   "  {}\n"
                                   "images:\n"
                                   "  {}\n";

const std::vector<FormatterParamType> find_formatter_outputs{
    {&table_formatter, &empty_find_reply, "No images or blueprints found.\n", "table_find_empty"},
    {&table_formatter, &empty_find_reply_only_images, "No images found.\n", "table_find_empty_only_images"},
    {&table_formatter, &empty_find_reply_only_blueprints, "No blueprints found.\n", "table_find_empty_only_blueprints"},
    {&table_formatter, &find_one_reply,
     "Image                       Aliases           Version          Description\n"
     "ubuntu                                        20190516         Ubuntu 18.04 LTS\n"
     "\n",
     "table_find_one_image"},
    {&table_formatter, &find_one_blueprint_reply,
     "Blueprint                   Aliases           Version          Description\n"
     "anbox-cloud-appliance                         latest           Anbox Cloud Appliance\n"
     "\n",
     "table_find_one_blueprint"},
    {&table_formatter, &find_multiple_reply,
     "Image                       Aliases           Version          Description\n"
     "lts                                           20190516         Ubuntu 18.04 LTS\n"
     "daily:19.10                 eoan,devel        20190516         Ubuntu 19.10\n"
     "\n"
     "Blueprint                   Aliases           Version          Description\n"
     "anbox-cloud-appliance                         latest           Anbox Cloud Appliance\n"
     "\n",
     "table_find_multiple"},
    {&table_formatter, &find_one_reply_no_os,
     "Image                       Aliases           Version          Description\n"
     "snapcraft:core18                              20190520         Snapcraft builder for core18\n"
     "\n",
     "table_find_no_os"},
    {&table_formatter, &find_multiple_reply_duplicate_image,
     "Image                       Aliases           Version          Description\n"
     "core18                                        20190520         Ubuntu Core 18\n"
     "snapcraft:core18                              20190520         Snapcraft builder for core18\n"
     "\n",
     "table_find_multiple_duplicate_image"},
    {&json_formatter, &empty_find_reply, json_empty_find_reply, "json_find_empty"},
    {&json_formatter, &empty_find_reply_only_images, json_empty_find_reply, "json_find_empty_only_images"},
    {&json_formatter, &empty_find_reply_only_blueprints, json_empty_find_reply, "json_find_empty_only_blueprints"},
    {&json_formatter, &find_one_reply,
     "{\n"
     "    \"blueprints\": {\n"
     "    },\n"
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
    {&json_formatter, &find_one_blueprint_reply,
     "{\n"
     "    \"blueprints\": {\n"
     "        \"anbox-cloud-appliance\": {\n"
     "            \"aliases\": [\n"
     "            ],\n"
     "            \"os\": \"\",\n"
     "            \"release\": \"Anbox Cloud Appliance\",\n"
     "            \"remote\": \"\",\n"
     "            \"version\": \"latest\"\n"
     "        }\n"
     "    },\n"
     "    \"errors\": [\n"
     "    ],\n"
     "    \"images\": {\n"
     "    }\n"
     "}\n",
     "json_find_one_blueprint"},
    {&json_formatter, &find_multiple_reply,
     "{\n"
     "    \"blueprints\": {\n"
     "        \"anbox-cloud-appliance\": {\n"
     "            \"aliases\": [\n"
     "            ],\n"
     "            \"os\": \"\",\n"
     "            \"release\": \"Anbox Cloud Appliance\",\n"
     "            \"remote\": \"\",\n"
     "            \"version\": \"latest\"\n"
     "        }\n"
     "    },\n"
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
     "    \"blueprints\": {\n"
     "    },\n"
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
    {&csv_formatter, &empty_find_reply, csv_empty_find_reply, "csv_find_empty"},
    {&csv_formatter, &empty_find_reply_only_images, csv_empty_find_reply, "csv_find_empty_only_images"},
    {&csv_formatter, &empty_find_reply_only_blueprints, csv_empty_find_reply, "csv_find_empty_only_blueprints"},
    {&csv_formatter, &find_one_reply,
     "Image,Remote,Aliases,OS,Release,Version,Type\n"
     "ubuntu,,,Ubuntu,18.04 LTS,20190516,Cloud Image\n",
     "csv_find_one"},
    {&csv_formatter, &find_one_blueprint_reply,
     "Image,Remote,Aliases,OS,Release,Version,Type\n"
     "anbox-cloud-appliance,,,,Anbox Cloud Appliance,latest,Blueprint\n",
     "csv_find_one_blueprint"},
    {&csv_formatter, &find_multiple_reply,
     "Image,Remote,Aliases,OS,Release,Version,Type\n"
     "lts,,,Ubuntu,18.04 LTS,20190516,Cloud Image\n"
     "daily:19.10,daily,eoan;devel,Ubuntu,19.10,20190516,Cloud Image\n"
     "anbox-cloud-appliance,,,,Anbox Cloud Appliance,latest,Blueprint\n",
     "csv_find_multiple"},
    {&csv_formatter, &find_multiple_reply_duplicate_image,
     "Image,Remote,Aliases,OS,Release,Version,Type\n"
     "core18,,,Ubuntu,Core 18,20190520,Cloud Image\n"
     "snapcraft:core18,snapcraft,,,Snapcraft builder for core18,20190520,Cloud Image\n",
     "csv_find_multiple_duplicate_image"},
    {&yaml_formatter, &empty_find_reply, yaml_empty_find_reply, "yaml_find_empty"},
    {&yaml_formatter, &empty_find_reply_only_images, yaml_empty_find_reply, "yaml_find_empty_only_images"},
    {&yaml_formatter, &empty_find_reply_only_blueprints, yaml_empty_find_reply, "yaml_find_empty_only_blueprints"},
    {&yaml_formatter, &find_one_reply,
     "errors:\n"
     "  []\n"
     "blueprints:\n"
     "  {}\n"
     "images:\n"
     "  ubuntu:\n"
     "    aliases:\n"
     "      []\n"
     "    os: Ubuntu\n"
     "    release: 18.04 LTS\n"
     "    version: 20190516\n"
     "    remote: \"\"\n",
     "yaml_find_one"},
    {&yaml_formatter, &find_one_blueprint_reply,
     "errors:\n"
     "  []\n"
     "blueprints:\n"
     "  anbox-cloud-appliance:\n"
     "    aliases:\n"
     "      []\n"
     "    os: \"\"\n"
     "    release: Anbox Cloud Appliance\n"
     "    version: latest\n"
     "    remote: \"\"\n"
     "images:\n"
     "  {}\n",
     "yaml_find_one_blueprint"},
    {&yaml_formatter, &find_multiple_reply,
     "errors:\n"
     "  []\n"
     "blueprints:\n"
     "  anbox-cloud-appliance:\n"
     "    aliases:\n"
     "      []\n"
     "    os: \"\"\n"
     "    release: Anbox Cloud Appliance\n"
     "    version: latest\n"
     "    remote: \"\"\n"
     "images:\n"
     "  \"daily:19.10\":\n"
     "    aliases:\n"
     "      - eoan\n"
     "      - devel\n"
     "    os: Ubuntu\n"
     "    release: 19.10\n"
     "    version: 20190516\n"
     "    remote: daily\n"
     "  lts:\n"
     "    aliases:\n"
     "      []\n"
     "    os: Ubuntu\n"
     "    release: 18.04 LTS\n"
     "    version: 20190516\n"
     "    remote: \"\"\n",
     "yaml_find_multiple"},
    {&yaml_formatter, &find_multiple_reply_duplicate_image,
     "errors:\n"
     "  []\n"
     "blueprints:\n"
     "  {}\n"
     "images:\n"
     "  core18:\n"
     "    aliases:\n"
     "      []\n"
     "    os: Ubuntu\n"
     "    release: Core 18\n"
     "    version: 20190520\n"
     "    remote: \"\"\n"
     "  \"snapcraft:core18\":\n"
     "    aliases:\n"
     "      []\n"
     "    os: \"\"\n"
     "    release: Snapcraft builder for core18\n"
     "    version: 20190520\n"
     "    remote: snapcraft\n",
     "yaml_find_multiple_duplicate_image"}};

const auto version_client_reply = mp::VersionReply();
const auto version_daemon_no_update_reply = construct_version_info_multipassd_up_to_date();
const auto version_daemon_update_reply = construct_version_info_multipassd_update_available();

const std::vector<FormatterParamType> version_formatter_outputs{
    {&table_formatter, &version_client_reply, "multipass   Client version\n", "table_version_client"},
    {&table_formatter, &version_daemon_no_update_reply,
     "multipass   Client version\n"
     "multipassd  Daemon version\n",
     "table_version_daemon_no_updates"},
    {&table_formatter, &version_daemon_update_reply,
     "multipass   Client version\n"
     "multipassd  Daemon version\n"
     "\n##################################################\n"
     "update title information\n"
     "update description information\n"
     "\nGo here for more information: http://multipass.web\n"
     "##################################################\n",
     "table_version_daemon_updates"},
    {&json_formatter, &version_client_reply,
     "{\n"
     "    \"multipass\": \"Client version\"\n"
     "}\n",
     "json_version_client"},
    {&json_formatter, &version_daemon_no_update_reply,
     "{\n"
     "    \"multipass\": \"Client version\",\n"
     "    \"multipassd\": \"Daemon version\"\n"
     "}\n",
     "json_version_daemon_no_updates"},
    {&json_formatter, &version_daemon_update_reply,
     "{\n"
     "    \"multipass\": \"Client version\",\n"
     "    \"multipassd\": \"Daemon version\",\n"
     "    \"update\": {\n"
     "        \"description\": \"update description information\",\n"
     "        \"title\": \"update title information\",\n"
     "        \"url\": \"http://multipass.web\"\n"
     "    }\n"
     "}\n",
     "json_version_daemon_updates"},
    {&csv_formatter, &version_client_reply,
     "Multipass,Multipassd,Title,Description,URL\n"
     "Client version,,,,\n",
     "csv_version_client"},
    {&csv_formatter, &version_daemon_no_update_reply,
     "Multipass,Multipassd,Title,Description,URL\n"
     "Client version,Daemon version,,,\n",
     "csv_version_daemon_no_updates"},
    {&csv_formatter, &version_daemon_update_reply,
     "Multipass,Multipassd,Title,Description,URL\n"
     "Client version,Daemon version,update title information,update description information,http://multipass.web\n",
     "csv_version_daemon_updates"},
    {&yaml_formatter, &version_client_reply, "multipass: Client version\n", "yaml_version_client"},
    {&yaml_formatter, &version_daemon_no_update_reply,
     "multipass: Client version\n"
     "multipassd: Daemon version\n",
     "yaml_version_daemon_no_updates"},
    {&yaml_formatter, &version_daemon_update_reply,
     "multipass: Client version\n"
     "multipassd: Daemon version\n"
     "update:\n  title: update title information\n"
     "  description: update description information\n"
     "  url: \"http://multipass.web\"\n",
     "yaml_version_daemon_updates"}};

} // namespace

TEST_P(FormatterSuite, properly_formats_output)
{
    const auto& [formatter, reply, expected_output, test_name] = GetParam();
    Q_UNUSED(test_name); // gcc 7.4 can't do [[maybe_unused]] for structured bindings

    std::string output;

    if (auto input = dynamic_cast<const mp::ListReply*>(reply))
        output = formatter->format(*input);
    else if (auto input = dynamic_cast<const mp::NetworksReply*>(reply))
        output = formatter->format(*input);
    else if (auto input = dynamic_cast<const mp::InfoReply*>(reply))
        output = formatter->format(*input);
    else if (auto input = dynamic_cast<const mp::FindReply*>(reply))
        output = formatter->format(*input);
    else if (auto input = dynamic_cast<const mp::VersionReply*>(reply))
        output = formatter->format(*input, "Client version");
    else
        FAIL() << "Not a supported reply type.";

    EXPECT_EQ(output, expected_output);
}

INSTANTIATE_TEST_SUITE_P(OrderableListInfoOutputFormatter, FormatterSuite,
                         ValuesIn(orderable_list_info_formatter_outputs), print_param_name);
INSTANTIATE_TEST_SUITE_P(NonOrderableListInfoOutputFormatter, FormatterSuite,
                         ValuesIn(non_orderable_list_info_formatter_outputs), print_param_name);
INSTANTIATE_TEST_SUITE_P(FindOutputFormatter, FormatterSuite, ValuesIn(find_formatter_outputs), print_param_name);
INSTANTIATE_TEST_SUITE_P(NonOrderableNetworksOutputFormatter, FormatterSuite,
                         ValuesIn(non_orderable_networks_formatter_outputs), print_param_name);
INSTANTIATE_TEST_SUITE_P(VersionInfoOutputFormatter, FormatterSuite, ValuesIn(version_formatter_outputs),
                         print_param_name);

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

        if (input->has_instance_list())
            reply_copy.mutable_instance_list();
        else
            reply_copy.mutable_snapshot_list();

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
            regex = fmt::format("((Name|Instance)[[:print:]]*\n{0}[[:space:]]+.*)", petenv_name());
        else if (dynamic_cast<const mp::CSVFormatter*>(formatter))
            regex = fmt::format("(Name|Instance)[[:print:]]*\n{},.*", petenv_name());
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
            add_petenv_to_reply(reply_copy,
                                dynamic_cast<const mp::CSVFormatter*>(formatter),
                                test_name.find("snapshot") != std::string::npos);
            reply_copy.MergeFrom(*input);
        }
        else
        {
            reply_copy.CopyFrom(*input);
            add_petenv_to_reply(reply_copy,
                                dynamic_cast<const mp::CSVFormatter*>(formatter),
                                test_name.find("snapshot") != std::string::npos);
        }
        output = formatter->format(reply_copy);

        if (dynamic_cast<const mp::TableFormatter*>(formatter))
            regex = fmt::format("(Name:[[:space:]]+{0}.+)"
                                "(Snapshot:[[:print:]]*\nInstance:[[:space:]]+{0}.+)",
                                petenv_name());
        else if (dynamic_cast<const mp::CSVFormatter*>(formatter))
            regex = fmt::format("(Name[[:print:]]*\n{0},.*)|"
                                "(Snapshot[[:print:]]*\n[[:print:]]*,{0},.*)",
                                petenv_name());
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
