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

#include <tests/unit/common.h>
#include <tests/unit/file_operations.h>
#include <tests/unit/temp_dir.h>

#include <src/daemon/mount_motd_cloud_init.h>

#include <multipass/format.h>

#include <QProcess>

#include <optional>
#include <ranges>
#include <string>
#include <vector>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
const auto replacement = std::string{"\uFFFD"}; // replacement character: �

struct Record
{
    QString encoded_target; // the record's file name
    std::string target;
    std::string source;
    std::string terminator = "\n";
    std::optional<std::string> shown_source = std::nullopt; // when not shown verbatim
};

struct Scenario
{
    std::string name;
    std::vector<Record> records; // in the expected output order
};

struct MountMotdScript : public Test
{
    void plant(const Record& record)
    {
        mpt::make_file_with_content(records_dir.filePath(record.encoded_target),
                                    record.source + record.terminator);
    }

    // The output expected for the given records, in order
    static std::string listing(const std::vector<Record>& records)
    {
        if (records.empty())
            return {};

        auto ret = std::string{"\nFolders mounted from your host by Multipass:\n\n"};
        for (const auto& record : records)
            ret += fmt::format(" * {} <- {}\n",
                               record.target,
                               record.shown_source.value_or(record.source));

        return ret;
    }

    // Run the script against the given records directory, returning its output
    std::string run(const QString& dir)
    {
        auto script = QString::fromStdString(
            mp::mount_motd_cloud_init()["content"].as<std::string>());
        script.replace("/run/multipass/mounts", dir);

        QProcess bash;
        bash.start("bash", {"-c", script});
        EXPECT_TRUE(bash.waitForFinished());
        EXPECT_EQ(bash.exitStatus(), QProcess::NormalExit);
        EXPECT_EQ(bash.exitCode(), 0);
        EXPECT_THAT(bash.readAllStandardError().toStdString(), IsEmpty());

        return bash.readAllStandardOutput().toStdString();
    }

    std::string run()
    {
        return run(records_dir.path());
    }

    mpt::TempDir records_dir;
};

struct MountMotdScriptListing : public MountMotdScript, public WithParamInterface<Scenario>
{
};

TEST_F(MountMotdScript, printsNothingWithoutRecordsDir)
{
    EXPECT_THAT(run(records_dir.filePath("missing")), IsEmpty());
}

TEST_P(MountMotdScriptListing, listsRecordedMounts)
{
    const auto& records = GetParam().records;
    for (const auto& record : records | std::views::reverse) // so that order is up to the script
        plant(record);

    EXPECT_EQ(run(), listing(records));
}

const auto no_records = Scenario{.name = "noRecords", .records = {}};

const auto sorted_by_target = Scenario{
    .name = "sortedByTarget",
    .records = {Record{.encoded_target = "%2Fhome%2Fubuntu%2Fdata",
                       .target = "/home/ubuntu/data",
                       .source = "/home/alice/data"},
                Record{.encoded_target = "%2Fhome%2Fubuntu%2Fsrc",
                       .target = "/home/ubuntu/src",
                       .source = "/home/alice/src"},
                Record{.encoded_target = "%2Fmnt", .target = "/mnt", .source = "/srv"}}};

const auto decoded_targets = Scenario{
    .name = "decodedTargets",
    .records = {Record{.encoded_target = "%2Fhome%2Fubuntu%2Fmy data",
                       .target = "/home/ubuntu/my data",
                       .source = "/a"},
                Record{.encoded_target = "%2Fj%25%2525",
                       .target = "/j%%25", // literal %25
                       .source = "/b"},
                Record{.encoded_target = "%2Fl%252Fb",
                       .target = "/l%2Fb", // literal %2F
                       .source = "/c"}}};

const auto verbatim_sources = Scenario{
    .name = "verbatimSources",
    .records = {Record{.encoded_target = "%2Fdata",
                       .target = "/data",
                       .source = R"(  C:\Users\alice\my data  )"}}};

const auto no_trailing_newline = Scenario{.name = "noTrailingNewline",
                                          .records = {Record{.encoded_target = "%2Fdata",
                                                             .target = "/data",
                                                             .source = "/src",
                                                             .terminator = ""}}};

const auto neutralized_control_characters = Scenario{
    .name = "neutralizedControlCharacters",
    .records = {Record{.encoded_target = "%2Fx\x1b[2Jy\nz\tw",
                       .target = fmt::format("/x{0}[2Jy{0}z{0}w", replacement),
                       .source = "/a\x1b]0;title\ab",
                       .shown_source = fmt::format("/a{0}]0;title{0}b", replacement)}}};

INSTANTIATE_TEST_SUITE_P(MountMotdScript,
                         MountMotdScriptListing,
                         Values(no_records,
                                sorted_by_target,
                                decoded_targets,
                                verbatim_sources,
                                no_trailing_newline,
                                neutralized_control_characters),
                         [](const TestParamInfo<Scenario>& info) { return info.param.name; });
} // namespace
