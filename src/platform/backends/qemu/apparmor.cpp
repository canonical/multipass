/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include "apparmor.h"

#include <multipass/logging/log.h>
#include <sys/apparmor.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

mp::AppArmor::AppArmor() : aa_interface(nullptr)
{
    int ret = aa_is_enabled();
    if (ret < 0)
    {
        throw std::runtime_error("AppArmor is not enabled");
    }

    ret = aa_kernel_interface_new(&aa_interface, nullptr, nullptr);
    if (ret < 0)
    {
        throw std::runtime_error("Failed to get AppArmor kernel interface");
    }
}

mp::AppArmor::~AppArmor()
{
    aa_kernel_interface_unref(aa_interface);
}

void mp::AppArmor::load_policy(const QByteArray& policy) const
{
    int ret = aa_kernel_interface_load_policy(aa_interface, policy.constData(), policy.size());
    if (ret < 0)
    {
        throw std::runtime_error("Failed to load AppArmor policy");
    }
}

void mp::AppArmor::replace_policy(const QByteArray& policy) const
{
    int ret = aa_kernel_interface_replace_policy(aa_interface, policy.constData(), policy.size());
    if (ret < 0)
    {
        throw std::runtime_error("Failed to replace AppArmor policy");
    }
}

void mp::AppArmor::remove_policy(const QByteArray& policy_name) const
{
    int ret = aa_kernel_interface_remove_policy(aa_interface, policy_name.constData());
    if (ret < 0)
    {
        throw std::runtime_error("Failed to remove AppArmor policy");
    }
}
