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

#include "mock_aa_syscalls.h"

#include "tests/unit/common.h"
#include "tests/unit/mock_environment_helpers.h"
#include "tests/unit/mock_logger.h"
#include "tests/unit/reset_process_factory.h"
#include "tests/unit/temp_dir.h"
#include "tests/unit/test_with_mocked_bin_path.h"

#include <src/platform/backends/shared/linux/apparmored_process.h>
#include <src/platform/backends/shared/linux/process_factory.h>

#include <multipass/format.h>
#include <multipass/process/process.h>

#include <QCoreApplication>
#include <QFile>

#include <utility>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
const auto apparmor_output_file = "/tmp/multipass-apparmor-profile.txt";
const auto apparmor_profile_template = "profile %1 { stuff }";
class TestProcessSpec : public mp::ProcessSpec
{
public:
    explicit TestProcessSpec(QString identifier = {}) : profile_identifier{std::move(identifier)}
    {
    }

    QString program() const override
    {
        return "mock_process";
    }
    QStringList arguments() const override
    {
        return {"one", "two"};
    }
    QString apparmor_profile(const QString& profile_name) const override
    {
        return QString{apparmor_profile_template}.arg(profile_name);
    }
    QString identifier() const override
    {
        return profile_identifier;
    }

private:
    const QString profile_identifier;
};

} // namespace

struct ApparmoredProcessTest : public mpt::TestWithMockedBinPath
{
    using AppArmoredProcessAndPolicy =
        std::pair<std::unique_ptr<mp::AppArmoredProcess>, QByteArray>;

    ApparmoredProcessTest()
    {
        QFile::remove(apparmor_output_file);
        is_enabled.returnValue(1);
    }

    void TearDown() override
    {
        QFile::remove(apparmor_output_file);
    }

    std::unique_ptr<mp::AppArmoredProcess> create_apparmored_process(QString identifier = {})
    {
        auto process = MP_PROCFACTORY.create_process(
            std::make_unique<TestProcessSpec>(std::move(identifier)));

        auto apparmored_process = dynamic_cast<mp::AppArmoredProcess*>(process.get());
        if (!apparmored_process)
            return {};

        process.release();
        return std::unique_ptr<mp::AppArmoredProcess>(apparmored_process);
    }

    AppArmoredProcessAndPolicy setup_apparmored_process(QString identifier = {})
    {
        auto process = create_apparmored_process(identifier);
        if (!process)
            return {};

        // Check apparmor policy is received by the process correctly
        const auto expected_policy =
            QString{apparmor_profile_template}.arg(process->apparmor_profile_name()).toLatin1();
        EXPECT_EQ(process->apparmor_profile(), expected_policy);

        return {std::move(process), expected_policy};
    }

    mpt::UnsetEnvScope env{"DISABLE_APPARMOR"};
    mpt::ResetProcessFactory scope; // will otherwise pollute other tests
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
    decltype(MOCK(aa_is_enabled)) is_enabled{MOCK(aa_is_enabled)};
};

TEST_F(ApparmoredProcessTest, loadsProfileWithApparmor)
{
    auto [process, expected_policy] = setup_apparmored_process();
    ASSERT_NE(process, nullptr);

    // apparmor profile should have been installed
    QFile apparmor_input(apparmor_output_file);
    ASSERT_TRUE(apparmor_input.open(QIODevice::ReadOnly | QIODevice::Text));
    auto input = apparmor_input.readAll();

    EXPECT_TRUE(input.contains("args: -W, --abort-on-error, -r,"));
    EXPECT_TRUE(input.contains(expected_policy));
}

TEST_F(ApparmoredProcessTest, snapEnablesCacheWithExpectedArgs)
{
    mpt::TempDir cache_dir;
    const QByteArray snap_name{"multipass"};

    mpt::SetEnvScope env_scope("SNAP_COMMON", cache_dir.path().toUtf8());
    mpt::SetEnvScope env_scope2("SNAP_NAME", snap_name);

    auto [process, expected_policy] = setup_apparmored_process();
    ASSERT_NE(process, nullptr);

    // apparmor profile should have been installed
    QFile apparmor_input(apparmor_output_file);
    ASSERT_TRUE(apparmor_input.open(QIODevice::ReadOnly | QIODevice::Text));
    auto input = apparmor_input.readAll();

    EXPECT_TRUE(
        input.contains(QString("args: -WL, %1/apparmor.d/cache/multipass, --abort-on-error, -r,")
                           .arg(cache_dir.path())
                           .toUtf8()));
    EXPECT_TRUE(input.contains(expected_policy));
}

TEST_F(ApparmoredProcessTest, noOutputFileWhenNoApparmor)
{
    REPLACE(aa_is_enabled, [] { return 0; });
    const mp::ProcessFactory& process_factory{MP_PROCFACTORY};
    auto process = process_factory.create_process(std::make_unique<TestProcessSpec>());

    EXPECT_EQ(dynamic_cast<mp::AppArmoredProcess*>(process.get()), nullptr);
    EXPECT_FALSE(QFile::exists(apparmor_output_file));
}

TEST_F(ApparmoredProcessTest, unloadsProfileWithApparmorOnProcessOutOfScope)
{
    auto [process, expected_policy] = setup_apparmored_process();
    ASSERT_NE(process, nullptr);

    process.reset();

    // apparmor profile should have been removed
    QFile apparmor_input(apparmor_output_file);
    ASSERT_TRUE(apparmor_input.open(QIODevice::ReadOnly | QIODevice::Text));
    auto input = apparmor_input.readAll();

    EXPECT_TRUE(input.contains("args: -W, -R,"));
    EXPECT_TRUE(input.contains(expected_policy));
}

TEST_F(ApparmoredProcessTest, processesUseDistinctApparmorProfiles)
{
    auto [first_process, first_policy] = setup_apparmored_process();
    ASSERT_NE(first_process, nullptr);

    auto [second_process, second_policy] = setup_apparmored_process();
    ASSERT_NE(second_process, nullptr);

    EXPECT_NE(first_process->apparmor_profile_name(), second_process->apparmor_profile_name());

    first_process.reset();

    QFile apparmor_input(apparmor_output_file);
    ASSERT_TRUE(apparmor_input.open(QIODevice::ReadOnly | QIODevice::Text));
    const auto input = apparmor_input.readAll();

    EXPECT_TRUE(input.contains("args: -W, -R,"));
    EXPECT_TRUE(input.contains(first_policy));
    EXPECT_FALSE(input.contains(second_policy));
}

TEST_F(ApparmoredProcessTest, identifiedProcessesUseSequenceAndIdentifierInApparmorProfileName)
{
    auto [first_process, first_policy] = setup_apparmored_process("test-identifier");
    ASSERT_NE(first_process, nullptr);

    auto [second_process, second_policy] = setup_apparmored_process("test-identifier");
    ASSERT_NE(second_process, nullptr);

    const auto process_id = QCoreApplication::applicationPid();
    EXPECT_EQ(first_process->apparmor_profile_name(),
              QString{"multipass.%1.0.test-identifier.mock_process"}.arg(process_id));
    EXPECT_EQ(second_process->apparmor_profile_name(),
              QString{"multipass.%1.1.test-identifier.mock_process"}.arg(process_id));
    EXPECT_NE(first_policy, second_policy);
}

TEST_F(ApparmoredProcessTest, logsAllExpectedMessagesOnStart)
{
    logger_scope.mock_logger->screen_logs(mpl::Level::error);
    logger_scope.mock_logger->expect_log(mpl::Level::info, "Using AppArmor support");
    logger_scope.mock_logger->expect_log(mpl::Level::trace,
                                         "Loading AppArmor policy:\nprofile multipass.");

    const mp::ProcessFactory& process_factory{MP_PROCFACTORY};
    auto process = process_factory.create_process(std::make_unique<TestProcessSpec>());

    logger_scope.mock_logger->expect_log(mpl::Level::debug, "Applied AppArmor policy: multipass.");
    logger_scope.mock_logger->expect_log(mpl::Level::trace,
                                         "Removing AppArmor policy:\nprofile multipass.");
    logger_scope.mock_logger->expect_log(
        mpl::Level::debug,
        fmt::format("started: {} {}", process->program(), process->arguments().join(' ')));

    process->start();
    process->kill();
    process->wait_for_finished();
}
