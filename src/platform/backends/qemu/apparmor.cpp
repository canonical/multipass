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

#include <fmt/format.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

mp::AppArmor::AppArmor() : aa_interface(nullptr)
{
    int ret = aa_is_enabled();
    if (ret < 0)
    {
        throw std::runtime_error("AppArmor is not enabled");
    }

    aa_features *features = nullptr;
    aa_features_new_from_kernel(&features);

    ret = aa_kernel_interface_new(&aa_interface, features, nullptr);
    if (ret < 0)
    {
        throw std::runtime_error(fmt::format("Failed to get AppArmor kernel interface: errno={} ({})", errno, strerror(errno)));
    }

    aa_kernel_interface_ref(aa_interface);
}

mp::AppArmor::~AppArmor()
{
    aa_kernel_interface_unref(aa_interface);
}

#include <iostream>

void mp::AppArmor::load_policy(const QString& policy) const
{
    auto copy = policy.trimmed().toLatin1();
    std::cout << "load policy " << qPrintable(copy) << std::endl;
    int ret = aa_kernel_interface_load_policy(aa_interface, copy.constData(), copy.size());
    if (ret < 0)
    {
        if (errno != EEXIST) { // does not already exist
            throw std::runtime_error(fmt::format("Failed to load AppArmor policy: errno={} ({})", errno, strerror(errno)));
        } else {
            replace_policy(policy);
        }
    }
}

void mp::AppArmor::replace_policy(const QString& policy) const
{
    int ret = aa_kernel_interface_replace_policy(aa_interface, policy.toLatin1().constData(), policy.toLatin1().size());
    if (ret < 0)
    {
        throw std::runtime_error(fmt::format("Failed to replace AppArmor policy errno={} ({})", errno, strerror(errno)));
    }
}

void mp::AppArmor::remove_policy(const QString& policy_name) const
{
    int ret = aa_kernel_interface_remove_policy(aa_interface, policy_name.toLatin1().constData());
    if (ret < 0)
    {
        if (errno != ENOENT) { // was already removed
            throw std::runtime_error(fmt::format("Failed to remove AppArmort policy errno={} ({})", errno, strerror(errno)));
        }
    }
}
