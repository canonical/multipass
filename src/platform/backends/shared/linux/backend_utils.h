/*
 * Copyright (C) 2018-2021 Canonical, Ltd.
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

#ifndef MULTIPASS_BACKEND_UTILS_H
#define MULTIPASS_BACKEND_UTILS_H

#include <multipass/path.h>
#include <multipass/singleton.h>

#include <chrono>
#include <stdexcept>
#include <string>

#define MP_BACKEND multipass::Backend::instance()

class QDBusError;

namespace multipass
{
class MemorySize;
class ProcessFactory;

namespace backend
{
std::string generate_random_subnet();
std::string get_subnet(const Path& network_dir, const QString& bridge_name);
void check_for_kvm_support();
void check_if_kvm_is_in_use();

class CreateBridgeException : public std::runtime_error
{
public:
    CreateBridgeException(const std::string& detail, const QDBusError& dbus_error, bool rollback = false);
};
} // namespace backend

class Backend : public Singleton<Backend>
{
public:
    using Singleton<Backend>::Singleton;

    virtual std::string create_bridge_with(const std::string& interface);
};
} // namespace multipass
#endif // MULTIPASS_BACKEND_UTILS_H
