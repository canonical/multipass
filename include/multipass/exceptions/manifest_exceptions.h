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

#ifndef MULTIPASS_MANIFEST_EXCEPTIONS_H
#define MULTIPASS_MANIFEST_EXCEPTIONS_H

#include <stdexcept>

namespace multipass
{
class GenericManifestException : public std::runtime_error
{
public:
    GenericManifestException(const std::string& details) : runtime_error(details)
    {
    }
};

class EmptyManifestException : public GenericManifestException
{
public:
    EmptyManifestException(const std::string& details) : GenericManifestException(details)
    {
    }
};
} // namespace multipass
#endif // MULTIPASS_MANIFEST_EXCEPTIONS_H
