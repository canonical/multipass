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
#include "disabling_macros.h"
#include "file_operations.h"
#include "mock_image_host.h"
#include "mock_json_utils.h"
#include "mock_logger.h"
#include "mock_process_factory.h"
#include "path.h"
#include "stub_url_downloader.h"
#include "temp_dir.h"
#include "temp_file.h"
#include "tracking_url_downloader.h"

#include <src/daemon/default_vm_image_vault.h>

#include <multipass/exceptions/aborted_download_exception.h>
#include <multipass/exceptions/create_image_exception.h>
#include <multipass/exceptions/image_vault_exceptions.h>
#include <multipass/exceptions/unsupported_image_exception.h>
#include <multipass/format.h>
#include <multipass/query.h>
#include <multipass/url_downloader.h>
#include <multipass/utils.h>

#include <QDateTime>
#include <QThread>
#include <QUrl>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
const QDateTime default_last_modified{QDate(2019, 6, 25), QTime(13, 15, 0)};

struct BadURLDownloader : public mp::URLDownloader
{
    BadURLDownloader() : mp::URLDownloader{std::chrono::seconds(10)}
    {
    }
    void download_to(const QUrl& url, const QString& file_name, int64_t size, const int download_type,
                     const mp::ProgressMonitor&) override
    {
        mpt::make_file_with_content(file_name, "Bad hash");
    }

    QByteArray download(const QUrl& url) override
    {
        return {};
    }
};

struct HttpURLDownloader : public mp::URLDownloader
{
    HttpURLDownloader() : mp::URLDownloader{std::chrono::seconds(10)}
    {
    }
    void download_to(const QUrl& url, const QString& file_name, int64_t size, const int download_type,
                     const mp::ProgressMonitor&) override
    {
        mpt::make_file_with_content(file_name, "");
        downloaded_urls << url.toString();
        downloaded_files << file_name;
    }

    QByteArray download(const QUrl& url) override
    {
        return {};
    }

    QDateTime last_modified(const QUrl& url) override
    {
        return default_last_modified;
    }

    QStringList downloaded_files;
    QStringList downloaded_urls;
};

struct RunningURLDownloader : public mp::URLDownloader
{
    RunningURLDownloader() : mp::URLDownloader{std::chrono::seconds(10)}
    {
    }
    void download_to(const QUrl& url, const QString& file_name, int64_t size, const int download_type,
                     const mp::ProgressMonitor&) override
    {
        while (!abort_downloads)
            QThread::yieldCurrentThread();

        throw mp::AbortedDownloadException("Aborted!");
    }

    QByteArray download(const QUrl& url) override
    {
        return {};
    }
};

struct ImageVault : public testing::Test
{
    void SetUp()
    {
        hosts.push_back(&host);
    }

    QByteArray fake_img_info(const mp::MemorySize& size)
    {
        return QByteArray::fromStdString(
            fmt::format("some\nother\ninfo\nfirst\nvirtual size: {} ({} bytes)\nmore\ninfo\nafter\n",
                        size.in_gigabytes(), size.in_bytes()));
    }

    void simulate_qemuimg_info(const mpt::MockProcess* process, const mp::ProcessState& produce_result,
                               const QByteArray& produce_output = {})
    {
        ASSERT_EQ(process->program().toStdString(), "qemu-img");

        const auto args = process->arguments();
        ASSERT_EQ(args.size(), 2);
        EXPECT_EQ(args.constFirst(), "info");

        EXPECT_CALL(*process, execute).WillOnce(Return(produce_result));
        if (produce_result.completed_successfully())
            EXPECT_CALL(*process, read_all_standard_output).WillOnce(Return(produce_output));
        else if (produce_result.exit_code)
            EXPECT_CALL(*process, read_all_standard_error).WillOnce(Return(produce_output));
        else
            ON_CALL(*process, read_all_standard_error).WillByDefault(Return(produce_output));
    }

    std::unique_ptr<mp::test::MockProcessFactory::Scope>
    inject_fake_qemuimg_callback(const mp::ProcessState& qemuimg_exit_status, const QByteArray& qemuimg_output)
    {
        std::unique_ptr<mp::test::MockProcessFactory::Scope> mock_factory_scope = mpt::MockProcessFactory::Inject();

        mock_factory_scope->register_callback(
            [&](mpt::MockProcess* process) { simulate_qemuimg_info(process, qemuimg_exit_status, qemuimg_output); });

        return mock_factory_scope;
    }

    QString host_url{QUrl::fromLocalFile(mpt::test_data_path()).toString()};
    mpt::TrackingURLDownloader url_downloader;
    std::vector<mp::VMImageHost*> hosts;
    NiceMock<mpt::MockImageHost> host;
    mpt::MockJsonUtils::GuardedMock mock_json_utils_injection = mpt::MockJsonUtils::inject<NiceMock>();
    mpt::MockJsonUtils& mock_json_utils = *mock_json_utils_injection.first;
    mp::ProgressMonitor stub_monitor{[](int, int) { return true; }};
    mp::VMImageVault::PrepareAction stub_prepare{
        [](const mp::VMImage& source_image) -> mp::VMImage { return source_image; }};
    mpt::TempDir cache_dir;
    mpt::TempDir data_dir;
    mpt::TempDir save_dir;
    std::string instance_name{"valley-pied-piper"};
    QString instance_dir = save_dir.filePath(QString::fromStdString(instance_name));
    mp::Query default_query{instance_name, "xenial", false, "", mp::Query::Type::Alias};
};
} // namespace

TEST_F(ImageVault, downloads_image)
{
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto vm_image = vault.fetch_image(mp::FetchType::ImageOnly,
                                      default_query,
                                      stub_prepare,
                                      stub_monitor,
                                      false,
                                      std::nullopt,
                                      instance_dir);

    EXPECT_THAT(url_downloader.downloaded_files.size(), Eq(1));
    EXPECT_TRUE(url_downloader.downloaded_urls.contains(host.image.url()));
}

TEST_F(ImageVault, returned_image_contains_instance_name)
{
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto vm_image = vault.fetch_image(mp::FetchType::ImageOnly,
                                      default_query,
                                      stub_prepare,
                                      stub_monitor,
                                      false,
                                      std::nullopt,
                                      instance_dir);

    EXPECT_TRUE(vm_image.image_path.contains(QString::fromStdString(instance_name)));
}

TEST_F(ImageVault, imageClone)
{
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    vault.fetch_image(mp::FetchType::ImageOnly,
                      default_query,
                      stub_prepare,
                      stub_monitor,
                      false,
                      std::nullopt,
                      instance_dir);

    const std::string dest_name = instance_name + "clone";
    EXPECT_NO_THROW(vault.clone(instance_name, dest_name));
    EXPECT_TRUE(vault.has_record_for(dest_name));
}

TEST_F(ImageVault, calls_prepare)
{
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};

    bool prepare_called{false};
    auto prepare = [&prepare_called](const mp::VMImage& source_image) -> mp::VMImage {
        prepare_called = true;
        return source_image;
    };
    auto vm_image = vault.fetch_image(mp::FetchType::ImageOnly,
                                      default_query,
                                      prepare,
                                      stub_monitor,
                                      false,
                                      std::nullopt,
                                      instance_dir);

    EXPECT_TRUE(prepare_called);
}

TEST_F(ImageVault, records_instanced_images)
{
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    int prepare_called_count{0};
    auto prepare = [&prepare_called_count](const mp::VMImage& source_image) -> mp::VMImage {
        ++prepare_called_count;
        return source_image;
    };
    auto vm_image1 = vault.fetch_image(mp::FetchType::ImageOnly,
                                       default_query,
                                       prepare,
                                       stub_monitor,
                                       false,
                                       std::nullopt,
                                       instance_dir);
    auto vm_image2 = vault.fetch_image(mp::FetchType::ImageOnly,
                                       default_query,
                                       prepare,
                                       stub_monitor,
                                       false,
                                       std::nullopt,
                                       instance_dir);

    EXPECT_THAT(url_downloader.downloaded_files.size(), Eq(1));
    EXPECT_THAT(prepare_called_count, Eq(1));
    EXPECT_THAT(vm_image1.image_path, Eq(vm_image2.image_path));
    EXPECT_THAT(vm_image1.id, Eq(vm_image2.id));
}

TEST_F(ImageVault, caches_prepared_images)
{
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    int prepare_called_count{0};
    auto prepare = [&prepare_called_count](const mp::VMImage& source_image) -> mp::VMImage {
        ++prepare_called_count;
        return source_image;
    };
    auto vm_image1 = vault.fetch_image(mp::FetchType::ImageOnly,
                                       default_query,
                                       prepare,
                                       stub_monitor,
                                       false,
                                       std::nullopt,
                                       instance_dir);

    auto another_query = default_query;
    another_query.name = "valley-pied-piper-chat";
    auto vm_image2 = vault.fetch_image(mp::FetchType::ImageOnly,
                                       another_query,
                                       prepare,
                                       stub_monitor,
                                       false,
                                       std::nullopt,
                                       save_dir.filePath(QString::fromStdString(another_query.name)));

    EXPECT_THAT(url_downloader.downloaded_files.size(), Eq(1));
    EXPECT_THAT(prepare_called_count, Eq(1));

    EXPECT_THAT(vm_image1.image_path, Ne(vm_image2.image_path));
    EXPECT_THAT(vm_image1.id, Eq(vm_image2.id));
}

TEST_F(ImageVault, remembers_instance_images)
{
    int prepare_called_count{0};
    auto prepare = [&prepare_called_count](const mp::VMImage& source_image) -> mp::VMImage {
        ++prepare_called_count;
        return source_image;
    };

    EXPECT_CALL(mock_json_utils, write_json).WillRepeatedly([this](auto&&... args) {
        return mock_json_utils.JsonUtils::write_json(std::forward<decltype(args)>(args)...); // call the real thing
    });

    mp::DefaultVMImageVault first_vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto vm_image1 = first_vault.fetch_image(mp::FetchType::ImageOnly,
                                             default_query,
                                             prepare,
                                             stub_monitor,
                                             false,
                                             std::nullopt,
                                             instance_dir);

    mp::DefaultVMImageVault another_vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto vm_image2 = another_vault.fetch_image(mp::FetchType::ImageOnly,
                                               default_query,
                                               prepare,
                                               stub_monitor,
                                               false,
                                               std::nullopt,
                                               instance_dir);

    EXPECT_THAT(url_downloader.downloaded_files.size(), Eq(1));
    EXPECT_THAT(prepare_called_count, Eq(1));
    EXPECT_THAT(vm_image1.image_path, Eq(vm_image2.image_path));
}

TEST_F(ImageVault, remembers_prepared_images)
{
    int prepare_called_count{0};
    auto prepare = [&prepare_called_count](const mp::VMImage& source_image) -> mp::VMImage {
        ++prepare_called_count;
        return source_image;
    };

    EXPECT_CALL(mock_json_utils, write_json).WillRepeatedly([this](auto&&... args) {
        return mock_json_utils.JsonUtils::write_json(std::forward<decltype(args)>(args)...); // call the real thing
    });

    mp::DefaultVMImageVault first_vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto vm_image1 = first_vault.fetch_image(mp::FetchType::ImageOnly,
                                             default_query,
                                             prepare,
                                             stub_monitor,
                                             false,
                                             std::nullopt,
                                             instance_dir);

    auto another_query = default_query;
    another_query.name = "valley-pied-piper-chat";
    mp::DefaultVMImageVault another_vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto vm_image2 = another_vault.fetch_image(mp::FetchType::ImageOnly,
                                               another_query,
                                               prepare,
                                               stub_monitor,
                                               false,
                                               std::nullopt,
                                               save_dir.filePath(QString::fromStdString(another_query.name)));

    EXPECT_THAT(url_downloader.downloaded_files.size(), Eq(1));
    EXPECT_THAT(prepare_called_count, Eq(1));
    EXPECT_THAT(vm_image1.image_path, Ne(vm_image2.image_path));
    EXPECT_THAT(vm_image1.id, Eq(vm_image2.id));
}

TEST_F(ImageVault, uses_image_from_prepare)
{
    constexpr auto expected_data = "12345-pied-piper-rats";

    QDir dir{cache_dir.path()};
    auto file_name = dir.filePath("prepared-image");
    mpt::make_file_with_content(file_name, expected_data);

    auto prepare = [&file_name](const mp::VMImage& source_image) -> mp::VMImage {
        return {file_name, source_image.id, "", "", "", {}};
    };

    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto vm_image = vault.fetch_image(mp::FetchType::ImageOnly,
                                      default_query,
                                      prepare,
                                      stub_monitor,
                                      false,
                                      std::nullopt,
                                      instance_dir);

    const auto image_data = mp::utils::contents_of(vm_image.image_path);
    EXPECT_THAT(image_data, StrEq(expected_data));
    EXPECT_THAT(vm_image.id, Eq(mpt::default_id));
}

TEST_F(ImageVault, image_purged_expired)
{
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};

    QDir images_dir{MP_UTILS.make_dir(cache_dir.path(), "images")};
    auto file_name = images_dir.filePath("mock_image.img");

    auto prepare = [&file_name](const mp::VMImage& source_image) -> mp::VMImage {
        mpt::make_file_with_content(file_name);
        return {file_name, source_image.id, "", "", "", {}};
    };
    auto vm_image = vault.fetch_image(mp::FetchType::ImageOnly,
                                      default_query,
                                      prepare,
                                      stub_monitor,
                                      false,
                                      std::nullopt,
                                      instance_dir);

    EXPECT_TRUE(QFileInfo::exists(file_name));

    vault.prune_expired_images();

    EXPECT_FALSE(QFileInfo::exists(file_name));
}

TEST_F(ImageVault, image_exists_not_expired)
{
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{1}};

    QDir images_dir{MP_UTILS.make_dir(cache_dir.path(), "images")};
    auto file_name = images_dir.filePath("mock_image.img");

    auto prepare = [&file_name](const mp::VMImage& source_image) -> mp::VMImage {
        mpt::make_file_with_content(file_name);
        return {file_name, source_image.id, "", "", "", {}};
    };
    auto vm_image = vault.fetch_image(mp::FetchType::ImageOnly,
                                      default_query,
                                      prepare,
                                      stub_monitor,
                                      false,
                                      std::nullopt,
                                      instance_dir);

    EXPECT_TRUE(QFileInfo::exists(file_name));

    vault.prune_expired_images();

    EXPECT_TRUE(QFileInfo::exists(file_name));
}

TEST_F(ImageVault, invalid_image_dir_is_removed)
{
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{1}};

    QDir invalid_image_dir(MP_UTILS.make_dir(cache_dir.path(), "vault/images/invalid_image"));
    auto file_name = invalid_image_dir.filePath("mock_image.img");

    mpt::make_file_with_content(file_name);

    EXPECT_TRUE(QFileInfo::exists(file_name));

    vault.prune_expired_images();

    EXPECT_FALSE(QFileInfo::exists(file_name));
    EXPECT_FALSE(QFileInfo::exists(invalid_image_dir.absolutePath()));
}

TEST_F(ImageVault, DISABLE_ON_WINDOWS_AND_MACOS(file_based_fetch_copies_image_and_returns_expected_info))
{
    mpt::TempFile file;
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto query = default_query;

    query.release = file.url().toStdString();
    query.query_type = mp::Query::Type::LocalFile;

    auto vm_image = vault.fetch_image(mp::FetchType::ImageOnly,
                                      query,
                                      stub_prepare,
                                      stub_monitor,
                                      false,
                                      std::nullopt,
                                      instance_dir);

    EXPECT_TRUE(QFileInfo::exists(vm_image.image_path));
    EXPECT_EQ(vm_image.id, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST_F(ImageVault, invalid_custom_image_file_throws)
{
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto query = default_query;

    query.release = "file://foo";
    query.query_type = mp::Query::Type::LocalFile;

    EXPECT_THROW(vault.fetch_image(mp::FetchType::ImageOnly,
                                   query,
                                   stub_prepare,
                                   stub_monitor,
                                   false,
                                   std::nullopt,
                                   instance_dir),
                 std::runtime_error);
}

TEST_F(ImageVault, DISABLE_ON_WINDOWS_AND_MACOS(custom_image_url_downloads))
{
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto query = default_query;

    query.release = "http://www.foo.com/fake.img";
    query.query_type = mp::Query::Type::HttpDownload;

    vault.fetch_image(mp::FetchType::ImageOnly, query, stub_prepare, stub_monitor, false, std::nullopt, instance_dir);

    EXPECT_THAT(url_downloader.downloaded_files.size(), Eq(1));
    EXPECT_TRUE(url_downloader.downloaded_urls.contains(QString::fromStdString(query.release)));
}

TEST_F(ImageVault, missing_downloaded_image_throws)
{
    mpt::StubURLDownloader stub_url_downloader;
    mp::DefaultVMImageVault vault{hosts, &stub_url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    EXPECT_THROW(vault.fetch_image(mp::FetchType::ImageOnly,
                                   default_query,
                                   stub_prepare,
                                   stub_monitor,
                                   false,
                                   std::nullopt,
                                   instance_dir),
                 mp::CreateImageException);
}

TEST_F(ImageVault, hash_mismatch_throws)
{
    BadURLDownloader bad_url_downloader;
    mp::DefaultVMImageVault vault{hosts, &bad_url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    EXPECT_THROW(vault.fetch_image(mp::FetchType::ImageOnly,
                                   default_query,
                                   stub_prepare,
                                   stub_monitor,
                                   false,
                                   std::nullopt,
                                   instance_dir),
                 mp::CreateImageException);
}

TEST_F(ImageVault, invalid_remote_throws)
{
    mpt::StubURLDownloader stub_url_downloader;
    mp::DefaultVMImageVault vault{hosts, &stub_url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto query = default_query;

    query.remote_name = "foo";

    EXPECT_THROW(vault.fetch_image(mp::FetchType::ImageOnly,
                                   query,
                                   stub_prepare,
                                   stub_monitor,
                                   false,
                                   std::nullopt,
                                   instance_dir),
                 std::runtime_error);
}

TEST_F(ImageVault, DISABLE_ON_WINDOWS_AND_MACOS(invalid_image_alias_throw))
{
    mpt::StubURLDownloader stub_url_downloader;
    mp::DefaultVMImageVault vault{hosts, &stub_url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto query = default_query;

    query.release = "foo";

    EXPECT_THROW(vault.fetch_image(mp::FetchType::ImageOnly,
                                   query,
                                   stub_prepare,
                                   stub_monitor,
                                   false,
                                   std::nullopt,
                                   instance_dir),
                 mp::CreateImageException);
}

TEST_F(ImageVault, valid_remote_and_alias_returns_valid_image_info)
{
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto query = default_query;

    query.release = "default";
    query.remote_name = "release";

    mp::VMImage image;
    EXPECT_NO_THROW(image = vault.fetch_image(mp::FetchType::ImageOnly,
                                              query,
                                              stub_prepare,
                                              stub_monitor,
                                              false,
                                              std::nullopt,
                                              instance_dir));

    EXPECT_THAT(image.original_release, Eq("18.04 LTS"));
    EXPECT_THAT(image.id, Eq("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
}

TEST_F(ImageVault, DISABLE_ON_WINDOWS_AND_MACOS(http_download_returns_expected_image_info))
{
    HttpURLDownloader http_url_downloader;
    mp::DefaultVMImageVault vault{hosts, &http_url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};

    auto image_url{"http://www.foo.com/images/foo.img"};
    mp::Query query{instance_name, image_url, false, "", mp::Query::Type::HttpDownload};

    mp::VMImage image;
    EXPECT_NO_THROW(image = vault.fetch_image(mp::FetchType::ImageOnly,
                                              query,
                                              stub_prepare,
                                              stub_monitor,
                                              false,
                                              std::nullopt,
                                              instance_dir));

    // Hash is based on image url
    EXPECT_THAT(image.id, Eq("7404f51c9b4f40312fa048a0ad36e07b74b718a2d3a5a08e8cca906c69059ddf"));
    EXPECT_THAT(image.release_date, Eq(default_last_modified.toString().toStdString()));
}

TEST_F(ImageVault, image_update_creates_new_dir_and_removes_old)
{
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{1}};
    vault.fetch_image(mp::FetchType::ImageOnly,
                      default_query,
                      stub_prepare,
                      stub_monitor,
                      false,
                      std::nullopt,
                      instance_dir);

    auto original_file{url_downloader.downloaded_files[0]};
    auto original_absolute_path{QFileInfo(original_file).absolutePath()};
    EXPECT_TRUE(QFileInfo::exists(original_file));
    EXPECT_TRUE(original_absolute_path.contains(mpt::default_version));

    // Mock an update to the image and don't verify because of hash mismatch
    const QString new_date_string{"20180825"};
    host.mock_bionic_image_info.id = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b856";
    host.mock_bionic_image_info.version = new_date_string;
    host.mock_bionic_image_info.verify = false;

    vault.update_images(mp::FetchType::ImageOnly, stub_prepare, stub_monitor);

    auto updated_file{url_downloader.downloaded_files[1]};
    EXPECT_TRUE(QFileInfo::exists(updated_file));
    EXPECT_TRUE(QFileInfo(updated_file).absolutePath().contains(new_date_string));

    // Old image and directory should be removed
    EXPECT_FALSE(QFileInfo::exists(original_file));
    EXPECT_FALSE(QFileInfo::exists(original_absolute_path));
}

TEST_F(ImageVault, aborted_download_throws)
{
    RunningURLDownloader running_url_downloader;
    mp::DefaultVMImageVault vault{hosts, &running_url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};

    running_url_downloader.abort_all_downloads();

    EXPECT_THROW(vault.fetch_image(mp::FetchType::ImageOnly,
                                   default_query,
                                   stub_prepare,
                                   stub_monitor,
                                   false,
                                   std::nullopt,
                                   instance_dir),
                 mp::AbortedDownloadException);
}

TEST_F(ImageVault, minimum_image_size_returns_expected_size)
{
    const mp::MemorySize image_size{"1048576"};
    const mp::ProcessState qemuimg_exit_status{0, std::nullopt};
    const QByteArray qemuimg_output(fake_img_info(image_size));
    auto mock_factory_scope = inject_fake_qemuimg_callback(qemuimg_exit_status, qemuimg_output);

    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto vm_image = vault.fetch_image(mp::FetchType::ImageOnly,
                                      default_query,
                                      stub_prepare,
                                      stub_monitor,
                                      false,
                                      std::nullopt,
                                      instance_dir);

    const auto size = vault.minimum_image_size_for(vm_image.id);

    EXPECT_EQ(image_size, size);
}

TEST_F(ImageVault, DISABLE_ON_WINDOWS_AND_MACOS(file_based_minimum_size_returns_expected_size))
{
    const mp::MemorySize image_size{"2097152"};
    const mp::ProcessState qemuimg_exit_status{0, std::nullopt};
    const QByteArray qemuimg_output(fake_img_info(image_size));
    auto mock_factory_scope = inject_fake_qemuimg_callback(qemuimg_exit_status, qemuimg_output);

    mpt::TempFile file;
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto query = default_query;

    query.release = file.url().toStdString();
    query.query_type = mp::Query::Type::LocalFile;

    auto vm_image = vault.fetch_image(mp::FetchType::ImageOnly,
                                      query,
                                      stub_prepare,
                                      stub_monitor,
                                      false,
                                      std::nullopt,
                                      instance_dir);

    const auto size = vault.minimum_image_size_for(vm_image.id);

    EXPECT_EQ(image_size, size);
}

TEST_F(ImageVault, minimum_image_size_throws_when_not_cached)
{
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{1}};

    const std::string id{"12345"};
    MP_EXPECT_THROW_THAT(vault.minimum_image_size_for(id), std::runtime_error,
                         mpt::match_what(StrEq(fmt::format("Cannot determine minimum image size for id \'{}\'", id))));
}

TEST_F(ImageVault, minimum_image_size_throws_when_qemuimg_info_crashes)
{
    const mp::ProcessState qemuimg_exit_status{std::nullopt, mp::ProcessState::Error{QProcess::Crashed, "core dumped"}};
    const QByteArray qemuimg_output("about to crash");
    auto mock_factory_scope = inject_fake_qemuimg_callback(qemuimg_exit_status, qemuimg_output);

    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto vm_image = vault.fetch_image(mp::FetchType::ImageOnly,
                                      default_query,
                                      stub_prepare,
                                      stub_monitor,
                                      false,
                                      std::nullopt,
                                      instance_dir);

    MP_EXPECT_THROW_THAT(vault.minimum_image_size_for(vm_image.id), std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("qemu-img failed"), HasSubstr("with output"))));
}

TEST_F(ImageVault, minimum_image_size_throws_when_qemuimg_info_cannot_find_the_image)
{
    const mp::ProcessState qemuimg_exit_status{1, std::nullopt};
    const QByteArray qemuimg_output("Could not find");
    auto mock_factory_scope = inject_fake_qemuimg_callback(qemuimg_exit_status, qemuimg_output);

    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto vm_image = vault.fetch_image(mp::FetchType::ImageOnly,
                                      default_query,
                                      stub_prepare,
                                      stub_monitor,
                                      false,
                                      std::nullopt,
                                      instance_dir);

    MP_EXPECT_THROW_THAT(vault.minimum_image_size_for(vm_image.id), std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("qemu-img failed"), HasSubstr("Could not find"))));
}

TEST_F(ImageVault, minimum_image_size_throws_when_qemuimg_info_does_not_understand_the_image_size)
{
    const mp::ProcessState qemuimg_exit_status{0, std::nullopt};
    const QByteArray qemuimg_output("virtual size: an unintelligible string");
    auto mock_factory_scope = inject_fake_qemuimg_callback(qemuimg_exit_status, qemuimg_output);

    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto vm_image = vault.fetch_image(mp::FetchType::ImageOnly,
                                      default_query,
                                      stub_prepare,
                                      stub_monitor,
                                      false,
                                      std::nullopt,
                                      instance_dir);

    MP_EXPECT_THROW_THAT(vault.minimum_image_size_for(vm_image.id), std::runtime_error,
                         mpt::match_what(HasSubstr("Could not obtain image's virtual size")));
}

TEST_F(ImageVault, all_info_for_no_remote_given_returns_expected_data)
{
    mpt::StubURLDownloader stub_url_downloader;
    mp::DefaultVMImageVault vault{hosts, &stub_url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};

    const std::string remote_name{"release"};
    EXPECT_CALL(host, all_info_for(_))
        .WillOnce(Return(std::vector<std::pair<std::string, mp::VMImageInfo>>{
            {remote_name, host.mock_bionic_image_info}, {remote_name, host.mock_another_image_info}}));

    auto images = vault.all_info_for({"", "e3", false, "", mp::Query::Type::Alias, true});

    EXPECT_EQ(images.size(), 2u);

    const auto& [first_image_remote, first_image_info] = images[0];
    EXPECT_EQ(first_image_remote, remote_name);
    EXPECT_EQ(first_image_info.id.toStdString(), mpt::default_id);
    EXPECT_EQ(first_image_info.version.toStdString(), mpt::default_version);

    const auto& [second_image_remote, second_image_info] = images[1];
    EXPECT_EQ(second_image_remote, remote_name);
    EXPECT_EQ(second_image_info.id.toStdString(), mpt::another_image_id);
    EXPECT_EQ(second_image_info.version.toStdString(), mpt::another_image_version);
}

TEST_F(ImageVault, all_info_for_remote_given_returns_expected_data)
{
    mpt::StubURLDownloader stub_url_downloader;
    mp::DefaultVMImageVault vault{hosts, &stub_url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};

    const std::string remote_name{"release"};
    EXPECT_CALL(host, all_info_for(_))
        .WillOnce(Return(std::vector<std::pair<std::string, mp::VMImageInfo>>{
            {remote_name, host.mock_bionic_image_info}, {remote_name, host.mock_another_image_info}}));

    auto images = vault.all_info_for({"", "e3", false, remote_name, mp::Query::Type::Alias, true});

    EXPECT_EQ(images.size(), 2u);

    const auto& [first_image_remote, first_image_info] = images[0];
    EXPECT_EQ(first_image_remote, remote_name);
    EXPECT_EQ(first_image_info.id.toStdString(), mpt::default_id);
    EXPECT_EQ(first_image_info.version.toStdString(), mpt::default_version);

    const auto& [second_image_remote, second_image_info] = images[1];
    EXPECT_EQ(second_image_remote, remote_name);
    EXPECT_EQ(second_image_info.id.toStdString(), mpt::another_image_id);
    EXPECT_EQ(second_image_info.version.toStdString(), mpt::another_image_version);
}

TEST_F(ImageVault, allInfoForNoImagesReturnsEmpty)
{
    mpt::StubURLDownloader stub_url_downloader;
    mp::DefaultVMImageVault vault{hosts, &stub_url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};

    const std::string name{"foo"};
    EXPECT_CALL(host, all_info_for(_)).WillOnce(Return(std::vector<std::pair<std::string, mp::VMImageInfo>>{}));

    EXPECT_TRUE(vault.all_info_for({"", name, false, "", mp::Query::Type::Alias, true}).empty());
}

TEST_F(ImageVault, updateImagesLogsWarningOnUnsupportedImage)
{
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject(mpl::Level::warning);
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{1}};
    vault.fetch_image(mp::FetchType::ImageOnly,
                      default_query,
                      stub_prepare,
                      stub_monitor,
                      false,
                      std::nullopt,
                      instance_dir);

    EXPECT_CALL(host, info_for(_)).WillOnce(Throw(mp::UnsupportedImageException(default_query.release)));

    logger_scope.mock_logger->screen_logs(mpl::Level::warning);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(mpl::Level::warning, mpt::MockLogger::make_cstring_matcher(StrEq("image vault")),
                    mpt::MockLogger::make_cstring_matcher(StrEq(fmt::format(
                        "Skipping update: The {} release is no longer supported.", default_query.release)))));

    EXPECT_NO_THROW(vault.update_images(mp::FetchType::ImageOnly, stub_prepare, stub_monitor));
}

TEST_F(ImageVault, updateImagesLogsWarningOnEmptyVault)
{
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject(mpl::Level::warning);
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{1}};
    vault.fetch_image(mp::FetchType::ImageOnly,
                      default_query,
                      stub_prepare,
                      stub_monitor,
                      false,
                      std::nullopt,
                      instance_dir);

    EXPECT_CALL(host, info_for(_)).WillOnce(Return(std::nullopt));

    logger_scope.mock_logger->screen_logs(mpl::Level::warning);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(mpl::Level::warning, mpt::MockLogger::make_cstring_matcher(StrEq("image vault")),
                    mpt::MockLogger::make_cstring_matcher(
                        StrEq(fmt::format("Skipping update: Unable to find an image matching \"{}\" in remote \"{}\".",
                                          default_query.release, default_query.remote_name)))));

    EXPECT_NO_THROW(vault.update_images(mp::FetchType::ImageOnly, stub_prepare, stub_monitor));
}

TEST_F(ImageVault, fetchLocalImageThrowsOnEmptyVault)
{
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{1}};

    EXPECT_CALL(host, info_for(_)).WillOnce(Return(std::nullopt));

    EXPECT_THROW(vault.fetch_image(mp::FetchType::ImageOnly,
                                   default_query,
                                   stub_prepare,
                                   stub_monitor,
                                   false,
                                   std::nullopt,
                                   instance_dir),
                 mp::ImageNotFoundException);
}

TEST_F(ImageVault, fetchRemoteImageThrowsOnMissingKernel)
{
    mp::Query query{instance_name, "xenial", false, "", mp::Query::Type::Alias};
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{1}};

    EXPECT_CALL(host, info_for(_)).WillOnce(Return(std::nullopt));

    EXPECT_THROW(vault.fetch_image(mp::FetchType::ImageOnly,
                                   query,
                                   stub_prepare,
                                   stub_monitor,
                                   false,
                                   std::nullopt,
                                   instance_dir),
                 mp::ImageNotFoundException);
}
