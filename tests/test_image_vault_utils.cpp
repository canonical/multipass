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
#include "mock_file_ops.h"
#include "mock_image_decoder.h"
#include "mock_image_host.h"
#include "mock_image_vault_utils.h"

#include <QBuffer>
#include <multipass/progress_monitor.h>
#include <multipass/vm_image_vault_utils.h>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{

struct TestImageVaultUtils : public ::testing::Test
{
    const mpt::MockFileOps::GuardedMock mock_file_ops_guard{mpt::MockFileOps::inject<NiceMock>()};
    mpt::MockFileOps& mock_file_ops{*mock_file_ops_guard.first};

    const QDir test_dir{"secrets/secret_filled_folder"};
    const QString test_path{"not_secrets/a_secret.txt"};
    const mp::fs::path fs_test_path{test_path.toStdU16String()};
    const QFileInfo test_info{test_path};

    const QString test_output{"secrets/secret_filled_folder/a_secret.txt"};
    const mp::fs::path fs_test_output{test_output.toStdU16String()};
};

TEST_F(TestImageVaultUtils, copyToDirHandlesEmptyFile)
{
    EXPECT_EQ(MP_IMAGE_VAULT_UTILS.copy_to_dir("", test_dir), "");
}

TEST_F(TestImageVaultUtils, copyToDirThrowsOnNonexistantFile)
{
    EXPECT_CALL(mock_file_ops, exists(test_info)).WillOnce(Return(false));

    MP_EXPECT_THROW_THAT(
        MP_IMAGE_VAULT_UTILS.copy_to_dir(test_path, test_dir),
        std::runtime_error,
        mpt::match_what(AllOf(HasSubstr(test_path.toStdString()), HasSubstr("not found"))));
}

TEST_F(TestImageVaultUtils, copyToDirThrowsOnFailToCopy)
{
    EXPECT_CALL(mock_file_ops, exists(test_info)).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, copy(test_path, test_output)).WillOnce(Return(false));

    MP_EXPECT_THROW_THAT(MP_IMAGE_VAULT_UTILS.copy_to_dir(test_path, test_dir),
                         std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr(test_path.toStdString()),
                                               HasSubstr("Failed to copy"),
                                               HasSubstr(test_output.toStdString()))));
}

TEST_F(TestImageVaultUtils, copyToDirCopysToDir)
{
    EXPECT_CALL(mock_file_ops, exists(test_info)).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, copy(test_path, test_output)).WillOnce(Return(true));

    auto result = MP_IMAGE_VAULT_UTILS.copy_to_dir(test_path, test_dir);
    EXPECT_EQ(result, test_output);
}

TEST_F(TestImageVaultUtils, computeHashThrowsWhenCantRead)
{
    QBuffer buffer{}; // note: buffer is not opened
    MP_EXPECT_THROW_THAT(std::ignore = MP_IMAGE_VAULT_UTILS.compute_hash(buffer),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("Failed to read")));
}

TEST_F(TestImageVaultUtils, computeHashComputesSha256)
{
    QByteArray data = ":)";
    QBuffer buffer{&data};

    ASSERT_TRUE(buffer.open(QIODevice::ReadOnly));

    auto hash = MP_IMAGE_VAULT_UTILS.compute_hash(buffer);
    EXPECT_EQ(hash, "54d626e08c1c802b305dad30b7e54a82f102390cc92c7d4db112048935236e9c");
}

TEST_F(TestImageVaultUtils, computeFileHashThrowsWhenCantOpen)
{
    EXPECT_CALL(mock_file_ops,
                open(mpt::FileNameMatches(Eq(test_path)),
                     Truly([](const auto& mode) { return (mode & QFile::ReadOnly) > 0; })))
        .WillOnce(Return(false));

    MP_EXPECT_THROW_THAT(
        std::ignore = MP_IMAGE_VAULT_UTILS.compute_file_hash(test_path),
        std::runtime_error,
        mpt::match_what(AllOf(HasSubstr(test_path.toStdString()), HasSubstr("Failed to open"))));
}

TEST_F(TestImageVaultUtils, verifyFileHashThrowsOnBadHash)
{
    auto [mock_utils, _] = mpt::MockImageVaultUtils::inject<StrictMock>();
    EXPECT_CALL(*mock_utils, compute_file_hash(test_path, QCryptographicHash::Sha256))
        .WillOnce(Return(":("));

    MP_EXPECT_THROW_THAT(mock_utils->ImageVaultUtils::verify_file_hash(test_path, ":)"),
                         std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr(test_path.toStdString()),
                                               HasSubstr(":)"),
                                               HasSubstr("does not match"))));
}

TEST_F(TestImageVaultUtils, verifyFileHashDoesntThrowOnGoodHash)
{
    auto [mock_utils, _] = mpt::MockImageVaultUtils::inject<StrictMock>();
    EXPECT_CALL(*mock_utils, compute_file_hash(test_path, QCryptographicHash::Sha256))
        .WillOnce(Return(":)"));

    EXPECT_NO_THROW(mock_utils->ImageVaultUtils::verify_file_hash(test_path, ":)"));
}

TEST_F(TestImageVaultUtils, verifyFileHashParsesAlgo)
{
    auto [mock_utils, _] = mpt::MockImageVaultUtils::inject<StrictMock>();
    EXPECT_CALL(*mock_utils, compute_file_hash(test_path, QCryptographicHash::Sha512))
        .WillOnce(Return("1234567890abcdef"));

    EXPECT_NO_THROW(
        mock_utils->ImageVaultUtils::verify_file_hash(test_path, "sha512:1234567890abcdef"));
}

TEST_F(TestImageVaultUtils, extractFileWillDeleteFile)
{
    auto decoder = [](const QString&, const QString&) {};

    EXPECT_CALL(mock_file_ops, remove(Property(&QFile::fileName, test_path)));

    MP_IMAGE_VAULT_UTILS.extract_file(test_path, decoder, true);
}

TEST_F(TestImageVaultUtils, extractFileWontDeleteFile)
{
    EXPECT_CALL(mock_file_ops, remove_extension(fs_test_path)).WillOnce(Return(fs_test_output));

    int calls = 0;
    auto decoder = [&](const QString& path, const QString& target) {
        EXPECT_EQ(test_path, path);
        EXPECT_EQ(test_output, target);
        ++calls;
    };

    EXPECT_CALL(mock_file_ops, remove(_)).Times(0);

    MP_IMAGE_VAULT_UTILS.extract_file(test_path, decoder, false);
    EXPECT_EQ(calls, 1);
}

TEST_F(TestImageVaultUtils, extractFileExtractsFile)
{
    EXPECT_CALL(mock_file_ops, remove_extension(fs_test_path)).WillOnce(Return(fs_test_output));

    int calls = 0;
    auto decoder = [&](const QString& path, const QString& target) {
        EXPECT_EQ(test_path, path);
        EXPECT_EQ(test_output, target);
        ++calls;
    };

    auto res = MP_IMAGE_VAULT_UTILS.extract_file(test_path, decoder, false);
    EXPECT_EQ(res, test_output);
    EXPECT_EQ(calls, 1);
}

TEST_F(TestImageVaultUtils, extractFileWithDecoderBindsMonitor)
{
    EXPECT_CALL(mock_file_ops, remove_extension(fs_test_path)).WillOnce(Return(fs_test_output));

    int type = 1337;
    int progress = 42;
    int calls = 0;
    auto monitor = [&calls, &type, &progress](int in_type, int in_progress) {
        EXPECT_EQ(in_type, type);
        EXPECT_EQ(in_progress, progress);
        ++calls;
        return true;
    };

    mpt::MockImageDecoder decoder{};
    EXPECT_CALL(decoder, decode_to(fs_test_path, fs_test_output, Truly([&](const auto& m) {
                                       return m(type, progress);
                                   })));

    MP_IMAGE_VAULT_UTILS.extract_file(test_path, monitor, false, decoder);

    EXPECT_EQ(calls, 1);
}

TEST_F(TestImageVaultUtils, emptyHostsProducesEmptyMap)
{
    auto map = MP_IMAGE_VAULT_UTILS.configure_image_host_map({});
    EXPECT_TRUE(map.empty());
}

TEST_F(TestImageVaultUtils, configureImageHostMapMapsHosts)
{
    mpt::MockImageHost mock1{};
    std::vector<std::string> hosts1{"this", "is", "a", "remotes"};
    EXPECT_CALL(mock1, supported_remotes()).WillOnce(Return(hosts1));

    mpt::MockImageHost mock2{};
    std::vector<std::string> hosts2{"hi"};
    EXPECT_CALL(mock2, supported_remotes()).WillOnce(Return(hosts2));

    auto map = MP_IMAGE_VAULT_UTILS.configure_image_host_map({&mock1, &mock2});

    EXPECT_EQ(map.size(), hosts1.size() + hosts2.size());

    for (const auto& host : hosts1)
    {
        if (auto it = map.find(host); it != map.end())
            EXPECT_EQ(it->second, &mock1);
        else
            ADD_FAILURE() << fmt::format("{} was not mapped", host);
    }

    for (const auto& host : hosts2)
    {
        if (auto it = map.find(host); it != map.end())
            EXPECT_EQ(it->second, &mock2);
        else
            ADD_FAILURE() << fmt::format("{} was not mapped", host);
    }
}

} // namespace
