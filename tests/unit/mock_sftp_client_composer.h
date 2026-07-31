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

#include "common.h"

#include <multipass/sshfs_mount/sftp_client_composer.h>

namespace multipass::test
{
struct MockSftpClientComposer : public SftpClientComposer
{
    MOCK_METHOD(std::string,
                compose_client_command,
                (SSHSession & session, const std::string& source, const std::string& target),
                (const, override));
};
} // namespace multipass::test
