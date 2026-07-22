/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
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

#include <optional>
#include <string>

namespace multipass
{
struct WindowsNetworkUtils : public Singleton<WindowsNetworkUtils>
{
    WindowsNetworkUtils(const Singleton<WindowsNetworkUtils>::PrivatePass&) noexcept;

    [[nodiscard]] virtual std::optional<std::string> permanent_ipv4_neighbor(
        const std::string& mac_address) const;
};

inline const WindowsNetworkUtils& windows_network_utils()
{
    return WindowsNetworkUtils::instance();
}
} // namespace multipass
