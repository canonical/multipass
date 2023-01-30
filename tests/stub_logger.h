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

#ifndef MULTIPASS_STUB_LOGGER_H
#define MULTIPASS_STUB_LOGGER_H

#include <multipass/logging/logger.h>

namespace multipass
{
namespace test
{
class StubLogger : public logging::Logger
{
    void log(logging::Level, logging::CString, logging::CString) const
    {
    }
};
} // namespace test
} // namespace multipass

#endif // MULTIPASS_STUB_LOGGER_H
