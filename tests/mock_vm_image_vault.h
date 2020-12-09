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

#ifndef MULTIPASS_MOCK_IMAGE_VAULT_H
#define MULTIPASS_MOCK_IMAGE_VAULT_H

#include <multipass/query.h>
#include <multipass/vm_image_vault.h>

#include "temp_file.h"

#include <gmock/gmock.h>

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
        ON_CALL(*this, fetch_image(_, _, _, _)).WillByDefault([this](auto, auto, const PrepareAction& prepare, auto) {
            return prepare({dummy_image.name(), dummy_image.name(), dummy_image.name(), {}, {}, {}, {}, {}});
        });
        ON_CALL(*this, has_record_for(_)).WillByDefault(Return(true));
        ON_CALL(*this, minimum_image_size_for(_)).WillByDefault(Return(MemorySize{"1048576"}));
    };

    MOCK_METHOD4(fetch_image, VMImage(const FetchType&, const Query&, const PrepareAction&, const ProgressMonitor&));
    MOCK_METHOD1(remove, void(const std::string&));
    MOCK_METHOD1(has_record_for, bool(const std::string&));
    MOCK_METHOD0(prune_expired_images, void());
    MOCK_METHOD3(update_images, void(const FetchType&, const PrepareAction&, const ProgressMonitor&));
    MOCK_METHOD1(minimum_image_size_for, MemorySize(const std::string&));
    MOCK_METHOD1(image_host_for, VMImageHost*(const std::string&));

private:
    TempFile dummy_image;
};
} // namespace test
} // namespace multipass

#endif // MULTIPASS_MOCK_IMAGE_VAULT_H
