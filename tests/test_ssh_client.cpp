#include "common.h"
#include "disabling_macros.h"
#include "fake_key_data.h"
#include "mock_ssh_client.h"
#include "mock_ssh_test_fixture.h"
#include "stub_console.h"

#include <multipass/ssh/ssh_client.h>
#include <multipass/ssh/ssh_session.h>

namespace mp = multipass;
namespace mpt = multipass::test;

namespace
{
struct SSHClient : public testing::Test
{
    mp::SSHClient make_ssh_client()
    {
        return {std::make_unique<mp::SSHSession>("a", 42), console_creator};
    }

    mpt::MockSSHTestFixture mock_ssh_test_fixture;
    mp::SSHClient::ConsoleCreator console_creator = [](auto /*channel*/) {
        return std::make_unique<mpt::StubConsole>();
    };
};
}

TEST_F(SSHClient, standardCtorDoesNotThrow)
{
    EXPECT_NO_THROW(mp::SSHClient("a", 42, "foo", mpt::fake_key_data, console_creator));
}

TEST_F(SSHClient, execSingleCommandReturnsOKNoFailure)
{
    auto client = make_ssh_client();

    EXPECT_EQ(client.exec({"foo"}), SSH_OK);
}

TEST_F(SSHClient, execMultipleCommandsReturnsOKNoFailure)
{
    auto client = make_ssh_client();

    std::vector<std::vector<std::string>> commands{{"ls", "-la"}, {"pwd"}};
    EXPECT_EQ(client.exec(commands), SSH_OK);
}

TEST_F(SSHClient, execReturnsErrorCodeOnFailure)
{
    const int failure_code{127};
    auto client = make_ssh_client();

    REPLACE(ssh_channel_get_exit_status, [&failure_code](auto) { return failure_code; });

    EXPECT_EQ(client.exec({"foo"}), failure_code);
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

    REPLACE(ssh_event_dopoll, event_dopoll);

    EXPECT_EQ(client.exec({"foo"}), SSH_OK);
    EXPECT_EQ(poll_count, 1);
}

TEST_F(SSHClient, throws_when_unable_to_open_session)
{
    REPLACE(ssh_channel_open_session, [](auto...) { return SSH_ERROR; });

    EXPECT_THROW(make_ssh_client(), std::runtime_error);
}

TEST_F(SSHClient, throw_when_request_shell_fails)
{
    auto client = make_ssh_client();
    REPLACE(ssh_channel_request_pty, [](auto...) { return SSH_OK; });
    REPLACE(ssh_channel_change_pty_size, [](auto...) { return SSH_OK; });
    REPLACE(ssh_channel_request_shell, [](auto...) { return SSH_ERROR; });

    EXPECT_THROW(client.connect(), std::runtime_error);
}

TEST_F(SSHClient, throw_when_request_exec_fails)
{
    auto client = make_ssh_client();
    REPLACE(ssh_channel_request_pty, [](auto...) { return SSH_OK; });
    REPLACE(ssh_channel_change_pty_size, [](auto...) { return SSH_OK; });
    REPLACE(ssh_channel_request_exec, [](auto...) { return SSH_ERROR; });

    EXPECT_THROW(client.exec({"foo"}), std::runtime_error);
}
