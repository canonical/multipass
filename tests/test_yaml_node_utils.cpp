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

#include <multipass/network_interface.h>
#include <multipass/yaml_node_utils.h>

#include <gtest/gtest.h>

namespace mp = multipass;
namespace mpu = mp::utils;

TEST(YAMLNodeUtilsTests, makeCloudInitMetaConfig)
{
    const YAML::Node meta_data_node = mpu::make_cloud_init_meta_config("vm1");
    EXPECT_EQ(meta_data_node["instance-id"].as<std::string>(), "vm1");
    EXPECT_EQ(meta_data_node["local-hostname"].as<std::string>(), "vm1");
    EXPECT_EQ(meta_data_node["cloud-name"].as<std::string>(), "multipass");
}

TEST(YAMLNodeUtilsTests, makeCloudInitMetaConfigWithYAMLStr)
{
    constexpr std::string_view meta_data_content = R"(#cloud-config
instance-id: vm2_e_e
local-hostname: vm2
cloud-name: multipass)";
    const YAML::Node meta_data_node =
        mpu::make_cloud_init_meta_config("vm1", std::string{meta_data_content});
    EXPECT_EQ(meta_data_node["instance-id"].as<std::string>(), "vm1_e_e");
    EXPECT_EQ(meta_data_node["local-hostname"].as<std::string>(), "vm1");
    EXPECT_EQ(meta_data_node["cloud-name"].as<std::string>(), "multipass");
}

TEST(YAMLNodeUtilsTests, addOneExtraInterfaceNonEmptyNetworkFileContent)
{
    constexpr std::string_view original_network_config_file_content = R"(#cloud-config
version: 2
ethernets:
  eth0:
    match:
      macaddress: "52:54:00:51:84:0c"
    dhcp4: true
    dhcp-identifier: mac
  eth1:
    match:
      macaddress: "52:54:00:d8:12:9b"
    dhcp4: true
    dhcp-identifier: mac
    dhcp4-overrides:
      route-metric: 200
    optional: true)";

    constexpr std::string_view expected_new_network_config_file_content = R"(#cloud-config
version: 2
ethernets:
  eth0:
    match:
      macaddress: "52:54:00:51:84:0c"
    dhcp4: true
    dhcp-identifier: mac
  eth1:
    match:
      macaddress: "52:54:00:d8:12:9b"
    dhcp4: true
    dhcp-identifier: mac
    dhcp4-overrides:
      route-metric: 200
    optional: true
  eth2:
    match:
      macaddress: "52:54:00:d8:12:9c"
    dhcp4: true
    dhcp-identifier: mac
    dhcp4-overrides:
      route-metric: 200
    optional: true
    set-name: eth2
)";

    const mp::NetworkInterface extra_interface{"id", "52:54:00:d8:12:9c", true};
    const std::string default_mac_addr = "52:54:00:56:78:90";

    const auto new_network_node = mpu::add_extra_interface_to_network_config(
        default_mac_addr,
        extra_interface,
        std::string(original_network_config_file_content));

    EXPECT_EQ(mpu::emit_cloud_config(new_network_node), expected_new_network_config_file_content);
}

TEST(YAMLNodeUtilsTests, addOneExtraInterfaceEmptyNetworkFileContent)
{
    constexpr std::string_view expected_new_network_config_file_content = R"(#cloud-config
version: 2
ethernets:
  eth0:
    match:
      macaddress: "52:54:00:56:78:90"
    dhcp4: true
    dhcp-identifier: mac
    set-name: eth0
  eth1:
    match:
      macaddress: "52:54:00:d8:12:9c"
    dhcp4: true
    dhcp-identifier: mac
    dhcp4-overrides:
      route-metric: 200
    optional: true
    set-name: eth1
)";

    const mp::NetworkInterface extra_interface{"id", "52:54:00:d8:12:9c", true};
    const std::string default_mac_addr = "52:54:00:56:78:90";

    const auto new_network_node =
        mpu::add_extra_interface_to_network_config(default_mac_addr, extra_interface, "");

    EXPECT_EQ(mpu::emit_cloud_config(new_network_node), expected_new_network_config_file_content);
}

TEST(YAMLNodeUtilsTests, addOneExtraInterfaceFalseExtraInterface)
{
    const mp::NetworkInterface extra_interface{"id", "52:54:00:d8:12:9c", false};
    const auto new_network_node =
        mpu::add_extra_interface_to_network_config("", extra_interface, "");
    EXPECT_TRUE(new_network_node.IsNull());
}

TEST(YAMLNodeUtilsTests, makeCloudInitMetaConfigWithIdTweakGeneratedId)
{
    constexpr std::string_view meta_data_content = R"(#cloud-config
instance-id: vm1
local-hostname: vm1
cloud-name: multipass)";

    const YAML::Node meta_data_node =
        mpu::make_cloud_init_meta_config_with_id_tweak(std::string{meta_data_content});
    EXPECT_EQ(meta_data_node["instance-id"].as<std::string>(), "vm1_e");
    EXPECT_EQ(meta_data_node["local-hostname"].as<std::string>(), "vm1");
    EXPECT_EQ(meta_data_node["cloud-name"].as<std::string>(), "multipass");
}

TEST(YAMLNodeUtilsTests, makeCloudInitMetaConfigWithIdTweakNewId)
{
    constexpr std::string_view meta_data_content = R"(#cloud-config
instance-id: vm1
local-hostname: vm1
cloud-name: multipass)";

    const YAML::Node meta_data_node =
        mpu::make_cloud_init_meta_config_with_id_tweak(std::string{meta_data_content}, "vm2");
    EXPECT_EQ(meta_data_node["instance-id"].as<std::string>(), "vm2");
    EXPECT_EQ(meta_data_node["local-hostname"].as<std::string>(), "vm1");
    EXPECT_EQ(meta_data_node["cloud-name"].as<std::string>(), "multipass");
}

TEST(UtilsTests, emitYamlWithOctalString)
{
    YAML::Node node;
    node["permissions"] = "0755";
    node["another_permission"] = "0644";
    node["not_octal"] = "0abc";
    node["regular_string"] = "hello";

    const std::string result = mpu::emit_yaml(node);

    // The octal strings should be double-quoted to preserve their format as strings
    EXPECT_TRUE(result.find("permissions: \"0755\"") != std::string::npos);
    EXPECT_TRUE(result.find("another_permission: \"0644\"") != std::string::npos);
    // Non-octal strings should not be quoted (or may be quoted, both are acceptable)
    EXPECT_TRUE(result.find("not_octal: 0abc") != std::string::npos ||
                result.find("not_octal: \"0abc\"") != std::string::npos);
    EXPECT_TRUE(result.find("regular_string: hello") != std::string::npos ||
                result.find("regular_string: \"hello\"") != std::string::npos);
}

TEST(UtilsTests, emitYamlWithStringWithColons)
{
    YAML::Node node;
    node["key"] = "value:with:colons";
    const std::string result = mpu::emit_yaml(node);
    EXPECT_TRUE(result.find("key: \"value:with:colons\"") != std::string::npos);
}
