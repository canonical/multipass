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

#include "../mock_singleton_helpers.h"

#include <hyperv_api/hcs/hyperv_hcs_schema_version.h>

namespace multipass::test
{
class MockSchemaUtils : public hyperv::hcs::SchemaUtils
{
public:
    using SchemaUtils::SchemaUtils;

    MOCK_METHOD(hyperv::hcs::HcsSchemaVersion,
                get_os_supported_schema_version,
                (),
                (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockSchemaUtils, hyperv::hcs::SchemaUtils);
};
} // namespace multipass::test
