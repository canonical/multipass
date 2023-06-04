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

#ifndef MULTIPASS_MOCK_POCO_ZIP_UTILS_H
#define MULTIPASS_MOCK_POCO_ZIP_UTILS_H

#include "common.h"
#include "mock_singleton_helpers.h"

#include <multipass/poco_zip_utils.h>

namespace multipass::test
{
class MockPocoZipUtils : public PocoZipUtils
{
public:
    using PocoZipUtils::PocoZipUtils;

    MOCK_METHOD(Poco::Zip::ZipArchive, zip_archive_for, (std::ifstream&), (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockPocoZipUtils, PocoZipUtils);
};
} // namespace multipass::test

#endif // MULTIPASS_MOCK_POCO_ZIP_UTILS_H
