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

#include <multipass/yaml_node_utils.h>

#include <gtest/gtest.h>

namespace mp = multipass;
namespace mpu = mp::utils;

TEST(UtilsTests, makeCloudInitMetaConfig)
{
    const YAML::Node meta_data_node = mpu::make_cloud_init_meta_config("vm1");
    EXPECT_EQ(meta_data_node["instance-id"].as<std::string>(), "vm1");
    EXPECT_EQ(meta_data_node["local-hostname"].as<std::string>(), "vm1");
    EXPECT_EQ(meta_data_node["cloud-name"].as<std::string>(), "multipass");
}

TEST(UtilsTests, makeCloudInitMetaConfigWithYAMLStr)
{
    constexpr std::string_view meta_data_content = R"(#cloud-config
instance-id: vm2
local-hostname: vm2
cloud-name: multipass)";
    const YAML::Node meta_data_node = mpu::make_cloud_init_meta_config("vm1", std::string{meta_data_content});
    EXPECT_EQ(meta_data_node["instance-id"].as<std::string>(), "vm1");
    EXPECT_EQ(meta_data_node["local-hostname"].as<std::string>(), "vm1");
    EXPECT_EQ(meta_data_node["cloud-name"].as<std::string>(), "multipass");
}
