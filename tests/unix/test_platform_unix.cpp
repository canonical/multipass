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

#include <tests/common.h>
#include <tests/mock_environment_helpers.h>
#include <tests/mock_platform.h>
#include <tests/temp_file.h>

#include "mock_libc_functions.h"
#include "mock_signal_wrapper.h"

#include <multipass/constants.h>
#include <multipass/format.h>
#include <multipass/platform.h>

#include <thread>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct TestPlatformUnix : public Test
{
    mpt::TempFile file;
};
} // namespace

TEST_F(TestPlatformUnix, setServerSocketRestrictionsNotRestrictedIsCorrect)
{
    auto [mock_platform, guard] = mpt::MockPlatform::inject();

    EXPECT_CALL(*mock_platform, chown(_, 0, 0)).WillOnce(Return(0));
    EXPECT_CALL(*mock_platform, chmod(_, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))
        .WillOnce(Return(0));

    EXPECT_NO_THROW(MP_PLATFORM.Platform::set_server_socket_restrictions(fmt::format("unix:{}", file.name()), false));
}

TEST_F(TestPlatformUnix, setServerSocketRestrictionsRestrictedIsCorrect)
{
    auto [mock_platform, guard] = mpt::MockPlatform::inject();
    const int gid{42};
    struct group group;
    group.gr_gid = gid;

    EXPECT_CALL(*mock_platform, chown(_, 0, gid)).WillOnce(Return(0));
    EXPECT_CALL(*mock_platform, chmod(_, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)).WillOnce(Return(0));

    REPLACE(getgrnam, [&group](auto) { return &group; });

    EXPECT_NO_THROW(MP_PLATFORM.Platform::set_server_socket_restrictions(fmt::format("unix:{}", file.name()), true));
}

TEST_F(TestPlatformUnix, setServerSocketRestrictionsNoUnixPathThrows)
{
    MP_EXPECT_THROW_THAT(MP_PLATFORM.set_server_socket_restrictions(file.name().toStdString(), false),
                         std::runtime_error,
                         mpt::match_what(StrEq(fmt::format("invalid server address specified: {}", file.name()))));
}

TEST_F(TestPlatformUnix, setServerSocketRestrictionsNonUnixTypeReturns)
{
    auto [mock_platform, guard] = mpt::MockPlatform::inject();

    EXPECT_CALL(*mock_platform, chown).Times(0);
    EXPECT_CALL(*mock_platform, chmod).Times(0);

    EXPECT_NO_THROW(MP_PLATFORM.Platform::set_server_socket_restrictions(fmt::format("dns:{}", file.name()), false));
}

TEST_F(TestPlatformUnix, setServerSocketRestrictionsChownFailsThrows)
{
    auto [mock_platform, guard] = mpt::MockPlatform::inject();

    EXPECT_CALL(*mock_platform, chown(_, 0, 0)).WillOnce([](auto...) {
        errno = EPERM;
        return -1;
    });

    MP_EXPECT_THROW_THAT(
        MP_PLATFORM.Platform::set_server_socket_restrictions(fmt::format("unix:{}", file.name()), false),
        std::runtime_error,
        mpt::match_what(StrEq("Could not set ownership of the multipass socket: Operation not permitted")));
}

TEST_F(TestPlatformUnix, setServerSocketRestrictionsChmodFailsThrows)
{
    auto [mock_platform, guard] = mpt::MockPlatform::inject();

    EXPECT_CALL(*mock_platform, chown(_, 0, 0)).WillOnce(Return(0));
    EXPECT_CALL(*mock_platform, chmod(_, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))
        .WillOnce([](auto...) {
            errno = EPERM;
            return -1;
        });

    MP_EXPECT_THROW_THAT(
        MP_PLATFORM.Platform::set_server_socket_restrictions(fmt::format("unix:{}", file.name()), false),
        std::runtime_error,
        mpt::match_what(StrEq("Could not set permissions for the multipass socket: Operation not permitted")));
}

TEST_F(TestPlatformUnix, chmodSetsFileModsAndReturns)
{
    auto perms = QFileInfo(file.name()).permissions();
    ASSERT_EQ(perms, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadUser | QFileDevice::WriteUser);

    EXPECT_EQ(MP_PLATFORM.chmod(file.name().toStdString().c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), 0);

    perms = QFileInfo(file.name()).permissions();

    EXPECT_EQ(perms, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadUser | QFileDevice::WriteUser |
                         QFileDevice::ReadGroup | QFileDevice::WriteGroup);
}

TEST_F(TestPlatformUnix, multipassStorageLocationReturnsExpectedPath)
{
    mpt::SetEnvScope e(mp::multipass_storage_env_var, file.name().toUtf8());

    auto storage_path = MP_PLATFORM.multipass_storage_location();

    EXPECT_EQ(storage_path, file.name());
}

TEST_F(TestPlatformUnix, multipassStorageLocationNotSetReturnsEmpty)
{
    auto storage_path = MP_PLATFORM.multipass_storage_location();

    EXPECT_TRUE(storage_path.isEmpty());
}

TEST_F(TestPlatformUnix, get_cpus_returns_greater_than_zero)
{
    // On any real system, there should be at least 1 CPU
    EXPECT_GT(MP_PLATFORM.get_cpus(), 0);
}

TEST_F(TestPlatformUnix, get_total_ram_returns_greater_than_zero)
{
    // On any real system, there should be some RAM
    EXPECT_GT(MP_PLATFORM.get_total_ram(), 0LL);
}

void test_sigset_empty(const sigset_t& set)
{
    // there is no standard empty check to try a few different signals
    EXPECT_EQ(sigismember(&set, SIGABRT), 0);
    EXPECT_EQ(sigismember(&set, SIGALRM), 0);
    EXPECT_EQ(sigismember(&set, SIGCHLD), 0);
    EXPECT_EQ(sigismember(&set, SIGINT), 0);
    EXPECT_EQ(sigismember(&set, SIGSEGV), 0);
    EXPECT_EQ(sigismember(&set, SIGKILL), 0);
    EXPECT_EQ(sigismember(&set, SIGQUIT), 0);
    EXPECT_EQ(sigismember(&set, SIGTERM), 0);
    EXPECT_EQ(sigismember(&set, SIGUSR1), 0);
    EXPECT_EQ(sigismember(&set, SIGUSR2), 0);
}

bool test_sigset_has(const sigset_t& set, const std::vector<int>& sigs)
{
    bool good = true;
    for (auto sig : sigs)
    {
        auto has_sig = sigismember(&set, sig) == 1;
        if (!has_sig)
            good = false;

        EXPECT_TRUE(has_sig);
    }

    return good;
}

TEST_F(TestPlatformUnix, make_sigset_returns_emptyset)
{
    auto set = mp::platform::make_sigset({});
    test_sigset_empty(set);
}

TEST_F(TestPlatformUnix, make_sigset_makes_sigset)
{
    auto set = mp::platform::make_sigset({SIGINT, SIGUSR2});

    // check signals are set
    test_sigset_has(set, {SIGINT, SIGUSR2});

    // unset set signals
    sigdelset(&set, SIGUSR2);
    sigdelset(&set, SIGINT);

    // check other signals aren't set
    test_sigset_empty(set);
}

TEST_F(TestPlatformUnix, make_and_block_signals_works)
{
    auto [mock_signals, guard] = mpt::MockPosixSignal::inject<StrictMock>();

    EXPECT_CALL(
        *mock_signals,
        pthread_sigmask(SIG_BLOCK, Pointee(Truly([](const auto& set) { return test_sigset_has(set, {SIGINT}); })), _));

    auto set = mp::platform::make_and_block_signals({SIGINT});

    test_sigset_has(set, {SIGINT});

    sigdelset(&set, SIGINT);
    test_sigset_empty(set);
}

TEST_F(TestPlatformUnix, make_quit_watchdog_blocks_signals)
{
    auto [mock_signals, guard] = mpt::MockPosixSignal::inject<StrictMock>();

    EXPECT_CALL(*mock_signals,
                pthread_sigmask(SIG_BLOCK,
                                Pointee(Truly([](const auto& set) {
                                    return test_sigset_has(set, {SIGQUIT, SIGTERM, SIGHUP, SIGUSR2});
                                })),
                                _));

    mp::platform::make_quit_watchdog(std::chrono::milliseconds{1});
}

TEST_F(TestPlatformUnix, quit_watchdog_quits_on_condition)
{
    auto [mock_signals, guard] = mpt::MockPosixSignal::inject<StrictMock>();

    EXPECT_CALL(*mock_signals, pthread_sigmask(SIG_BLOCK, _, _));
    EXPECT_CALL(*mock_signals, sigwait(_, _)).WillRepeatedly(DoAll(SetArgReferee<1>(SIGUSR2), Return(0)));
    ON_CALL(*mock_signals, pthread_kill(pthread_self(), SIGUSR2)).WillByDefault(Return(0));

    auto watchdog = mp::platform::make_quit_watchdog(std::chrono::milliseconds{1});
    EXPECT_EQ(watchdog([] { return false; }), std::nullopt);
}

TEST_F(TestPlatformUnix, quit_watchdog_quits_on_signal)
{
    auto [mock_signals, guard] = mpt::MockPosixSignal::inject<StrictMock>();

    EXPECT_CALL(*mock_signals, pthread_sigmask(SIG_BLOCK, _, _));
    EXPECT_CALL(*mock_signals, sigwait(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(SIGUSR2), Return(0)))
        .WillOnce(DoAll(SetArgReferee<1>(SIGTERM), Return(0)));
    ON_CALL(*mock_signals, pthread_kill(pthread_self(), SIGUSR2)).WillByDefault(Return(0));

    auto watchdog = mp::platform::make_quit_watchdog(std::chrono::milliseconds{1});
    EXPECT_EQ(watchdog([] { return true; }), SIGTERM);
}

TEST_F(TestPlatformUnix, quit_watchdog_signals_itself_asynchronously)
{
    auto [mock_signals, guard] = mpt::MockPosixSignal::inject<StrictMock>();

    std::atomic<bool> signaled = false;
    std::atomic<int> times = 0;

    EXPECT_CALL(*mock_signals, pthread_sigmask(SIG_BLOCK, _, _));
    EXPECT_CALL(*mock_signals, sigwait(_, _))
        .WillRepeatedly(DoAll(
            [&signaled, &times] {
                // busy wait until signaled
                while (!signaled.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }
                times.fetch_add(1, std::memory_order_release);
                signaled.store(false, std::memory_order_release);
            },
            SetArgReferee<1>(SIGUSR2),
            Return(0)));

    EXPECT_CALL(*mock_signals, pthread_kill(pthread_self(), SIGUSR2))
        .WillRepeatedly(DoAll([&signaled] { signaled.store(true, std::memory_order_release); }, Return(0)));

    auto watchdog = mp::platform::make_quit_watchdog(std::chrono::milliseconds{1});
    EXPECT_EQ(watchdog([&times] { return times.load(std::memory_order_acquire) < 10; }), std::nullopt);
    EXPECT_GE(times.load(std::memory_order_acquire), 10);
}
