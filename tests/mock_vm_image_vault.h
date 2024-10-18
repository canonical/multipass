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

#ifndef MULTIPASS_MOCK_IMAGE_VAULT_H
#define MULTIPASS_MOCK_IMAGE_VAULT_H

#include "common.h"
#include "temp_file.h"

#include <multipass/query.h>
#include <multipass/vm_image_vault.h>

using namespace testing;

namespace multipass
{
namespace test
{
class MockVMImageVault : public VMImageVault
{
public:
    MockVMImageVault()
    {
        ON_CALL(*this, fetch_image(_, _, _, _, _, _, _))
            .WillByDefault([this](auto, auto, const PrepareAction& prepare, auto, auto, auto, auto) {
                return VMImage{dummy_image.name(), {}, {}, {}, {}, {}};
            });
        ON_CALL(*this, has_record_for(_)).WillByDefault(Return(true));
        ON_CALL(*this, minimum_image_size_for(_)).WillByDefault(Return(MemorySize{"1048576"}));
    };

    MOCK_METHOD(VMImage,
                fetch_image,
                (const FetchType&,
                 const Query&,
                 const PrepareAction&,
                 const ProgressMonitor&,
                 const bool,
                 const std::optional<std::string>&,
                 const mp::Path&),
                (override));
    MOCK_METHOD(void, remove, (const std::string&), (override));
    MOCK_METHOD(bool, has_record_for, (const std::string&), (override));
    MOCK_METHOD(void, prune_expired_images, (), (override));
    MOCK_METHOD(void, update_images, (const FetchType&, const PrepareAction&, const ProgressMonitor&), (override));
    MOCK_METHOD(MemorySize, minimum_image_size_for, (const std::string&), (override));
    MOCK_METHOD(void, clone, (const std::string&, const std::string&), (override));
    MOCK_METHOD(VMImageHost*, image_host_for, (const std::string&), (const, override));
    MOCK_METHOD((std::vector<std::pair<std::string, VMImageInfo>>), all_info_for, (const Query&), (const, override));

private:
    TempFile dummy_image;
};
} // namespace test
} // namespace multipass

#endif // MULTIPASS_MOCK_IMAGE_VAULT_H
