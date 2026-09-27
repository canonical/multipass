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

#include "formatted_exception_base.h"

namespace multipass
{

struct TimeoutException : public FormattedExceptionBase<std::runtime_error>
{
    using FormattedExceptionBase::FormattedExceptionBase;
};

struct SSHTimeoutException : public TimeoutException
{
    using TimeoutException::TimeoutException;
};

struct CloudInitTimeoutException : public TimeoutException
{
    using TimeoutException::TimeoutException;
};

} // namespace multipass
