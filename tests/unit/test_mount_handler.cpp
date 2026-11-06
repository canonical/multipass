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
#include "mock_file_ops.h"
#include "mock_logger.h"
#include "mock_virtual_machine.h"
#include "mount_record_commands.h"

#include <multipass/exceptions/ip_unavailable_exception.h>
#include <multipass/exceptions/ssh_exception.h>
#include <multipass/format.h>
#include <multipass/guest_mount_record.h>
#include <multipass/mount_handler.h>
#include <multipass/vm_mount.h>

#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;

namespace
{

struct PublicMountHandler : public mp::MountHandler
{
    template <typename... Ts>
    PublicMountHandler(Ts&&... params) : mp::MountHandler(std::forward<Ts>(params)...)
    {
    }

    void activate_impl(mp::ServerVariant, std::chrono::milliseconds) override
    {
    }

    void deactivate_impl(bool) override
    {
    }
};

TEST(MountHandler, providesMountSpec)
{
    namespace fs = std::filesystem;

    mp::VMMount mount{"asdf", {}, {}, multipass::VMMount::MountType::Native};

    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, status)
        .WillOnce(Return(fs::file_status{fs::file_type::directory, fs::perms::unknown}));

    PublicMountHandler handler{nullptr, nullptr, mount, ""};
    EXPECT_EQ(handler.get_mount_spec(), mount);
}

struct MountHandlerRecords : public Test
{
    MountHandlerRecords()
    {
        logger_scope.mock_logger->screen_logs(mpl::Level::warning);
    }

    static mpt::MockFileOps::GuardedMock inject_source_dir()
    {
        namespace fs = std::filesystem;

        auto injection = mpt::MockFileOps::inject<NiceMock>();
        ON_CALL(*injection.first, status) // this needs mocking before the MountHandler initializes
            .WillByDefault(Return(fs::file_status{fs::file_type::directory, fs::perms::unknown}));
        return injection;
    }

    const std::string source = "/home/alice/data";
    const std::string target = "/home/ubuntu/data";
    const mp::VMMount mount{source, {}, {}, mp::VMMount::MountType::Classic};
    const std::string expected_deploy_cmd = mpt::mount_record_cmd(mount.get_source_path(),
                                                                  target,
                                                                  &mp::GuestMountRecord::deploy);
    const std::string expected_undeploy_cmd = mpt::mount_record_cmd(
        mount.get_source_path(),
        target,
        &mp::GuestMountRecord::undeploy);

    mpt::MockFileOps::GuardedMock file_ops_injection = inject_source_dir();
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
    NiceMock<mpt::MockVirtualMachine> vm;

    PublicMountHandler handler{&vm, nullptr, mount, target};
};

TEST_F(MountHandlerRecords, activationDeploysRecord)
{
    EXPECT_CALL(vm, ssh_exec(expected_deploy_cmd, _));
    handler.activate({});
}

TEST_F(MountHandlerRecords, deactivationUndeploysRecord)
{
    InSequence seq;
    EXPECT_CALL(vm, ssh_exec(expected_deploy_cmd, _));
    EXPECT_CALL(vm, ssh_exec(expected_undeploy_cmd, _));

    handler.activate({});
    handler.deactivate();
}

TEST_F(MountHandlerRecords, inactiveDeactivationSkipsUndeploy)
{
    EXPECT_CALL(vm, ssh_exec).Times(0);
    handler.deactivate();
}

TEST_F(MountHandlerRecords, noRecordWithoutVm)
{
    PublicMountHandler vmless_handler{nullptr, nullptr, mount, target};

    EXPECT_CALL(vm, ssh_exec).Times(0);
    EXPECT_NO_THROW(vmless_handler.activate({}));
    EXPECT_NO_THROW(vmless_handler.deactivate());
}

TEST_F(MountHandlerRecords, activationPropagatesOtherDeployExceptions)
{
    using ExceptionType = std::runtime_error;
    EXPECT_CALL(vm, ssh_exec(expected_deploy_cmd, _)).WillOnce(Throw(ExceptionType{"unexpected"}));
    EXPECT_THROW(handler.activate({}), ExceptionType);
    EXPECT_TRUE(handler.is_active());
}

TEST_F(MountHandlerRecords, deactivationPropagatesOtherUndeployExceptions)
{
    handler.activate({});

    using ExceptionType = std::overflow_error;
    EXPECT_CALL(vm, ssh_exec(expected_undeploy_cmd, _))
        .WillOnce(Throw(ExceptionType{"unexpected"}));
    EXPECT_THROW(handler.deactivate(), ExceptionType);
    EXPECT_FALSE(handler.is_active());
}

struct MountHandlerRecordWarnings : public MountHandlerRecords,
                                    public WithParamInterface<std::exception_ptr>
{
    void mock_ssh_throw(const std::string& cmd)
    {
        EXPECT_CALL(vm, ssh_exec(cmd, _)).WillOnce(WithoutArgs([this]() -> std::string {
            std::rethrow_exception(e_ptr);
        }));
    }

    void expect_warning(const std::string& action) const
    {
        logger_scope.mock_logger->expect_log(
            mpl::Level::warning,
            fmt::format("Failed to {} the record of mount \"{}\" in instance '{}': {}",
                        action,
                        target,
                        vm.get_name(),
                        expected_what()));
    }

    std::string expected_what() const
    {
        try
        {
            std::rethrow_exception(e_ptr);
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
    }

    const std::exception_ptr& e_ptr = GetParam();
};

TEST_P(MountHandlerRecordWarnings, activationOnlyWarnsOnSshIssue)
{
    mock_ssh_throw(expected_deploy_cmd);
    expect_warning("deploy");

    EXPECT_NO_THROW(handler.activate({}));
    EXPECT_TRUE(handler.is_active());
}

TEST_P(MountHandlerRecordWarnings, deactivationOnlyWarnsOnSshIssue)
{
    handler.activate({});

    mock_ssh_throw(expected_undeploy_cmd);
    expect_warning("undeploy");

    EXPECT_NO_THROW(handler.deactivate());
    EXPECT_FALSE(handler.is_active());
}

INSTANTIATE_TEST_SUITE_P(MountHandlerRecords,
                         MountHandlerRecordWarnings,
                         Values(std::make_exception_ptr(mp::SSHExecFailure{"command failed", 1}),
                                std::make_exception_ptr(mp::IPUnavailableException{})));

} // namespace
