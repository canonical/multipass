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

#ifndef MULTIPASS_CLIENT_PLATFORM_H
#define MULTIPASS_CLIENT_PLATFORM_H

#include <multipass/singleton.h>

#include <QString>

#include <string>
#include <utility>

#define MP_CLIENT_PLATFORM multipass::cli::platform::Platform::instance()

namespace multipass
{
const auto default_id = -1;
const auto no_id_info_available = -2;

class Terminal;

namespace cli
{
namespace platform
{
class Platform : public Singleton<Platform>
{
public:
    Platform(const Singleton::PrivatePass&) noexcept;

    virtual std::string get_password(Terminal* term) const;
    virtual void enable_ansi_escape_chars() const;
};

void parse_transfer_entry(const QString& entry, QString& path, QString& instance_name);
int getuid();
int getgid();
void open_multipass_shell(const QString& instance_name); // precondition: requires a valid instance name
} // namespace platform
} // namespace cli
} // namespace multipass

inline multipass::cli::platform::Platform::Platform(const PrivatePass& pass) noexcept : Singleton(pass)
{
}

#endif // MULTIPASS_CLIENT_PLATFORM_H
