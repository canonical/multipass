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
#ifndef MULTIPASS_STUB_IMAGE_HOST_H
#define MULTIPASS_STUB_IMAGE_HOST_H

#include <multipass/vm_image.h>
#include <multipass/vm_image_host.h>

namespace multipass
{
namespace test
{
struct StubVMImageHost final : public multipass::VMImageHost
{
    std::optional<multipass::VMImageInfo> info_for(const multipass::Query& query) override
    {
        return std::optional<multipass::VMImageInfo>{VMImageInfo{{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, -1, {}}};
    };

    std::vector<std::pair<std::string, multipass::VMImageInfo>> all_info_for(const multipass::Query& query) override
    {
        return {};
    };

    multipass::VMImageInfo info_for_full_hash(const std::string& full_hash) override
    {
        return {{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, -1, {}};
    };

    std::vector<multipass::VMImageInfo> all_images_for(const std::string& remote_name,
                                                       const bool allow_unsupported) override
    {
        return {};
    };

    void for_each_entry_do(const Action&) override
    {
    }

    std::vector<std::string> supported_remotes() override
    {
        return {};
    }

    void update_manifests(const bool is_force_update_from_network) override
    {
    }
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_STUB_IMAGE_HOST_H
