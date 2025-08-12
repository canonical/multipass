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
#include "file_operations.h"
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
    fundamentals->set_comment(
        "A very long comment that should be truncated by the table formatter");
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
    fundamentals->set_comment(
        "Before restoring snap1\nContains a newline that\r\nshould be truncated");
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
    info_entry->mutable_instance_info()->set_id(
        "1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac");

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
    info_entry->mutable_instance_info()->set_id(
        "1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac");

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
    info_entry->mutable_instance_info()->set_id(
        "ab5191cc172564e7cc0eafd397312a32598823e645279c820f0935393aead509");
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
    info_entry->mutable_instance_info()->set_id(
        "ab5191cc172564e7cc0eafd397312a32598823e645279c820f0935393aead509");
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
    info_entry->mutable_instance_info()->set_id(
        "1797c5c82016c1e65f4008fcf89deae3a044ef76087a9ec5b907c6d64a3609ac");

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
    info_entry->mutable_instance_info()->set_id(
        "ab5191cc172564e7cc0eafd397312a32598823e645279c820f0935393aead509");
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
        entry->mutable_instance_info()->set_id(
            "1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd");
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
    return reply;
}

auto construct_find_one_reply()
{
    auto reply = mp::FindReply();

    auto image_entry = reply.add_images_info();
    image_entry->set_os("Ubuntu");
    image_entry->set_release("18.04 LTS");
    image_entry->set_version("20190516");
    image_entry->add_aliases("ubuntu");

    return reply;
}

auto construct_find_one_reply_no_os()
{
    auto reply = mp::FindReply();

    auto image_entry = reply.add_images_info();
    image_entry->set_release("Snapcraft builder for core18");
    image_entry->set_version("20190520");
    image_entry->add_aliases("core18");
    image_entry->set_remote_name("snapcraft");

    return reply;
}

auto construct_find_multiple_reply()
{
    auto reply = mp::FindReply();

    auto image_entry = reply.add_images_info();
    image_entry->set_os("Ubuntu");
    image_entry->set_release("18.04 LTS");
    image_entry->set_version("20190516");
    image_entry->add_aliases("ubuntu");
    image_entry->add_aliases("lts");
    image_entry->set_remote_name("release");

    image_entry = reply.add_images_info();
    image_entry->set_os("Ubuntu");
    image_entry->set_release("19.10");
    image_entry->set_version("20190516");
    image_entry->add_aliases("19.10");
    image_entry->add_aliases("eoan");
    image_entry->add_aliases("devel");
    image_entry->set_remote_name("daily");

    return reply;
}

auto construct_find_multiple_reply_duplicate_image()
{
    auto reply = mp::FindReply();

    auto image_entry = reply.add_images_info();
    image_entry->set_os("Ubuntu");
    image_entry->set_release("Core 18");
    image_entry->set_version("20190520");
    image_entry->add_aliases("core18");
    image_entry->set_remote_name("core");

    image_entry = reply.add_images_info();
    image_entry->set_release("Snapcraft builder for core18");
    image_entry->set_version("20190520");
    image_entry->add_aliases("core18");
    image_entry->set_remote_name("snapcraft");

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
        EXPECT_CALL(mock_format_utils, convert_to_user_locale(_))
            .WillRepeatedly([](const auto timestamp) {
                return google::protobuf::util::TimeUtil::ToString(timestamp);
            });
    }

    ~BaseFormatterSuite()
    {
        std::locale::global(saved_locale);
    }

protected:
    mpt::MockSettings::GuardedMock mock_settings_injection =
        mpt::MockSettings::inject<StrictMock>();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;

    mpt::MockFormatUtils::GuardedMock mock_format_utils_injection =
        mpt::MockFormatUtils::inject<NiceMock>();
    mpt::MockFormatUtils& mock_format_utils = *mock_format_utils_injection.first;

private:
    std::locale saved_locale;
};

typedef std::tuple<const mp::Formatter*,
                   const ::google::protobuf::Message*,
                   std::string /* output */,
                   std::string /* test name */>
    FormatterParamType;

struct FormatterSuite : public BaseFormatterSuite, public WithParamInterface<FormatterParamType>
{
};

auto print_param_name(const testing::TestParamInfo<FormatterSuite::ParamType>& info)
{
    return std::get<3>(info.param);
}

struct PetenvFormatterSuite
    : public BaseFormatterSuite,
      public WithParamInterface<std::tuple<QString, bool, FormatterParamType>>
{
};

auto print_petenv_param_name(const testing::TestParamInfo<PetenvFormatterSuite::ParamType>& info)
{
    const auto param_name = std::get<3>(std::get<2>(info.param));
    const auto petenv_name = std::get<0>(info.param);
    const auto prepend = std::get<1>(info.param);
    return fmt::format("{}_{}_{}",
                       param_name,
                       petenv_name.isEmpty() ? "default" : petenv_name,
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
const auto mixed_instance_and_snapshot_info_reply =
    construct_mixed_instance_and_snapshot_info_reply();
const auto multiple_mixed_instances_and_snapshots_info_reply =
    construct_multiple_mixed_instances_and_snapshots_info_reply();

const std::vector<FormatterParamType> orderable_list_info_formatter_outputs{
    {&table_formatter,
     &empty_list_reply,
     mpt::load_test_file("formatters/table/empty_list_reply.txt").toStdString(),
     "table_list_empty"},
    {&table_formatter,
     &empty_list_snapshot_reply,
     mpt::load_test_file("formatters/table/empty_list_snapshot_reply.txt").toStdString(),
     "table_list_snapshot_empty"},
    {&table_formatter,
     &single_instance_list_reply,
     mpt::load_test_file("formatters/table/single_instance_list_reply.txt").toStdString(),
     "table_list_single"},
    {&table_formatter,
     &multiple_instances_list_reply,
     mpt::load_test_file("formatters/table/multiple_instances_list_reply.txt").toStdString(),
     "table_list_multiple"},
    {&table_formatter,
     &unsorted_list_reply,
     mpt::load_test_file("formatters/table/unsorted_list_reply.txt").toStdString(),
     "table_list_unsorted"},
    {&table_formatter,
     &single_snapshot_list_reply,
     mpt::load_test_file("formatters/table/single_snapshot_list_reply.txt").toStdString(),
     "table_list_single_snapshot"},
    {&table_formatter,
     &multiple_snapshots_list_reply,
     mpt::load_test_file("formatters/table/multiple_snapshots_list_reply.txt").toStdString(),
     "table_list_multiple_snapshots"},

    {&table_formatter,
     &empty_info_reply,
     mpt::load_test_file("formatters/table/empty_info_reply.txt").toStdString(),
     "table_info_empty"},
    {&table_formatter,
     &empty_info_snapshot_reply,
     mpt::load_test_file("formatters/table/empty_info_snapshot_reply.txt").toStdString(),
     "table_info_snapshot_empty"},
    {&table_formatter,
     &single_instance_info_reply,
     mpt::load_test_file("formatters/table/single_instance_info_reply.txt").toStdString(),
     "table_info_single_instance"},
    {&table_formatter,
     &multiple_instances_info_reply,
     mpt::load_test_file("formatters/table/multiple_instances_info_reply.txt").toStdString(),
     "table_info_multiple_instances"},
    {&table_formatter,
     &single_snapshot_info_reply,
     mpt::load_test_file("formatters/table/single_snapshot_info_reply.txt").toStdString(),
     "table_info_single_snapshot"},
    {&table_formatter,
     &multiple_snapshots_info_reply,
     mpt::load_test_file("formatters/table/multiple_snapshots_info_reply.txt").toStdString(),
     "table_info_multiple_snapshots"},
    {&table_formatter,
     &mixed_instance_and_snapshot_info_reply,
     mpt::load_test_file("formatters/table/mixed_instance_and_snapshot_info_reply.txt")
         .toStdString(),
     "table_info_mixed_instance_and_snapshot"},
    {&table_formatter,
     &multiple_mixed_instances_and_snapshots_info_reply,
     mpt::load_test_file("formatters/table/multiple_mixed_instances_and_snapshots_info_reply.txt")
         .toStdString(),
     "table_info_multiple_mixed_instances_and_snapshots"},

    {&csv_formatter,
     &empty_list_reply,
     mpt::load_test_file("formatters/csv/empty_list_reply.csv").toStdString(),
     "csv_list_empty"},
    {&csv_formatter,
     &single_instance_list_reply,
     mpt::load_test_file("formatters/csv/single_instance_list_reply.csv").toStdString(),
     "csv_list_single"},
    {&csv_formatter,
     &multiple_instances_list_reply,
     mpt::load_test_file("formatters/csv/multiple_instances_list_reply.csv").toStdString(),
     "csv_list_multiple"},
    {&csv_formatter,
     &unsorted_list_reply,
     mpt::load_test_file("formatters/csv/unsorted_list_reply.csv").toStdString(),
     "csv_list_unsorted"},
    {&csv_formatter,
     &empty_list_snapshot_reply,
     mpt::load_test_file("formatters/csv/empty_list_snapshot_reply.csv").toStdString(),
     "csv_list_snapshot_empty"},
    {&csv_formatter,
     &single_snapshot_list_reply,
     mpt::load_test_file("formatters/csv/single_snapshot_list_reply.csv").toStdString(),
     "csv_list_single_snapshot"},
    {&csv_formatter,
     &multiple_snapshots_list_reply,
     mpt::load_test_file("formatters/csv/multiple_snapshots_list_reply.csv").toStdString(),
     "csv_list_multiple_snapshots"},

    {&csv_formatter, &empty_info_reply, "", "csv_info_empty"},
    {&csv_formatter,
     &single_instance_info_reply,
     mpt::load_test_file("formatters/csv/single_instance_info_reply.csv").toStdString(),
     "csv_info_single_instance"},
    {&csv_formatter,
     &single_snapshot_info_reply,
     mpt::load_test_file("formatters/csv/single_snapshot_info_reply.csv").toStdString(),
     "csv_info_single_snapshot_info_reply"},
    {&csv_formatter,
     &multiple_snapshots_info_reply,
     mpt::load_test_file("formatters/csv/multiple_snapshots_info_reply.csv").toStdString(),
     "csv_info_multiple_snapshot_info_reply"},
    {&csv_formatter,
     &multiple_instances_info_reply,
     mpt::load_test_file("formatters/csv/multiple_instances_info_reply.csv").toStdString(),
     "csv_info_multiple_instances"},

    {&yaml_formatter,
     &empty_list_reply,
     mpt::load_test_file("formatters/yaml/empty_list_reply.yaml").toStdString(),
     "yaml_list_empty"},
    {&yaml_formatter,
     &single_instance_list_reply,
     mpt::load_test_file("formatters/yaml/single_instance_list_reply.yaml").toStdString(),
     "yaml_list_single"},
    {&yaml_formatter,
     &multiple_instances_list_reply,
     mpt::load_test_file("formatters/yaml/multiple_instances_list_reply.yaml").toStdString(),
     "yaml_list_multiple"},
    {&yaml_formatter,
     &unsorted_list_reply,
     mpt::load_test_file("formatters/yaml/unsorted_list_reply.yaml").toStdString(),
     "yaml_list_unsorted"},
    {&yaml_formatter, &empty_list_snapshot_reply, "\n", "yaml_list_snapshot_empty"},
    {&yaml_formatter,
     &single_snapshot_list_reply,
     mpt::load_test_file("formatters/yaml/single_snapshot_list_reply.yaml").toStdString(),
     "yaml_list_single_snapshot"},
    {&yaml_formatter,
     &multiple_snapshots_list_reply,
     mpt::load_test_file("formatters/yaml/multiple_snapshots_list_reply.yaml").toStdString(),
     "yaml_list_multiple_snapshots"},

    {&yaml_formatter,
     &empty_info_reply,
     mpt::load_test_file("formatters/yaml/empty_info_reply.yaml").toStdString(),
     "yaml_info_empty"},
    {&yaml_formatter,
     &single_instance_info_reply,
     mpt::load_test_file("formatters/yaml/single_instance_info_reply.yaml").toStdString(),
     "yaml_info_single_instance"},
    {&yaml_formatter,
     &multiple_instances_info_reply,
     mpt::load_test_file("formatters/yaml/multiple_instances_info_reply.yaml").toStdString(),
     "yaml_info_multiple_instances"},
    {&yaml_formatter,
     &single_snapshot_info_reply,
     mpt::load_test_file("formatters/yaml/single_snapshot_info_reply.yaml").toStdString(),
     "yaml_info_single_snapshot_info_reply"},
    {&yaml_formatter,
     &multiple_snapshots_info_reply,
     mpt::load_test_file("formatters/yaml/multiple_snapshots_info_reply.yaml").toStdString(),
     "yaml_info_multiple_snapshots_info_reply"},
    {&yaml_formatter,
     &mixed_instance_and_snapshot_info_reply,
     mpt::load_test_file("formatters/yaml/mixed_instance_and_snapshot_info_reply.yaml")
         .toStdString(),
     "yaml_info_mixed_instance_and_snapshot_info_reply"},
    {&yaml_formatter,
     &multiple_mixed_instances_and_snapshots_info_reply,
     mpt::load_test_file("formatters/yaml/multiple_mixed_instances_and_snapshots_info_reply.yaml")
         .toStdString(),
     "yaml_info_multiple_mixed_instances_and_snapshots"}};

const std::vector<FormatterParamType> non_orderable_list_info_formatter_outputs{
    {&json_formatter,
     &empty_list_reply,
     mpt::load_test_file("formatters/json/empty_list_reply.json").toStdString(),
     "json_list_empty"},
    {&json_formatter,
     &single_instance_list_reply,
     mpt::load_test_file("formatters/json/single_instance_list_reply.json").toStdString(),
     "json_list_single"},
    {&json_formatter,
     &multiple_instances_list_reply,
     mpt::load_test_file("formatters/json/multiple_instances_list_reply.json").toStdString(),
     "json_list_multiple"},
    {&json_formatter,
     &single_snapshot_list_reply,
     mpt::load_test_file("formatters/json/single_snapshot_list_reply.json").toStdString(),
     "json_list_single_snapshot"},
    {&json_formatter,
     &multiple_snapshots_list_reply,
     mpt::load_test_file("formatters/json/multiple_snapshots_list_reply.json").toStdString(),
     "json_list_multiple_snapshots"},
    {&json_formatter,
     &empty_info_reply,
     mpt::load_test_file("formatters/json/empty_info_reply.json").toStdString(),
     "json_info_empty"},
    {&json_formatter,
     &single_instance_info_reply,
     mpt::load_test_file("formatters/json/single_instance_info_reply.json").toStdString(),
     "json_info_single_instance"},
    {&json_formatter,
     &multiple_instances_info_reply,
     mpt::load_test_file("formatters/json/multiple_instances_info_reply.json").toStdString(),
     "json_info_multiple_instances"},
    {&json_formatter,
     &single_snapshot_info_reply,
     mpt::load_test_file("formatters/json/single_snapshot_info_reply.json").toStdString(),
     "json_info_single_snapshot_info_reply"},
    {&json_formatter,
     &multiple_snapshots_info_reply,
     mpt::load_test_file("formatters/json/multiple_snapshots_info_reply.json").toStdString(),
     "json_info_multiple_snapshots_info_reply"},
    {&json_formatter,
     &mixed_instance_and_snapshot_info_reply,
     mpt::load_test_file("formatters/json/mixed_instance_and_snapshot_info_reply.json")
         .toStdString(),
     "json_info_mixed_instance_and_snapshot_info_reply"},
    {&json_formatter,
     &multiple_mixed_instances_and_snapshots_info_reply,
     mpt::load_test_file("formatters/json/multiple_mixed_instances_and_snapshots_info_reply.json")
         .toStdString(),
     "json_info_multiple_mixed_instances_and_snapshots"}};

const std::vector<FormatterParamType> non_orderable_networks_formatter_outputs{
    {&table_formatter,
     &empty_networks_reply,
     mpt::load_test_file("formatters/table/empty_networks_reply.txt").toStdString(),
     "table_networks_empty"},
    {&table_formatter,
     &one_short_line_networks_reply,
     mpt::load_test_file("formatters/table/one_short_line_networks_reply.txt").toStdString(),
     "table_networks_one_short_line"},
    {&table_formatter,
     &one_long_line_networks_reply,
     mpt::load_test_file("formatters/table/one_long_line_networks_reply.txt").toStdString(),
     "table_networks_one_long_line"},
    {&table_formatter,
     &multiple_lines_networks_reply,
     mpt::load_test_file("formatters/table/multiple_lines_networks_reply.txt").toStdString(),
     "table_networks_multiple_lines"},

    {&csv_formatter,
     &empty_networks_reply,
     mpt::load_test_file("formatters/csv/empty_networks_reply.csv").toStdString(),
     "csv_networks_empty"},
    {&csv_formatter,
     &one_short_line_networks_reply,
     mpt::load_test_file("formatters/csv/one_short_line_networks_reply.csv").toStdString(),
     "csv_networks_one_short_line"},
    {&csv_formatter,
     &one_long_line_networks_reply,
     mpt::load_test_file("formatters/csv/one_long_line_networks_reply.csv").toStdString(),
     "csv_networks_one_long_line"},
    {&csv_formatter,
     &multiple_lines_networks_reply,
     mpt::load_test_file("formatters/csv/multiple_lines_networks_reply.csv").toStdString(),
     "csv_networks_multiple_lines"},

    {&yaml_formatter,
     &empty_networks_reply,
     mpt::load_test_file("formatters/yaml/empty_networks_reply.yaml").toStdString(),
     "yaml_networks_empty"},
    {&yaml_formatter,
     &one_short_line_networks_reply,
     mpt::load_test_file("formatters/yaml/one_short_line_networks_reply.yaml").toStdString(),
     "yaml_networks_one_short_line"},
    {&yaml_formatter,
     &one_long_line_networks_reply,
     mpt::load_test_file("formatters/yaml/one_long_line_networks_reply.yaml").toStdString(),
     "yaml_networks_one_long_line"},
    {&yaml_formatter,
     &multiple_lines_networks_reply,
     mpt::load_test_file("formatters/yaml/multiple_lines_networks_reply.yaml").toStdString(),
     "yaml_networks_multiple_lines"},

    {&json_formatter,
     &empty_networks_reply,
     mpt::load_test_file("formatters/json/empty_networks_reply.json").toStdString(),
     "json_networks_empty"},
    {&json_formatter,
     &one_short_line_networks_reply,
     mpt::load_test_file("formatters/json/one_short_line_networks_reply.json").toStdString(),
     "json_networks_one_short_line"},
    {&json_formatter,
     &one_long_line_networks_reply,
     mpt::load_test_file("formatters/json/one_long_line_networks_reply.json").toStdString(),
     "json_networks_one_long_line"},
    {&json_formatter,
     &multiple_lines_networks_reply,
     mpt::load_test_file("formatters/json/multiple_lines_networks_reply.json").toStdString(),
     "json_networks_multiple_lines"}};

const auto empty_find_reply = construct_empty_reply();
const auto find_one_reply = construct_find_one_reply();
const auto find_multiple_reply = construct_find_multiple_reply();
const auto find_one_reply_no_os = construct_find_one_reply_no_os();
const auto find_multiple_reply_duplicate_image = construct_find_multiple_reply_duplicate_image();

const std::vector<FormatterParamType> find_formatter_outputs{
    {&table_formatter,
     &empty_find_reply,
     mpt::load_test_file("formatters/table/empty_find_reply.txt").toStdString(),
     "table_find_empty"},
    {&table_formatter,
     &find_one_reply,
     mpt::load_test_file("formatters/table/find_one_reply.txt").toStdString(),
     "table_find_one_image"},
    {&table_formatter,
     &find_multiple_reply,
     mpt::load_test_file("formatters/table/find_multiple_reply.txt").toStdString(),
     "table_find_multiple"},
    {&table_formatter,
     &find_one_reply_no_os,
     mpt::load_test_file("formatters/table/find_one_reply_no_os.txt").toStdString(),
     "table_find_no_os"},
    {&table_formatter,
     &find_multiple_reply_duplicate_image,
     mpt::load_test_file("formatters/table/find_multiple_reply_duplicate_image.txt").toStdString(),
     "table_find_multiple_duplicate_image"},
    {&json_formatter,
     &empty_find_reply,
     mpt::load_test_file("formatters/json/empty_find_reply.json").toStdString(),
     "json_find_empty"},
    {&json_formatter,
     &find_one_reply,
     mpt::load_test_file("formatters/json/find_one_reply.json").toStdString(),
     "json_find_one"},
    {&json_formatter,
     &find_multiple_reply,
     mpt::load_test_file("formatters/json/find_multiple_reply.json").toStdString(),
     "json_find_multiple"},
    {&json_formatter,
     &find_multiple_reply_duplicate_image,
     mpt::load_test_file("formatters/json/find_multiple_reply_duplicate_image.json").toStdString(),
     "json_find_multiple_duplicate_image"},
    {&csv_formatter,
     &empty_find_reply,
     mpt::load_test_file("formatters/csv/empty_find_reply.csv").toStdString(),
     "csv_find_empty"},
    {&csv_formatter,
     &find_one_reply,
     mpt::load_test_file("formatters/csv/find_one_reply.csv").toStdString(),
     "csv_find_one"},
    {&csv_formatter,
     &find_multiple_reply,
     mpt::load_test_file("formatters/csv/find_multiple_reply.csv").toStdString(),
     "csv_find_multiple"},
    {&csv_formatter,
     &find_multiple_reply_duplicate_image,
     mpt::load_test_file("formatters/csv/find_multiple_reply_duplicate_image.csv").toStdString(),
     "csv_find_multiple_duplicate_image"},
    {&yaml_formatter,
     &empty_find_reply,
     mpt::load_test_file("formatters/yaml/empty_find_reply.yaml").toStdString(),
     "yaml_find_empty"},
    {&yaml_formatter,
     &find_one_reply,
     mpt::load_test_file("formatters/yaml/find_one_reply.yaml").toStdString(),
     "yaml_find_one"},
    {&yaml_formatter,
     &find_multiple_reply,
     mpt::load_test_file("formatters/yaml/find_multiple_reply.yaml").toStdString(),
     "yaml_find_multiple"},
    {&yaml_formatter,
     &find_multiple_reply_duplicate_image,
     mpt::load_test_file("formatters/yaml/find_multiple_reply_duplicate_image.yaml").toStdString(),
     "yaml_find_multiple_duplicate_image"}};

const auto version_client_reply = mp::VersionReply();
const auto version_daemon_no_update_reply = construct_version_info_multipassd_up_to_date();
const auto version_daemon_update_reply = construct_version_info_multipassd_update_available();

const std::vector<FormatterParamType> version_formatter_outputs{
    {&table_formatter,
     &version_client_reply,
     mpt::load_test_file("formatters/table/version_client_reply.txt").toStdString(),
     "table_version_client"},
    {&table_formatter,
     &version_daemon_no_update_reply,
     mpt::load_test_file("formatters/table/version_daemon_no_update_reply.txt").toStdString(),
     "table_version_daemon_no_updates"},
    {&table_formatter,
     &version_daemon_update_reply,
     mpt::load_test_file("formatters/table/version_daemon_update_reply.txt").toStdString(),
     "table_version_daemon_updates"},
    {&json_formatter,
     &version_client_reply,
     mpt::load_test_file("formatters/json/version_client_reply.json").toStdString(),
     "json_version_client"},
    {&json_formatter,
     &version_daemon_no_update_reply,
     mpt::load_test_file("formatters/json/version_daemon_no_update_reply.json").toStdString(),
     "json_version_daemon_no_updates"},
    {&json_formatter,
     &version_daemon_update_reply,
     mpt::load_test_file("formatters/json/version_daemon_update_reply.json").toStdString(),
     "json_version_daemon_updates"},
    {&csv_formatter,
     &version_client_reply,
     mpt::load_test_file("formatters/csv/version_client_reply.csv").toStdString(),
     "csv_version_client"},
    {&csv_formatter,
     &version_daemon_no_update_reply,
     mpt::load_test_file("formatters/csv/version_daemon_no_update_reply.csv").toStdString(),
     "csv_version_daemon_no_updates"},
    {&csv_formatter,
     &version_daemon_update_reply,
     mpt::load_test_file("formatters/csv/version_daemon_update_reply.csv").toStdString(),
     "csv_version_daemon_updates"},
    {&yaml_formatter,
     &version_client_reply,
     mpt::load_test_file("formatters/yaml/version_client_reply.yaml").toStdString(),
     "yaml_version_client"},
    {&yaml_formatter,
     &version_daemon_no_update_reply,
     mpt::load_test_file("formatters/yaml/version_daemon_no_update_reply.yaml").toStdString(),
     "yaml_version_daemon_no_updates"},
    {&yaml_formatter,
     &version_daemon_update_reply,
     mpt::load_test_file("formatters/yaml/version_daemon_update_reply.yaml").toStdString(),
     "yaml_version_daemon_updates"}};

} // namespace

TEST_P(FormatterSuite, properlyFormatsOutput)
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

INSTANTIATE_TEST_SUITE_P(OrderableListInfoOutputFormatter,
                         FormatterSuite,
                         ValuesIn(orderable_list_info_formatter_outputs),
                         print_param_name);
INSTANTIATE_TEST_SUITE_P(NonOrderableListInfoOutputFormatter,
                         FormatterSuite,
                         ValuesIn(non_orderable_list_info_formatter_outputs),
                         print_param_name);
INSTANTIATE_TEST_SUITE_P(FindOutputFormatter,
                         FormatterSuite,
                         ValuesIn(find_formatter_outputs),
                         print_param_name);
INSTANTIATE_TEST_SUITE_P(NonOrderableNetworksOutputFormatter,
                         FormatterSuite,
                         ValuesIn(non_orderable_networks_formatter_outputs),
                         print_param_name);
INSTANTIATE_TEST_SUITE_P(VersionInfoOutputFormatter,
                         FormatterSuite,
                         ValuesIn(version_formatter_outputs),
                         print_param_name);

#if GTEST_HAS_POSIX_RE
TEST_P(PetenvFormatterSuite, petEnvFirstInOutput)
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
            regex =
                fmt::format("(errors:[[:space:]]+-[[:space:]]+~[[:space:]]+)?{}:.*", petenv_name());
        else
            FAIL() << "Not a supported formatter.";
    }
    else
        FAIL() << "Not a supported reply type.";

    EXPECT_THAT(output, MatchesRegex(regex));
}

INSTANTIATE_TEST_SUITE_P(PetenvOutputFormatter,
                         PetenvFormatterSuite,
                         Combine(Values(QStringLiteral(),
                                        QStringLiteral("aaa"),
                                        QStringLiteral("zzz")) /* primary name */,
                                 Bool() /* prepend or append */,
                                 ValuesIn(orderable_list_info_formatter_outputs)),
                         print_petenv_param_name);
#endif
