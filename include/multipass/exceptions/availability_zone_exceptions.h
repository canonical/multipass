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

#include "formatted_exception_base.h"

namespace multipass
{
struct AvailabilityZoneSerializationError final : FormattedExceptionBase<>
{
    using FormattedExceptionBase::FormattedExceptionBase;
};

struct AvailabilityZoneDeserializationError final : FormattedExceptionBase<>
{
    using FormattedExceptionBase::FormattedExceptionBase;
};

struct AvailabilityZoneManagerSerializationError final : FormattedExceptionBase<>
{
    using FormattedExceptionBase::FormattedExceptionBase;
};

struct AvailabilityZoneManagerDeserializationError final : FormattedExceptionBase<>
{
    using FormattedExceptionBase::FormattedExceptionBase;
};

struct AvailabilityZoneNotFound final : FormattedExceptionBase<>
{
    explicit AvailabilityZoneNotFound(const std::string& name)
        : FormattedExceptionBase{"no AZ with name {:?} found", name}
    {
    }
};

struct NoAvailabilityZoneAvailable final : FormattedExceptionBase<>
{
    NoAvailabilityZoneAvailable() : FormattedExceptionBase{"no AZ is available"}
    {
    }
};
} // namespace multipass

#endif // MULTIPASS_AVAILABILITY_ZONE_EXCEPTIONS_H
