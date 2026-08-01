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

    constexpr static auto source = "/host/source";
    constexpr static auto target = "/guest/target";
    constexpr static auto which_cmd = "sudo which sshfs";
    constexpr static auto base_options =
        "-o slave -o transform_symlinks -o allow_other -o Compression=no";

    std::map<std::string, ExecResult> exec_results;

    NiceMock<mpt::MockSSHSession> session;
    mp::SshfsClientComposer composer;
};
} // namespace

TEST_F(TestSshfsClientComposer, buildsCommandAroundAvailableSshfs)
{
    const std::string sshfs_path{"/some/usr/bin/sshfs"};
    exec_results[which_cmd] = {.std_out = sshfs_path};

    EXPECT_THAT(
        composer.compose_client_command(session, source, target),
        Eq(fmt::format("sudo -n {} {} :\"{}\" \"{}\"", sshfs_path, base_options, source, target)));
}
