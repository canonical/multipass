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

#ifndef MULTIPASS_SSH_EXCEPTION_H
#define MULTIPASS_SSH_EXCEPTION_H

#include <stdexcept>
#include <string>

namespace multipass
{
class SSHException : public std::runtime_error
{
public:
    explicit SSHException(const std::string& what_arg) : runtime_error(what_arg)
    {
    }
};

class SSHExecFailure : public SSHException
{
public:
    SSHExecFailure(const std::string& what_arg, int exit_code) : SSHException{what_arg}, ec{exit_code}
    {
    }

    int exit_code() const
    {
        return ec;
    }

private:
    int ec;
};
} // namespace multipass
#endif // MULTIPASS_SSH_EXCEPTION_H
