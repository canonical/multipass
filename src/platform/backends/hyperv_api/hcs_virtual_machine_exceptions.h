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

#ifndef MULTIPASS_HYPERV_API_HCS_VIRTUAL_MACHINE_EXCEPTIONS_H
#define MULTIPASS_HYPERV_API_HCS_VIRTUAL_MACHINE_EXCEPTIONS_H

#include <multipass/exceptions/formatted_exception_base.h>

namespace multipass::hyperv
{

struct InvalidAPIPointerException : FormattedExceptionBase<>
{
    using FormattedExceptionBase<>::FormattedExceptionBase;
};

struct CreateComputeSystemException : FormattedExceptionBase<>
{
    using FormattedExceptionBase<>::FormattedExceptionBase;
};

struct ComputeSystemStateException : FormattedExceptionBase<>
{
    using FormattedExceptionBase<>::FormattedExceptionBase;
};

struct CreateEndpointException : FormattedExceptionBase<>
{
    using FormattedExceptionBase<>::FormattedExceptionBase;
};

struct GrantVMAccessException : FormattedExceptionBase<>
{
    using FormattedExceptionBase<>::FormattedExceptionBase;
};

struct ImageConversionException : public FormattedExceptionBase<>
{
    using FormattedExceptionBase<>::FormattedExceptionBase;
};

struct ImageResizeException : public FormattedExceptionBase<>
{
    using FormattedExceptionBase<>::FormattedExceptionBase;
};

struct StartComputeSystemException : public FormattedExceptionBase<>
{
    using FormattedExceptionBase<>::FormattedExceptionBase;
};

} // namespace multipass::hyperv

#endif
