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

#ifndef MULTIPASS_MOCK_CLOUD_INIT_FILE_OPS_H
#define MULTIPASS_MOCK_CLOUD_INIT_FILE_OPS_H

#include "mock_singleton_helpers.h"

#include <multipass/cloud_init_iso.h>

namespace multipass::test
{
class MockCloudInitFileOps : public CloudInitFileOps
{
public:
    using CloudInitFileOps::CloudInitFileOps;

    MOCK_METHOD(
        void,
        update_cloud_init_with_new_extra_interfaces_and_new_id,
        (const std::string&, const std::vector<NetworkInterface>&, const std::string&, const std::filesystem::path&),
        (const, override));
    MOCK_METHOD(
        void,
        update_cloned_cloud_init,
        (const std::string&, const std::vector<NetworkInterface>&, const std::string&, const std::filesystem::path&),
        (const, override));
    MOCK_METHOD(void,
                add_extra_interface_to_cloud_init,
                (const std::string&, const NetworkInterface&, const std::filesystem::path&),
                (const, override));
    MOCK_METHOD(std::string, get_instance_id_from_cloud_init, (const std::filesystem::path&), (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockCloudInitFileOps, CloudInitFileOps);
};
} // namespace multipass::test

#endif // MULTIPASS_MOCK_CLOUD_INIT_FILE_OPS_H
