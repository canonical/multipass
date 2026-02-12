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

#include "mock_applevz_wrapper.h"
#include "tests/unit/common.h"
#include "tests/unit/mock_logger.h"
#include "tests/unit/mock_process_factory.h"
#include "tests/unit/temp_file.h"

#include <applevz/applevz_utils.h>

#include <QFile>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
struct AppleVZUtils_UnitTests : public testing::Test
{
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();

    mpt::MockAppleVZWrapper::GuardedMock mock_applevz_injection{
        mpt::MockAppleVZWrapper::inject<NiceMock>()};
    mpt::MockAppleVZWrapper& mock_applevz = *mock_applevz_injection.first;

    std::unique_ptr<mpt::MockProcessFactory::Scope> process_factory_scope =
        mpt::MockProcessFactory::Inject();

    mpt::TempFile test_image;
};

TEST_F(AppleVZUtils_UnitTests, convertUsesRawFormatOnPreMacOS26)
{
    EXPECT_CALL(mock_applevz, macos_at_least(26, 0, _)).WillOnce(Return(false));

    process_factory_scope->register_callback([](mpt::MockProcess* process) {
        if (process->program() == "qemu-img")
        {
            const auto args = process->arguments();
            if (args.size() >= 2 && args.at(0) == "info")
            {
                EXPECT_CALL(*process, execute).WillOnce(Return(mp::ProcessState{0, std::nullopt}));
                EXPECT_CALL(*process, read_all_standard_output)
                    .WillOnce(Return(QByteArray(R"({"format": "qcow2"})")));
            }
            else if (args.size() >= 5 && args.at(0) == "convert")
            {
                EXPECT_EQ(args.at(3), "raw");
                EXPECT_CALL(*process, execute).WillOnce(Return(mp::ProcessState{0, std::nullopt}));
            }
        }
    });

    auto result = MP_APPLEVZ_UTILS.convert_to_supported_format(test_image.name());

    EXPECT_TRUE(result.endsWith(".raw"));
    EXPECT_NE(result, test_image.name());
}

TEST_F(AppleVZUtils_UnitTests, convertIsNoOpWhenAlreadyRaw)
{
    EXPECT_CALL(mock_applevz, macos_at_least(26, 0, _)).WillOnce(Return(false));

    process_factory_scope->register_callback([](mpt::MockProcess* process) {
        if (process->program() == "qemu-img")
        {
            const auto args = process->arguments();
            if (args.size() >= 2 && args.at(0) == "info")
            {
                EXPECT_CALL(*process, execute).WillOnce(Return(mp::ProcessState{0, std::nullopt}));
                EXPECT_CALL(*process, read_all_standard_output)
                    .WillOnce(Return(QByteArray(R"({"format": "raw"})")));
            }
        }
    });

    auto result = MP_APPLEVZ_UTILS.convert_to_supported_format(test_image.name());

    EXPECT_EQ(result, test_image.name());
}

TEST_F(AppleVZUtils_UnitTests, asifImagesNotConvertedOnMacOS26)
{
    QFile file(test_image.name());
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("shdw", 4);
    file.close();

    EXPECT_CALL(mock_applevz, macos_at_least(26, 0, _)).WillOnce(Return(true));

    bool conversion_attempted = false;
    process_factory_scope->register_callback(
        [&conversion_attempted](mpt::MockProcess* process) { conversion_attempted = true; });

    auto result = MP_APPLEVZ_UTILS.convert_to_supported_format(test_image.name());

    EXPECT_EQ(result, test_image.name());
    EXPECT_FALSE(conversion_attempted) << "No process should be spawned for converting ASIF to ASIF images";
}
} // namespace
