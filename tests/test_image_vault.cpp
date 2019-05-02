/*
 * Copyright (C) 2017-2019 Canonical, Ltd.
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

#include "src/daemon/default_vm_image_vault.h"

#include "file_operations.h"
#include "path.h"
#include "stub_url_downloader.h"
#include "temp_dir.h"
#include "temp_file.h"

#include <multipass/exceptions/create_image_exception.h>
#include <multipass/optional.h>
#include <multipass/query.h>
#include <multipass/url_downloader.h>
#include <multipass/utils.h>
#include <multipass/vm_image_host.h>

#include <QUrl>

#include <gmock/gmock.h>

#include <unordered_set>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
constexpr auto default_id = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
constexpr auto default_version = "20160217.1";

struct ImageHost : public mp::VMImageHost
{
    mp::optional<mp::VMImageInfo> info_for(const mp::Query& query) override
    {
        return mp::optional<mp::VMImageInfo>{mp::VMImageInfo{{"default"},
                                                             "Ubuntu",
                                                             "xenial",
                                                             "16.04 LTS",
                                                             true,
                                                             image.url(),
                                                             kernel.url(),
                                                             initrd.url(),
                                                             default_id,
                                                             default_version,
                                                             1}};
    }

    std::vector<mp::VMImageInfo> all_info_for(const mp::Query& query) override
    {
        return {};
    }

    mp::VMImageInfo info_for_full_hash(const std::string& full_hash) override
    {
        return {{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, -1};
    }

    std::vector<mp::VMImageInfo> all_images_for(const std::string& remote_name, const bool allow_unsupported) override
    {
        return {};
    }

    void for_each_entry_do(const Action& action) override
    {
    }

    std::vector<std::string> supported_remotes() override
    {
        return {"release"};
    }

    mpt::TempFile image;
    mpt::TempFile kernel;
    mpt::TempFile initrd;
};

struct TrackingURLDownloader : public mp::URLDownloader
{
    TrackingURLDownloader() : mp::URLDownloader{std::chrono::seconds(10)}
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
        return QDateTime::currentDateTime();
    }

    QStringList downloaded_files;
    QStringList downloaded_urls;
};

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

struct ImageVault : public testing::Test
{
    void SetUp()
    {
        hosts.push_back(&host);
    }

    QString host_url{QUrl::fromLocalFile(mpt::test_data_path()).toString()};
    TrackingURLDownloader url_downloader;
    std::vector<mp::VMImageHost*> hosts;
    ImageHost host;
    mp::ProgressMonitor stub_monitor{[](int, int) { return true; }};
    mp::VMImageVault::PrepareAction stub_prepare{
        [](const mp::VMImage& source_image) -> mp::VMImage { return source_image; }};
    mpt::TempDir cache_dir;
    mpt::TempDir data_dir;
    std::string instance_name{"valley-pied-piper"};
    mp::Query default_query{instance_name, "xenial", false, "", mp::Query::Type::Alias};
};
} // namespace

TEST_F(ImageVault, downloads_image)
{
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto vm_image = vault.fetch_image(mp::FetchType::ImageOnly, default_query, stub_prepare, stub_monitor);

    EXPECT_THAT(url_downloader.downloaded_files.size(), Eq(1));
    EXPECT_TRUE(url_downloader.downloaded_urls.contains(host.image.url()));
}

TEST_F(ImageVault, returned_image_contains_instance_name)
{
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto vm_image = vault.fetch_image(mp::FetchType::ImageOnly, default_query, stub_prepare, stub_monitor);

    EXPECT_TRUE(vm_image.image_path.contains(QString::fromStdString(instance_name)));
}

TEST_F(ImageVault, downloads_kernel_and_initrd)
{
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto vm_image = vault.fetch_image(mp::FetchType::ImageKernelAndInitrd, default_query, stub_prepare, stub_monitor);

    EXPECT_THAT(url_downloader.downloaded_files.size(), Eq(3));
    EXPECT_TRUE(url_downloader.downloaded_urls.contains(host.image.url()));
    EXPECT_TRUE(url_downloader.downloaded_urls.contains(host.kernel.url()));
    EXPECT_TRUE(url_downloader.downloaded_urls.contains(host.initrd.url()));

    EXPECT_FALSE(vm_image.kernel_path.isEmpty());
    EXPECT_FALSE(vm_image.initrd_path.isEmpty());
}

TEST_F(ImageVault, calls_prepare)
{
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};

    bool prepare_called{false};
    auto prepare = [&prepare_called](const mp::VMImage& source_image) -> mp::VMImage {
        prepare_called = true;
        return source_image;
    };
    auto vm_image = vault.fetch_image(mp::FetchType::ImageOnly, default_query, prepare, stub_monitor);

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
    auto vm_image1 = vault.fetch_image(mp::FetchType::ImageOnly, default_query, prepare, stub_monitor);
    auto vm_image2 = vault.fetch_image(mp::FetchType::ImageOnly, default_query, prepare, stub_monitor);

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
    auto vm_image1 = vault.fetch_image(mp::FetchType::ImageOnly, default_query, prepare, stub_monitor);

    auto another_query = default_query;
    another_query.name = "valley-pied-piper-chat";
    auto vm_image2 = vault.fetch_image(mp::FetchType::ImageOnly, another_query, prepare, stub_monitor);

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

    mp::DefaultVMImageVault first_vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto vm_image1 = first_vault.fetch_image(mp::FetchType::ImageOnly, default_query, prepare, stub_monitor);

    mp::DefaultVMImageVault another_vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto vm_image2 = another_vault.fetch_image(mp::FetchType::ImageOnly, default_query, prepare, stub_monitor);

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

    mp::DefaultVMImageVault first_vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto vm_image1 = first_vault.fetch_image(mp::FetchType::ImageOnly, default_query, prepare, stub_monitor);

    auto another_query = default_query;
    another_query.name = "valley-pied-piper-chat";
    mp::DefaultVMImageVault another_vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto vm_image2 = another_vault.fetch_image(mp::FetchType::ImageOnly, another_query, prepare, stub_monitor);

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
        return {file_name, "", "", source_image.id, "", "", "", {}};
    };

    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto vm_image = vault.fetch_image(mp::FetchType::ImageOnly, default_query, prepare, stub_monitor);

    const auto image_data = mp::utils::contents_of(vm_image.image_path);
    EXPECT_THAT(image_data, StrEq(expected_data));
    EXPECT_THAT(vm_image.id, Eq(default_id));
}

TEST_F(ImageVault, image_purged_expired)
{
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};

    QDir images_dir{mp::utils::make_dir(cache_dir.path(), "images")};
    auto file_name = images_dir.filePath("mock_image.img");

    auto prepare = [&file_name](const mp::VMImage& source_image) -> mp::VMImage {
        mpt::make_file_with_content(file_name);
        return {file_name, "", "", source_image.id, "", "", "", {}};
    };
    auto vm_image = vault.fetch_image(mp::FetchType::ImageOnly, default_query, prepare, stub_monitor);

    EXPECT_TRUE(QFileInfo::exists(file_name));

    vault.prune_expired_images();

    EXPECT_FALSE(QFileInfo::exists(file_name));
}

TEST_F(ImageVault, image_exists_not_expired)
{
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{1}};

    QDir images_dir{mp::utils::make_dir(cache_dir.path(), "images")};
    auto file_name = images_dir.filePath("mock_image.img");

    auto prepare = [&file_name](const mp::VMImage& source_image) -> mp::VMImage {
        mpt::make_file_with_content(file_name);
        return {file_name, "", "", source_image.id, "", "", "", {}};
    };
    auto vm_image = vault.fetch_image(mp::FetchType::ImageOnly, default_query, prepare, stub_monitor);

    EXPECT_TRUE(QFileInfo::exists(file_name));

    vault.prune_expired_images();

    EXPECT_TRUE(QFileInfo::exists(file_name));
}

TEST_F(ImageVault, invalid_custom_image_file_throws)
{
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto query = default_query;

    query.release = "file://foo";
    query.query_type = mp::Query::Type::LocalFile;

    EXPECT_THROW(vault.fetch_image(mp::FetchType::ImageOnly, query, stub_prepare, stub_monitor), std::runtime_error);
}

TEST_F(ImageVault, custom_image_url_downloads)
{
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto query = default_query;

    query.release = "http://www.foo.com/fake.img";
    query.query_type = mp::Query::Type::HttpDownload;

    vault.fetch_image(mp::FetchType::ImageOnly, query, stub_prepare, stub_monitor);

    EXPECT_THAT(url_downloader.downloaded_files.size(), Eq(1));
    EXPECT_TRUE(url_downloader.downloaded_urls.contains(QString::fromStdString(query.release)));
}

TEST_F(ImageVault, missing_downloaded_image_throws)
{
    mpt::StubURLDownloader stub_url_downloader;
    mp::DefaultVMImageVault vault{hosts, &stub_url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    EXPECT_THROW(vault.fetch_image(mp::FetchType::ImageOnly, default_query, stub_prepare, stub_monitor),
                 mp::CreateImageException);
}

TEST_F(ImageVault, hash_mismatch_throws)
{
    BadURLDownloader bad_url_downloader;
    mp::DefaultVMImageVault vault{hosts, &bad_url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    EXPECT_THROW(vault.fetch_image(mp::FetchType::ImageOnly, default_query, stub_prepare, stub_monitor),
                 mp::CreateImageException);
}

TEST_F(ImageVault, invalid_remote_throws)
{
    mpt::StubURLDownloader stub_url_downloader;
    mp::DefaultVMImageVault vault{hosts, &stub_url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto query = default_query;

    query.remote_name = "foo";

    EXPECT_THROW(vault.fetch_image(mp::FetchType::ImageOnly, query, stub_prepare, stub_monitor), std::runtime_error);
}

TEST_F(ImageVault, invalid_image_alias_throw)
{
    mpt::StubURLDownloader stub_url_downloader;
    mp::DefaultVMImageVault vault{hosts, &stub_url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto query = default_query;

    query.release = "foo";

    EXPECT_THROW(vault.fetch_image(mp::FetchType::ImageOnly, query, stub_prepare, stub_monitor),
                 mp::CreateImageException);
}

TEST_F(ImageVault, valid_remote_and_alias_returns_valid_image_info)
{
    mp::DefaultVMImageVault vault{hosts, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto query = default_query;

    query.release = "default";
    query.remote_name = "release";

    mp::VMImage image;
    EXPECT_NO_THROW(image = vault.fetch_image(mp::FetchType::ImageOnly, query, stub_prepare, stub_monitor));

    EXPECT_THAT(image.original_release, Eq("16.04 LTS"));
    EXPECT_THAT(image.id, Eq("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
}
