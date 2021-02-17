/*
 * Copyright (C) 2020-2021 Canonical, Ltd.
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

#ifndef MULTIPASS_MOCK_IMAGE_HOST_H
#define MULTIPASS_MOCK_IMAGE_HOST_H

#include <multipass/vm_image_host.h>

#include "temp_file.h"

#include <gmock/gmock.h>

using namespace testing;

namespace multipass
{
namespace test
{
constexpr auto default_id = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
constexpr auto default_version = "20200519.1";
constexpr auto default_stream_location = "https://some/stream";

constexpr auto snapcraft_image_id = "c14a2047c6ba57722bc612115b1d44bea4a29ac2212fcc0628c49aa832dba867";
constexpr auto lxd_snapcraft_image_id = "da708063589b9c83dfeaec7049deac82da96f8969b413d1346dc067897e5934b";
constexpr auto snapcraft_release_info = "Snapcraft builder for Core 20";
constexpr auto snapcraft_image_version = "20200901";

constexpr auto custom_image_id = "aedb5a84aaf2e4e443e090511156366a2800c26cec1b6a46f44d153c4bf04205";
constexpr auto lxd_custom_image_id = "bc5a973bd6f2bef30658fb51177cf5e506c1d60958a4c97813ee26416dc368da";
constexpr auto custom_release_info = "Custom Ubuntu for Testing";
constexpr auto custom_image_version = "20200909";

class MockImageHost : public VMImageHost
{
public:
    MockImageHost()
    {
        ON_CALL(*this, info_for(_)).WillByDefault([this](const auto& query) {
            if (query.release == "snapcraft")
            {
                return mock_snapcraft_image_info;
            }
            else if (query.release == "custom")
            {
                return mock_custom_image_info;
            }
            else
            {
                return mock_bionic_image_info;
            }
        });
        ON_CALL(*this, all_info_for(_)).WillByDefault(Return(empty_image_info_vector_pair));
        ON_CALL(*this, info_for_full_hash(_)).WillByDefault(Return(empty_vm_image_info));
        ON_CALL(*this, all_images_for(_, _)).WillByDefault(Return(empty_image_info_vector));
        ON_CALL(*this, supported_remotes()).WillByDefault(Return(remote));
    };

    MOCK_METHOD1(info_for, optional<VMImageInfo>(const Query&));
    MOCK_METHOD1(all_info_for, std::vector<std::pair<std::string, VMImageInfo>>(const Query&));
    MOCK_METHOD1(info_for_full_hash, VMImageInfo(const std::string&));
    MOCK_METHOD2(all_images_for, std::vector<VMImageInfo>(const std::string&, const bool));
    MOCK_METHOD1(for_each_entry_do, void(const Action&));
    MOCK_METHOD0(supported_remotes, std::vector<std::string>());

    TempFile image;
    TempFile kernel;
    TempFile initrd;
    VMImageInfo mock_bionic_image_info{{"default"},
                                       "Ubuntu",
                                       "bionic",
                                       "18.04 LTS",
                                       true,
                                       image.url(),
                                       kernel.url(),
                                       initrd.url(),
                                       default_id,
                                       default_stream_location,
                                       default_version,
                                       1,
                                       true};
    VMImageInfo mock_snapcraft_image_info{
        {"snapcraft"}, "Ubuntu",           "core20", snapcraft_release_info,  true, image.url(), kernel.url(),
        initrd.url(),  snapcraft_image_id, "",       snapcraft_image_version, 1,    true};
    VMImageInfo mock_custom_image_info{{"custom"},
                                       "Ubuntu",
                                       "Custom Core",
                                       custom_release_info,
                                       true,
                                       image.url(),
                                       kernel.url(),
                                       initrd.url(),
                                       custom_image_id,
                                       "",
                                       custom_image_version,
                                       1,
                                       false};

private:
    std::vector<std::pair<std::string, VMImageInfo>> empty_image_info_vector_pair;
    std::vector<VMImageInfo> empty_image_info_vector;
    VMImageInfo empty_vm_image_info{{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, -1, {}};
    std::vector<std::string> remote{{"release"}};
};
} // namespace test
} // namespace multipass

#endif // MULTIPASS_MOCK_IMAGE_HOST_H
