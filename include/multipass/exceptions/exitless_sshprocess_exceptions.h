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

#ifndef MULTIPASS_EXITLESS_SSHPROCESS_EXCEPTIONS_H
#define MULTIPASS_EXITLESS_SSHPROCESS_EXCEPTIONS_H

#include "ssh_exception.h"

#include <fmt/format.h>

#include <chrono>
#include <string>

namespace multipass
{
class ExitlessSSHProcessException : public SSHException
{
protected:
    ExitlessSSHProcessException(const std::string& command, const std::string& cause)
        : SSHException{fmt::format("failed to obtain exit status for remote process '{}': {}", command, cause)}
    {
    }
};

class SSHProcessTimeoutException : public ExitlessSSHProcessException
{
public:
    SSHProcessTimeoutException(const std::string& command, std::chrono::milliseconds timeout)
        : ExitlessSSHProcessException{command, fmt::format("timed out after {} ms", timeout.count())}
    {
    }
};

class SSHProcessExitError : public ExitlessSSHProcessException
{
public:
    SSHProcessExitError(const std::string& command, const std::string& error)
        : ExitlessSSHProcessException{command, fmt::format("SSH error: {}", error)}
    {
    }
};

} // namespace multipass
#endif // MULTIPASS_EXITLESS_SSHPROCESS_EXCEPTIONS_H
