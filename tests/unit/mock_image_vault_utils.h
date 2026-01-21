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

    MOCK_METHOD(QString, copy_to_dir, (const QString&, const QDir&), (const, override));

    MOCK_METHOD(QString,
                compute_hash,
                (QIODevice&, const QCryptographicHash::Algorithm),
                (const, override));
    MOCK_METHOD(QString,
                compute_file_hash,
                (const QString&, const QCryptographicHash::Algorithm),
                (const, override));
    MOCK_METHOD(void, verify_file_hash, (const QString&, const QString&), (const, override));

    MOCK_METHOD(QString, extract_file, (const QString&, const Decoder&, bool), (const, override));

    MOCK_METHOD(HostMap, configure_image_host_map, (const Hosts&), (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockImageVaultUtils, ImageVaultUtils);
};

} // namespace multipass::test
