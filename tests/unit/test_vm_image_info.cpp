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

namespace mp = multipass;

using namespace testing;

namespace
{
// A single distro entry, matching the structure of a custom image host manifest value. The
// `release_title` and `release_codename` fields are given deliberately distinct values so that a
// mix-up between them (e.g. from positional aggregate initialization) is detected.
constexpr auto distro_json = R"({
    "aliases": "debian, bookworm",
    "os": "Debian",
    "release": "bookworm",
    "release_codename": "Bookworm",
    "release_title": "12",
    "items": {
        "x86_64": {
            "image_location": "https://example.com/debian-12-amd64.qcow2",
            "id": "debian-12-hash",
            "version": "20250804",
            "size": 444896256
        }
    }
})";
} // namespace

TEST(TestVMImageInfo, parsesJsonIntoExpectedFields)
{
    const auto json = boost::json::parse(distro_json);

    const auto info = value_to<mp::VMImageInfo>(json, mp::ArchContext{"x86_64"});

    EXPECT_EQ(info.aliases, (std::vector<std::string>{"debian", "bookworm"}));
    EXPECT_EQ(info.os, "Debian");
    EXPECT_EQ(info.release, "bookworm");
    // These two must not be swapped: `release_title` is "12", `release_codename` is "Bookworm".
    EXPECT_EQ(info.release_title, "12");
    EXPECT_EQ(info.release_codename, "Bookworm");
    EXPECT_TRUE(info.supported);
    EXPECT_EQ(info.image_location, "https://example.com/debian-12-amd64.qcow2");
    EXPECT_EQ(info.id, "debian-12-hash");
    EXPECT_EQ(info.stream_location, "");
    EXPECT_EQ(info.version, "20250804");
    EXPECT_EQ(info.size, 444896256);
    EXPECT_TRUE(info.verify);
}
