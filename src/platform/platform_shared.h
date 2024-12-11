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

#ifndef MULTIPASS_PLATFORM_SHARED_H
#define MULTIPASS_PLATFORM_SHARED_H

#include <string>
#include <unordered_set>

namespace multipass::platform
{
const std::unordered_set<std::string> supported_snapcraft_aliases{
    "core18",
    "18.04",
    "core20",
    "20.04",
    "core22",
    "22.04",
    "core24",
    "24.04",
    "devel",
};
} // namespace multipass::platform

#endif // MULTIPASS_PLATFORM_SHARED_H
