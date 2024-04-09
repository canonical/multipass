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
#include "mock_file_ops.h"
#include "temp_dir.h"

#include <multipass/cloud_init_iso.h>
#include <multipass/network_interface.h>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
constexpr std::string_view meta_data_content = R"(#cloud-config
instance-id: vm1
local-hostname: vm1
cloud-name: multipass)";

auto read_returns_failed_ifstream = [](std::ifstream& file, char*, std::streamsize) -> std::ifstream& {
    file.setstate(std::ios::failbit);
    return file;
};

// Since we can have a valid file to read from and FileOps::read is a cheap function, falls back to the original
// implementation would be an easy way to implement EXPECT_CALL multiple times instead of mocking multiple single calls
// with the correct output
auto original_implementation_of_read = [](std::ifstream& file, char* buffer, std::streamsize pos) -> std::ifstream& {
    return MP_FILEOPS.FileOps::read(file, buffer, pos);
};
} // namespace

struct CloudInitIso : public Test
{
    CloudInitIso()
    {
        iso_path = QDir{temp_dir.path()}.filePath("test.iso");
    }
    mpt::TempDir temp_dir;
    QString iso_path;
};

TEST_F(CloudInitIso, check_contains_false)
{
    mp::CloudInitIso iso;
    EXPECT_FALSE(iso.contains("non_exist_file"));
}

TEST_F(CloudInitIso, check_contains_true)
{
    mp::CloudInitIso iso;
    iso.add_file("test", "test data");
    EXPECT_TRUE(iso.contains("test"));
}

TEST_F(CloudInitIso, check_erase_false)
{
    mp::CloudInitIso iso;
    EXPECT_FALSE(iso.erase("non_exist_file"));
}

TEST_F(CloudInitIso, check_erase_true)
{
    mp::CloudInitIso iso;
    iso.add_file("test", "test data");
    EXPECT_TRUE(iso.contains("test"));
    EXPECT_TRUE(iso.erase("test"));
    EXPECT_FALSE(iso.contains("test"));
}

TEST_F(CloudInitIso, check_at_operator_throw)
{
    mp::CloudInitIso iso;
    MP_EXPECT_THROW_THAT(
        iso.at("non_exist_file"),
        std::runtime_error,
        mpt::match_what(StrEq("Did not find the target file non_exist_file in the CloudInitIso instance. ")));
}

TEST_F(CloudInitIso, check_at_operator_found_key)
{
    mp::CloudInitIso iso;
    iso.add_file("test", "test data");
    EXPECT_EQ(iso.at("test"), "test data");
}

TEST_F(CloudInitIso, check_index_operator_not_exist_default_return)
{
    mp::CloudInitIso iso;
    EXPECT_EQ(iso["test"], std::string());
}

TEST_F(CloudInitIso, check_index_operator_found_key)
{
    mp::CloudInitIso iso;
    iso.add_file("test", "test data");
    EXPECT_EQ(iso["test"], "test data");
}

TEST_F(CloudInitIso, creates_iso_file)
{
    mp::CloudInitIso iso;
    iso.add_file("test", "test data");
    iso.write_to(iso_path);

    QFile file{iso_path};
    EXPECT_TRUE(file.exists());
    EXPECT_THAT(file.size(), Ge(0));
}

TEST_F(CloudInitIso, reads_iso_file_failed_to_open_file)
{
    mp::CloudInitIso original_iso;
    original_iso.write_to(iso_path);

    const auto [mock_file_ops, _] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, is_open(An<const std::ifstream&>())).WillOnce(Return(false));
    mp::CloudInitIso new_iso;
    MP_EXPECT_THROW_THAT(new_iso.read_from(std::filesystem::path(iso_path.toStdString())),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("Failed to open file")));
}

TEST_F(CloudInitIso, reads_iso_file_failed_to_read_single_bytes)
{
    mp::CloudInitIso original_iso;
    original_iso.write_to(iso_path);

    const auto [mock_file_ops, _] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, is_open(An<const std::ifstream&>())).WillOnce(Return(true));

    EXPECT_CALL(*mock_file_ops, read(An<std::ifstream&>(), A<char*>(), A<std::streamsize>()))
        .WillOnce(read_returns_failed_ifstream);

    // failed on the first read_single_byte call
    mp::CloudInitIso new_iso;
    MP_EXPECT_THROW_THAT(new_iso.read_from(std::filesystem::path(iso_path.toStdString())),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("Can not read the next byte data from file at")));
}

TEST_F(CloudInitIso, reads_iso_file_failed_to_check_it_has_Joliet_volume_descriptor)
{
    mp::CloudInitIso original_iso;
    original_iso.write_to(iso_path);

    const auto [mock_file_ops, _] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, is_open(An<const std::ifstream&>())).WillOnce(Return(true));

    // value 2_u8 is for Joliet volume descriptor
    auto read_returns_one_byte_value_one = [](std::ifstream& file, char* one_byte, std::streamsize) -> std::ifstream& {
        const int non_joliet_volume_des_num{1U};
        *one_byte = static_cast<std::uint8_t>(non_joliet_volume_des_num);
        return file;
    };
    EXPECT_CALL(*mock_file_ops, read(An<std::ifstream&>(), A<char*>(), A<std::streamsize>()))
        .WillOnce(read_returns_one_byte_value_one);

    mp::CloudInitIso new_iso;
    MP_EXPECT_THROW_THAT(new_iso.read_from(std::filesystem::path(iso_path.toStdString())),
                         std::runtime_error,
                         mpt::match_what(StrEq("The Joliet volume descriptor is not in place.")));
}

TEST_F(CloudInitIso, reads_iso_file_Joliet_volume_descriptor_malformed)
{
    mp::CloudInitIso original_iso;
    original_iso.write_to(iso_path);

    const auto [mock_file_ops, _] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, is_open(An<const std::ifstream&>())).WillOnce(Return(true));

    const InSequence seq;
    EXPECT_CALL(*mock_file_ops, read(An<std::ifstream&>(), A<char*>(), A<std::streamsize>()))
        .WillOnce(original_implementation_of_read);

    auto read_returns_five_bytes_string = [](std::ifstream& file, char* buffer, std::streamsize) -> std::ifstream& {
        std::strcpy(buffer, "NonJo");
        return file;
    };
    EXPECT_CALL(*mock_file_ops, read(An<std::ifstream&>(), A<char*>(), A<std::streamsize>()))
        .WillOnce(read_returns_five_bytes_string);

    mp::CloudInitIso new_iso;
    MP_EXPECT_THROW_THAT(new_iso.read_from(std::filesystem::path(iso_path.toStdString())),
                         std::runtime_error,
                         mpt::match_what(StrEq("The Joliet descriptor is malformed.")));
}

TEST_F(CloudInitIso, reads_iso_file_failed_to_read_array)
{
    mp::CloudInitIso original_iso;
    original_iso.write_to(iso_path);

    const auto [mock_file_ops, _] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, is_open(An<const std::ifstream&>())).WillOnce(Return(true));

    const InSequence seq;
    EXPECT_CALL(*mock_file_ops, read(An<std::ifstream&>(), A<char*>(), A<std::streamsize>()))
        .WillOnce(original_implementation_of_read);

    EXPECT_CALL(*mock_file_ops, read(An<std::ifstream&>(), A<char*>(), A<std::streamsize>()))
        .WillOnce(read_returns_failed_ifstream);

    mp::CloudInitIso new_iso;
    MP_EXPECT_THROW_THAT(new_iso.read_from(std::filesystem::path(iso_path.toStdString())),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("bytes data from file at")));
}

TEST_F(CloudInitIso, reads_iso_file_failed_to_check_root_dir_record_data)
{
    mp::CloudInitIso original_iso;
    original_iso.write_to(iso_path);

    const auto [mock_file_ops, _] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, is_open(An<const std::ifstream&>())).WillOnce(Return(true));

    const InSequence seq;
    EXPECT_CALL(*mock_file_ops, read(An<std::ifstream&>(), A<char*>(), A<std::streamsize>()))
        .Times(2)
        .WillRepeatedly(original_implementation_of_read);

    // default buffer values makes the buffer[0] not 34_u8 which causes root directory record data checking fail
    auto read_return_default_buffer = [](std::ifstream& file, char* buffer, std::streamsize) -> std::ifstream& {
        return file;
    };

    EXPECT_CALL(*mock_file_ops, read(An<std::ifstream&>(), A<char*>(), A<std::streamsize>()))
        .WillOnce(read_return_default_buffer);

    mp::CloudInitIso new_iso;
    MP_EXPECT_THROW_THAT(new_iso.read_from(std::filesystem::path(iso_path.toStdString())),
                         std::runtime_error,
                         mpt::match_what(StrEq("The root directory record data is malformed.")));
}

TEST_F(CloudInitIso, reads_iso_file_failed_to_read_vec)
{
    mp::CloudInitIso original_iso;
    // At least one actual file entry is need to reach the read_bytes_to_vec call
    original_iso.add_file("test1", "test data1");
    original_iso.write_to(iso_path);

    const auto [mock_file_ops, _] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, is_open(An<const std::ifstream&>())).WillOnce(Return(true));

    // The first read_bytes_to_vec call invoke the 7th call of MP_FILEOPS.read
    const InSequence seq;
    EXPECT_CALL(*mock_file_ops, read(An<std::ifstream&>(), A<char*>(), A<std::streamsize>()))
        .Times(6)
        .WillRepeatedly(original_implementation_of_read);

    EXPECT_CALL(*mock_file_ops, read(An<std::ifstream&>(), A<char*>(), A<std::streamsize>()))
        .WillOnce(read_returns_failed_ifstream);

    mp::CloudInitIso new_iso;
    MP_EXPECT_THROW_THAT(new_iso.read_from(std::filesystem::path(iso_path.toStdString())),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("bytes data from file at")));
}

TEST_F(CloudInitIso, reads_iso_file_encoded_file_name_is_not_even_length)
{
    mp::CloudInitIso original_iso;
    // At least one actual file entry is need to reach the convert_u16_name_back call
    original_iso.add_file("test1", "test data1");
    original_iso.write_to(iso_path);

    const auto [mock_file_ops, _] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, is_open(An<const std::ifstream&>())).WillOnce(Return(true));

    const InSequence seq;
    EXPECT_CALL(*mock_file_ops, read(An<std::ifstream&>(), A<char*>(), A<std::streamsize>()))
        .Times(7)
        .WillRepeatedly(original_implementation_of_read);

    auto read_returns_one_byte_value_three =
        [](std::ifstream& file, char* one_byte, std::streamsize) -> std::ifstream& {
        const int encoded_file_name_length{3U}; // any odd number will do it
        *one_byte = static_cast<std::uint8_t>(encoded_file_name_length);
        return file;
    };

    // read_single_byte encoded_file_name_length odd number
    EXPECT_CALL(*mock_file_ops, read(An<std::ifstream&>(), A<char*>(), A<std::streamsize>()))
        .WillOnce(read_returns_one_byte_value_three);

    EXPECT_CALL(*mock_file_ops, read(An<std::ifstream&>(), A<char*>(), A<std::streamsize>()))
        .WillOnce(original_implementation_of_read);

    mp::CloudInitIso new_iso;
    MP_EXPECT_THROW_THAT(new_iso.read_from(std::filesystem::path(iso_path.toStdString())),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("is not even, which does not conform to u16 name format")));
}

TEST_F(CloudInitIso, reads_iso_file_with_random_string_data)
{
    mp::CloudInitIso original_iso;

    original_iso.add_file("test1", "test data1");
    original_iso.add_file("test test 2", "test some data2");
    original_iso.add_file("test_random_name_3", "more \r test \n \n data3");
    original_iso.add_file("test-title_4", "random_test_data: \n - path: /etc/pollinate/add-user-agent");
    original_iso.add_file("t5", "");
    original_iso.write_to(iso_path);

    mp::CloudInitIso new_iso;
    new_iso.read_from(iso_path.toStdString());
    EXPECT_EQ(original_iso, new_iso);
}

TEST_F(CloudInitIso, reads_iso_file_with_mocked_real_file_data)
{
    constexpr std::string_view user_data_content = R"(#cloud-config
{})";
    constexpr std::string_view vendor_data_content = R"(#cloud-config
growpart:
  mode: auto
  devices: [/]
  ignore_growroot_disabled: false
users:
  - default
manage_etc_hosts: true
ssh_authorized_keys:
  - ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQChYxmeUq14WG5KW+PQ9QvlytbZqMC2wIUxHyRzKbieOge2INvi7cG6NhoZ/KUp9RxVMkC1lll38VfHW3xupqxKj1ECDrMNAjuqOB+i8iS+XB3CTzlCs/3I7sW4nbG0fVwXTN6wUpQ9c9PZe09fmB/Va06gtyEb88lBzUq0Q932ZAqOYN+e/0r9TcIrNdzNlGDviiwykC94kzRJ8IapngxJkPzv3ohiOX3rpWCB1I0l2fLc0ZlZulLYxWphDFticoPl6l1mRlhM/1vRJzyjJXmHoFEmabIUe6nkjDy3JAo1btJ5L6CuN0yBsSLshk8XS/ACSNGvS8VvmLGXT0nbTyDH ubuntu@localhost
timezone: Europe/Amsterdam
system_info:
  default_user:
    name: ubuntu
write_files:
  - path: /etc/pollinate/add-user-agent
    content: "multipass/version/1.14.0-dev.1209+g5b2c7f7d # written by Multipass\nmultipass/driver/qemu-8.0.4 # written by Multipass\nmultipass/host/ubuntu-23.10 # written by Multipass\nmultipass/alias/default # written by Multipass\n"
)";
    mp::CloudInitIso original_iso;

    original_iso.add_file("meta-data", std::string(meta_data_content));
    original_iso.add_file("vendor_data_content", std::string(vendor_data_content));
    original_iso.add_file("user-data", std::string(user_data_content));
    original_iso.add_file("network-data", "some random network-data");
    original_iso.write_to(iso_path);

    mp::CloudInitIso new_iso;
    new_iso.read_from(iso_path.toStdString());
    EXPECT_EQ(original_iso, new_iso);
}

TEST_F(CloudInitIso, updateCloudInitWithNewNonEmptyExtraInterfaces)
{
    mp::CloudInitIso original_iso;

    original_iso.add_file("meta-data", std::string(meta_data_content));
    original_iso.add_file("network-data", "");
    original_iso.write_to(iso_path);

    const std::string default_mac_addr = "52:54:00:56:78:90";
    const std::vector<mp::NetworkInterface> extra_interfaces = {{"id", "52:54:00:56:78:91", true}};
    EXPECT_NO_THROW(
        MP_CLOUD_INIT_FILE_OPS.update_cloud_init_with_new_extra_interfaces_and_new_id(default_mac_addr,
                                                                                      extra_interfaces,
                                                                                      "vm2",
                                                                                      iso_path.toStdString()));

    constexpr std::string_view expected_modified_meta_data_content = R"(#cloud-config
instance-id: vm2
local-hostname: vm1
cloud-name: multipass
)";
    constexpr std::string_view expected_generated_network_config_data_content = R"(#cloud-config
version: 2
ethernets:
  default:
    match:
      macaddress: "52:54:00:56:78:90"
    dhcp4: true
  extra0:
    match:
      macaddress: "52:54:00:56:78:91"
    dhcp4: true
    dhcp4-overrides:
      route-metric: 200
    optional: true
)";

    mp::CloudInitIso new_iso;
    new_iso.read_from(iso_path.toStdString());
    EXPECT_EQ(new_iso.at("meta-data"), expected_modified_meta_data_content);
    EXPECT_EQ(new_iso.at("network-config"), expected_generated_network_config_data_content);
}

TEST_F(CloudInitIso, updateCloudInitWithNewEmptyExtraInterfaces)
{
    mp::CloudInitIso original_iso;
    original_iso.add_file("meta-data", std::string(meta_data_content));
    original_iso.add_file("network-data", "");
    original_iso.write_to(iso_path);

    const std::string& default_mac_addr = "52:54:00:56:78:90";
    const std::vector<mp::NetworkInterface> empty_extra_interfaces{};
    EXPECT_NO_THROW(
        MP_CLOUD_INIT_FILE_OPS.update_cloud_init_with_new_extra_interfaces_and_new_id(default_mac_addr,
                                                                                      empty_extra_interfaces,
                                                                                      std::string(),
                                                                                      iso_path.toStdString()));
    mp::CloudInitIso new_iso;
    new_iso.read_from(iso_path.toStdString());
    EXPECT_FALSE(new_iso.contains("network-config"));
}

TEST_F(CloudInitIso, addExtraInterfaceToCloudInit)
{
    mp::CloudInitIso original_iso;
    original_iso.add_file("meta-data", std::string(meta_data_content));
    original_iso.write_to(iso_path);

    const mp::NetworkInterface dummy_extra_interface{};
    EXPECT_NO_THROW(
        MP_CLOUD_INIT_FILE_OPS.add_extra_interface_to_cloud_init("", dummy_extra_interface, iso_path.toStdString()));
}

TEST_F(CloudInitIso, getInstanceIdFromCloudInit)
{
    mp::CloudInitIso original_iso;
    original_iso.add_file("meta-data", std::string(meta_data_content));
    original_iso.write_to(iso_path);

    EXPECT_EQ(MP_CLOUD_INIT_FILE_OPS.get_instance_id_from_cloud_init(iso_path.toStdString()), "vm1");
}
