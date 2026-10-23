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

#include "server_port.h"

#include <multipass/constants.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/utils.h>

#include "wsa_init_wrapper.h"

#include <windows.h>

#include <ws2tcpip.h>

#include <iphlpapi.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace multipass
{
namespace
{
namespace mpl = multipass::logging;

constexpr auto log_category = "port";

using Root = ServerPortConfig::RegistryLocation::Root;

HKEY hkey_of(Root root)
{
    return root == Root::current_user ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
}

// The registry API is natively UTF-16, so text crosses the boundary as wide
// strings (converted from the UTF-8 used elsewhere in Multipass).
std::wstring to_wide(const std::string& text)
{
    if (text.empty())
        return {};

    const auto size =
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0)
        return {};

    auto wide = std::wstring(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), size);
    return wide;
}

bool can_bind_and_listen(int family, const sockaddr* address, socklen_t address_size)
{
    const auto sock = ::socket(family, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
        return true; // address family unavailable: nothing can be listening on it

    const auto bindable = bind(sock, address, address_size) == 0 && listen(sock, 1) == 0;
    closesocket(sock);
    return bindable;
}

bool loopback_pair_available(std::uint16_t port)
{
    sockaddr_in ipv4{};
    ipv4.sin_family = AF_INET;
    ipv4.sin_port = htons(port);
    ipv4.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    sockaddr_in6 ipv6{};
    ipv6.sin6_family = AF_INET6;
    ipv6.sin6_port = htons(port);
    ipv6.sin6_addr = in6addr_loopback;

    return can_bind_and_listen(AF_INET, reinterpret_cast<const sockaddr*>(&ipv4), sizeof ipv4) &&
           can_bind_and_listen(AF_INET6, reinterpret_cast<const sockaddr*>(&ipv6), sizeof ipv6);
}

bool is_port_in_use(std::uint16_t port)
{
    try
    {
        wsa_init_wrapper wsa;
        return !loopback_pair_available(port);
    }
    catch (...)
    {
        return false; // cannot tell; do not block on this
    }
}

std::optional<std::vector<DWORD>> listener_pids_of_family(ULONG family, std::uint16_t port)
{
    auto table_size = DWORD{};
    if (GetExtendedTcpTable(nullptr, &table_size, FALSE, family, TCP_TABLE_OWNER_PID_LISTENER, 0) !=
        ERROR_INSUFFICIENT_BUFFER)
        return std::nullopt;

    auto table = std::vector<char>(table_size);
    if (GetExtendedTcpTable(table.data(),
                            &table_size,
                            FALSE,
                            family,
                            TCP_TABLE_OWNER_PID_LISTENER,
                            0) != NO_ERROR)
        return std::nullopt;

    const auto network_port = htons(port); // ports in these tables are in network byte order
    auto pids = std::vector<DWORD>{};

    const auto add_matching_pids = [&network_port, &pids](auto&& row) {
        if (row.dwLocalPort == network_port &&
            std::find(pids.begin(), pids.end(), row.dwOwningPid) == pids.end())
        {
            pids.push_back(row.dwOwningPid);
        }
    };

    if (family == AF_INET)
    {
        const auto* tcp_table = reinterpret_cast<const MIB_TCPTABLE_OWNER_PID*>(table.data());
        for (DWORD i = 0; i < tcp_table->dwNumEntries; ++i)
            add_matching_pids(tcp_table->table[i]);
    }
    else
    {
        const auto* tcp6_table = reinterpret_cast<const MIB_TCP6TABLE_OWNER_PID*>(table.data());
        for (DWORD i = 0; i < tcp6_table->dwNumEntries; ++i)
            add_matching_pids(tcp6_table->table[i]);
    }

    return pids;
}

std::vector<DWORD> listener_pids(std::uint16_t port)
{
    auto pids = std::vector<DWORD>{};
    for (const auto family : {AF_INET, AF_INET6})
    {
        if (const auto family_pids = listener_pids_of_family(family, port))
            pids.insert(pids.end(), family_pids->begin(), family_pids->end());
    }
    return pids;
}

std::string process_description(DWORD pid)
{
    auto image = std::wstring(1024, L'\0');
    auto image_size = static_cast<DWORD>(image.size());

    const auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process != nullptr)
    {
        const auto named = QueryFullProcessImageNameW(process, 0, image.data(), &image_size) != 0;
        CloseHandle(process);

        if (named)
        {
            image.resize(image_size);
            return fmt::format("'{}' (PID {})",
                               std::filesystem::path{image}.filename().string(),
                               pid);
        }
    }

    return fmt::format("an inaccessible process (PID {})", pid);
}

std::string describe_port_owner(std::uint16_t port)
{
    auto description = std::string{};
    for (const auto pid : listener_pids(port))
    {
        if (!description.empty())
            description += ", ";

        description += process_description(pid);
    }

    return description;
}

std::string because_in_use_by(std::uint16_t port)
{
    const auto owner = describe_port_owner(port);
    return owner.empty() ? "" : fmt::format(" (it is already in use by {})", owner);
}

bool held_by_multipass_daemon(std::uint16_t port)
{
    return describe_port_owner(port).find("multipassd") != std::string::npos;
}

std::optional<std::uint16_t> tcp_port_of(const std::string& server_address)
{
    if (server_address.starts_with("unix"))
        return std::nullopt;

    const auto separator = server_address.find_last_of(':');
    if (separator == std::string::npos)
        return std::nullopt;

    const auto port_token = server_address.substr(separator + 1);
    const auto digits_only = std::all_of(port_token.begin(), port_token.end(), [](unsigned char c) {
        return std::isdigit(c) != 0;
    });
    if (port_token.empty() || !digits_only)
        return std::nullopt;

    try
    {
        const auto port = std::stoi(port_token);
        if (port <= 0 || port > max_tcp_port)
            return std::nullopt;

        return static_cast<std::uint16_t>(port);
    }
    catch (const std::exception&)
    {
        return std::nullopt; // e.g. out of range
    }
}

// Check an address pinned by the user (the --address option or the
// MULTIPASS_SERVER_ADDRESS environment variable): throw a crystal clear error
// - naming the process in the way and how to resolve it - if its TCP port is
// not available, since silently moving away from an explicit choice is not an
// option. Does nothing for unix sockets.
void throw_if_address_in_use(const std::string& server_address)
{
    const auto port = tcp_port_of(server_address);
    if (!port || !is_port_in_use(*port))
        return;

    if (held_by_multipass_daemon(*port))
        throw std::runtime_error(fmt::format(
            "another Multipass daemon seems to be running and listening on port {} already",
            *port));

    const auto in_use_by = because_in_use_by(*port);
    throw std::runtime_error(fmt::format(
        "the Multipass daemon cannot listen on '{}', because port {} is not"
        " available{}.\n"
        "Free up the port, or drop the pinned address (the --address option or the {}"
        " environment variable) to let Multipass pick and remember a usable port by"
        " itself.",
        server_address,
        *port,
        in_use_by.empty() ? " (it may be reserved by Windows or another service)" : in_use_by,
        server_address_env_var));
}

std::optional<std::uint16_t> read_configured_port(
    const ServerPortConfig::RegistryLocation& location)
{
    auto port = DWORD{};
    auto port_size = static_cast<DWORD>(sizeof port);

    const auto status = RegGetValueW(hkey_of(location.root),
                                     to_wide(location.key_path).c_str(),
                                     to_wide(location.value_name).c_str(),
                                     RRF_RT_REG_DWORD,
                                     nullptr,
                                     &port,
                                     &port_size);
    if (status != ERROR_SUCCESS || port == 0 || port > max_tcp_port)
        return std::nullopt;

    return static_cast<std::uint16_t>(port);
}

bool write_configured_port(std::uint16_t port, const ServerPortConfig::RegistryLocation& location)
{
    auto key = HKEY{};
    if (RegCreateKeyExW(hkey_of(location.root),
                        to_wide(location.key_path).c_str(),
                        0,
                        nullptr,
                        REG_OPTION_NON_VOLATILE,
                        KEY_SET_VALUE,
                        nullptr,
                        &key,
                        nullptr) != ERROR_SUCCESS)
        return false;

    // RAII guard that closes the key on any exit path.
    const std::unique_ptr<std::remove_pointer_t<HKEY>, decltype(&RegCloseKey)> key_guard{
        key,
        RegCloseKey};

    const auto port_data = static_cast<DWORD>(port);
    return RegSetValueExW(key_guard.get(),
                          to_wide(location.value_name).c_str(),
                          0,
                          REG_DWORD,
                          reinterpret_cast<const BYTE*>(&port_data),
                          sizeof port_data) == ERROR_SUCCESS;
}
} // namespace

PortCandidateGenerator::PortCandidateGenerator(std::uint16_t preferred_port, int max_attempts)
    : preferred{preferred_port}, next_candidate{preferred_port}, attempts_left{max_attempts}
{
}

std::optional<std::uint16_t> PortCandidateGenerator::next_free()
{
    try
    {
        wsa_init_wrapper wsa;

        while (attempts_left > 0 && next_candidate <= max_tcp_port)
        {
            const auto candidate = static_cast<std::uint16_t>(next_candidate);
            ++next_candidate;
            --attempts_left;

            if (loopback_pair_available(candidate))
            {
                mpl::debug(log_category, "port {} is available", candidate);
                return candidate;
            }

            const auto in_use_by = because_in_use_by(candidate);
            if (in_use_by.empty())
                mpl::info(log_category, "port {} is not available; trying the next one", candidate);
            else
                mpl::info(log_category, "port {} is{}; trying the next one", candidate, in_use_by);
        }
    }
    catch (...)
    {
    }

    mpl::warn(log_category, "no free port is available (search started at {})", preferred);
    return std::nullopt;
}

ServerPortConfig::ServerPortConfig() : ServerPortConfig{RegistryLocation{}}
{
}

ServerPortConfig::ServerPortConfig(RegistryLocation loc) : location{std::move(loc)}
{
}

std::optional<std::uint16_t> ServerPortConfig::configured() const
{
    return read_configured_port(location);
}

std::string ServerPortConfig::resolve_address(const std::string& cli_address,
                                              const std::string& env_address,
                                              int max_attempts) const
{
    const auto& pinned = cli_address.empty() ? env_address : cli_address;
    if (!pinned.empty())
    {
        utils::validate_server_address(pinned);
        throw_if_address_in_use(pinned);
        return pinned;
    }

    return fmt::format("localhost:{}", usable_port(default_grpc_server_tcp_port, max_attempts));
}

std::uint16_t ServerPortConfig::usable_port(std::uint16_t preferred_port, int max_attempts) const
{
    if (const auto configured = read_configured_port(location))
    {
        if (held_by_multipass_daemon(*configured))
            throw std::runtime_error(fmt::format(
                "another Multipass daemon seems to be running and listening on port {} already",
                *configured));

        if (!is_port_in_use(*configured))
        {
            mpl::debug(log_category,
                       "keeping configured port {}, which is still usable",
                       *configured);
            return *configured;
        }

        mpl::info(log_category,
                  "configured port {} is not available{}; looking for another one",
                  *configured,
                  because_in_use_by(*configured));
    }

    auto candidates = PortCandidateGenerator{preferred_port, max_attempts};
    while (const auto candidate = candidates.next_free())
    {
        if (write_configured_port(*candidate, location))
        {
            mpl::info(log_category, "daemon port set to {}", *candidate);
            return *candidate;
        }

        throw std::runtime_error(
            fmt::format("could not save the Multipass daemon port to the registry (value '{}' under"
                        " '{}'); an administrator prompt is needed to set it manually",
                        location.value_name,
                        location.key_path));
    }

    throw std::runtime_error(fmt::format(
        "could not find a free TCP port for the Multipass daemon (searched from {} upwards).\n"
        "Free up a port, or configure a specific one in an administrator prompt:\n"
        "  reg add \"HKLM\\{}\" /v {} /t REG_DWORD /d <free port> /f",
        preferred_port,
        windows_daemon_port_registry_key,
        windows_daemon_port_registry_value));
}
} // namespace multipass
