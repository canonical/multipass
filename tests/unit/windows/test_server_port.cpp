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
#include "tests/unit/mock_logger.h"

#include "shared/windows/server_port.h"
#include "shared/windows/wsa_init_wrapper.h"

#include <multipass/constants.h>

#include <winsock2.h>
#include <ws2tcpip.h>

#include <windows.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

namespace mp = multipass;
namespace mpt = mp::test;
namespace mpl = mp::logging;
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

        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        ASSERT_NE(sock, INVALID_SOCKET);
        ASSERT_EQ(bind(sock, reinterpret_cast<const sockaddr*>(&address), sizeof address), 0);
        ASSERT_EQ(listen(sock, 1), 0);

        auto bound = sockaddr_in{};
        auto bound_size = static_cast<int>(sizeof bound);
        ASSERT_EQ(getsockname(sock, reinterpret_cast<sockaddr*>(&bound), &bound_size), 0);
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

bool port_is_free(std::uint16_t port) // IPv4 loopback probe, like the implementation's
{
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    const auto sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
        return true; // cannot tell

    const auto free = bind(sock, reinterpret_cast<const sockaddr*>(&address), sizeof address) ==
                          0 &&
                      listen(sock, 1) == 0;
    closesocket(sock);
    return free;
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
    ASSERT_EQ(
        RegSetValueExA(key, "ServerPort", 0, REG_SZ, reinterpret_cast<const BYTE*>("25051"), 6),
        ERROR_SUCCESS);
    RegCloseKey(key);

    EXPECT_EQ(mp::ServerPortConfig{test_location}.configured(), std::nullopt);
}

TEST_F(ServerPort, configuredIgnoresOutOfRangeRegistryValues)
{
    for (const auto bogus : {0u, 65536u, 100000u})
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
                                 "ServerPort",
                                 0,
                                 REG_DWORD,
                                 reinterpret_cast<const BYTE*>(&bogus),
                                 sizeof bogus),
                  ERROR_SUCCESS);
        RegCloseKey(key);

        EXPECT_EQ(mp::ServerPortConfig{test_location}.configured(), std::nullopt)
            << "value " << bogus << " should have been ignored";
    }
}

TEST_F(ServerPort, usablePortPicksPreferredWhenFreeAndUnset)
{
    const auto preferred = grab_free_port();
    ASSERT_TRUE(preferred.has_value());

    auto quiet_logger = mpt::MockLogger::inject();
    const auto config = mp::ServerPortConfig{test_location};

    const auto port = config.usable_port(*preferred);
    EXPECT_EQ(port, *preferred);
    EXPECT_EQ(config.configured(), preferred);
}

TEST_F(ServerPort, usablePortKeepsUsableConfiguredPort)
{
    const auto configured = grab_free_port();
    const auto preferred = grab_free_port();
    ASSERT_TRUE(configured.has_value() && preferred.has_value() && *configured != *preferred);

    {
        auto quiet_logger = mpt::MockLogger::inject();
        ASSERT_EQ(mp::ServerPortConfig{test_location}.usable_port(*configured), *configured);
    }

    auto logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs();
    logger_scope.mock_logger->expect_log(mpl::Level::debug,
                                         fmt::format("keeping configured port {}", *configured));

    const auto config = mp::ServerPortConfig{test_location};
    EXPECT_EQ(config.usable_port(*preferred), *configured);
    EXPECT_EQ(config.configured(), configured);
}

TEST_F(ServerPort, usablePortReplacesBusyConfiguredPort)
{
    const auto preferred = grab_free_port();
    ASSERT_TRUE(preferred.has_value());

    {
        auto quiet_logger = mpt::MockLogger::inject();
        ASSERT_EQ(mp::ServerPortConfig{test_location}.usable_port(*preferred), *preferred);
    }

    const ListeningSocket socket{*preferred};

    auto logger_scope = mpt::MockLogger::inject();
    EXPECT_CALL(*logger_scope.mock_logger,
                log(mpl::Level::info,
                    _,
                    HasSubstr(fmt::format("configured port {} is not available", *preferred))));
    EXPECT_CALL(*logger_scope.mock_logger,
                log(mpl::Level::info,
                    _,
                    HasSubstr(fmt::format("port {} is (it is already in use by", *preferred))));

    const auto config = mp::ServerPortConfig{test_location};
    const auto port = config.usable_port(*preferred);
    EXPECT_NE(port, *preferred);
    EXPECT_EQ(config.configured(), std::optional<std::uint16_t>{port});
    EXPECT_TRUE(port_is_free(port));
}

TEST_F(ServerPort, usablePortSkipsBusyPreferredPort)
{
    const ListeningSocket socket; // busy preferred port

    auto quiet_logger = mpt::MockLogger::inject();
    const auto config = mp::ServerPortConfig{test_location};

    const auto port = config.usable_port(socket.port_number());
    EXPECT_NE(port, socket.port_number());
    EXPECT_EQ(config.configured(), std::optional<std::uint16_t>{port});
    EXPECT_TRUE(port_is_free(port));
}

TEST_F(ServerPort, usablePortThrowsWhenNoFreePortFound)
{
    const ListeningSocket socket; // busy preferred port, single attempt allowed

    auto quiet_logger = mpt::MockLogger::inject();
    MP_EXPECT_THROW_THAT(mp::ServerPortConfig{test_location}.usable_port(socket.port_number(), 1),
                         std::runtime_error,
                         match_what(HasSubstr("could not find a free TCP port")));
}

TEST_F(ServerPort, generatorReturnsPreferredPortWhenFree)
{
    const auto port = grab_free_port();
    ASSERT_TRUE(port.has_value());

    auto logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs();
    logger_scope.mock_logger->expect_log(mpl::Level::debug,
                                         fmt::format("port {} is available", *port));

    mp::PortCandidateGenerator candidates{*port};
    EXPECT_EQ(candidates.next_free(), port);
}

TEST_F(ServerPort, generatorSkipsBusyPortAndLogsIt)
{
    const auto port = grab_free_port();
    ASSERT_TRUE(port.has_value());

    const ListeningSocket socket{*port};

    auto logger_scope = mpt::MockLogger::inject();
    EXPECT_CALL(*logger_scope.mock_logger,
                log(mpl::Level::info, _, HasSubstr("is already in use by")));
    EXPECT_CALL(*logger_scope.mock_logger,
                log(mpl::Level::info, _, HasSubstr(fmt::format("PID {}", GetCurrentProcessId()))));

    mp::PortCandidateGenerator candidates{*port};
    const auto found = candidates.next_free();
    ASSERT_TRUE(found.has_value());
    EXPECT_NE(*found, *port);
    EXPECT_GT(*found, *port);
    EXPECT_TRUE(port_is_free(*found));
}

TEST_F(ServerPort, generatorResumesAfterHandedOutPort)
{
    const auto port = grab_free_port();
    ASSERT_TRUE(port.has_value());

    const ListeningSocket socket{static_cast<std::uint16_t>(*port + 1)}; // busy next candidate

    auto quiet_logger = mpt::MockLogger::inject();
    mp::PortCandidateGenerator candidates{*port};

    const auto first = candidates.next_free();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, *port);

    // The search must resume after the port handed out above, skipping the
    // one that got busy in between.
    const auto second = candidates.next_free();
    ASSERT_TRUE(second.has_value());
    EXPECT_NE(*second, *port + 1);
    EXPECT_GT(*second, *port);
    EXPECT_TRUE(port_is_free(*second));
}

TEST_F(ServerPort, generatorExhaustsAttempts)
{
    const auto port = grab_free_port();
    ASSERT_TRUE(port.has_value());

    const ListeningSocket socket{*port};

    auto logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs();
    logger_scope.mock_logger->expect_log(mpl::Level::info,
                                         fmt::format("port {} is (it is already in use by", *port));
    logger_scope.mock_logger->expect_log(mpl::Level::warning, "no free port is available");

    mp::PortCandidateGenerator candidates{*port, 1};
    EXPECT_EQ(candidates.next_free(), std::nullopt);
}

TEST_F(ServerPort, resolveAddressAcceptsPinnedUnixAddress)
{
    // Unix addresses carry no TCP port to verify and pass through untouched.
    EXPECT_EQ(mp::ServerPortConfig{test_location}.resolve_address("unix:/run/multipass_socket", ""),
              "unix:/run/multipass_socket");
}

TEST_F(ServerPort, resolveAddressPrefersCliOverEnvironmentAndRegistry)
{
    const auto configured = grab_free_port();
    const auto env_port = grab_free_port();
    const auto cli_port = grab_free_port();
    ASSERT_TRUE(configured.has_value() && env_port.has_value() && cli_port.has_value());

    {
        auto quiet_logger = mpt::MockLogger::inject();
        ASSERT_EQ(mp::ServerPortConfig{test_location}.usable_port(*configured), *configured);
    }

    const auto address = mp::ServerPortConfig{test_location}.resolve_address(
        fmt::format("localhost:{}", *cli_port),
        fmt::format("localhost:{}", *env_port));
    EXPECT_EQ(address, fmt::format("localhost:{}", *cli_port));

    // A pinned address leaves the registry untouched.
    EXPECT_EQ(mp::ServerPortConfig{test_location}.configured(),
              std::optional<std::uint16_t>{*configured});
}

TEST_F(ServerPort, resolveAddressPrefersEnvironmentOverRegistry)
{
    const auto configured = grab_free_port();
    const auto env_port = grab_free_port();
    ASSERT_TRUE(configured.has_value() && env_port.has_value());

    {
        auto quiet_logger = mpt::MockLogger::inject();
        ASSERT_EQ(mp::ServerPortConfig{test_location}.usable_port(*configured), *configured);
    }

    const auto address = mp::ServerPortConfig{test_location}.resolve_address(
        "",
        fmt::format("localhost:{}", *env_port));
    EXPECT_EQ(address, fmt::format("localhost:{}", *env_port));
    EXPECT_EQ(mp::ServerPortConfig{test_location}.configured(),
              std::optional<std::uint16_t>{*configured});
}

TEST_F(ServerPort, resolveAddressFallsBackToUsableRegistryPort)
{
    const auto configured = grab_free_port();
    ASSERT_TRUE(configured.has_value());

    {
        auto quiet_logger = mpt::MockLogger::inject();
        ASSERT_EQ(mp::ServerPortConfig{test_location}.usable_port(*configured), *configured);
    }

    auto quiet_logger = mpt::MockLogger::inject();
    EXPECT_EQ(mp::ServerPortConfig{test_location}.resolve_address("", ""),
              fmt::format("localhost:{}", *configured));
}

TEST_F(ServerPort, resolveAddressRejectsBusyPinnedAddress)
{
    const auto port = grab_free_port();
    ASSERT_TRUE(port.has_value());

    const ListeningSocket socket{*port};
    MP_EXPECT_THROW_THAT(
        mp::ServerPortConfig{test_location}.resolve_address(fmt::format("localhost:{}", *port), ""),
        std::runtime_error,
        (AllOf(match_what(HasSubstr("cannot listen on")),
               match_what(HasSubstr(fmt::format("PID {}", GetCurrentProcessId()))))));
}

TEST_F(ServerPort, resolveAddressRejectsInvalidPinnedAddress)
{
    MP_EXPECT_THROW_THAT(mp::ServerPortConfig{test_location}.resolve_address("", "localhost:abc"),
                         std::runtime_error,
                         match_what(HasSubstr("invalid port number in address")));
}
} // namespace
