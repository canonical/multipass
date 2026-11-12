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

#include "tests/unit/common.h"

#include "shared/windows/server_port.h"
#include "shared/windows/wsa_init_wrapper.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <windows.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

namespace
{
const std::string test_key_path = "SOFTWARE\\multipass_tests\\server_port";
const mp::ServerPortConfig::RegistryLocation test_location{
    mp::ServerPortConfig::RegistryLocation::Root::current_user,
    test_key_path};

class ListeningSocket
{
public:
    explicit ListeningSocket(std::uint16_t port = 0)
    {
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        // Gtest fatal assertions cannot be used in constructors (MSVC C2534),
        // so failures are reported by throwing instead.
        const auto fail = [](const char* what) {
            throw std::runtime_error(
                fmt::format("{} failed with WSA error {}", what, WSAGetLastError()));
        };

        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET)
            fail("socket");

        if (bind(sock, reinterpret_cast<const sockaddr*>(&address), sizeof address) != 0)
            fail("bind");

        if (listen(sock, 1) != 0)
            fail("listen");

        auto bound = sockaddr_in{};
        auto bound_size = static_cast<int>(sizeof bound);
        if (getsockname(sock, reinterpret_cast<const sockaddr*>(&bound), &bound_size) != 0)
            fail("getsockname");

        this->port = ntohs(bound.sin_port);
    }

    ~ListeningSocket()
    {
        if (sock != INVALID_SOCKET)
            closesocket(sock);
    }

    ListeningSocket(const ListeningSocket&) = delete;
    ListeningSocket& operator=(const ListeningSocket&) = delete;

    std::uint16_t port_number() const
    {
        return port;
    }

private:
    SOCKET sock{INVALID_SOCKET};
    std::uint16_t port{0};
};

std::optional<std::uint16_t> grab_free_port()
{
    ListeningSocket socket; // binds to an ephemeral port
    return socket.port_number();
}

void seed_registry_port(DWORD port) // raw setup, independent of the code under test
{
    auto key = HKEY{};
    ASSERT_EQ(RegCreateKeyExA(HKEY_CURRENT_USER,
                              test_key_path.c_str(),
                              0,
                              nullptr,
                              REG_OPTION_NON_VOLATILE,
                              KEY_SET_VALUE,
                              nullptr,
                              &key,
                              nullptr),
              ERROR_SUCCESS);
    ASSERT_EQ(RegSetValueExA(key,
                             mp::windows_daemon_port_registry_value,
                             0,
                             REG_DWORD,
                             reinterpret_cast<const BYTE*>(&port),
                             sizeof port),
              ERROR_SUCCESS);
    RegCloseKey(key);
}

class ServerPort : public Test
{
protected:
    void SetUp() override
    {
        wsa.emplace();
        RegDeleteTreeA(HKEY_CURRENT_USER, test_key_path.c_str());
    }

    void TearDown() override
    {
        RegDeleteTreeA(HKEY_CURRENT_USER, test_key_path.c_str());
    }

    std::optional<mp::wsa_init_wrapper> wsa;
};

TEST_F(ServerPort, configuredIsNothingWhenRegistryValueMissing)
{
    EXPECT_EQ(mp::ServerPortConfig{test_location}.configured(), std::nullopt);
}

TEST_F(ServerPort, configuredReadsRegistryValue)
{
    seed_registry_port(25051);
    EXPECT_EQ(mp::ServerPortConfig{test_location}.configured(),
              std::optional<std::uint16_t>{25051});
}

TEST_F(ServerPort, configuredIgnoresNonDWORDRegistryValues)
{
    auto key = HKEY{};
    ASSERT_EQ(RegCreateKeyExA(HKEY_CURRENT_USER,
                              test_key_path.c_str(),
                              0,
                              nullptr,
                              REG_OPTION_NON_VOLATILE,
                              KEY_SET_VALUE,
                              nullptr,
                              &key,
                              nullptr),
              ERROR_SUCCESS);
    ASSERT_EQ(RegSetValueExA(key,
                             mp::windows_daemon_port_registry_value,
                             0,
                             REG_SZ,
                             reinterpret_cast<const BYTE*>("25051"),
                             6),
              ERROR_SUCCESS);
    RegCloseKey(key);

    EXPECT_EQ(mp::ServerPortConfig{test_location}.configured(), std::nullopt);
}

TEST_F(ServerPort, configuredIgnoresOutOfRangeRegistryValues)
{
    for (const auto bogus : {0u, 65536u, 100000u})
    {
        seed_registry_port(bogus);
        EXPECT_EQ(mp::ServerPortConfig{test_location}.configured(), std::nullopt)
            << "value " << bogus << " should have been ignored";
    }
}

TEST_F(ServerPort, addressCheckThrowsClearErrorWhenPortTaken)
{
    const auto port = grab_free_port();
    ASSERT_TRUE(port.has_value());

    const ListeningSocket socket{*port};
    MP_EXPECT_THROW_THAT(
        mp::throw_if_address_in_use(fmt::format("localhost:{}", *port)),
        std::runtime_error,
        (AllOf(mpt::match_what(HasSubstr(fmt::format("localhost:{}", *port))),
               mpt::match_what(HasSubstr("is not available")),
               mpt::match_what(HasSubstr(fmt::format("PID {}", GetCurrentProcessId()))),
               mpt::match_what(HasSubstr("reg add")),
               mpt::match_what(HasSubstr(mp::windows_daemon_port_registry_value)))));
}

TEST_F(ServerPort, addressCheckPassesWhenPortFree)
{
    const auto port = grab_free_port();
    ASSERT_TRUE(port.has_value());

    EXPECT_NO_THROW(mp::throw_if_address_in_use(fmt::format("localhost:{}", *port)));
}

TEST_F(ServerPort, addressCheckIgnoresUnixAddresses)
{
    const auto port = grab_free_port();
    ASSERT_TRUE(port.has_value());

    const ListeningSocket socket{*port};
    EXPECT_NO_THROW(mp::throw_if_address_in_use("unix:/run/multipass_socket"));
}
} // namespace
