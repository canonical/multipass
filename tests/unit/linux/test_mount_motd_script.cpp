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

#include <QProcess>

#include <string>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
constexpr auto header = "\nHost directories mounted by Multipass:\n\n";

struct MountMotdScript : public Test
{
    void plant(const QString& name, const std::string& source)
    {
        mpt::make_file_with_content(records_dir.filePath(name), source);
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
    plant("%2Fhome%2Fubuntu%2Fsrc", "/home/alice/src\n");
    plant("%2Fhome%2Fubuntu%2Fdata", "/home/alice/data\n");
    plant("%2Fmnt", "/srv\n");

    EXPECT_EQ(run(),
              std::string{header} + " * /home/alice/data (on /home/ubuntu/data)\n"
                                    " * /home/alice/src (on /home/ubuntu/src)\n"
                                    " * /srv (on /mnt)\n");
}

TEST_F(MountMotdScript, decodesTargets)
{
    plant("%2Fhome%2Fubuntu%2Fmy data", "/a\n");
    plant("%2Fz%25%2525", "/b\n");

    EXPECT_EQ(run(),
              std::string{header} + " * /a (on /home/ubuntu/my data)\n"
                                    " * /b (on /z%%25)\n");
}

TEST_F(MountMotdScript, printsSourcesVerbatim)
{
    plant("%2Fdata", "C:\\Users\\alice\\my data\n");

    EXPECT_EQ(run(), std::string{header} + " * C:\\Users\\alice\\my data (on /data)\n");
}

TEST_F(MountMotdScript, acceptsRecordsWithoutTrailingNewline)
{
    plant("%2Fdata", "/src");

    EXPECT_EQ(run(), std::string{header} + " * /src (on /data)\n");
}
} // namespace
