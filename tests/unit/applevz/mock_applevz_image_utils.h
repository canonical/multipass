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

#include "tests/unit/common.h"
#include "tests/unit/mock_singleton_helpers.h"

#include <applevz/applevz_utils.h>

namespace multipass::test
{
class MockAppleVZImageUtils : public multipass::applevz::AppleVZImageUtils
{
public:
    using AppleVZImageUtils::AppleVZImageUtils;

    MOCK_METHOD(multipass::Path,
                convert_to_supported_format,
                (const multipass::Path& image_path),
                (const, override));
    MOCK_METHOD(void,
                resize_image,
                (const multipass::MemorySize& disk_space, const multipass::Path& image_path),
                (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockAppleVZImageUtils, AppleVZImageUtils);
};
} // namespace multipass::test
