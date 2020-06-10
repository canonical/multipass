/*
 * Copyright (C) 2020 Canonical, Ltd.
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

class MockImageHost : public VMImageHost
{
public:
    MockImageHost()
    {
        ON_CALL(*this, info_for(_)).WillByDefault([this](auto...) { return mock_image_info; });
        ON_CALL(*this, all_info_for(_)).WillByDefault(Return(empty_image_info_vector));
        ON_CALL(*this, info_for_full_hash(_)).WillByDefault(Return(empty_vm_image_info));
        ON_CALL(*this, all_images_for(_, _)).WillByDefault(Return(empty_image_info_vector));
        ON_CALL(*this, supported_remotes()).WillByDefault(Return(remote));
    };

    MOCK_METHOD1(info_for, optional<VMImageInfo>(const Query&));
    MOCK_METHOD1(all_info_for, std::vector<VMImageInfo>(const Query&));
    MOCK_METHOD1(info_for_full_hash, VMImageInfo(const std::string&));
    MOCK_METHOD2(all_images_for, std::vector<VMImageInfo>(const std::string&, const bool));
    MOCK_METHOD1(for_each_entry_do, void(const Action&));
    MOCK_METHOD0(supported_remotes, std::vector<std::string>());

    TempFile image;
    TempFile kernel;
    TempFile initrd;
    VMImageInfo mock_image_info{{"default"},
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

private:
    std::vector<VMImageInfo> empty_image_info_vector;
    VMImageInfo empty_vm_image_info{{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, -1, {}};
    std::vector<std::string> remote{{"release"}};
};
} // namespace test
} // namespace multipass

#endif // MULTIPASS_MOCK_IMAGE_HOST_H
