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

#include <string>
#include <vector>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
struct Record
{
    QString name; // the percent-encoded target
    std::string target;
    std::string source;
};

struct MountMotdScript : public Test
{
    void plant(const Record& record, const std::string& terminator = "\n")
    {
        mpt::make_file_with_content(records_dir.filePath(record.name), record.source + terminator);
    }

    // The output expected for the given records, in order
    static std::string listing(const std::vector<Record>& records)
    {
        auto ret = std::string{"\nHost directories mounted by Multipass:\n\n"};
        for (const auto& record : records)
            ret += fmt::format(" * {} (on {})\n", record.source, record.target);

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

TEST_F(MountMotdScript, printsNothingWithoutRecords)
{
    EXPECT_THAT(run(), IsEmpty());
}

TEST_F(MountMotdScript, printsNothingWithoutRecordsDir)
{
    EXPECT_THAT(run(records_dir.filePath("missing")), IsEmpty());
}

TEST_F(MountMotdScript, listsMountsSortedByTarget)
{
    const Record src{"%2Fhome%2Fubuntu%2Fsrc", "/home/ubuntu/src", "/home/alice/src"};
    const Record data{"%2Fhome%2Fubuntu%2Fdata", "/home/ubuntu/data", "/home/alice/data"};
    const Record mnt{"%2Fmnt", "/mnt", "/srv"};

    for (const auto& record : {src, data, mnt})
        plant(record);

    EXPECT_EQ(run(), listing({data, src, mnt}));
}

TEST_F(MountMotdScript, decodesTargets)
{
    const Record spaced{"%2Fhome%2Fubuntu%2Fmy data", "/home/ubuntu/my data", "/a"};
    const Record percent{"%2Fz%25%2525", "/z%%25", "/b"};

    plant(spaced);
    plant(percent);

    EXPECT_EQ(run(), listing({spaced, percent}));
}

TEST_F(MountMotdScript, printsSourcesVerbatim)
{
    const Record windows{"%2Fdata", "/data", "C:\\Users\\alice\\my data"};

    plant(windows);

    EXPECT_EQ(run(), listing({windows}));
}

TEST_F(MountMotdScript, acceptsRecordsWithoutTrailingNewline)
{
    const Record record{"%2Fdata", "/data", "/src"};

    plant(record, /*terminator=*/"");

    EXPECT_EQ(run(), listing({record}));
}
} // namespace
