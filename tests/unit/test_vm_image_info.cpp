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

#include <multipass/exceptions/unsupported_arch_exception.h>
#include <multipass/vm_image_info.h>

#include <boost/json.hpp>
#include <fmt/format.h>

namespace mp = multipass;

using namespace testing;

namespace
{
// keep fields distinct
const std::string os = "Debian";
const std::string release = "bookworm";
const std::string release_codename = "Bookworm";
const std::string release_title = "12";
const std::string image_location = "https://example.com/debian-12-amd64.qcow2";
const std::string id = "debian-12-hash";
const std::string version = "20250804";
constexpr auto size = 444896256;

const auto distro_json = fmt::format(R"({{
    "aliases": "debian, bookworm",
    "os": "{}",
    "release": "{}",
    "release_codename": "{}",
    "release_title": "{}",
    "items": {{
        "x86_64": {{
            "image_location": "{}",
            "id": "{}",
            "version": "{}",
            "size": {}
        }}
    }}
}})",
                                     os,
                                     release,
                                     release_codename,
                                     release_title,
                                     image_location,
                                     id,
                                     version,
                                     size);
} // namespace

TEST(TestVMImageInfo, parsesJsonIntoExpectedFields)
{
    const auto json = boost::json::parse(distro_json);

    const auto info = value_to<mp::VMImageInfo>(json, mp::ArchContext{"x86_64"});

    EXPECT_EQ(info.aliases, (std::vector<std::string>{"debian", "bookworm"}));
    EXPECT_EQ(info.os, os);
    EXPECT_EQ(info.release, release);
    // These two must not be swapped: `release_title` is "12", `release_codename` is "Bookworm".
    EXPECT_EQ(info.release_title, release_title);
    EXPECT_EQ(info.release_codename, release_codename);
    EXPECT_TRUE(info.supported);
    EXPECT_EQ(info.image_location, image_location);
    EXPECT_EQ(info.id, id);
    EXPECT_EQ(info.stream_location, "");
    EXPECT_EQ(info.version, version);
    EXPECT_EQ(info.size, size);
    EXPECT_TRUE(info.verify);
}
