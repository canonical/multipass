/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "tests/unit/common.h"

#include <src/platform/backends/hyperv/hyperv_migration_service.h>

#include <functional>
#include <map>
#include <string>
#include <vector>
namespace mhv = multipass::hyperv;
using namespace testing;

namespace
{
// Records every reporting call so tests can assert on the exact stream of user messages.
struct RecordingProgress : public mhv::MigrationProgress
{
    void phase(const std::string& instance, const std::string& message) override
    {
        phases.emplace_back(instance, message);
    }
    void skipped(const std::string& instance, const std::string& reason) override
    {
        skips.emplace_back(instance, reason);
    }
    void failed(const std::string& instance, const std::string& reason) override
    {
        failures.emplace_back(instance, reason);
    }
    void finished(const std::vector<std::string>& migrated_names) override
    {
        finished_with = migrated_names;
        finished_called = true;
    }

    std::vector<std::pair<std::string, std::string>> phases;
    std::vector<std::pair<std::string, std::string>> skips;
    std::vector<std::pair<std::string, std::string>> failures;
    std::vector<std::string> finished_with;
    bool finished_called{false};
};

// Drives per-instance behavior from a scripted map keyed by instance name. Records the
// exact order in which instances were processed so lexicographic ordering can be asserted.
struct FakeMigrator : public mhv::InstanceMigrator
{
    using Action = std::function<mhv::InstanceMigrationResult(mhv::MigrationProgress&)>;

    std::vector<std::string> source_names() override
    {
        return names;
    }

    mhv::InstanceMigrationResult migrate(const std::string& name,
                                         mhv::MigrationProgress& progress) override
    {
        processed.push_back(name);
        return actions.at(name)(progress);
    }

    std::vector<std::string> names;
    std::map<std::string, Action> actions;
    std::vector<std::string> processed;
};

mhv::InstanceMigrationResult migrated_action(mhv::MigrationProgress&)
{
    return std::nullopt;
}

mhv::MigrationCancellation never_cancel()
{
    return [] { return false; };
}
} // namespace

TEST(HyperVBulkMigration, processesNamesLexicographically)
{
    FakeMigrator migrator;
    migrator.names = {"zeta", "alpha", "mike", "bravo"};
    for (const auto& name : migrator.names)
        migrator.actions[name] = migrated_action;

    RecordingProgress progress;
    const auto cancel = never_cancel();
    const auto result = mhv::run_bulk_migration(migrator, progress, cancel);

    EXPECT_EQ(migrator.processed, (std::vector<std::string>{"alpha", "bravo", "mike", "zeta"}));
    EXPECT_TRUE(result.success);
    EXPECT_EQ(progress.finished_with, (std::vector<std::string>{"alpha", "bravo", "mike", "zeta"}));
}

TEST(HyperVBulkMigration, skipsAloneAreSuccess)
{
    FakeMigrator migrator;
    migrator.names = {"a", "b"};
    migrator.actions["a"] = [](mhv::MigrationProgress&) {
        return mhv::InstanceMigrationResult{"running"};
    };
    migrator.actions["b"] = [](mhv::MigrationProgress&) {
        return mhv::InstanceMigrationResult{"deleted"};
    };

    RecordingProgress progress;
    const auto cancel = never_cancel();
    const auto result = mhv::run_bulk_migration(migrator, progress, cancel);

    EXPECT_TRUE(result.success);
    ASSERT_EQ(progress.skips.size(), 2u);
    EXPECT_EQ(progress.skips[0].second, "running");
    EXPECT_EQ(progress.skips[1].second, "deleted");
    EXPECT_TRUE(progress.finished_called);
    EXPECT_TRUE(progress.finished_with.empty());
}

TEST(HyperVBulkMigration, recoverableFailureIsNonzeroButProcessingContinues)
{
    FakeMigrator migrator;
    migrator.names = {"a", "b", "c"};
    migrator.actions["a"] = migrated_action;
    migrator.actions["b"] = [](mhv::MigrationProgress&) -> mhv::InstanceMigrationResult {
        throw mhv::InstanceMigrationError{"copy failed"};
    };
    migrator.actions["c"] = migrated_action;

    RecordingProgress progress;
    const auto cancel = never_cancel();
    const auto result = mhv::run_bulk_migration(migrator, progress, cancel);

    // b failed, but a and c were still processed and committed.
    EXPECT_EQ(migrator.processed, (std::vector<std::string>{"a", "b", "c"}));
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.aborted);
    ASSERT_EQ(progress.failures.size(), 1u);
    EXPECT_EQ(progress.failures[0].first, "b");
    EXPECT_EQ(progress.finished_with, (std::vector<std::string>{"a", "c"}));
}

TEST(HyperVBulkMigration, unsafeTargetStoreFailureAbortsRemaining)
{
    FakeMigrator migrator;
    migrator.names = {"a", "b", "c"};
    migrator.actions["a"] = migrated_action;
    migrator.actions["b"] = [](mhv::MigrationProgress&) -> mhv::InstanceMigrationResult {
        throw mhv::MigrationAbortError{"target store corrupted"};
    };
    migrator.actions["c"] = migrated_action;

    RecordingProgress progress;
    const auto cancel = never_cancel();
    const auto result = mhv::run_bulk_migration(migrator, progress, cancel);

    // c must never be processed after an abort.
    EXPECT_EQ(migrator.processed, (std::vector<std::string>{"a", "b"}));
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.aborted);
    ASSERT_EQ(progress.failures.size(), 1u);
    EXPECT_EQ(progress.failures.front().first, "b");
    // Earlier commit is still reported.
    EXPECT_EQ(progress.finished_with, (std::vector<std::string>{"a"}));
}

TEST(HyperVBulkMigration, cancellationStopsAtInstanceBoundaryRetainingEarlierCommits)
{
    FakeMigrator migrator;
    migrator.names = {"a", "b", "c"};
    for (const auto& name : migrator.names)
        migrator.actions[name] = migrated_action;

    // Cancel only takes effect once "a" has been committed.
    mhv::MigrationCancellation cancel = [&migrator] { return !migrator.processed.empty(); };

    RecordingProgress progress;
    const auto result = mhv::run_bulk_migration(migrator, progress, cancel);

    EXPECT_EQ(migrator.processed, (std::vector<std::string>{"a"}));
    EXPECT_TRUE(result.cancelled);
    EXPECT_TRUE(result.success); // no failures occurred
    EXPECT_EQ(progress.finished_with, (std::vector<std::string>{"a"}));
}

TEST(HyperVBulkMigration, cancellationBeforeAnyInstanceMigratesNothing)
{
    FakeMigrator migrator;
    migrator.names = {"a", "b"};
    for (const auto& name : migrator.names)
        migrator.actions[name] = migrated_action;

    mhv::MigrationCancellation cancel = [] { return true; };

    RecordingProgress progress;
    const auto result = mhv::run_bulk_migration(migrator, progress, cancel);

    EXPECT_TRUE(migrator.processed.empty());
    EXPECT_TRUE(result.cancelled);
    EXPECT_TRUE(progress.finished_called);
    EXPECT_TRUE(progress.finished_with.empty());
}

// -------------------------------------------------------------------------------------------------
// Extra-interface network translation
// -------------------------------------------------------------------------------------------------

namespace
{
multipass::NetworkInterfaceInfo make_switch(std::string id, std::vector<std::string> links)
{
    return {.id = std::move(id),
            .type = "switch",
            .description = "vSwitch",
            .links = std::move(links)};
}
} // namespace

TEST(HyperVNetworkTranslation, resolvesSwitchToItsSinglePhysicalAdapterPreservingOrderAndMac)
{
    const std::vector<multipass::NetworkInterface> source{
        {.id = "ExtSwitch (nic-b)", .mac_address = "52:54:00:00:00:0b", .auto_mode = true},
        {.id = "ExtSwitch (nic-a)", .mac_address = "52:54:00:00:00:0a", .auto_mode = false}};

    const std::vector<multipass::NetworkInterfaceInfo> networks{
        make_switch("ExtSwitch (nic-a)", {"Ethernet 1"}),
        make_switch("ExtSwitch (nic-b)", {"Ethernet 2"}),
        {.id = "Ethernet 1", .type = "Ethernet", .description = "adapter"}};

    const auto translated = mhv::translate_extra_interfaces(source, networks);

    ASSERT_EQ(translated.size(), 2u);
    // Order preserved (source order, not networks order).
    EXPECT_EQ(translated[0],
              (multipass::NetworkInterface{.id = "Ethernet 2",
                                           .mac_address = "52:54:00:00:00:0b",
                                           .auto_mode = true}));
    EXPECT_EQ(translated[1],
              (multipass::NetworkInterface{.id = "Ethernet 1",
                                           .mac_address = "52:54:00:00:00:0a",
                                           .auto_mode = false}));
}

TEST(HyperVNetworkTranslation, emptySourceYieldsEmptyResult)
{
    EXPECT_TRUE(mhv::translate_extra_interfaces({}, {make_switch("s", {"eth0"})}).empty());
}

TEST(HyperVNetworkTranslation, failsWhenSwitchIsUnknown)
{
    const std::vector<multipass::NetworkInterface> source{
        {.id = "missing", .mac_address = "52:54:00:00:00:01", .auto_mode = true}};

    EXPECT_THROW((void)mhv::translate_extra_interfaces(source, {make_switch("other", {"eth0"})}),
                 mhv::InstanceMigrationError);
}

TEST(HyperVNetworkTranslation, failsWhenSwitchBridgesNoPhysicalAdapter)
{
    const std::vector<multipass::NetworkInterface> source{
        {.id = "internal", .mac_address = "52:54:00:00:00:01", .auto_mode = true}};

    EXPECT_THROW((void)mhv::translate_extra_interfaces(source, {make_switch("internal", {})}),
                 mhv::InstanceMigrationError);
}

TEST(HyperVNetworkTranslation, failsWhenSwitchBridgesMultiplePhysicalAdapters)
{
    const std::vector<multipass::NetworkInterface> source{
        {.id = "team", .mac_address = "52:54:00:00:00:01", .auto_mode = true}};

    EXPECT_THROW(
        (void)mhv::translate_extra_interfaces(source,
                                              {make_switch("team", {"Ethernet 1", "Ethernet 2"})}),
        mhv::InstanceMigrationError);
}
