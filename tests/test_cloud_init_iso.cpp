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
constexpr auto* meta_data_content_template = R"(#cloud-config
instance-id: {0}
local-hostname: {1}
cloud-name: multipass
)";
const std::string default_meta_data_content = fmt::format(meta_data_content_template, "vm1", "vm1");
constexpr auto* network_config_data_content_template = R"(#cloud-config
version: 2
ethernets:
  eth0:
    match:
      macaddress: "{0}"
    dhcp4: true
    dhcp-identifier: mac
    set-name: eth0
  eth1:
    match:
      macaddress: "{1}"
    dhcp4: true
    dhcp-identifier: mac
    dhcp4-overrides:
      route-metric: 200
    optional: true
    set-name: eth1
)";

auto read_returns_failed_ifstream =
    [](std::ifstream& file, char*, std::streamsize) -> std::ifstream& {
    file.setstate(std::ios::failbit);
    return file;
};

// Since we can have a valid file to read from and FileOps::read is a cheap function, falls back to
// the original implementation would be an easy way to implement EXPECT_CALL multiple times instead
// of mocking multiple single calls with the correct output
auto original_implementation_of_read =
    [](std::ifstream& file, char* buffer, std::streamsize pos) -> std::ifstream& {
    return MP_FILEOPS.FileOps::read(file, buffer, pos);
};
} // namespace

struct CloudInitIso : public Test
{
    CloudInitIso()
    {
        iso_path = QDir{temp_dir.path()}.filePath("test.iso").toStdString();
    }
    mpt::TempDir temp_dir;
    std::filesystem::path iso_path;
};

TEST_F(CloudInitIso, checkContainsFalse)
{
    mp::CloudInitIso iso;
    EXPECT_FALSE(iso.contains("non_exist_file"));
}

TEST_F(CloudInitIso, checkContainsTrue)
{
    mp::CloudInitIso iso;
    iso.add_file("test", "test data");
    EXPECT_TRUE(iso.contains("test"));
}

TEST_F(CloudInitIso, checkEraseFalse)
{
    mp::CloudInitIso iso;
    EXPECT_FALSE(iso.erase("non_exist_file"));
}

TEST_F(CloudInitIso, checkEraseTrue)
{
    mp::CloudInitIso iso;
    iso.add_file("test", "test data");
    EXPECT_TRUE(iso.contains("test"));
    EXPECT_TRUE(iso.erase("test"));
    EXPECT_FALSE(iso.contains("test"));
}

TEST_F(CloudInitIso, checkAtOperatorThrow)
{
    mp::CloudInitIso iso;
    MP_EXPECT_THROW_THAT(
        iso.at("non_exist_file"),
        std::runtime_error,
        mpt::match_what(
            StrEq("Did not find the target file non_exist_file in the CloudInitIso instance.")));
}

TEST_F(CloudInitIso, checkAtOperatorFoundKey)
{
    mp::CloudInitIso iso;
    iso.add_file("test", "test data");
    EXPECT_EQ(iso.at("test"), "test data");
}

TEST_F(CloudInitIso, checkIndexOperatorNotExistDefaultReturn)
{
    mp::CloudInitIso iso;
    EXPECT_EQ(iso["test"], std::string());
}

TEST_F(CloudInitIso, checkIndexOperatorFoundKey)
{
    mp::CloudInitIso iso;
    iso.add_file("test", "test data");
    EXPECT_EQ(iso["test"], "test data");
}

TEST_F(CloudInitIso, createsIsoFile)
{
    mp::CloudInitIso iso;
    iso.add_file("test", "test data");
    iso.write_to(iso_path);

    std::ofstream file{iso_path, std::ios::binary};
    EXPECT_TRUE(file.is_open());
    EXPECT_THAT(std::filesystem::file_size(iso_path), Ge(0));
}

TEST_F(CloudInitIso, readsIsoFileFailedToOpenFile)
{
    mp::CloudInitIso original_iso;
    original_iso.write_to(iso_path);

    const auto [mock_file_ops, _] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, is_open(An<const std::ifstream&>())).WillOnce(Return(false));
    mp::CloudInitIso new_iso;
    MP_EXPECT_THROW_THAT(new_iso.read_from(iso_path),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("Failed to open file")));
}

TEST_F(CloudInitIso, readsIsoFileFailedToReadSingleBytes)
{
    mp::CloudInitIso original_iso;
    original_iso.write_to(iso_path);

    const auto [mock_file_ops, _] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, is_open(An<const std::ifstream&>())).WillOnce(Return(true));

    EXPECT_CALL(*mock_file_ops, read(An<std::ifstream&>(), A<char*>(), A<std::streamsize>()))
        .WillOnce(read_returns_failed_ifstream);

    // failed on the first read_single_byte call
    mp::CloudInitIso new_iso;
    MP_EXPECT_THROW_THAT(
        new_iso.read_from(iso_path),
        std::runtime_error,
        mpt::match_what(HasSubstr("Can not read the next byte data from file at")));
}

TEST_F(CloudInitIso, readsIsoFileFailedToCheckItHasJolietVolumeDescriptor)
{
    mp::CloudInitIso original_iso;
    original_iso.write_to(iso_path);

    const auto [mock_file_ops, _] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, is_open(An<const std::ifstream&>())).WillOnce(Return(true));

    // value 2_u8 is for Joliet volume descriptor
    auto read_returns_one_byte_value_one =
        [](std::ifstream& file, char* one_byte, std::streamsize) -> std::ifstream& {
        const int non_joliet_volume_des_num{1U};
        *one_byte = static_cast<std::uint8_t>(non_joliet_volume_des_num);
        return file;
    };
    EXPECT_CALL(*mock_file_ops, read(An<std::ifstream&>(), A<char*>(), A<std::streamsize>()))
        .WillOnce(read_returns_one_byte_value_one);

    mp::CloudInitIso new_iso;
    MP_EXPECT_THROW_THAT(new_iso.read_from(iso_path),
                         std::runtime_error,
                         mpt::match_what(StrEq("The Joliet volume descriptor is not in place.")));
}

TEST_F(CloudInitIso, readsIsoFileJolietVolumeDescriptorMalformed)
{
    mp::CloudInitIso original_iso;
    original_iso.write_to(iso_path);

    const auto [mock_file_ops, _] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, is_open(An<const std::ifstream&>())).WillOnce(Return(true));

    const InSequence seq;
    EXPECT_CALL(*mock_file_ops, read(An<std::ifstream&>(), A<char*>(), A<std::streamsize>()))
        .WillOnce(original_implementation_of_read);

    auto read_returns_five_bytes_string =
        [](std::ifstream& file, char* buffer, std::streamsize) -> std::ifstream& {
        std::strcpy(buffer, "NonJo");
        return file;
    };
    EXPECT_CALL(*mock_file_ops, read(An<std::ifstream&>(), A<char*>(), A<std::streamsize>()))
        .WillOnce(read_returns_five_bytes_string);

    mp::CloudInitIso new_iso;
    MP_EXPECT_THROW_THAT(new_iso.read_from(iso_path),
                         std::runtime_error,
                         mpt::match_what(StrEq("The Joliet descriptor is malformed.")));
}

TEST_F(CloudInitIso, readsIsoFileFailedToReadArray)
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
    MP_EXPECT_THROW_THAT(new_iso.read_from(iso_path),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("bytes data from file at")));
}

TEST_F(CloudInitIso, readsIsoFileFailedToCheckRootDirRecordData)
{
    mp::CloudInitIso original_iso;
    original_iso.write_to(iso_path);

    const auto [mock_file_ops, _] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, is_open(An<const std::ifstream&>())).WillOnce(Return(true));

    const InSequence seq;
    EXPECT_CALL(*mock_file_ops, read(An<std::ifstream&>(), A<char*>(), A<std::streamsize>()))
        .Times(2)
        .WillRepeatedly(original_implementation_of_read);

    // default buffer values makes the buffer[0] not 34_u8 which causes root directory record data
    // checking fail
    auto read_return_default_buffer =
        [](std::ifstream& file, char* buffer, std::streamsize) -> std::ifstream& { return file; };

    EXPECT_CALL(*mock_file_ops, read(An<std::ifstream&>(), A<char*>(), A<std::streamsize>()))
        .WillOnce(read_return_default_buffer);

    mp::CloudInitIso new_iso;
    MP_EXPECT_THROW_THAT(new_iso.read_from(iso_path),
                         std::runtime_error,
                         mpt::match_what(StrEq("The root directory record data is malformed.")));
}

TEST_F(CloudInitIso, readsIsoFileFailedToReadVec)
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
    MP_EXPECT_THROW_THAT(new_iso.read_from(iso_path),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("bytes data from file at")));
}

TEST_F(CloudInitIso, readsIsoFileEncodedFileNameIsNotEvenLength)
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
    MP_EXPECT_THROW_THAT(
        new_iso.read_from(iso_path),
        std::runtime_error,
        mpt::match_what(HasSubstr("is not even, which does not conform to u16 name format")));
}

TEST_F(CloudInitIso, readsIsoFileWithRandomStringData)
{
    mp::CloudInitIso original_iso;

    original_iso.add_file("test1", "test data1");
    original_iso.add_file("test test 2", "test some data2");
    original_iso.add_file("test_random_name_3", "more \r test \n \n data3");
    original_iso.add_file("test-title_4",
                          "random_test_data: \n - path: /etc/pollinate/add-user-agent");
    original_iso.add_file("t5", "");
    original_iso.write_to(iso_path);

    mp::CloudInitIso new_iso;
    new_iso.read_from(iso_path);
    EXPECT_EQ(original_iso, new_iso);
}

TEST_F(CloudInitIso, readsIsoFileWithMockedRealFileData)
{
    constexpr auto* user_data_content = R"(#cloud-config
{})";
    constexpr auto* vendor_data_content = R"(#cloud-config
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

    original_iso.add_file("meta-data", default_meta_data_content);
    original_iso.add_file("vendor_data_content", vendor_data_content);
    original_iso.add_file("user-data", user_data_content);
    original_iso.add_file("network-config", "some random network-data");
    original_iso.write_to(iso_path);

    mp::CloudInitIso new_iso;
    new_iso.read_from(iso_path);
    EXPECT_EQ(original_iso, new_iso);
}

TEST_F(CloudInitIso, updateCloudInitWithNewNonEmptyExtraInterfaces)
{
    mp::CloudInitIso original_iso;

    original_iso.add_file("meta-data", default_meta_data_content);
    original_iso.add_file("network-config", "dummy_data");
    original_iso.write_to(iso_path);

    const std::string default_mac_addr = "52:54:00:56:78:90";
    const std::vector<mp::NetworkInterface> extra_interfaces = {{"id", "52:54:00:56:78:91", true}};
    EXPECT_NO_THROW(MP_CLOUD_INIT_FILE_OPS.update_cloud_init_with_new_extra_interfaces_and_new_id(
        default_mac_addr,
        extra_interfaces,
        "vm2",
        iso_path));

    const std::string expected_modified_meta_data_content =
        fmt::format(meta_data_content_template, "vm2", "vm1");
    const std::string expected_generated_network_config_data_content =
        fmt::format(network_config_data_content_template, "52:54:00:56:78:90", "52:54:00:56:78:91");

    mp::CloudInitIso new_iso;
    new_iso.read_from(iso_path);
    EXPECT_EQ(new_iso.at("meta-data"), expected_modified_meta_data_content);
    EXPECT_EQ(new_iso.at("network-config"), expected_generated_network_config_data_content);
}

TEST_F(CloudInitIso, updateCloudInitWithNewEmptyExtraInterfaces)
{
    mp::CloudInitIso original_iso;
    original_iso.add_file("meta-data", default_meta_data_content);
    original_iso.add_file("network-config", "dummy_data");
    original_iso.write_to(iso_path);

    const std::string& default_mac_addr = "52:54:00:56:78:90";
    const std::vector<mp::NetworkInterface> empty_extra_interfaces{};
    EXPECT_NO_THROW(MP_CLOUD_INIT_FILE_OPS.update_cloud_init_with_new_extra_interfaces_and_new_id(
        default_mac_addr,
        empty_extra_interfaces,
        std::string(),
        iso_path));
    mp::CloudInitIso new_iso;
    new_iso.read_from(iso_path);
    EXPECT_TRUE(new_iso.contains("network-config"));
}

TEST_F(CloudInitIso, updateCloneCloudInitSrcFileWithExtraInterfaces)
{
    const std::string src_meta_data_content =
        fmt::format(meta_data_content_template, "vm1_e_e", "vm1");
    const std::string src_network_config_data_content =
        fmt::format(network_config_data_content_template, "00:00:00:00:00:00", "00:00:00:00:00:01");

    mp::CloudInitIso original_iso;
    original_iso.add_file("meta-data", src_meta_data_content);
    original_iso.add_file("network-config", src_network_config_data_content);
    original_iso.write_to(iso_path);

    const std::string default_mac_addr = "52:54:00:56:78:90";
    const std::vector<mp::NetworkInterface> extra_interfaces = {{"id", "52:54:00:56:78:91", true}};
    EXPECT_NO_THROW(MP_CLOUD_INIT_FILE_OPS.update_identifiers(default_mac_addr,
                                                              extra_interfaces,
                                                              "vm1-clone1",
                                                              iso_path));

    const std::string expected_modified_meta_data_content =
        fmt::format(meta_data_content_template, "vm1-clone1_e_e", "vm1-clone1");
    const std::string expected_generated_network_config_data_content =
        fmt::format(network_config_data_content_template, "52:54:00:56:78:90", "52:54:00:56:78:91");

    mp::CloudInitIso new_iso;
    new_iso.read_from(iso_path);
    EXPECT_EQ(new_iso.at("meta-data"), expected_modified_meta_data_content);
    EXPECT_EQ(new_iso.at("network-config"), expected_generated_network_config_data_content);
}

TEST_F(CloudInitIso, addExtraInterfaceToCloudInit)
{
    mp::CloudInitIso original_iso;
    original_iso.add_file("meta-data", default_meta_data_content);
    original_iso.write_to(iso_path);

    const mp::NetworkInterface dummy_extra_interface{};
    EXPECT_NO_THROW(MP_CLOUD_INIT_FILE_OPS.add_extra_interface_to_cloud_init("",
                                                                             dummy_extra_interface,
                                                                             iso_path));
}

TEST_F(CloudInitIso, getInstanceIdFromCloudInit)
{
    mp::CloudInitIso original_iso;
    original_iso.add_file("meta-data", default_meta_data_content);
    original_iso.write_to(iso_path);

    EXPECT_EQ(MP_CLOUD_INIT_FILE_OPS.get_instance_id_from_cloud_init(iso_path), "vm1");
}
