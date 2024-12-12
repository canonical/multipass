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

#ifndef MULTIPASS_MOCK_IMAGE_VALUE_UTILS_H
#define MULTIPASS_MOCK_IMAGE_VALUE_UTILS_H

#include "common.h"
#include "mock_image_decoder.h"

#include <multipass/vm_image_vault_utils.h>

namespace multipass::test
{

class MockImageVaultUtils
{
public:
    MOCK_METHOD(QString, copy_to_dir, (const QString&, const QDir&), (const));
    MOCK_METHOD(QString, compute_hash, (QIODevice&), (const));
    MOCK_METHOD(QString, compute_file_hash, (const QString&), (const));
    MOCK_METHOD(void, verify_file_hash, (const QString&, const QString&), (const));

    MOCK_METHOD(void, extract_file, (const QString&, const ProgressMonitor&, bool, const MockImageDecoder&), (const));
};

} // namespace multipass::test

#endif // MULTIPASS_MOCK_IMAGE_VALUE_UTILS_H
