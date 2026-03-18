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

#pragma once

#include "common.h"

#include <multipass/vm_image_vault_utils.h>

namespace multipass::test
{

class MockImageVaultUtils : public ImageVaultUtils
{
public:
    using ImageVaultUtils::ImageVaultUtils;

    MOCK_METHOD(std::filesystem::path,
                copy_to_dir,
                (const std::filesystem::path&, const QDir&),
                (const, override));

    MOCK_METHOD(std::string,
                compute_hash,
                (std::istream&, const QCryptographicHash::Algorithm),
                (const, override));
    MOCK_METHOD(std::string,
                compute_file_hash,
                (const std::filesystem::path&, const QCryptographicHash::Algorithm),
                (const, override));
    MOCK_METHOD(void,
                verify_file_hash,
                (const std::filesystem::path&, const std::string&),
                (const, override));

    MOCK_METHOD(std::filesystem::path,
                extract_file,
                (const std::filesystem::path&, const Decoder&, bool),
                (const, override));

    MOCK_METHOD(HostMap, configure_image_host_map, (const Hosts&), (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockImageVaultUtils, ImageVaultUtils);
};

} // namespace multipass::test
