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

#ifndef MULTIPASS_MOCK_IMAGE_HOST_H
#define MULTIPASS_MOCK_IMAGE_HOST_H

#include "common.h"
#include "temp_file.h"

#include <multipass/query.h>
#include <multipass/vm_image_host.h>

using namespace testing;

namespace multipass
{
namespace test
{
constexpr auto default_id = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
constexpr auto default_release_info = "18.04 LTS";
constexpr auto default_version = "20200519.1";
constexpr auto default_stream_location = "https://some/stream";
constexpr auto default_alias = "default";

constexpr auto snapcraft_image_id = "c14a2047c6ba57722bc612115b1d44bea4a29ac2212fcc0628c49aa832dba867";
constexpr auto lxd_snapcraft_image_id = "da708063589b9c83dfeaec7049deac82da96f8969b413d1346dc067897e5934b";
constexpr auto snapcraft_release_info = "Snapcraft builder for Core 20";
constexpr auto snapcraft_image_version = "20200901";
constexpr auto snapcraft_alias = "snapcraft";

constexpr auto custom_image_id = "aedb5a84aaf2e4e443e090511156366a2800c26cec1b6a46f44d153c4bf04205";
constexpr auto lxd_custom_image_id = "bc5a973bd6f2bef30658fb51177cf5e506c1d60958a4c97813ee26416dc368da";
constexpr auto custom_release_info = "Custom Ubuntu for Testing";
constexpr auto custom_image_version = "20200909";
constexpr auto custom_alias = "custom";

constexpr auto another_image_id = "e34a2047c6ba57722bc612115b1d44bea4a29ac2212fcc0628c49aa832dba867";
constexpr auto another_image_version = "20200501";
constexpr auto another_release_info = "Another Ubuntu Version";
constexpr auto another_alias = "another";

constexpr auto release_remote = "release";
constexpr auto snapcraft_remote = "snapcraft";
constexpr auto custom_remote = "custom";

class MockImageHost : public VMImageHost
{
public:
    MockImageHost()
    {
        ON_CALL(*this, info_for(_)).WillByDefault([this](const auto& query) {
            if (query.release == snapcraft_remote)
            {
                return mock_snapcraft_image_info;
            }
            else if (query.release == custom_remote)
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
        ON_CALL(*this, for_each_entry_do(_)).WillByDefault([this](const Action& action) {
            action(release_remote, mock_bionic_image_info);
            action(release_remote, mock_another_image_info);
            action(snapcraft_remote, mock_snapcraft_image_info);
            action(custom_remote, mock_custom_image_info);
        });
        ON_CALL(*this, supported_remotes()).WillByDefault(Return(remote));
    };

    MOCK_METHOD(std::optional<VMImageInfo>, info_for, (const Query&), (override));
    MOCK_METHOD((std::vector<std::pair<std::string, VMImageInfo>>), all_info_for, (const Query&), (override));
    MOCK_METHOD(VMImageInfo, info_for_full_hash, (const std::string&), (override));
    MOCK_METHOD(std::vector<VMImageInfo>, all_images_for, (const std::string&, const bool), (override));
    MOCK_METHOD(void, for_each_entry_do, (const Action&), (override));
    MOCK_METHOD(std::vector<std::string>, supported_remotes, (), (override));
    MOCK_METHOD(void, update_manifests, (bool), (override));

    TempFile image;
    VMImageInfo mock_bionic_image_info{{default_alias},
                                       "Ubuntu",
                                       "bionic",
                                       default_release_info,
                                       "Bionic Beaver",
                                       true,
                                       image.url(),
                                       default_id,
                                       default_stream_location,
                                       default_version,
                                       1,
                                       true};
    VMImageInfo mock_snapcraft_image_info{{snapcraft_alias},
                                          "Ubuntu",
                                          "core20",
                                          snapcraft_release_info,
                                          "Core 20",
                                          true,
                                          image.url(),
                                          snapcraft_image_id,
                                          "",
                                          snapcraft_image_version,
                                          1,
                                          true};
    VMImageInfo mock_custom_image_info{{custom_alias},
                                       "Ubuntu",
                                       "Custom Core",
                                       custom_release_info,
                                       "Custom Core",
                                       true,
                                       image.url(),
                                       custom_image_id,
                                       "",
                                       custom_image_version,
                                       1,
                                       false};
    VMImageInfo mock_another_image_info{{another_alias},
                                        "Ubuntu",
                                        "another",
                                        another_release_info,
                                        "Another",
                                        true,
                                        image.url(),
                                        another_image_id,
                                        "",
                                        another_image_version,
                                        1,
                                        false};

private:
    std::vector<std::pair<std::string, VMImageInfo>> empty_image_info_vector_pair;
    std::vector<VMImageInfo> empty_image_info_vector;
    VMImageInfo empty_vm_image_info{{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, -1, {}};
    std::vector<std::string> remote{{"release"}};
};
} // namespace test
} // namespace multipass

#endif // MULTIPASS_MOCK_IMAGE_HOST_H
