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
#include "mock_ssh_process.h"
#include "mock_ssh_session.h"

#include <src/sshfs_mount/sshfs_client_composer.h>

#include <multipass/exceptions/sshfs_missing_error.h>
#include <multipass/format.h>

#include <map>
#include <memory>
#include <string>
#include <type_traits>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
static_assert(std::has_virtual_destructor_v<mp::SshfsClientComposer>);
static_assert(!std::is_copy_constructible_v<mp::SshfsClientComposer>);
static_assert(!std::is_copy_assignable_v<mp::SshfsClientComposer>);

struct TestSshfsClientComposer : public Test
{
    TestSshfsClientComposer()
    {
        ON_CALL(session, exec)
            .WillByDefault([this](const std::string& cmd, bool) -> std::unique_ptr<mp::SSHProcess> {
                const auto& result = result_for(cmd);
                auto proc = std::make_unique<NiceMock<mpt::MockSSHProcess>>();

                ON_CALL(*proc, exit_code).WillByDefault(Return(result.exit_code));
                ON_CALL(*proc, read_std_output).WillByDefault(Return(result.std_out));
                ON_CALL(*proc, get_cmd).WillByDefault(ReturnRefOfCopy(cmd));

                return proc;
            });
    }

    /**
     * What a mocked guest command reports back. Commands that no test registers succeed silently.
     */
    struct ExecResult
    {
        int exit_code = 0;
        std::string std_out = {};
    };

    const ExecResult& result_for(const std::string& cmd) const
    {
        static const ExecResult default_result{};

        const auto it = exec_results.find(cmd);
        return it == exec_results.end() ? default_result : it->second;
    }

    /**
     * Have the guest report no snap, but an sshfs on PATH answering @p version_info to `-V`.
     */
    void mock_distro_sshfs(const std::string& version_info = {})
    {
        exec_results[snap_env_cmd] = {.exit_code = 1};
        exec_results[which_cmd] = {.std_out = distro_sshfs};
        exec_results[fmt::format("sudo {} -V", distro_sshfs)] = {.std_out = version_info};
    }

    /**
     * The command expected to mount with the sshfs on PATH, given the @p extra_options that its
     * fuse version calls for.
     */
    std::string expected_distro_command(const std::string& extra_options = {}) const
    {
        return fmt::format("sudo -n {} {}{} :\"{}\" \"{}\"",
                           distro_sshfs,
                           base_options,
                           extra_options,
                           source,
                           target);
    }

    constexpr static auto source = "/host/source";
    constexpr static auto target = "/guest/target";
    constexpr static auto snap_env_cmd = "snap run multipass-sshfs.env";
    constexpr static auto which_cmd = "sudo which sshfs";
    constexpr static auto distro_sshfs = "/usr/bin/sshfs";
    constexpr static auto base_options =
        "-o slave -o transform_symlinks -o allow_other -o Compression=no";

    std::map<std::string, ExecResult> exec_results;

    NiceMock<mpt::MockSSHSession> session;
    mp::SshfsClientComposer composer;
};
} // namespace

TEST_F(TestSshfsClientComposer, buildsCommandAroundSnapSshfs)
{
    exec_results[snap_env_cmd] = {.std_out = "LD_LIBRARY_PATH=/snap/multipass-sshfs/x1/lib\n"
                                             "SNAP=/snap/multipass-sshfs/x1\n"};

    EXPECT_THAT(composer.compose_client_command(session, source, target),
                Eq(fmt::format("sudo -n env LD_LIBRARY_PATH=/snap/multipass-sshfs/x1/lib "
                               "/snap/multipass-sshfs/x1/bin/sshfs {} :\"{}\" \"{}\"",
                               base_options,
                               source,
                               target)));
}

TEST_F(TestSshfsClientComposer, throwsWhenGuestHasNoSshfs)
{
    exec_results[snap_env_cmd] = {.exit_code = 1};
    exec_results[which_cmd] = {.exit_code = 1};

    EXPECT_THROW(composer.compose_client_command(session, source, target), mp::SSHFSMissingError);
}

TEST_F(TestSshfsClientComposer, fallsBackToAvailableSshfs)
{
    mock_distro_sshfs();

    EXPECT_THAT(composer.compose_client_command(session, source, target),
                Eq(expected_distro_command()));
}

TEST_F(TestSshfsClientComposer, asksLegacyFuseToMountOverNonEmptyDirsWithoutCaching)
{
    mock_distro_sshfs("FUSE library version: 2.9.9\n");

    EXPECT_THAT(composer.compose_client_command(session, source, target),
                Eq(expected_distro_command(" -o nonempty -o cache=no")));
}

TEST_F(TestSshfsClientComposer, asksCurrentFuseNotToCacheDirs)
{
    mock_distro_sshfs("FUSE library version 3.10.5\n");

    EXPECT_THAT(composer.compose_client_command(session, source, target),
                Eq(expected_distro_command(" -o dir_cache=no")));
}

TEST_F(TestSshfsClientComposer, leavesFuseOptionsOutWhenTheVersionIsUnparseable)
{
    mock_distro_sshfs("FUSE library version and then some gibberish\n");

    EXPECT_THAT(composer.compose_client_command(session, source, target),
                Eq(expected_distro_command()));
}

TEST_F(TestSshfsClientComposer, leavesFuseOptionsOutWhenTheVersionIsAbsent)
{
    mock_distro_sshfs("FUSE library version\n");

    EXPECT_THAT(composer.compose_client_command(session, source, target),
                Eq(expected_distro_command()));
}
