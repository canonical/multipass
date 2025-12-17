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

#include <multipass/progress_monitor.h>

namespace multipass::test
{

class MockImageDecoder
{
public:
    MOCK_METHOD(void,
                decode_to,
                (const std::filesystem::path&,
                 const std::filesystem::path&,
                 const ProgressMonitor&),
                (const));
};

} // namespace multipass::test
