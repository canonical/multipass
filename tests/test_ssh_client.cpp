/*
 * Copyright (C) 2018-2022 Canonical, Ltd.
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
    constexpr auto key_data = "-----BEGIN RSA PRIVATE KEY-----\n"
                              "MIIEpAIBAAKCAQEAv3lEFtxT3kd2OrWQ8k3v1SHILNDwwm9U7awNbLDqVEresZNd\n"
                              "mRGmH381fO8tHpNdeQ+XEMff16FZiMKRQWx5OlHTQ33cS7X/huZ5Ge5YVKsBMmqy\n"
                              "vJADK7Ud38mNaKi3hqepD87labVmY1ARTNHSLDG68c5bIyquvzbawwwkM7qPTbw5\n"
                              "ZHOEpKehwPZ/034oPnmPV3BAbX1HySi/zrOZE/D1JW3uHvhF1yprDWhJumOVBSYB\n"
                              "zDloJSsFfFEEYOzkdmAwZ0q3yfMVxLiwTlP2Hhf+i8kOzjQfz29PPfNwroYJZqKT\n"
                              "Eg8YAqBr28ryHzHa8W+htUoZbntID2w9aDeJ2wIDAQABAoIBABpk0vf7wyve2fNZ\n"
                              "1/MuvyK4F2nmG2oSArkIgIk9EfAwqeX8lGhnQGkTFgJ0zdlrIvVvKrnLc5W7ziXF\n"
                              "/FPyafuaD+87yEQ/gEvONV9XtaFmOTID90N67pT10HpqxC1rJHFRZ0KgmIsr0ENc\n"
                              "ZCYcvkYNTOHMOk/ssE33d8xvPgZLMf/EvVqcgPyhJXXg0Y1po9cLPQjCBCfmigiM\n"
                              "U+3Hlrz8w6rtp3RUuM2jzrA+uHQGSa4fC/Wn2WT5fR2RZz7BPdJ+kHlTfFRq27oy\n"
                              "lsTITYDJf26ej1wmsWIV4AqznV33xSRZBK6KZjq98D6MKc28fyfZQKxnc1jWG1Xi\n"
                              "erLM+YECgYEA73wVxCdX/9eBXlquQ5wrj94UFOV9zlbr/oE0CYZC8SkG6fCf9/a/\n"
                              "lUUz25CK+kugJcOFHOmOXIjydXzDDFXEUgsf6MeX0WiLy6rvFdlq3xnQ9oUKBzCv\n"
                              "6gLL9s2Ozo4NMeY3rlqxAswdyGx3f5HHkB722MeUlafjXPkJ82m61GECgYEAzK2V\n"
                              "iX1v+b76HG9XnDd1eK0L/JINSJrFJUMD1KhY84FmXqPoBWcuoEtUPA+cvOhAzr3l\n"
                              "TFqKbXbY5JVx1c66uEkCMYYVPYyOVZNnEz+bGOmrK2NaYDwIySG6WhD2Zh69VIXa\n"
                              "kfvMzba0M26FXjWBDN3oluT6RLBHb2xdZgMHx7sCgYB1B3QziO5t7cggbbve+kAn\n"
                              "a+TwWT1jSgLFOipNxTiNVPk19QqXSBNTRKAU2cuwiKhYC/XOrSuOeLXTSAagzoDD\n"
                              "fwA25uJ/yNEX1A5F5RteruT4swa1gMtWVcuKbeUtdylnixMGtvbtYQXk3WyAAKM/\n"
                              "AIKsaMtpXsOyuVhthOtxwQKBgQCBvIGtzcHdd01IGtdYoNqoLGANr3IWFGxkSw8x\n"
                              "i6geaWY/FPvr+NRYLIdvLqI2J610nm+qrzVRX2Tpt0SZttkqGLT4OTpbci2CVtWe\n"
                              "INIpv2uNLAPMPiF/hA6AKoJUhqWR3uqFYCsYNfgRJbwJ1DZBtqNIikmMooQVP4YQ\n"
                              "NFmJIwKBgQCjxMF4SFzzRbNfiHKLL39D8RHlCPalbmX2CXaiUT4H1rq2oK3EiI+O\n"
                              "+SzzmxbHAjFRRuKeqhmC9+yhhHssBt6lJe71Fl3e01McjOcW9P1AZQdgYsDyCqR0\n"
                              "Yy460TKDO1em0N9GlXfsYgiSFJv1WmD7M/kvGpGxSERcnR4+bBd2BQ==\n"
                              "-----END RSA PRIVATE KEY-----\n";

    EXPECT_NO_THROW(mp::SSHClient("a", 42, "foo", key_data, console_creator));
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
