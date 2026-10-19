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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#pragma once

#include <string>
#include <string_view>

namespace multipass
{
class SimpleStreamsIndex
{
public:
    static SimpleStreamsIndex get_image_downloads(std::string_view json);

    // We're only interested in the following fields from the index:
    const std::string manifest_path;
    const std::string updated_at;
};
} // namespace multipass
