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

#ifndef MULTIPASS_IMAGE_VAULT_EXCEPTIONS_H
#define MULTIPASS_IMAGE_VAULT_EXCEPTIONS_H

#include <stdexcept>

#include <multipass/format.h>

namespace multipass
{
class ImageNotFoundException : public std::runtime_error
{
public:
    ImageNotFoundException(const std::string& image, const std::string& remote)
        : runtime_error(fmt::format("Unable to find an image matching \"{}\" in remote \"{}\".", image, remote))
    {
    }
};
} // namespace multipass
#endif // MULTIPASS_IMAGE_VAULT_EXCEPTIONS_H
