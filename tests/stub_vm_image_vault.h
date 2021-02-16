/*
 * Copyright (C) 2017-2021 Canonical, Ltd.
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

#ifndef MULTIPASS_STUB_VM_IMAGE_VAULT_H
#define MULTIPASS_STUB_VM_IMAGE_VAULT_H

#include <multipass/vm_image_vault.h>
#include "temp_file.h"

namespace multipass
{
namespace test
{
struct StubVMImageVault final : public multipass::VMImageVault
{
    multipass::VMImage fetch_image(const multipass::FetchType&, const multipass::Query&, const PrepareAction& prepare,
                                   const multipass::ProgressMonitor&) override
    {
        return prepare({dummy_image.name(), dummy_image.name(), dummy_image.name(), {}, {}, {}, {}, {}});
    };

    void remove(const std::string&) override{};
    bool has_record_for(const std::string&) override
    {
        return false;
    }

    void prune_expired_images() override{};
    void update_images(const FetchType& fetch_type, const PrepareAction& prepare,
                       const ProgressMonitor& monitor) override{};

    MemorySize minimum_image_size_for(const std::string& image) override
    {
        return MemorySize{};
    }

    VMImageHost* image_host_for(const std::string& remote_name) const override
    {
        return nullptr;
    }

    std::vector<VMImageInfo> all_info_for(const Query& query) const override
    {
        return {};
    }

    TempFile dummy_image;
};
}
}
#endif // MULTIPASS_STUB_VM_IMAGE_VAULT_H
