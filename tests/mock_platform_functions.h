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

#ifndef MULTIPASS_MOCK_PLATFORM_FUNCTIONS_H
#define MULTIPASS_MOCK_PLATFORM_FUNCTIONS_H

#include <premock.hpp>

#include <multipass/platform.h>

namespace multipass
{
namespace platform
{
DECL_MOCK(is_remote_supported);
DECL_MOCK(is_alias_supported);
} // namespace platform
} // namespace multipass
#endif // MULTIPASS_MOCK_PLATFORM_FUNCTIONS_H
