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

#include <atomic>
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
    auto* handler = expect_handler("/t", native);
    EXPECT_CALL(*handler, activate_impl).Times(0);
    EXPECT_CALL(report, Call(MountMap{{"/t", native}}));

    mounts.add("/t", native, server);
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

TEST_F(VirtualMachineMounts, replaceRecreatesFailedMountDuringActivateAll)
{
    auto* first = add_inactive("/first", native);
    auto* second = add_inactive("/second", native);
    std::atomic<bool> failed_one{false};
    std::promise<std::string> failed_target;
    mp::Signal blocking, release;

    auto on_activate = [&](const std::string& target) {
        if (!failed_one.exchange(true))
        {
            failed_target.set_value(target);
            throw std::runtime_error{"boom"};
        }
        blocking.signal();
        release.wait();
    };
    EXPECT_CALL(*first, activate_impl).WillOnce([&](auto&&...) { on_activate("/first"); });
    EXPECT_CALL(*second, activate_impl).WillOnce([&](auto&&...) { on_activate("/second"); });

    auto activation = std::async(std::launch::async, [&] { return mounts.activate_all(server); });
    blocking.wait();
    const auto failed = failed_target.get_future().get();

    const MountMap restored{{"/first", native}, {"/second", native}};
    auto* recreated = expect_handler(failed, native);
    EXPECT_CALL(report, Call(restored));
    mounts.replace(restored);

    EXPECT_CALL(report, Call(restored));
    release.signal();
    const auto result = activation.get();
    EXPECT_THAT(result.pruned, ElementsAre(Pair(failed, "boom")));
    EXPECT_EQ(mounts.specs(), restored);

    EXPECT_CALL(*recreated, activate_impl);
    EXPECT_THAT(mounts.activate_all(server).pruned, IsEmpty());
}

TEST_F(VirtualMachineMounts, replaceWaitsForDyingHandlerBeforeRecreatingSameSpec)
{
    auto* handler = add_inactive("/t", native);
    mp::Signal destroying, release;
    std::atomic<bool> destroyed_done{false};

    EXPECT_CALL(*handler, activate_impl).WillOnce(Throw(std::runtime_error{"boom"}));
    EXPECT_CALL(*handler, destroyed).WillOnce([&] {
        destroying.signal();
        release.wait();
        destroyed_done = true;
    });

    auto activation = std::async(std::launch::async, [&] { return mounts.activate_all(server); });
    destroying.wait();

    auto replacement = std::make_unique<NiceMock<TestMountHandler>>("/t", native);
    auto* replacement_raw = replacement.get();
    EXPECT_CALL(make_handler, Call("/t", native)).WillOnce([&](auto&&...) {
        EXPECT_TRUE(destroyed_done.load());
        return std::move(replacement);
    });
    // Both replace() and activate_all() may report, in either order; final state must contain /t.
    EXPECT_CALL(report, Call(_)).Times(AtLeast(1));

    auto replacing = std::async(std::launch::async, [&] { mounts.replace({{"/t", native}}); });
    EXPECT_EQ(replacing.wait_for(std::chrono::milliseconds{50}), std::future_status::timeout);

    release.signal();
    activation.get();
    replacing.get();

    EXPECT_EQ(mounts.specs(), (MountMap{{"/t", native}}));
    EXPECT_CALL(*replacement_raw, activate_impl);
    EXPECT_THAT(mounts.activate_all(server).pruned, IsEmpty());
}

TEST_F(VirtualMachineMounts, replaceWaitsForInFlightAddBeforeReplacing)
{
    running = true;
    mp::Signal activating, release;
    auto* added = expect_handler("/t", native);
    EXPECT_CALL(*added, activate_impl).WillOnce([&](auto&&...) {
        activating.signal();
        release.wait();
    });
    {
        InSequence seq;
        EXPECT_CALL(report, Call(MountMap{{"/t", native}}));
        EXPECT_CALL(*added, destroyed);
        expect_handler("/t", classic);
        EXPECT_CALL(report, Call(MountMap{{"/t", classic}}));
    }

    auto adding = std::async(std::launch::async, [&] { mounts.add("/t", native, server); });
    activating.wait();

    auto replacing = std::async(std::launch::async, [&] { mounts.replace({{"/t", classic}}); });
    EXPECT_EQ(replacing.wait_for(std::chrono::milliseconds{50}), std::future_status::timeout);

    release.signal();
    adding.get();
    replacing.get();

    EXPECT_EQ(mounts.specs(), (MountMap{{"/t", classic}}));
}

TEST_F(VirtualMachineMounts, addWaitsForReplaceInFlight)
{
    auto* old = add_inactive("/old", native);
    mp::Signal destroying, release;
    EXPECT_CALL(*old, destroyed).WillOnce([&] {
        destroying.signal();
        release.wait();
    });
    expect_handler("/t", classic);
    EXPECT_CALL(report, Call(MountMap{{"/t", classic}}));

    auto replacing = std::async(std::launch::async, [&] { mounts.replace({{"/t", classic}}); });
    destroying.wait();

    auto adding = std::async(std::launch::async, [&] { mounts.add("/t", native, server); });
    EXPECT_EQ(adding.wait_for(std::chrono::milliseconds{50}), std::future_status::timeout);

    release.signal();
    replacing.get();
    EXPECT_THROW(adding.get(), mp::MountTargetInUseError);
    EXPECT_EQ(mounts.specs(), (MountMap{{"/t", classic}}));
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

TEST_F(VirtualMachineMounts, replaceReportsReinsertionAfterConcurrentRemove)
{
    const MountMap restored{{"/kept", native}};
    add_inactive("/kept", native);
    auto* dying = add_inactive("/dying", native);
    mp::Signal destroying, release;
    EXPECT_CALL(*dying, destroyed).WillOnce([&] {
        destroying.signal();
        release.wait();
    });
    MountMap last_report;
    EXPECT_CALL(report, Call(_)).WillRepeatedly(SaveArg<0>(&last_report));
    expect_handler("/kept", native);

    auto removing = std::async(std::launch::async, [&] { mounts.remove("/dying"); });
    destroying.wait();
    auto replacing = std::async(std::launch::async, [&] { mounts.replace(restored); });
    EXPECT_EQ(replacing.wait_for(std::chrono::milliseconds{50}), std::future_status::timeout);

    mounts.remove("/kept");
    release.signal();
    removing.get();
    replacing.get();

    EXPECT_EQ(mounts.specs(), restored);
    EXPECT_EQ(last_report, restored);
}

TEST_F(VirtualMachineMounts, replaceDoesNotHoldMapLockDuringHandlerCreation)
{
    add_inactive("/kept", native);

    EXPECT_CALL(make_handler, Call("/new", native)).WillOnce([&](const auto& target, const auto& s) {
        EXPECT_EQ(mounts.specs(), (MountMap{{"/kept", native}}));
        return std::make_unique<NiceMock<TestMountHandler>>(target, s);
    });
    const MountMap target{{"/kept", native}, {"/new", native}};
    EXPECT_CALL(report, Call(target));

    mounts.replace(target);
    EXPECT_EQ(mounts.specs(), target);
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

TEST_F(VirtualMachineMounts, unloadWaitsForInFlightActivationAndDoesNotReport)
{
    auto* handler = add_inactive("/t", native);
    mp::Signal activating, release;
    EXPECT_CALL(*handler, activate_impl).WillOnce([&](auto&&...) {
        activating.signal();
        release.wait();
    });
    EXPECT_CALL(*handler, destroyed);

    auto activation = std::async(std::launch::async, [&] { return mounts.activate_all(server); });
    activating.wait();

    auto unloading = std::async(std::launch::async, [&] { mounts.unload(); });
    EXPECT_EQ(unloading.wait_for(std::chrono::milliseconds{50}), std::future_status::timeout);

    release.signal();
    unloading.get();
    EXPECT_THAT(activation.get().pruned, IsEmpty());
    EXPECT_TRUE(mounts.empty());
    EXPECT_THAT(mounts.specs(), IsEmpty());
}
} // namespace
