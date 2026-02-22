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

#include <stdexcept>

#include <multipass/format.h>

namespace multipass
{
class UnsupportedImageException : public std::runtime_error
{
public:
    UnsupportedImageException(const std::string& release)
        : runtime_error(fmt::format("Image '{}' is no longer supported by Multipass.\n"
                                    "Use --allow-unsupported to launch it anyway, or run "
                                    "'multipass find' to see supported images.", release))
    {
    }
};
} // namespace multipass
