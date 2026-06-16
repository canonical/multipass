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

#include "mock_libc_functions.h"
#include "mock_signal_wrapper.h"

#include <tests/unit/common.h>
#include <tests/unit/file_operations.h>
#include <tests/unit/mock_environment_helpers.h>
#include <tests/unit/mock_platform.h>
#include <tests/unit/mock_utils.h>
#include <tests/unit/temp_dir.h>
#include <tests/unit/temp_file.h>

#include <multipass/constants.h>
#include <multipass/file_ops.h>
#include <multipass/format.h>
#include <multipass/platform.h>
#include <multipass/socket.h>

#include <libssh/sftp.h>

#include <fcntl.h>

#include <thread>

#include <sys/socket.h>
#include <unistd.h>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct TestPlatformUnix : public Test
{
    mpt::TempFile file;

    static constexpr auto restricted_permissions{
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write |
        std::filesystem::perms::group_read | std::filesystem::perms::group_write};
    static constexpr auto relaxed_permissions{restricted_permissions |
                                              std::filesystem::perms::others_read |
                                              std::filesystem::perms::others_write};
};
} // namespace

TEST_F(TestPlatformUnix, setServerSocketRestrictionsNotRestrictedIsCorrect)
{
    auto [mock_platform, guard] = mpt::MockPlatform::inject();

    EXPECT_CALL(*mock_platform, chown(_, 0, 0)).WillOnce(Return(0));
    EXPECT_CALL(*mock_platform, set_permissions(_, relaxed_permissions, false))
        .WillOnce(Return(true));

    EXPECT_NO_THROW(
        MP_PLATFORM.Platform::set_server_socket_restrictions(fmt::format("unix:{}", file.name()),
                                                             false));
}

TEST_F(TestPlatformUnix, setServerSocketRestrictionsRestrictedIsCorrect)
{
    auto [mock_platform, guard] = mpt::MockPlatform::inject();
    const int gid{42};
    struct group group;
    group.gr_gid = gid;

    EXPECT_CALL(*mock_platform, chown(_, 0, gid)).WillOnce(Return(0));
    EXPECT_CALL(*mock_platform, set_permissions(_, restricted_permissions, false))
        .WillOnce(Return(true));

    REPLACE(getgrnam, [&group](auto) { return &group; });

    EXPECT_NO_THROW(
        MP_PLATFORM.Platform::set_server_socket_restrictions(fmt::format("unix:{}", file.name()),
                                                             true));
}

TEST_F(TestPlatformUnix, setServerSocketRestrictionsNoUnixPathThrows)
{
    MP_EXPECT_THROW_THAT(
        MP_PLATFORM.set_server_socket_restrictions(file.name().toStdString(), false),
        std::runtime_error,
        mpt::match_what(StrEq(fmt::format("invalid server address specified: {}", file.name()))));
}

TEST_F(TestPlatformUnix, setServerSocketRestrictionsNonUnixTypeReturns)
{
    auto [mock_platform, guard] = mpt::MockPlatform::inject();

    EXPECT_CALL(*mock_platform, chown).Times(0);
    EXPECT_CALL(*mock_platform, set_permissions).Times(0);

    EXPECT_NO_THROW(
        MP_PLATFORM.Platform::set_server_socket_restrictions(fmt::format("dns:{}", file.name()),
                                                             false));
}

TEST_F(TestPlatformUnix, setServerSocketRestrictionsChownFailsThrows)
{
    auto [mock_platform, guard] = mpt::MockPlatform::inject();

    EXPECT_CALL(*mock_platform, chown(_, 0, 0)).WillOnce([](auto...) {
        errno = EPERM;
        return -1;
    });

    MP_EXPECT_THROW_THAT(
        MP_PLATFORM.Platform::set_server_socket_restrictions(fmt::format("unix:{}", file.name()),
                                                             false),
        std::runtime_error,
        mpt::match_what(
            StrEq("Could not set ownership of the multipass socket: Operation not permitted")));
}

TEST_F(TestPlatformUnix, setServerSocketRestrictionsChmodFailsThrows)
{
    auto [mock_platform, guard] = mpt::MockPlatform::inject();

    EXPECT_CALL(*mock_platform, chown(_, 0, 0)).WillOnce(Return(0));
    EXPECT_CALL(*mock_platform, set_permissions(_, relaxed_permissions, false))
        .WillOnce([](auto...) {
            errno = EPERM;
            return false;
        });

    MP_EXPECT_THROW_THAT(
        MP_PLATFORM.Platform::set_server_socket_restrictions(fmt::format("unix:{}", file.name()),
                                                             false),
        std::runtime_error,
        mpt::match_what(StrEq("Could not set permissions for the multipass socket")));
}

TEST_F(TestPlatformUnix, setPermissionsSetsFileModsAndReturns)
{
    auto perms = QFileInfo(file.name()).permissions();
    ASSERT_EQ(perms,
              QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadUser |
                  QFileDevice::WriteUser);

    EXPECT_EQ(MP_PLATFORM.set_permissions(file.name().toStdU16String(), restricted_permissions),
              true);

    perms = QFileInfo(file.name()).permissions();

    EXPECT_EQ(perms,
              QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadUser |
                  QFileDevice::WriteUser | QFileDevice::ReadGroup | QFileDevice::WriteGroup);
}

TEST_F(TestPlatformUnix, multipassStorageLocationReturnsExpectedPath)
{
    mpt::SetEnvScope e(mp::multipass_storage_env_var, file.name().toUtf8());

    auto storage_path = MP_PLATFORM.multipass_storage_location();

    EXPECT_EQ(storage_path, file.name());
}

TEST_F(TestPlatformUnix, shutdownSocketSucceeds)
{
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    EXPECT_NO_THROW(MP_PLATFORM.Platform::shutdown_socket(fds[0]));

    char buf;
    EXPECT_EQ(recv(fds[0], &buf, 1, 0), 0);

    close(fds[0]);
    close(fds[1]);
}

TEST_F(TestPlatformUnix, shutdownSocketEnotconnDoesNotThrow)
{
    auto fd = socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_GE(fd, 0);

    EXPECT_NO_THROW(MP_PLATFORM.Platform::shutdown_socket(fd));

    close(fd);
}

TEST_F(TestPlatformUnix, shutdownSocketOtherErrorThrows)
{
    auto fd = socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_GE(fd, 0);

    close(fd);
    MP_EXPECT_THROW_THAT(MP_PLATFORM.Platform::shutdown_socket(fd),
                         std::system_error,
                         AllOf(mpt::match_what(HasSubstr("Failed to shutdown socket")),
                               Property(&std::system_error::code,
                                        Eq(std::error_code(EBADF, std::generic_category())))));
}

TEST_F(TestPlatformUnix, multipassStorageLocationNotSetReturnsEmpty)
{
    auto storage_path = MP_PLATFORM.multipass_storage_location();

    EXPECT_TRUE(storage_path.isEmpty());
}

TEST_F(TestPlatformUnix, getCpusReturnsGreaterThanZero)
{
    // On any real system, there should be at least 1 CPU
    EXPECT_GT(MP_PLATFORM.get_cpus(), 0);
}

TEST_F(TestPlatformUnix, getTotalRamReturnsGreaterThanZero)
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

TEST_F(TestPlatformUnix, makeSigsetReturnsEmptyset)
{
    auto set = mp::platform::make_sigset({});
    test_sigset_empty(set);
}

TEST_F(TestPlatformUnix, makeSigsetMakesSigset)
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

TEST_F(TestPlatformUnix, makeAndBlockSignalsWorks)
{
    auto [mock_signals, guard] = mpt::MockPosixSignal::inject<StrictMock>();

    EXPECT_CALL(*mock_signals,
                pthread_sigmask(
                    SIG_BLOCK,
                    Pointee(Truly([](const auto& set) { return test_sigset_has(set, {SIGINT}); })),
                    _));

    auto set = mp::platform::make_and_block_signals({SIGINT});

    test_sigset_has(set, {SIGINT});

    sigdelset(&set, SIGINT);
    test_sigset_empty(set);
}

TEST_F(TestPlatformUnix, makeAndBlockSignalsThrowsOnError)
{
    auto [mock_signals, guard] = mpt::MockPosixSignal::inject<StrictMock>();

    EXPECT_CALL(*mock_signals, pthread_sigmask(SIG_BLOCK, _, _)).WillOnce(Return(EPERM));

    MP_EXPECT_THROW_THAT(
        mp::platform::make_and_block_signals({SIGINT}),
        std::runtime_error,
        mpt::match_what(StrEq("Failed to block signals: Operation not permitted")));
}

TEST_F(TestPlatformUnix, makeQuitWatchdogBlocksSignals)
{
    auto [mock_signals, guard] = mpt::MockPosixSignal::inject<StrictMock>();

    EXPECT_CALL(
        *mock_signals,
        pthread_sigmask(SIG_BLOCK,
                        Pointee(Truly([](const auto& set) {
                            return test_sigset_has(set, {SIGQUIT, SIGTERM, SIGHUP, SIGUSR2});
                        })),
                        _));

    mp::platform::make_quit_watchdog(std::chrono::milliseconds{1});
}

TEST_F(TestPlatformUnix, quitWatchdogQuitsOnCondition)
{
    auto [mock_signals, guard] = mpt::MockPosixSignal::inject<StrictMock>();

    EXPECT_CALL(*mock_signals, pthread_sigmask(SIG_BLOCK, _, _));
    EXPECT_CALL(*mock_signals, sigwait(_, _))
        .WillRepeatedly(DoAll(SetArgReferee<1>(SIGUSR2), Return(0)));
    ON_CALL(*mock_signals, pthread_kill(pthread_self(), SIGUSR2)).WillByDefault(Return(0));

    auto watchdog = mp::platform::make_quit_watchdog(std::chrono::milliseconds{1});
    EXPECT_EQ(watchdog([] { return false; }), std::nullopt);
}

TEST_F(TestPlatformUnix, quitWatchdogQuitsOnSignal)
{
    auto [mock_signals, guard] = mpt::MockPosixSignal::inject<StrictMock>();

    std::atomic<bool> signaled = false;
    auto signal_killed = [&signaled]() {
        signaled = true;
        signaled.notify_all();
    };
    auto wait_for_killed = [&signaled]() { signaled.wait(false); };

    EXPECT_CALL(*mock_signals, pthread_sigmask(SIG_BLOCK, _, _));
    EXPECT_CALL(*mock_signals, pthread_kill(pthread_self(), SIGUSR2))
        .Times(Between(1, 2))
        .WillRepeatedly(DoAll(signal_killed, Return(0)));
    EXPECT_CALL(*mock_signals, sigwait(_, _))
        .WillOnce(DoAll(wait_for_killed, SetArgReferee<1>(SIGUSR2), Return(0)))
        .WillOnce(DoAll(SetArgReferee<1>(SIGTERM), Return(0)));

    auto watchdog = mp::platform::make_quit_watchdog(std::chrono::milliseconds{1});
    EXPECT_EQ(watchdog([] { return true; }), SIGTERM);
}

TEST_F(TestPlatformUnix, quitWatchdogSignalsItselfAsynchronously)
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
        .WillRepeatedly(
            DoAll([&signaled] { signaled.store(true, std::memory_order_release); }, Return(0)));

    auto watchdog = mp::platform::make_quit_watchdog(std::chrono::milliseconds{1});
    EXPECT_EQ(watchdog([&times] { return times.load(std::memory_order_acquire) < 10; }),
              std::nullopt);
    EXPECT_GE(times.load(std::memory_order_acquire), 10);
}

TEST_F(TestPlatformUnix, test_qstr_path_conversion)
{
    // ASCII
    QString ascii = QStringLiteral("/some/plain/path.txt");
    EXPECT_EQ(MP_PLATFORM.path_to_qstr(MP_PLATFORM.qstr_to_path(ascii)), ascii);

    // BMP Unicode (Turkish, CJK)
    QString bmp = QStringLiteral("/belgeler/İzmir/日本語.txt");
    EXPECT_EQ(MP_PLATFORM.path_to_qstr(MP_PLATFORM.qstr_to_path(bmp)), bmp);

    // Astral plane (emoji – surrogate pair in UTF-16)
    QString astral = QStringLiteral("/tmp/\U0001F600/file.txt");
    EXPECT_EQ(MP_PLATFORM.path_to_qstr(MP_PLATFORM.qstr_to_path(astral)), astral);

    // Empty
    EXPECT_EQ(MP_PLATFORM.qstr_to_path(QString{}), std::filesystem::path{});

    // Spaces and special filesystem characters
    QString special = QStringLiteral("/path with spaces/file (1).txt");
    EXPECT_EQ(MP_PLATFORM.path_to_qstr(MP_PLATFORM.qstr_to_path(special)), special);
}

struct TestPlatformUnixSftp : public Test
{
    mpt::TempDir temp_dir;

    std::string make_file(const std::string& name, const std::string& content = "data")
    {
        auto path = temp_dir.path().toStdString() + "/" + name;
        mpt::make_file_with_content(QString::fromStdString(path), content);
        return path;
    }

    std::string make_dir(const std::string& name)
    {
        auto path = temp_dir.path().toStdString() + "/" + name;
        ::mkdir(path.c_str(), 0755);
        return path;
    }
};

// ── lstat_attr_from ──────────────────────────────────────────────────────

TEST_F(TestPlatformUnixSftp, lstatAttrFromRegularFileReturnsCorrectType)
{
    auto path = make_file("regular");
    sftp_attributes_struct attr{};

    EXPECT_EQ(MP_PLATFORM.lstat_attr_from(path.c_str(), attr), 0);
    EXPECT_TRUE((attr.permissions & SSH_S_IFMT) == SSH_S_IFREG);
}

TEST_F(TestPlatformUnixSftp, lstatAttrFromDirectoryReturnsCorrectType)
{
    auto path = make_dir("mydir");
    sftp_attributes_struct attr{};

    EXPECT_EQ(MP_PLATFORM.lstat_attr_from(path.c_str(), attr), 0);
    EXPECT_TRUE((attr.permissions & SSH_S_IFMT) == SSH_S_IFDIR);
}

TEST_F(TestPlatformUnixSftp, lstatAttrFromSymlinkReturnsSymlinkType)
{
    auto target = make_file("target");
    auto link = temp_dir.path().toStdString() + "/link";
    ASSERT_EQ(::symlink(target.c_str(), link.c_str()), 0);

    sftp_attributes_struct attr{};
    EXPECT_EQ(MP_PLATFORM.lstat_attr_from(link.c_str(), attr), 0);
    // lstat must NOT follow the symlink
    EXPECT_TRUE((attr.permissions & SSH_S_IFMT) == SSH_S_IFLNK);
}

TEST_F(TestPlatformUnixSftp, lstatAttrFromMissingFileReturnsMinus1AndSetsEnoent)
{
    sftp_attributes_struct attr{};
    EXPECT_EQ(MP_PLATFORM.lstat_attr_from("/this/does/not/exist", attr), -1);
    EXPECT_EQ(errno, ENOENT);
}

TEST_F(TestPlatformUnixSftp, lstatAttrFromFillsTimestampsAndSize)
{
    auto path = make_file("ts-file", "hello");
    sftp_attributes_struct attr{};

    ASSERT_EQ(MP_PLATFORM.lstat_attr_from(path.c_str(), attr), 0);
    EXPECT_GT(attr.mtime, 0u);
    EXPECT_GT(attr.atime, 0u);
    EXPECT_EQ(attr.size, 5u); // "hello"
}

// ── stat_attr_from ───────────────────────────────────────────────────────

TEST_F(TestPlatformUnixSftp, statAttrFromFollowsSymlink)
{
    auto target = make_file("target");
    auto link = temp_dir.path().toStdString() + "/link";
    ASSERT_EQ(::symlink(target.c_str(), link.c_str()), 0);

    sftp_attributes_struct attr{};
    EXPECT_EQ(MP_PLATFORM.stat_attr_from(link.c_str(), attr), 0);
    // stat must follow the symlink → result is a regular file
    EXPECT_TRUE((attr.permissions & SSH_S_IFMT) == SSH_S_IFREG);
}

TEST_F(TestPlatformUnixSftp, statAttrFromMissingFileReturnsMinus1AndSetsEnoent)
{
    sftp_attributes_struct attr{};
    EXPECT_EQ(MP_PLATFORM.stat_attr_from("/this/does/not/exist", attr), -1);
    EXPECT_EQ(errno, ENOENT);
}

// ── fstat_attr_from ──────────────────────────────────────────────────────

TEST_F(TestPlatformUnixSftp, fstatAttrFromOpenFdReturnsCorrectType)
{
    auto path = make_file("fstat-file");
    auto handle =
        std::get<std::unique_ptr<mp::NamedFd>>(MP_FILEOPS.open_fd(path.c_str(), O_RDONLY, 0));
    int fd = handle->fd;
    ASSERT_NE(fd, -1);

    sftp_attributes_struct attr{};
    EXPECT_EQ(MP_PLATFORM.fstat_attr_from(fd, attr), 0);
    EXPECT_TRUE((attr.permissions & SSH_S_IFMT) == SSH_S_IFREG);
}

TEST_F(TestPlatformUnixSftp, fstatAttrFromBadFdReturnsMinus1)
{
    sftp_attributes_struct attr{};
    EXPECT_EQ(MP_PLATFORM.fstat_attr_from(-1, attr), -1);
}

TEST_F(TestPlatformUnixSftp, fstatAttrFromFillsSizeAndTimestamps)
{
    auto path = make_file("fstat-sz", "abcde");
    auto handle =
        std::get<std::unique_ptr<mp::NamedFd>>(MP_FILEOPS.open_fd(path.c_str(), O_RDONLY, 0));
    int fd = handle->fd;
    ASSERT_NE(fd, -1);

    sftp_attributes_struct attr{};
    ASSERT_EQ(MP_PLATFORM.fstat_attr_from(fd, attr), 0);
    EXPECT_EQ(attr.size, 5u);
    EXPECT_GT(attr.mtime, 0u);
}

// ── pread ────────────────────────────────────────────────────────────────

TEST_F(TestPlatformUnixSftp, preadReadsAtOffset)
{
    auto path = make_file("pread-file", "Hello, World!");
    auto handle =
        std::get<std::unique_ptr<mp::NamedFd>>(MP_FILEOPS.open_fd(path.c_str(), O_RDONLY, 0));
    int fd = handle->fd;
    ASSERT_NE(fd, -1);

    std::array<char, 5> buf{};
    // Read "World" starting at offset 7
    auto r = MP_PLATFORM.pread(fd, buf.data(), buf.size(), 7);
    EXPECT_EQ(r, 5);
    EXPECT_EQ(std::string(buf.data(), buf.size()), "World");
}

TEST_F(TestPlatformUnixSftp, preadDoesNotAdvanceFilePosition)
{
    auto path = make_file("pread-pos", "ABCDEFGH");
    auto handle =
        std::get<std::unique_ptr<mp::NamedFd>>(MP_FILEOPS.open_fd(path.c_str(), O_RDONLY, 0));
    int fd = handle->fd;
    ASSERT_NE(fd, -1);

    std::array<char, 4> buf{};
    MP_PLATFORM.pread(fd, buf.data(), buf.size(), 4); // read at offset 4

    // File position must still be 0 — a second pread at 0 gives the start
    MP_PLATFORM.pread(fd, buf.data(), buf.size(), 0);
    EXPECT_EQ(std::string(buf.data(), buf.size()), "ABCD");
}

TEST_F(TestPlatformUnixSftp, preadAtEofReturnsZero)
{
    auto path = make_file("pread-eof", "hi");
    auto handle =
        std::get<std::unique_ptr<mp::NamedFd>>(MP_FILEOPS.open_fd(path.c_str(), O_RDONLY, 0));
    int fd = handle->fd;
    ASSERT_NE(fd, -1);

    std::array<char, 4> buf{};
    auto r = MP_PLATFORM.pread(fd, buf.data(), buf.size(), 100); // past EOF
    EXPECT_EQ(r, 0);
}

// ── pwrite ───────────────────────────────────────────────────────────────

TEST_F(TestPlatformUnixSftp, pwriteWritesAtOffset)
{
    auto path = make_file("pwrite-file", "Hello, World!");
    auto handle =
        std::get<std::unique_ptr<mp::NamedFd>>(MP_FILEOPS.open_fd(path.c_str(), O_RDWR, 0));
    int fd = handle->fd;
    ASSERT_NE(fd, -1);

    const std::string patch = "XXXXX";
    auto w = MP_PLATFORM.pwrite(fd, const_cast<char*>(patch.data()), patch.size(), 7);
    EXPECT_EQ(w, 5);

    // Read back full file to verify
    std::array<char, 13> result{};
    ::pread(fd, result.data(), result.size(), 0);
    EXPECT_EQ(std::string(result.data(), result.size()), "Hello, XXXXX!");
}

TEST_F(TestPlatformUnixSftp, pwriteDoesNotAdvanceFilePosition)
{
    auto path = make_file("pwrite-pos", "00000000");
    auto handle =
        std::get<std::unique_ptr<mp::NamedFd>>(MP_FILEOPS.open_fd(path.c_str(), O_RDWR, 0));
    int fd = handle->fd;
    ASSERT_NE(fd, -1);

    const std::string data = "AAAA";
    EXPECT_NE(MP_PLATFORM.pwrite(fd, const_cast<char*>(data.data()), data.size(), 4), -1);

    // Position must still be 0 — reading from 0 gives original start bytes
    std::array<char, 4> buf{};
    ::pread(fd, buf.data(), buf.size(), 0);
    EXPECT_EQ(std::string(buf.data(), buf.size()), "0000");
}

// ── ftruncate ────────────────────────────────────────────────────────────

TEST_F(TestPlatformUnixSftp, ftruncateShrinksTruncatesFile)
{
    auto path = make_file("trunc-file", "Hello, World!");
    auto handle =
        std::get<std::unique_ptr<mp::NamedFd>>(MP_FILEOPS.open_fd(path.c_str(), O_RDWR, 0));
    int fd = handle->fd;
    ASSERT_NE(fd, -1);

    EXPECT_EQ(MP_PLATFORM.ftruncate(fd, 5), 0);

    struct stat st{};
    ::fstat(fd, &st);
    EXPECT_EQ(st.st_size, 5);
}

TEST_F(TestPlatformUnixSftp, ftruncateBadFdReturnsMinus1)
{
    EXPECT_NE(MP_PLATFORM.ftruncate(-1, 0), 0);
}

// ── futimes ──────────────────────────────────────────────────────────────

TEST_F(TestPlatformUnixSftp, futimesSetsModificationTime)
{
    auto path = make_file("utime-file");
    auto handle =
        std::get<std::unique_ptr<mp::NamedFd>>(MP_FILEOPS.open_fd(path.c_str(), O_RDONLY, 0));
    int fd = handle->fd;
    ASSERT_NE(fd, -1);

    constexpr int set_timestamp = 1'000'000;
    EXPECT_EQ(MP_PLATFORM.futimes(fd, set_timestamp, set_timestamp), 0);

    struct stat st{};
    ::fstat(fd, &st);
    EXPECT_EQ(static_cast<int>(st.st_mtime), set_timestamp);
}

// ── fchown ───────────────────────────────────────────────────────────────

TEST_F(TestPlatformUnixSftp, fchownWithCurrentOwnerSucceeds)
{
    // Calling fchown with the process's own uid/gid must succeed for a
    // file we own (non-root cannot chown to other users, but self-chown is a no-op)
    auto path = make_file("fchown-file");
    auto handle =
        std::get<std::unique_ptr<mp::NamedFd>>(MP_FILEOPS.open_fd(path.c_str(), O_RDONLY, 0));
    int fd = handle->fd;
    ASSERT_NE(fd, -1);

    EXPECT_EQ(MP_PLATFORM.fchown(fd, ::getuid(), ::getgid()), 0);
}

// ── set_permissions_sftp ─────────────────────────────────────────────────

TEST_F(TestPlatformUnixSftp, setPermissionsSftpSetsPermissions)
{
    namespace fs = std::filesystem;
    auto path = make_file("perms-file");

    const auto perms = fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read;
    EXPECT_TRUE(MP_PLATFORM.set_permissions_sftp(path, perms));

    auto actual = fs::status(path).permissions();
    // Mask to the three groups — ignore sticky/setuid bits
    EXPECT_EQ(actual & fs::perms::mask, perms);
}

TEST_F(TestPlatformUnixSftp, setPermissionsSftpOnNonExistentPathReturnsFalse)
{
    namespace fs = std::filesystem;
    EXPECT_FALSE(MP_PLATFORM.set_permissions_sftp("/does/not/exist", fs::perms::owner_read));
}
