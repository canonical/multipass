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

#include "mock_applevz_utils.h"
#include "mock_applevz_wrapper.h"
#include "tests/unit/common.h"
#include "tests/unit/mock_logger.h"
#include "tests/unit/mock_process_factory.h"
#include "tests/unit/temp_file.h"

#include <applevz/applevz_utils.h>
#include <multipass/utils.h>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
struct AppleVZUtils_UnitTests : public testing::Test
{
    AppleVZUtils_UnitTests()
    {
        ON_CALL(mock_applevz_utils, convert_to_supported_format(_, _))
            .WillByDefault([this](const std::filesystem::path& path, bool destructive) {
                return mock_applevz_utils.AppleVZUtils::convert_to_supported_format(path,
                                                                                    destructive);
            });
    }

    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();

    mpt::MockAppleVZWrapper::GuardedMock mock_applevz_injection{
        mpt::MockAppleVZWrapper::inject<NiceMock>()};
    mpt::MockAppleVZWrapper& mock_applevz = *mock_applevz_injection.first;

    mpt::MockAppleVZUtils::GuardedMock mock_applevz_utils_injection{
        mpt::MockAppleVZUtils::inject<NiceMock>()};
    mpt::MockAppleVZUtils& mock_applevz_utils = *mock_applevz_utils_injection.first;

    std::unique_ptr<mpt::MockProcessFactory::Scope> process_factory_scope =
        mpt::MockProcessFactory::Inject();

    mpt::TempFile test_image;
};

TEST_F(AppleVZUtils_UnitTests, convertUsesRawFormatOnPreMacOS26)
{
    EXPECT_CALL(mock_applevz_utils, macos_at_least(26, 0, _)).WillOnce(Return(false));

    process_factory_scope->register_callback([](mpt::MockProcess* process) {
        if (process->program() == "qemu-img")
        {
            const auto args = process->arguments();
            if (args.contains("info"))
                EXPECT_CALL(*process, read_all_standard_output)
                    .WillOnce(Return(QByteArray(R"({"format": "qcow2"})")));
            else if (args.contains("convert"))
                EXPECT_EQ(args.at(3), "raw");
        }
    });

    auto result = MP_APPLEVZ_UTILS.convert_to_supported_format(test_image.path(), true);

    EXPECT_EQ(result.extension(), ".raw");
    EXPECT_NE(result, test_image.path());
}

TEST_F(AppleVZUtils_UnitTests, convertIsNoOpWhenAlreadyRaw)
{
    EXPECT_CALL(mock_applevz_utils, macos_at_least(26, 0, _)).WillOnce(Return(false));

    process_factory_scope->register_callback([](mpt::MockProcess* process) {
        if (process->program() == "qemu-img")
        {
            const auto args = process->arguments();
            if (args.contains("info"))
                EXPECT_CALL(*process, read_all_standard_output)
                    .WillOnce(Return(QByteArray(R"({"format": "raw"})")));
        }
    });

    auto result = MP_APPLEVZ_UTILS.convert_to_supported_format(test_image.path(), true);

    EXPECT_EQ(result, test_image.path());
}

TEST_F(AppleVZUtils_UnitTests, asifImagesNotConvertedOnMacOS26)
{
    EXPECT_CALL(mock_applevz_utils, macos_at_least(26, 0, _)).WillOnce(Return(true));

    MP_UTILS.make_file_with_content(test_image.path(), "shdw", true);

    bool conversion_attempted = false;
    process_factory_scope->register_callback(
        [&conversion_attempted](mpt::MockProcess* process) { conversion_attempted = true; });

    auto result = MP_APPLEVZ_UTILS.convert_to_supported_format(test_image.path(), true);

    EXPECT_EQ(result, test_image.path());
    EXPECT_FALSE(conversion_attempted);
}

TEST_F(AppleVZUtils_UnitTests, nonAsifBytesTriggerConversionOnMacOS26)
{
    EXPECT_CALL(mock_applevz_utils, macos_at_least(26, 0, _)).WillOnce(Return(true));

    MP_UTILS.make_file_with_content(test_image.path(), std::string(4, '\xFF'), true);

    bool asif_created = false;
    process_factory_scope->register_callback([&asif_created](mpt::MockProcess* process) {
        if (process->program() == "qemu-img")
        {
            const auto args = process->arguments();
            if (args.contains("info"))
                EXPECT_CALL(*process, read_all_standard_output)
                    .WillOnce(Return(QByteArray(R"({"format": "raw"})")));
        }
        else if (process->program() == "diskutil")
        {
            const auto args = process->arguments();
            if (args.contains("image") && args.contains("create"))
                asif_created = true;
        }
    });

    const auto result = MP_APPLEVZ_UTILS.convert_to_supported_format(test_image.path(), true);

    EXPECT_TRUE(asif_created);
    EXPECT_EQ(result.extension(), ".asif");
}

TEST_F(AppleVZUtils_UnitTests, conversionKeepsRawFileWhenNonDestructive)
{
    EXPECT_CALL(mock_applevz_utils, macos_at_least(26, 0, _)).WillOnce(Return(true));

    MP_UTILS.make_file_with_content(test_image.path(), std::string(4, '\xFF'), true);

    process_factory_scope->register_callback([](mpt::MockProcess* process) {
        if (process->program() == "qemu-img")
        {
            const auto args = process->arguments();
            if (args.contains("info"))
                EXPECT_CALL(*process, read_all_standard_output)
                    .WillOnce(Return(QByteArray(R"({"format": "raw"})")));
        }
    });

    const auto result = MP_APPLEVZ_UTILS.convert_to_supported_format(test_image.path(), false);

    EXPECT_EQ(result.extension(), ".asif");
    EXPECT_TRUE(std::filesystem::exists(test_image.path()));
}

TEST_F(AppleVZUtils_UnitTests, conversionRemovesAsifOnFailure)
{
    EXPECT_CALL(mock_applevz_utils, macos_at_least(26, 0, _)).WillOnce(Return(true));

    MP_UTILS.make_file_with_content(test_image.path(), std::string(4, '\xFF'), true);

    QString asif_path;
    process_factory_scope->register_callback([&asif_path](mpt::MockProcess* process) {
        if (process->program() == "qemu-img")
        {
            const auto args = process->arguments();
            if (args.contains("info"))
            {
                EXPECT_CALL(*process, read_all_standard_output)
                    .WillOnce(Return(QByteArray(R"({"format": "raw"})")));
            }
        }
        else if (process->program() == "diskutil")
        {
            const auto args = process->arguments();
            if (args.contains("image") && args.contains("create"))
            {
                asif_path = args.at(4);
                MP_UTILS.make_file_with_content(asif_path.toStdString(), "placeholder", true);
                EXPECT_CALL(*process, execute).WillOnce(Return(mp::ProcessState{1, std::nullopt}));
            }
        }
    });

    EXPECT_THROW(MP_APPLEVZ_UTILS.convert_to_supported_format(test_image.path(), true),
                 std::runtime_error);
    EXPECT_FALSE(std::filesystem::exists(asif_path.toStdString()));
}

TEST_F(AppleVZUtils_UnitTests, conversionKeepsRawOnNonDestructiveFailure)
{
    EXPECT_CALL(mock_applevz_utils, macos_at_least(26, 0, _)).WillOnce(Return(true));

    MP_UTILS.make_file_with_content(test_image.path(), std::string(4, '\xFF'), true);

    QString asif_path;
    process_factory_scope->register_callback([&asif_path](mpt::MockProcess* process) {
        if (process->program() == "qemu-img")
        {
            const auto args = process->arguments();
            if (args.contains("info"))
                EXPECT_CALL(*process, read_all_standard_output)
                    .WillOnce(Return(QByteArray(R"({"format": "raw"})")));
        }
        else if (process->program() == "diskutil")
        {
            const auto args = process->arguments();
            if (args.contains("image") && args.contains("create"))
            {
                asif_path = args.at(4);
                MP_UTILS.make_file_with_content(asif_path.toStdString(), "placeholder", false);
                EXPECT_CALL(*process, execute).WillOnce(Return(mp::ProcessState{1, std::nullopt}));
            }
        }
    });

    EXPECT_THROW(MP_APPLEVZ_UTILS.convert_to_supported_format(test_image.path(), false),
                 std::runtime_error);
    EXPECT_FALSE(std::filesystem::exists(asif_path.toStdString()));
    EXPECT_TRUE(std::filesystem::exists(test_image.path()));
}

TEST_F(AppleVZUtils_UnitTests, asifImageResizesViaDiskutil)
{
    MP_UTILS.make_file_with_content(test_image.path(), "shdw", true);

    const auto target_size = mp::MemorySize::from_bytes(512LL * 1024 * 1024);
    bool diskutil_resize_called = false;

    process_factory_scope->register_callback(
        [&diskutil_resize_called, &target_size, &image = test_image](mpt::MockProcess* process) {
            if (process->program() == "diskutil")
            {
                const auto args = process->arguments();
                if (args.contains("image") && args.contains("resize"))
                {
                    diskutil_resize_called = true;
                    EXPECT_EQ(args.at(3), QString::number(target_size.in_bytes()));
                    EXPECT_EQ(args.at(4), image.name());
                }
            }
        });

    mock_applevz_utils.AppleVZUtils::resize_image(target_size, test_image.path());

    EXPECT_TRUE(diskutil_resize_called);
}

TEST_F(AppleVZUtils_UnitTests, nonAsifImageResizesToExactSize)
{
    MP_UTILS.make_file_with_content(test_image.path(), "test", true);

    constexpr auto target_size = 4096;
    mock_applevz_utils.AppleVZUtils::resize_image(mp::MemorySize::from_bytes(target_size),
                                                  test_image.path());

    EXPECT_EQ(std::filesystem::file_size(test_image.path()), target_size);
}

TEST_F(AppleVZUtils_UnitTests, resizeAsifImageThrowsOnDiskutilFailure)
{
    MP_UTILS.make_file_with_content(test_image.path(), "shdw", true);

    process_factory_scope->register_callback([](mpt::MockProcess* process) {
        if (process->program() == "diskutil")
        {
            const auto args = process->arguments();
            if (args.contains("image") && args.contains("resize"))
                EXPECT_CALL(*process, execute).WillOnce(Return(mp::ProcessState{1, std::nullopt}));
        }
    });

    const auto target_size = mp::MemorySize::from_bytes(512LL * 1024 * 1024);
    EXPECT_THROW(mock_applevz_utils.AppleVZUtils::resize_image(target_size, test_image.path()),
                 std::runtime_error);
}

TEST_F(AppleVZUtils_UnitTests, makeSparseThrowsWhenResizeFileFails)
{
    MP_UTILS.make_file_with_content(test_image.path(), "test", true);
    QFile::setPermissions(test_image.name(), QFileDevice::ReadOwner | QFileDevice::ReadUser);

    const auto target_size = mp::MemorySize::from_bytes(512LL * 1024 * 1024);
    EXPECT_THROW(mock_applevz_utils.AppleVZUtils::resize_image(target_size, test_image.path()),
                 std::runtime_error);

    QFile::setPermissions(test_image.name(),
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadUser |
                              QFileDevice::WriteUser);
}

TEST_F(AppleVZUtils_UnitTests, fileShorterThanMagicTreatedAsRaw)
{
    MP_UTILS.make_file_with_content(test_image.path(), "ab", true);

    constexpr auto target_size = 4096;
    mock_applevz_utils.AppleVZUtils::resize_image(mp::MemorySize::from_bytes(target_size),
                                                  test_image.path());

    EXPECT_EQ(std::filesystem::file_size(test_image.path()), target_size);
}

TEST_F(AppleVZUtils_UnitTests, conversionDeletesIntermediateRawOnSuccess)
{
    EXPECT_CALL(mock_applevz_utils, macos_at_least(26, 0, _)).WillOnce(Return(true));

    MP_UTILS.make_file_with_content(test_image.path(), std::string(4, '\xFF'), true);

    QString raw_path;
    process_factory_scope->register_callback([&raw_path](mpt::MockProcess* process) {
        if (process->program() == "qemu-img")
        {
            const auto args = process->arguments();
            if (args.contains("info"))
                EXPECT_CALL(*process, read_all_standard_output)
                    .WillOnce(Return(QByteArray(R"({"format": "qcow2"})")));
            else if (args.contains("convert"))
            {
                raw_path = args.at(5);
                MP_UTILS.make_file_with_content(raw_path.toStdString(),
                                                std::string(4, '\xFF'),
                                                false);
            }
        }
    });

    const auto result = MP_APPLEVZ_UTILS.convert_to_supported_format(test_image.path(), true);

    EXPECT_EQ(result.extension(), ".asif");
    EXPECT_FALSE(std::filesystem::exists(raw_path.toStdString()));
    EXPECT_TRUE(std::filesystem::exists(test_image.path()));
}

TEST_F(AppleVZUtils_UnitTests, conversionDeletesFilesOnFailure)
{
    EXPECT_CALL(mock_applevz_utils, macos_at_least(26, 0, _)).WillOnce(Return(true));

    MP_UTILS.make_file_with_content(test_image.path(), std::string(4, '\xFF'), true);

    QString raw_path;
    QString asif_path;
    process_factory_scope->register_callback([&raw_path, &asif_path](mpt::MockProcess* process) {
        if (process->program() == "qemu-img")
        {
            const auto args = process->arguments();
            if (args.contains("info"))
                EXPECT_CALL(*process, read_all_standard_output)
                    .WillOnce(Return(QByteArray(R"({"format": "qcow2"})")));
            else if (args.contains("convert"))
            {
                // args: ["convert", "-p", "-O", "raw", source, dest]
                raw_path = args.at(5);
                MP_UTILS.make_file_with_content(raw_path.toStdString(),
                                                std::string(4, '\xFF'),
                                                false);
            }
        }
        else if (process->program() == "diskutil")
        {
            const auto args = process->arguments();
            if (args.contains("image") && args.contains("create"))
            {
                asif_path = args.at(4);
                MP_UTILS.make_file_with_content(asif_path.toStdString(), "placeholder", false);
                EXPECT_CALL(*process, execute).WillOnce(Return(mp::ProcessState{1, std::nullopt}));
            }
        }
    });

    EXPECT_THROW(MP_APPLEVZ_UTILS.convert_to_supported_format(test_image.path(), true),
                 std::runtime_error);

    EXPECT_FALSE(std::filesystem::exists(raw_path.toStdString()));
    EXPECT_FALSE(std::filesystem::exists(asif_path.toStdString()));
    EXPECT_TRUE(std::filesystem::exists(test_image.path()));
}
} // namespace
