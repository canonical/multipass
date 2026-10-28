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
#include "temp_dir.h"

#include <multipass/exceptions/mount_exceptions.h>
#include <multipass/exceptions/sshfs_missing_error.h>
#include <multipass/mount_handler.h>
#include <multipass/signal.h>
#include <shared/virtual_machine_mounts.h>

#include <future>
#include <stdexcept>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
using MountMap = mp::VirtualMachineMounts::MountMap;

struct TestMountHandler : public mp::MountHandler
{
    TestMountHandler(const std::string& target, const mp::VMMount& spec)
        : MountHandler{nullptr, nullptr, spec, target}
    {
    }

    ~TestMountHandler() override
    {
        destroyed();
    }

    MOCK_METHOD(void, destroyed, ());
    MOCK_METHOD(void, activate_impl, (mp::ServerVariant, std::chrono::milliseconds), (override));
    MOCK_METHOD(void, deactivate_impl, (bool), (override));
};

struct VirtualMachineMounts : public Test
{
    ~VirtualMachineMounts() override
    {
        mounts.unload();
    }

    NiceMock<TestMountHandler>* expect_handler(const std::string& target, const mp::VMMount& spec)
    {
        auto handler = std::make_unique<NiceMock<TestMountHandler>>(target, spec);
        auto* raw = handler.get();
        EXPECT_CALL(make_handler, Call(target, spec)).WillOnce(Return(std::move(handler)));

        return raw;
    }

    // Adds a mount without activating it
    NiceMock<TestMountHandler>* add_inactive(const std::string& target, const mp::VMMount& spec)
    {
        auto* handler = expect_handler(target, spec);
        EXPECT_CALL(report, Call(_));
        mounts.add(target, spec, server);
        Mock::VerifyAndClearExpectations(&report);

        return handler;
    }

    mpt::TempDir source_dir;
    const mp::VMMount native{source_dir.path().toStdString(),
                             {},
                             {},
                             mp::VMMount::MountType::Native};
    const mp::VMMount classic{source_dir.path().toStdString(),
                              {},
                              {},
                              mp::VMMount::MountType::Classic};
    const mp::ServerVariant server{};

    StrictMock<MockFunction<mp::MountHandler::UPtr(const std::string&, const mp::VMMount&)>>
        make_handler;
    StrictMock<MockFunction<void(const MountMap&)>> report;
    bool running = false;
    mp::VirtualMachineMounts mounts{"vm",
                                    make_handler.AsStdFunction(),
                                    report.AsStdFunction(),
                                    [this] { return running; }};
};

TEST_F(VirtualMachineMounts, loadReportsOnlyWhenMountsAreDropped)
{
    expect_handler("/good", native);
    EXPECT_CALL(make_handler, Call("/bad", _)).WillOnce(Throw(std::runtime_error{"no source"}));
    EXPECT_CALL(report, Call(MountMap{{"/good", native}}));

    mounts.load({{"/good", native}, {"/bad", native}});
}

TEST_F(VirtualMachineMounts, loadDoesNotReportWhenAllMountsLoad)
{
    expect_handler("/good", native);

    mounts.load({{"/good", native}});
    EXPECT_EQ(mounts.specs(), (MountMap{{"/good", native}}));
}

TEST_F(VirtualMachineMounts, addActivatesWhenRunningAndReports)
{
    running = true;
    auto* handler = expect_handler("/t", native);
    EXPECT_CALL(*handler, activate_impl);
    EXPECT_CALL(report, Call(MountMap{{"/t", native}}));

    mounts.add("/t", native, server);
}

TEST_F(VirtualMachineMounts, addDoesNotActivateWhenNotRunning)
{
    auto* handler = add_inactive("/t", native);

    EXPECT_CALL(*handler, activate_impl).Times(0);
    EXPECT_EQ(mounts.specs(), (MountMap{{"/t", native}}));
}

TEST_F(VirtualMachineMounts, addRejectsTargetInUse)
{
    add_inactive("/t", native);

    EXPECT_THROW(mounts.add("/t", classic, server), mp::MountTargetInUseError);
    EXPECT_EQ(mounts.specs(), (MountMap{{"/t", native}}));
}

TEST_F(VirtualMachineMounts, addRejectsTargetWithAddInFlight)
{
    running = true;
    mp::Signal activating, release;
    auto* handler = expect_handler("/t", native);
    EXPECT_CALL(*handler, activate_impl).WillOnce([&](auto&&...) {
        activating.signal();
        release.wait();
    });
    EXPECT_CALL(report, Call(MountMap{{"/t", native}}));

    auto first = std::async(std::launch::async, [&] { mounts.add("/t", native, server); });
    activating.wait();

    EXPECT_THROW(mounts.add("/t", native, server), mp::MountTargetInUseError);

    release.signal();
    first.get();
}

TEST_F(VirtualMachineMounts, addWrapsActivationFailuresAndRollsBack)
{
    running = true;
    auto* handler = expect_handler("/t", native);
    EXPECT_CALL(*handler, activate_impl).WillOnce(Throw(std::runtime_error{"boom"}));

    MP_EXPECT_THROW_THAT(mounts.add("/t", native, server),
                         mp::MountActivationError,
                         mpt::match_what(StrEq("boom")));
    EXPECT_TRUE(mounts.empty());

    expect_handler("/t", native);
    running = false;
    EXPECT_CALL(report, Call(_));
    EXPECT_NO_THROW(mounts.add("/t", native, server));
}

TEST_F(VirtualMachineMounts, addPropagatesSshfsMissingUnwrapped)
{
    running = true;
    auto* handler = expect_handler("/t", classic);
    EXPECT_CALL(*handler, activate_impl).WillOnce(Throw(mp::SSHFSMissingError{}));

    EXPECT_THROW(mounts.add("/t", classic, server), mp::SSHFSMissingError);
    EXPECT_TRUE(mounts.empty());
}

TEST_F(VirtualMachineMounts, addPropagatesCreationFailuresAndReleasesTarget)
{
    EXPECT_CALL(make_handler, Call("/t", _)).WillOnce(Throw(std::runtime_error{"no source"}));

    MP_EXPECT_THROW_THAT(mounts.add("/t", native, server),
                         std::runtime_error,
                         mpt::match_what(StrEq("no source")));
    EXPECT_TRUE(mounts.empty());
}

TEST_F(VirtualMachineMounts, removeDeactivatesThenErasesAndReports)
{
    running = true;
    auto* handler = expect_handler("/t", native);
    expect_handler("/kept", native);
    EXPECT_CALL(report, Call(_)).Times(2);
    mounts.add("/t", native, server);
    mounts.add("/kept", native, server);

    InSequence seq;
    EXPECT_CALL(*handler, deactivate_impl(false));
    EXPECT_CALL(report, Call(MountMap{{"/kept", native}}));

    mounts.remove("/t");
    EXPECT_EQ(mounts.specs(), (MountMap{{"/kept", native}}));
    EXPECT_THROW(mounts.remove("/t"), mp::MountNotFoundError);
}

TEST_F(VirtualMachineMounts, removeKeepsMountWhenDeactivationFails)
{
    running = true;
    auto* handler = expect_handler("/t", native);
    EXPECT_CALL(report, Call(_));
    mounts.add("/t", native, server);

    EXPECT_CALL(*handler, deactivate_impl(false)).WillOnce(Throw(std::runtime_error{"busy"}));

    EXPECT_THROW(mounts.remove("/t"), std::runtime_error);
    EXPECT_EQ(mounts.specs(), (MountMap{{"/t", native}}));

    EXPECT_CALL(*handler, deactivate_impl(false));
    EXPECT_CALL(report, Call(MountMap{}));
    mounts.remove("/t");
    EXPECT_TRUE(mounts.empty());
}

TEST_F(VirtualMachineMounts, removeThrowsForUnknownTarget)
{
    EXPECT_THROW(mounts.remove("/nope"), mp::MountNotFoundError);
}

TEST_F(VirtualMachineMounts, removeAllReturnsFailuresAndReportsOnce)
{
    running = true;
    auto* busy = expect_handler("/busy", native);
    expect_handler("/free", native);
    EXPECT_CALL(report, Call(_)).Times(2);
    mounts.add("/busy", native, server);
    mounts.add("/free", native, server);

    EXPECT_CALL(*busy, deactivate_impl).WillOnce(Throw(std::runtime_error{"busy"}));
    EXPECT_CALL(report, Call(MountMap{{"/busy", native}}));

    EXPECT_THAT(mounts.remove_all(), ElementsAre(Pair("/busy", "busy")));
    EXPECT_EQ(mounts.specs(), (MountMap{{"/busy", native}}));
    EXPECT_THROW(mounts.remove("/free"), mp::MountNotFoundError);

    EXPECT_CALL(*busy, deactivate_impl(false));
    EXPECT_CALL(report, Call(MountMap{}));
    EXPECT_THAT(mounts.remove_all(), IsEmpty());
    EXPECT_TRUE(mounts.empty());
}

TEST_F(VirtualMachineMounts, activateAllPrunesFailuresAndReports)
{
    auto* bad = add_inactive("/bad", native);
    auto* good = add_inactive("/good", native);
    EXPECT_CALL(*bad, activate_impl).WillOnce(Throw(std::runtime_error{"boom"}));
    EXPECT_CALL(*good, activate_impl);
    EXPECT_CALL(report, Call(MountMap{{"/good", native}}));

    const auto result = mounts.activate_all(server);

    EXPECT_THAT(result.pruned, ElementsAre(Pair("/bad", "boom")));
    EXPECT_FALSE(result.sshfs_missing);
}

TEST_F(VirtualMachineMounts, activateAllStopsAtSshfsMissingWithoutPruning)
{
    auto* handler = add_inactive("/t", classic);
    EXPECT_CALL(*handler, activate_impl).WillOnce(Throw(mp::SSHFSMissingError{}));

    const auto result = mounts.activate_all(server);

    EXPECT_TRUE(result.sshfs_missing);
    EXPECT_THAT(result.pruned, IsEmpty());
    EXPECT_EQ(mounts.specs(), (MountMap{{"/t", classic}}));
}

TEST_F(VirtualMachineMounts, replaceDestroysBorrowedHandlersBeforeCreatingReplacements)
{
    auto* first = add_inactive("/first", native);
    auto* second = add_inactive("/second", native);
    std::promise<std::string> activating;
    mp::Signal release;
    EXPECT_CALL(*first, activate_impl).Times(AtMost(1)).WillOnce([&](auto&&...) {
        activating.set_value("/first");
        release.wait();
    });
    EXPECT_CALL(*second, activate_impl).Times(AtMost(1)).WillOnce([&](auto&&...) {
        activating.set_value("/second");
        release.wait();
    });

    auto activation = std::async(std::launch::async, [&] { return mounts.activate_all(server); });
    const auto active_target = activating.get_future().get();
    const auto replaced_target = active_target == "/first" ? "/second" : "/first";
    auto* replaced_handler = active_target == "/first" ? second : first;

    EXPECT_CALL(*replaced_handler, activate_impl).Times(0);
    {
        InSequence seq;
        EXPECT_CALL(*replaced_handler, destroyed);
        expect_handler(replaced_target, classic);
    }
    const MountMap restored{{active_target, native}, {replaced_target, classic}};
    EXPECT_CALL(report, Call(restored));
    mounts.replace(restored);

    release.signal();
    const auto result = activation.get();
    EXPECT_THAT(result.pruned, IsEmpty());
    EXPECT_EQ(mounts.specs(), restored);
}

TEST_F(VirtualMachineMounts, replaceKeepsUnchangedAndReplacesChangedMounts)
{
    add_inactive("/same", native);
    add_inactive("/changed", native);
    add_inactive("/gone", native);

    expect_handler("/changed", classic);
    expect_handler("/new", native);
    const MountMap target{{"/same", native}, {"/changed", classic}, {"/new", native}};
    EXPECT_CALL(report, Call(target));

    mounts.replace(target);
    EXPECT_EQ(mounts.specs(), target);
}

TEST_F(VirtualMachineMounts, replaceDoesNotReportWithoutChanges)
{
    add_inactive("/t", native);

    mounts.replace({{"/t", native}});
}

TEST_F(VirtualMachineMounts, stopAllForcesDeactivation)
{
    running = true;
    auto* handler = expect_handler("/t", native);
    EXPECT_CALL(report, Call(_));
    mounts.add("/t", native, server);

    EXPECT_CALL(*handler, deactivate_impl(true));

    mounts.stop_all();
    EXPECT_EQ(mounts.specs(), (MountMap{{"/t", native}}));
}
} // namespace
