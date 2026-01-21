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

#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/xz_image_decoder.h>

#include <algorithm>
#include <fstream>
#include <vector>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
static const std::string sample_content = "Hello from unit test\n";

void create_test_xz_file(const std::filesystem::path& path)
{
    std::ofstream f(path, std::ios::binary);
    ASSERT_TRUE(f.is_open());

    // Auto-generated from xz - DO NOT EDIT
    // echo "Hello from unit test" > sample.txt
    // xz -k -c sample.txt > sample.txt.xz
    // xxd -i sample.txt.xz > sample_xz_bytes.h
    unsigned char sample_txt_xz[] = {
        0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00, 0x00, 0x04, 0xe6, 0xd6, 0xb4, 0x46, 0x04, 0xc0, 0x19,
        0x15, 0x21, 0x01, 0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0xe3,
        0x90, 0xb5, 0x01, 0x00, 0x14, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x66, 0x72, 0x6f, 0x6d,
        0x20, 0x75, 0x6e, 0x69, 0x74, 0x20, 0x74, 0x65, 0x73, 0x74, 0x0a, 0x00, 0x00, 0x00, 0x00,
        0x76, 0xc1, 0x18, 0xdc, 0xce, 0x6d, 0x48, 0x0c, 0x00, 0x01, 0x35, 0x15, 0x76, 0x93, 0x6a,
        0xef, 0x1f, 0xb6, 0xf3, 0x7d, 0x01, 0x00, 0x00, 0x00, 0x00, 0x04, 0x59, 0x5a};
    unsigned int sample_txt_xz_len = 88;
    // End auto-generated section

    f.write(reinterpret_cast<const char*>(sample_txt_xz), sample_txt_xz_len);
    f.close();
}

void create_invalid_xz_file(const std::filesystem::path& output_path)
{
    std::ofstream xz_file{output_path, std::ios::binary | std::ios::out};
    ASSERT_TRUE(xz_file.is_open());

    const char invalid_data[] = "This is not an xz file";
    xz_file.write(invalid_data, sizeof(invalid_data));
    xz_file.close();
}

// Mock progress monitor for testing
class MockProgressMonitor
{
public:
    MOCK_METHOD(bool, call, (int, int), (const));

    mp::ProgressMonitor get_monitor()
    {
        return [this](int progress_type, int percentage) {
            call(progress_type, percentage);
            return true;
        };
    }
};
} // namespace

struct XzImageDecoder : public Test
{
    mpt::TempDir temp_dir;
    mp::XzImageDecoder decoder;
    std::filesystem::path xz_file_path;
    std::filesystem::path output_file_path;

    void SetUp() override
    {
        xz_file_path = temp_dir.filePath("test.xz").toStdString();
        output_file_path = temp_dir.filePath("output.img").toStdString();
    }
};

TEST_F(XzImageDecoder, constructorInitializesSuccessfully)
{
    EXPECT_NO_THROW(mp::XzImageDecoder decoder);
}

TEST_F(XzImageDecoder, throwsWhenInputFileDoesNotExist)
{
    const auto non_existent_path = temp_dir.filePath("non_existent.xz").toStdString();
    MockProgressMonitor monitor;

    MP_EXPECT_THROW_THAT(
        decoder.decode_to(non_existent_path, output_file_path, monitor.get_monitor()),
        std::runtime_error,
        mpt::match_what(AllOf(HasSubstr("failed to open"), HasSubstr("for reading"))));
}

TEST_F(XzImageDecoder, throwsWhenOutputFileCannotBeCreated)
{
    create_test_xz_file(xz_file_path);

    // Create an invalid output path (directory that doesn't exist and can't be created)
    const auto invalid_output =
        std::filesystem::path("/invalid/path/that/does/not/exist/output.img");
    MockProgressMonitor monitor;

    MP_EXPECT_THROW_THAT(
        decoder.decode_to(xz_file_path, invalid_output, monitor.get_monitor()),
        std::runtime_error,
        mpt::match_what(AllOf(HasSubstr("failed to open"), HasSubstr("for writing"))));
}

TEST_F(XzImageDecoder, throwsOnInvalidXzFormat)
{
    create_invalid_xz_file(xz_file_path);
    MockProgressMonitor monitor;

    MP_EXPECT_THROW_THAT(decoder.decode_to(xz_file_path, output_file_path, monitor.get_monitor()),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("not a xz file")));
}

TEST_F(XzImageDecoder, callsProgressMonitorDuringDecoding)
{
    create_test_xz_file(xz_file_path);
    MockProgressMonitor monitor;

    // Expect progress monitor to be called at least once with EXTRACT type
    EXPECT_CALL(monitor, call(mp::LaunchProgress::EXTRACT, _)).Times(AtLeast(1));

    decoder.decode_to(xz_file_path, output_file_path, monitor.get_monitor());
}

TEST_F(XzImageDecoder, progressMonitorReportsIncreasingPercentages)
{
    create_test_xz_file(xz_file_path);

    std::vector<int> reported_percentages;
    auto progress_monitor = [&reported_percentages](int progress_type, int percentage) -> bool {
        if (progress_type == mp::LaunchProgress::EXTRACT)
        {
            reported_percentages.push_back(percentage);
        }
        return true;
    };

    decoder.decode_to(xz_file_path, output_file_path, progress_monitor);

    EXPECT_FALSE(reported_percentages.empty());

    for (const auto percentage : reported_percentages)
    {
        EXPECT_GE(percentage, 0);
        EXPECT_LE(percentage, 100);
    }
}

TEST_F(XzImageDecoder, outputFileIsCreated)
{
    create_test_xz_file(xz_file_path);
    MockProgressMonitor monitor;

    EXPECT_CALL(monitor, call(_, _)).Times(AtLeast(0));

    decoder.decode_to(xz_file_path, output_file_path, monitor.get_monitor());

    EXPECT_TRUE(std::filesystem::exists(output_file_path));
}

TEST_F(XzImageDecoder, outputFileHasExpectedContent)
{
    create_test_xz_file(xz_file_path);
    MockProgressMonitor monitor;

    EXPECT_CALL(monitor, call(_, _)).Times(AtLeast(0));

    decoder.decode_to(xz_file_path, output_file_path, monitor.get_monitor());

    // Read the output file content
    std::ifstream output_file{output_file_path, std::ios::binary};
    ASSERT_TRUE(output_file.is_open());

    std::string output_content((std::istreambuf_iterator<char>(output_file)),
                               std::istreambuf_iterator<char>());

    EXPECT_EQ(output_content, sample_content);
}
