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

#include <multipass/singleton.h>
#include <multipass/ssh/ssh_coordinates.h>
#include <multipass/ssh/ssh_session.h>

#define MP_SSH_FACTORY multipass::SSHFactory::instance()

namespace multipass
{
struct SSHKeyDeleter
{
    void operator()(ssh_key key) const;
};
using SSHKeyUPtr = std::unique_ptr<ssh_key_struct, SSHKeyDeleter>;
using SSHSessionUPtr = std::unique_ptr<SSHSession>;

class SSHFactory : public Singleton<SSHFactory>
{
public:
    SSHFactory(const Singleton<SSHFactory>::PrivatePass&) noexcept;

    virtual SSHKeyUPtr make_key(const std::string& private_key_as_base64) const;

    virtual SSHSessionUPtr make_session(const SSHCoordinates& ssh_coordinates) const;
};
} // namespace multipass
