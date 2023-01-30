/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef RETURN_CODES_H
#define RETURN_CODES_H

namespace multipass
{
enum class ParseCode
{
    Ok,
    CommandLineError,
    CommandFail,
    HelpRequested
};

enum ReturnCode
{
    Ok = 0,
    CommandLineError = 1,
    CommandFail = 2,
    DaemonFail = 3,
    Retry = 4
};
} // namespace multipass

#endif // RETURN_CODES_H
