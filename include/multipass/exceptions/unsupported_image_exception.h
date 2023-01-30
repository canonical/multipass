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

#ifndef MULTIPASS_UNSUPPORTED_IMAGE_EXCEPTION_H
#define MULTIPASS_UNSUPPORTED_IMAGE_EXCEPTION_H

#include <stdexcept>

#include <multipass/format.h>

namespace multipass
{
class UnsupportedImageException : public std::runtime_error
{
public:
    UnsupportedImageException(const std::string& release)
        : runtime_error(fmt::format("The {} release is no longer supported.", release))
    {
    }
};
} // namespace multipass
#endif // MULTIPASS_UNSUPPORTED_IMAGE_EXCEPTION_H
