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
#include "mock_virtual_machine.h"

#include <multipass/exceptions/ip_unavailable_exception.h>
#include <multipass/exceptions/ssh_exception.h>
#include <multipass/format.h>
#include <multipass/guest_mount_record.h>

#include <string>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
struct Scenario
{
    std::string name;
    std::string source;
    std::string target;
    // what the shell must receive: the escaped source and the escaped, encoded target
    std::string shell_source;
    std::string shell_record_name;
};

struct TestGuestMountRecord : public Test
{
    static std::string deploy_cmd(const Scenario& scenario)
    {
        return fmt::format("sudo mkdir -p {0} && printf '%s\\n' {1} | sudo tee {0}/{2} >/dev/null",
                           mp::GuestMountRecord::records_dir,
                           scenario.shell_source,
                           scenario.shell_record_name);
    }

    static std::string undeploy_cmd(const Scenario& scenario)
    {
        return fmt::format("sudo rm -f {}/{}",
                           mp::GuestMountRecord::records_dir,
                           scenario.shell_record_name);
    }

    NiceMock<mpt::MockVirtualMachine> vm;
};

TEST_F(TestGuestMountRecord, deployPropagatesExceptions)
{
    const mp::GuestMountRecord record{"/src", "/dst"};

    EXPECT_CALL(vm, ssh_exec).WillOnce(Throw(mp::SSHExecFailure{"failed", 1}));
    EXPECT_THROW(record.deploy(vm), mp::SSHExecFailure);
}

TEST_F(TestGuestMountRecord, undeployPropagatesExceptions)
{
    const mp::GuestMountRecord record{"/src", "/dst"};

    EXPECT_CALL(vm, ssh_exec).WillOnce(Throw(mp::IPUnavailableException{}));
    EXPECT_THROW(record.undeploy(vm), mp::IPUnavailableException);
}

struct TestGuestMountRecordCommands : public TestGuestMountRecord,
                                      public WithParamInterface<Scenario>
{
};

TEST_P(TestGuestMountRecordCommands, deploysRecord)
{
    const auto& scenario = GetParam();
    const mp::GuestMountRecord record{scenario.source, scenario.target};

    EXPECT_CALL(vm, ssh_exec(deploy_cmd(scenario), _));
    record.deploy(vm);
}

TEST_P(TestGuestMountRecordCommands, undeploysRecord)
{
    const auto& scenario = GetParam();
    const mp::GuestMountRecord record{scenario.source, scenario.target};

    EXPECT_CALL(vm, ssh_exec(undeploy_cmd(scenario), _));
    record.undeploy(vm);
}

const auto plain_paths = Scenario{.name = "plainPaths",
                                  .source = "/home/alice/projects",
                                  .target = "/home/ubuntu/projects",
                                  .shell_source = "/home/alice/projects",
                                  .shell_record_name = "%2Fhome%2Fubuntu%2Fprojects"};

const auto windows_source = Scenario{.name = "windowsSource",
                                     .source = R"(C:\Users\alice\my data)",
                                     .target = "/home/ubuntu/data",
                                     .shell_source = R"(C:\\Users\\alice\\my\ data)",
                                     .shell_record_name = "%2Fhome%2Fubuntu%2Fdata"};

const auto shell_metacharacters = Scenario{.name = "shellMetacharacters",
                                           .source = R"(/a $b`c`;'d"e\f)",
                                           .target = R"(/x;$(y) z\w)",
                                           .shell_source = R"(/a\ \$b\`c\`\;\'d\"e\\f)",
                                           .shell_record_name = R"(%2Fx\;\$\(y\)\ z\\w)"};

const auto encoded_slash = Scenario{.name = "encodedSlash",
                                    .source = "/src",
                                    .target = "/a/b",
                                    .shell_source = "/src",
                                    .shell_record_name = "%2Fa%2Fb"};

const auto literal_percent_slash = Scenario{.name = "literalPercentSlash",
                                            .source = "/src",
                                            .target = "/a%2Fb",
                                            .shell_source = "/src",
                                            .shell_record_name = "%2Fa%252Fb"};

INSTANTIATE_TEST_SUITE_P(
    GuestMountRecord,
    TestGuestMountRecordCommands,
    Values(plain_paths, windows_source, shell_metacharacters, encoded_slash, literal_percent_slash),
    [](const TestParamInfo<Scenario>& info) { return info.param.name; });

} // namespace
