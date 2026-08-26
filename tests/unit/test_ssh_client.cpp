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

#include "common.h"
#include "disabling_macros.h"
#include "fake_key_data.h"
#include "mock_logger.h"
#include "mock_ssh_client.h"
#include "mock_ssh_factory.h"
#include "mock_ssh_session.h"
#include "mock_ssh_test_fixture.h"
#include "stub_console.h"
#include "stub_ssh_key_provider.h"

#include <multipass/ssh/plain_ssh_session.h>
#include <multipass/ssh/ssh_client.h>
#include <multipass/ssh/ssh_factory.h>

#include <type_traits>
#include <utility>

namespace mp = multipass;
namespace mpt = multipass::test;
namespace mpl = multipass::logging;

namespace
{
void expect_ssh_coordinates_eq(const mp::SSHCoordinates& actual, const mp::SSHCoordinates& expected)
{
    EXPECT_EQ(actual.username, expected.username);
    EXPECT_EQ(actual.private_key_as_base64, expected.private_key_as_base64);
    EXPECT_EQ(actual.port, expected.port);
    EXPECT_EQ(actual.tcp_host, expected.tcp_host);

    std::visit(
        [&actual](const auto& expected_vsock_host) {
            using VSOCKHostData = std::decay_t<decltype(expected_vsock_host)>;
            ASSERT_TRUE(std::holds_alternative<VSOCKHostData>(actual.vsock_host));

            const auto& actual_vsock_host = std::get<VSOCKHostData>(actual.vsock_host);
            if constexpr (std::is_same_v<VSOCKHostData, mp::HVSOCKData>)
                EXPECT_EQ(actual_vsock_host.vmid, expected_vsock_host.vmid);
            else if constexpr (std::is_same_v<VSOCKHostData, mp::VSOCKData>)
                EXPECT_EQ(actual_vsock_host.cid, expected_vsock_host.cid);
            else if constexpr (std::is_same_v<VSOCKHostData, mp::USOCKData>)
                EXPECT_EQ(actual_vsock_host.socket_address, expected_vsock_host.socket_address);
        },
        expected.vsock_host);
}

struct SSHClient : public testing::Test
{
    mp::SSHClient make_ssh_client()
    {
        auto coord = make_ssh_coordinates(mp::VSOCKHost{std::monostate{}});
        return {MP_SSH_FACTORY.make_session(coord), console_creator};
    }

    mp::SSHCoordinates make_ssh_coordinates(mp::VSOCKHost vsock_host) const
    {
        return {"ubuntu",
                key_provider.private_key_as_base64(),
                42,
                "theanswertoeverything",
                std::move(vsock_host)};
    }

    mp::SSHCoordinates construct_ssh_client_and_capture_coordinates(
        const mp::SSHCoordinates& coordinates)
    {
        REPLACE(ssh_channel_new,
                [](auto...) { return reinterpret_cast<ssh_channel>(0xdeadbeefdeadbeef); });
        REPLACE(ssh_channel_free, [](auto...) { return; });

        auto [mock_ssh_factory, guard] = mpt::MockSSHFactory::inject();
        mp::SSHCoordinates captured_coordinates;
        EXPECT_CALL(*mock_ssh_factory, make_session(testing::_))
            .WillOnce([&captured_coordinates](const auto& forwarded_coordinates) {
                captured_coordinates = forwarded_coordinates;
                return std::make_unique<testing::NiceMock<mpt::MockSSHSession>>();
            });

        [[maybe_unused]] mp::SSHClient client{coordinates, console_creator};
        return captured_coordinates;
    }

    const mpt::StubSSHKeyProvider key_provider;
    mpt::MockSSHTestFixture mock_ssh_test_fixture;
    mp::SSHClient::ConsoleCreator console_creator = [](auto /*channel*/) {
        return std::make_unique<mpt::StubConsole>();
    };
};
} // namespace

TEST_F(SSHClient, standardCtorDoesNotThrow)
{
    mp::SSHCoordinates coord{"ubuntu",
                             key_provider.private_key_as_base64(),
                             42,
                             "theanswertoeverything",
                             {}};
    EXPECT_NO_THROW(mp::SSHClient(coord, console_creator));
}

TEST_F(SSHClient, forwardsNoVsockCoordinatesToMakeSession)
{
    const auto coordinates = make_ssh_coordinates(mp::VSOCKHost{std::monostate{}});

    expect_ssh_coordinates_eq(construct_ssh_client_and_capture_coordinates(coordinates),
                              coordinates);
}

TEST_F(SSHClient, forwardsHvsockCoordinatesToMakeSession)
{
    const auto coordinates = make_ssh_coordinates(mp::VSOCKHost{mp::HVSOCKData{"test-vmid"}});

    expect_ssh_coordinates_eq(construct_ssh_client_and_capture_coordinates(coordinates),
                              coordinates);
}

TEST_F(SSHClient, forwardsVsockCoordinatesToMakeSession)
{
    const auto coordinates = make_ssh_coordinates(mp::VSOCKHost{mp::VSOCKData{1234}});

    expect_ssh_coordinates_eq(construct_ssh_client_and_capture_coordinates(coordinates),
                              coordinates);
}

TEST_F(SSHClient, forwardsUsockCoordinatesToMakeSession)
{
    const auto coordinates = make_ssh_coordinates(mp::VSOCKHost{mp::USOCKData{"unix/socket/path"}});

    expect_ssh_coordinates_eq(construct_ssh_client_and_capture_coordinates(coordinates),
                              coordinates);
}

TEST_F(SSHClient, execSingleCommandReturnsOKNoFailure)
{
    REPLACE(ssh_channel_get_exit_state, [](ssh_channel_struct*, unsigned int* val, char**, int*) {
        *val = 0;
        return SSH_OK;
    });
    auto client = make_ssh_client();

    EXPECT_EQ(client.exec({{"foo"}}), SSH_OK);
}

TEST_F(SSHClient, execMultipleCommandsReturnsOKNoFailure)
{
    auto client = make_ssh_client();

    std::vector<std::vector<std::string>> commands{{"ls", "-la"}, {"pwd"}};
    REPLACE(ssh_channel_get_exit_state, [](ssh_channel_struct*, unsigned int* val, char**, int*) {
        *val = 0;
        return SSH_OK;
    });
    EXPECT_EQ(client.exec(commands), SSH_OK);
}

TEST_F(SSHClient, execReturnsErrorCodeOnFailure)
{
    auto client = make_ssh_client();
    constexpr int failure_code{127};
    REPLACE(ssh_channel_get_exit_state, [](ssh_channel_struct*, unsigned int* val, char**, int*) {
        *val = failure_code;
        return SSH_OK;
    });

    EXPECT_EQ(client.exec({{"foo"}}), failure_code);
}

TEST_F(SSHClient, DISABLE_ON_WINDOWS(execPollingWorksAsExpected))
{
    int poll_count{0};
    auto client = make_ssh_client();

    mock_ssh_test_fixture.is_eof.returnValue(0);

    auto event_dopoll = [this, &poll_count](auto...) {
        ++poll_count;
        mock_ssh_test_fixture.is_eof.returnValue(true);
        return SSH_OK;
    };

    REPLACE(ssh_channel_get_exit_state, [](ssh_channel_struct*, unsigned int* val, char**, int*) {
        *val = 0;
        return SSH_OK;
    });
    REPLACE(ssh_event_dopoll, event_dopoll);

    EXPECT_EQ(client.exec({{"foo"}}), SSH_OK);
    EXPECT_EQ(poll_count, 1);
}

TEST_F(SSHClient, throwsWhenUnableToOpenSession)
{
    REPLACE(ssh_channel_open_session, [](auto...) { return SSH_ERROR; });

    EXPECT_THROW(make_ssh_client(), std::runtime_error);
}

TEST_F(SSHClient, throwWhenRequestShellFails)
{
    auto client = make_ssh_client();
    REPLACE(ssh_channel_request_pty, [](auto...) { return SSH_OK; });
    REPLACE(ssh_channel_change_pty_size, [](auto...) { return SSH_OK; });
    REPLACE(ssh_channel_request_shell, [](auto...) { return SSH_ERROR; });

    EXPECT_THROW(client.connect(), std::runtime_error);
}

TEST_F(SSHClient, throwWhenRequestExecFails)
{
    auto client = make_ssh_client();
    REPLACE(ssh_channel_request_pty, [](auto...) { return SSH_OK; });
    REPLACE(ssh_channel_change_pty_size, [](auto...) { return SSH_OK; });
    REPLACE(ssh_channel_request_exec, [](auto...) { return SSH_ERROR; });

    EXPECT_THROW(client.exec({{"foo"}}), std::runtime_error);
}

TEST_F(SSHClient, logSignalTerminationFromExitState)
{
    auto client = make_ssh_client();
    constexpr int failure_code{128 + 2}; // SIGINT exit status
    REPLACE(ssh_channel_get_exit_state,
            [](ssh_channel_struct*, unsigned int* val, char** signal, int* core_dump) {
                *val = failure_code;
                *signal = strdup("INT");
                *core_dump = 0;
                return SSH_OK;
            });
    auto logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs(mpl::Level::error);
    logger_scope.mock_logger->expect_log(mpl::Level::error, "Process terminated by signal: INT");
    EXPECT_EQ(client.exec({{"foo"}}), failure_code);
}
