/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include "src/daemon/default_vm_image_vault.h"

#include "file_operations.h"
#include "path.h"
#include "temp_file.h"

#include <multipass/query.h>
#include <multipass/url_downloader.h>
#include <multipass/vm_image_host.h>

#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QUrl>

#include <gmock/gmock.h>

#include <unordered_set>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
static constexpr auto default_id = "42";
static constexpr auto default_version = "20160217.1";

struct ImageHost : public mp::VMImageHost
{
    mp::VMImageInfo info_for(const mp::Query& query) override
    {
        return {{"default"}, "xenial",   "16.04 LTS",     true, image.url(), kernel.url(),
                initrd.url(),  default_id, default_version, 1};
    }

    std::vector<mp::VMImageInfo> all_info_for(const mp::Query& query) override
    {
        return {};
    }

    mp::VMImageInfo info_for_full_hash(const std::string& full_hash) override
    {
        return {{}, {}, {}, {}, {}, {}, {}, {}, {}, -1};
    }

    void for_each_entry_do(const Action& action) override
    {
    }

    std::string get_default_remote() override
    {
        return {};
    }

    mpt::TempFile image;
    mpt::TempFile kernel;
    mpt::TempFile initrd;
};

struct TrackingURLDownloader : public mp::URLDownloader
{
    void download_to(const QUrl& url, const QString& file_name, int64_t size, const int download_type,
                     const mp::ProgressMonitor&) override
    {
        downloaded_urls << url.toString();
        downloaded_files << file_name;
    }

    QByteArray download(const QUrl& url) override
    {
        return {};
    }

    QStringList downloaded_files;
    QStringList downloaded_urls;
};

struct ImageVault : public testing::Test
{
    QString host_url{QUrl::fromLocalFile(mpt::test_data_path()).toString()};
    TrackingURLDownloader url_downloader;
    ImageHost host;
    mp::ProgressMonitor stub_monitor{[](int, int) { return true; }};
    mp::VMImageVault::PrepareAction stub_prepare{
        [](const mp::VMImage& source_image) -> mp::VMImage { return source_image; }};
    QTemporaryDir cache_dir;
    QTemporaryDir data_dir;
    std::string instance_name{"valley-pied-piper"};
    mp::Query default_query{instance_name, "xenial", false, "", mp::Query::Type::SimpleStreams};
};
}

TEST_F(ImageVault, downloads_image)
{
    mp::DefaultVMImageVault vault{&host, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto vm_image = vault.fetch_image(mp::FetchType::ImageOnly, default_query, stub_prepare, stub_monitor);

    EXPECT_THAT(url_downloader.downloaded_files.size(), Eq(1));
    EXPECT_TRUE(url_downloader.downloaded_urls.contains(host.image.url()));
}

TEST_F(ImageVault, returned_image_contains_instance_name)
{
    mp::DefaultVMImageVault vault{&host, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto vm_image = vault.fetch_image(mp::FetchType::ImageOnly, default_query, stub_prepare, stub_monitor);

    EXPECT_TRUE(vm_image.image_path.contains(QString::fromStdString(instance_name)));
}

TEST_F(ImageVault, downloads_kernel_and_initrd)
{
    mp::DefaultVMImageVault vault{&host, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
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
    mp::DefaultVMImageVault vault{&host, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};

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
    mp::DefaultVMImageVault vault{&host, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
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
    mp::DefaultVMImageVault vault{&host, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
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

    mp::DefaultVMImageVault first_vault{&host, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto vm_image1 = first_vault.fetch_image(mp::FetchType::ImageOnly, default_query, prepare, stub_monitor);

    mp::DefaultVMImageVault another_vault{&host, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
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

    mp::DefaultVMImageVault first_vault{&host, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto vm_image1 = first_vault.fetch_image(mp::FetchType::ImageOnly, default_query, prepare, stub_monitor);

    auto another_query = default_query;
    another_query.name = "valley-pied-piper-chat";
    mp::DefaultVMImageVault another_vault{&host, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto vm_image2 = another_vault.fetch_image(mp::FetchType::ImageOnly, another_query, prepare, stub_monitor);

    EXPECT_THAT(url_downloader.downloaded_files.size(), Eq(1));
    EXPECT_THAT(prepare_called_count, Eq(1));
    EXPECT_THAT(vm_image1.image_path, Ne(vm_image2.image_path));
    EXPECT_THAT(vm_image1.id, Eq(vm_image2.id));
}

TEST_F(ImageVault, reads_fallback_db)
{
    int prepare_called_count{0};
    auto prepare = [&prepare_called_count](const mp::VMImage& source_image) -> mp::VMImage {
        ++prepare_called_count;
        return source_image;
    };

    // Note this uses cache_dir for both cache and data paths
    mp::DefaultVMImageVault first_vault{&host, &url_downloader, cache_dir.path(), cache_dir.path(), mp::days{0}};
    auto vm_image1 = first_vault.fetch_image(mp::FetchType::ImageOnly, default_query, prepare, stub_monitor);

    auto another_query = default_query;
    another_query.name = "valley-pied-piper-chat";
    mp::DefaultVMImageVault another_vault{&host, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto vm_image2 = another_vault.fetch_image(mp::FetchType::ImageOnly, another_query, prepare, stub_monitor);

    EXPECT_THAT(url_downloader.downloaded_files.size(), Eq(1));
    EXPECT_THAT(prepare_called_count, Eq(1));
    EXPECT_THAT(vm_image1.image_path, Ne(vm_image2.image_path));
    EXPECT_THAT(vm_image1.id, Eq(vm_image2.id));
}

TEST_F(ImageVault, uses_image_from_prepare)
{
    QByteArray expected_data;
    expected_data.append(QString("12345-pied-piper-rats"));

    QDir dir{cache_dir.path()};
    auto file_name = dir.filePath("prepared-image");

    QFile file{file_name};
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(expected_data);
        file.flush();
    }

    auto prepare = [&file_name](const mp::VMImage& source_image) -> mp::VMImage {
        return {file_name, "", "", source_image.id, "", "", {}};
    };

    mp::DefaultVMImageVault vault{&host, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto vm_image = vault.fetch_image(mp::FetchType::ImageOnly, default_query, prepare, stub_monitor);

    auto image_data = mpt::load(vm_image.image_path);
    EXPECT_THAT(image_data, Eq(expected_data));
    EXPECT_THAT(vm_image.id, Eq(default_id));
}

TEST_F(ImageVault, image_purged_expired)
{
    mp::DefaultVMImageVault vault{&host, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};

    QDir dir{cache_dir.path()};
    dir.mkpath("images");
    dir.cd("images");
    auto file_name = dir.filePath("mock_image.img");

    auto prepare = [&file_name](const mp::VMImage& source_image) -> mp::VMImage {
        QFile file{file_name};
        if (file.open(QIODevice::WriteOnly))
        {
            file.write("");
            file.flush();
        }
        return {file_name, "", "", source_image.id, "", "", {}};
    };
    auto vm_image = vault.fetch_image(mp::FetchType::ImageOnly, default_query, prepare, stub_monitor);

    EXPECT_TRUE(QFileInfo::exists(file_name));

    vault.prune_expired_images();

    EXPECT_FALSE(QFileInfo::exists(file_name));
}

TEST_F(ImageVault, image_exists_not_expired)
{
    mp::DefaultVMImageVault vault{&host, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{1}};

    QDir dir{cache_dir.path()};
    dir.mkpath("images");
    dir.cd("images");
    auto file_name = dir.filePath("mock_image.img");

    auto prepare = [&file_name](const mp::VMImage& source_image) -> mp::VMImage {
        QFile file{file_name};
        if (file.open(QIODevice::WriteOnly))
        {
            file.write("");
            file.flush();
        }
        return {file_name, "", "", source_image.id, "", "", {}};
    };
    auto vm_image = vault.fetch_image(mp::FetchType::ImageOnly, default_query, prepare, stub_monitor);

    EXPECT_TRUE(QFileInfo::exists(file_name));

    vault.prune_expired_images();

    EXPECT_TRUE(QFileInfo::exists(file_name));
}

TEST_F(ImageVault, invalid_custom_image_file_throws)
{
    mp::DefaultVMImageVault vault{&host, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto query = default_query;

    query.release = "file://foo";
    query.query_type = mp::Query::Type::LocalFile;

    EXPECT_THROW(vault.fetch_image(mp::FetchType::ImageOnly, query, stub_prepare, stub_monitor), std::runtime_error);
}

TEST_F(ImageVault, custom_image_url_downloads)
{
    mp::DefaultVMImageVault vault{&host, &url_downloader, cache_dir.path(), data_dir.path(), mp::days{0}};
    auto query = default_query;

    query.release = "http://www.foo.com/fake.img";
    query.query_type = mp::Query::Type::HttpDownload;

    vault.fetch_image(mp::FetchType::ImageOnly, query, stub_prepare, stub_monitor);

    EXPECT_THAT(url_downloader.downloaded_files.size(), Eq(1));
    EXPECT_TRUE(url_downloader.downloaded_urls.contains(QString::fromStdString(query.release)));
}
