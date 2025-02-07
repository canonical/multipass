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

#ifndef MULTIPASS_AVAILABILITY_ZONE_EXCEPTIONS_H
#define MULTIPASS_AVAILABILITY_ZONE_EXCEPTIONS_H

#include <stdexcept>
#include <string>

#include <multipass/format.h>

namespace multipass
{
struct AvailabilityZoneError : std::runtime_error
{
    template <typename... Args>
    explicit AvailabilityZoneError(const char* fmt, Args&&... args)
        : runtime_error(fmt::format(fmt, std::forward<Args>(args)...))
    {
    }
};

struct AvailabilityZoneSerializationError final : AvailabilityZoneError
{
    using AvailabilityZoneError::AvailabilityZoneError;
};

struct AvailabilityZoneDeserializationError final : AvailabilityZoneError
{
    using AvailabilityZoneError::AvailabilityZoneError;
};

struct AvailabilityZoneManagerError : std::runtime_error
{
    template <typename... Args>
    explicit AvailabilityZoneManagerError(const char* fmt, Args&&... args)
        : runtime_error(fmt::format(fmt, std::forward<Args>(args)...))
    {
    }
};

struct AvailabilityZoneManagerSerializationError final : AvailabilityZoneManagerError
{
    using AvailabilityZoneManagerError::AvailabilityZoneManagerError;
};

struct AvailabilityZoneManagerDeserializationError final : AvailabilityZoneManagerError
{
    using AvailabilityZoneManagerError::AvailabilityZoneManagerError;
};

struct AvailabilityZoneNotFound final : AvailabilityZoneManagerError
{
    explicit AvailabilityZoneNotFound(const std::string& name)
        : AvailabilityZoneManagerError{"no AZ with name {:?} found", name}
    {
    }
};

struct NoAvailabilityZoneAvailable final : AvailabilityZoneManagerError
{
    NoAvailabilityZoneAvailable() : AvailabilityZoneManagerError{"no AZ is available"}
    {
    }
};
} // namespace multipass

#endif // MULTIPASS_AVAILABILITY_ZONE_EXCEPTIONS_H
