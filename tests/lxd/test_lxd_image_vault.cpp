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

#include "mock_local_socket_reply.h"
#include "mock_lxd_server_responses.h"
#include "mock_network_access_manager.h"

#include "tests/common.h"
#include "tests/mock_image_host.h"
#include "tests/mock_logger.h"
#include "tests/mock_process_factory.h"
#include "tests/stub_url_downloader.h"
#include "tests/temp_dir.h"
#include "tests/tracking_url_downloader.h"

#include <src/platform/backends/lxd/lxd_vm_image_vault.h>

#include <multipass/exceptions/aborted_download_exception.h>
#include <multipass/exceptions/image_vault_exceptions.h>
#include <multipass/exceptions/local_socket_connection_exception.h>
#include <multipass/format.h>
#include <multipass/vm_image.h>

#include <QUrl>

#include <vector>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct LXDImageVault : public Test
{
    LXDImageVault() : mock_network_access_manager{std::make_unique<NiceMock<mpt::MockNetworkAccessManager>>()}
    {
        hosts.push_back(&host);

        ON_CALL(host, info_for_full_hash(_)).WillByDefault([this](auto...) { return host.mock_bionic_image_info; });

        logger_scope.mock_logger->screen_logs(mpl::Level::error);
    }

    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
    std::unique_ptr<NiceMock<mpt::MockNetworkAccessManager>> mock_network_access_manager;
    std::vector<mp::VMImageHost*> hosts;
    NiceMock<mpt::MockImageHost> host;
    QUrl base_url{"unix:///foo@1.0"};
    mp::ProgressMonitor stub_monitor{[](int, int) { return true; }};
    mp::VMImageVault::PrepareAction stub_prepare{
        [](const mp::VMImage& source_image) -> mp::VMImage { return source_image; }};
    std::string instance_name{"pied-piper-valley"};
    mp::Query default_query{instance_name, "xenial", false, "", mp::Query::Type::Alias};
    mpt::StubURLDownloader stub_url_downloader;
    mpt::TempDir cache_dir;
    mpt::TempDir save_dir;
};
} // namespace

TEST_F(LXDImageVault, instance_exists_fetch_returns_expected_image_info)
{
    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _)).WillByDefault([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();

        if (op == "GET")
        {
            if (url.contains("1.0/virtual-machines/pied-piper-valley"))
            {
                return new mpt::MockLocalSocketReply(mpt::vm_info_data);
            }
        }

        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    mp::VMImage image;
    EXPECT_NO_THROW(image = image_vault.fetch_image(mp::FetchType::ImageOnly,
                                                    default_query,
                                                    stub_prepare,
                                                    stub_monitor,
                                                    false,
                                                    std::nullopt,
                                                    save_dir.path()));

    EXPECT_EQ(image.id, mpt::default_id);
    EXPECT_EQ(image.original_release, "18.04 LTS");
}

TEST_F(LXDImageVault, instance_exists_custom_image_returns_expected_image_info)
{
    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _)).WillByDefault([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();

        if (op == "GET")
        {
            if (url.contains("1.0/virtual-machines/pied-piper-valley"))
            {
                return new mpt::MockLocalSocketReply(mpt::vm_custom_info_data);
            }
        }

        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    mp::VMImage image;
    EXPECT_NO_THROW(image = image_vault.fetch_image(mp::FetchType::ImageOnly,
                                                    default_query,
                                                    stub_prepare,
                                                    stub_monitor,
                                                    false,
                                                    std::nullopt,
                                                    save_dir.path()));

    EXPECT_EQ(image.id, "6937ddd3f4c3329182855843571fc91ae4fee24e8e0eb0f7cdcf2c22feed4dab");
    EXPECT_EQ(image.original_release, "Snapcraft builder for Core 20");
    EXPECT_EQ(image.release_date, "20200923");
}

TEST_F(LXDImageVault, instance_exists_uses_cached_release_title)
{
    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _)).WillByDefault([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();

        if (op == "GET")
        {
            if (url.contains("1.0/virtual-machines/pied-piper-valley"))
            {
                return new mpt::MockLocalSocketReply(mpt::vm_info_data_with_image_release);
            }
        }

        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    mp::VMImage image;
    EXPECT_NO_THROW(image = image_vault.fetch_image(mp::FetchType::ImageOnly,
                                                    default_query,
                                                    stub_prepare,
                                                    stub_monitor,
                                                    false,
                                                    std::nullopt,
                                                    save_dir.path()));

    EXPECT_EQ(image.id, mpt::default_id);
    EXPECT_EQ(image.original_release, "Fake Title");
}

TEST_F(LXDImageVault, instance_exists_no_cached_release_title_info_for_fails)
{
    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _)).WillByDefault([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();

        if (op == "GET")
        {
            if (url.contains("1.0/virtual-machines/pied-piper-valley"))
            {
                return new mpt::MockLocalSocketReply(mpt::vm_info_data);
            }
        }

        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    EXPECT_CALL(host, info_for(_)).WillRepeatedly(Throw(std::runtime_error("foo")));

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    mp::VMImage image;
    EXPECT_NO_THROW(image = image_vault.fetch_image(mp::FetchType::ImageOnly,
                                                    default_query,
                                                    stub_prepare,
                                                    stub_monitor,
                                                    false,
                                                    std::nullopt,
                                                    save_dir.path()));

    EXPECT_EQ(image.id, mpt::default_id);
    EXPECT_EQ(image.original_release, "");
}

TEST_F(LXDImageVault, returns_expected_info_with_valid_remote)
{
    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _)).WillByDefault([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();

        if (op == "GET")
        {
            if (url.contains("1.0/images/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"))
            {
                return new mpt::MockLocalSocketReply(mpt::image_info_data);
            }
        }

        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    mp::Query query{"", "bionic", false, "release", mp::Query::Type::Alias};

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    mp::VMImage image;
    EXPECT_NO_THROW(image = image_vault.fetch_image(mp::FetchType::ImageOnly,
                                                    query,
                                                    stub_prepare,
                                                    stub_monitor,
                                                    false,
                                                    std::nullopt,
                                                    save_dir.path()));

    EXPECT_EQ(image.id, mpt::default_id);
    EXPECT_EQ(image.original_release, "18.04 LTS");
    EXPECT_EQ(image.release_date, mpt::default_version);
}

TEST_F(LXDImageVault, throws_with_invalid_alias)
{
    ON_CALL(host, info_for(_)).WillByDefault([this](auto query) -> std::optional<mp::VMImageInfo> {
        if (query.release != "bionic")
        {
            return std::nullopt;
        }

        return host.mock_bionic_image_info;
    });

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _)).WillByDefault([](auto...) {
        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    const std::string alias{"xenial"};
    mp::Query query{"", alias, false, "release", mp::Query::Type::Alias};

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    MP_EXPECT_THROW_THAT(
        image_vault.fetch_image(mp::FetchType::ImageOnly,
                                query,
                                stub_prepare,
                                stub_monitor,
                                false,
                                std::nullopt,
                                save_dir.path()),
        std::runtime_error,
        mpt::match_what(
            StrEq(fmt::format("Unable to find an image matching \"{}\" in remote \"{}\".", alias, "release"))));
}

TEST_F(LXDImageVault, throws_with_invalid_remote)
{
    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _)).WillByDefault([](auto...) {
        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    const std::string remote{"bar"};
    mp::Query query{"", "foo", false, remote, mp::Query::Type::Alias};

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    MP_EXPECT_THROW_THAT(image_vault.fetch_image(mp::FetchType::ImageOnly,
                                                 query,
                                                 stub_prepare,
                                                 stub_monitor,
                                                 false,
                                                 std::nullopt,
                                                 save_dir.path()),
                         std::runtime_error,
                         mpt::match_what(HasSubstr(fmt::format("Remote \'{}\' is not found.", remote))));
}

TEST_F(LXDImageVault, does_not_download_if_image_exists)
{
    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _)).WillByDefault([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();

        if (op == "GET")
        {
            if (url.contains("1.0/images/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"))
            {
                return new mpt::MockLocalSocketReply(mpt::image_info_data);
            }
        }
        else if (op == "POST" && url.contains("1.0/images"))
        {
            ADD_FAILURE() << "Image download shouldn't be requested";
        }

        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    EXPECT_NO_THROW(image_vault.fetch_image(mp::FetchType::ImageOnly,
                                            default_query,
                                            stub_prepare,
                                            stub_monitor,
                                            false,
                                            std::nullopt,
                                            save_dir.path()));
}

TEST_F(LXDImageVault, instance_exists_missing_image_does_not_download_image)
{
    bool download_requested{false};

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _))
        .WillByDefault([&download_requested](auto, auto request, auto) {
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET")
            {
                if (url.contains("1.0/virtual-machines/pied-piper-valley"))
                {
                    return new mpt::MockLocalSocketReply(mpt::vm_info_data);
                }
            }
            else if (op == "POST" && url.contains("1.0/images"))
            {
                download_requested = true;
                return new mpt::MockLocalSocketReply(mpt::image_download_task_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    const auto missing_img_hash = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    EXPECT_CALL(host, info_for(_)).WillRepeatedly(Return(host.mock_bionic_image_info));
    EXPECT_CALL(host, info_for(Field(&mp::Query::release, missing_img_hash))).WillRepeatedly(Return(std::nullopt));

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    mp::VMImage image;
    EXPECT_NO_THROW(image = image_vault.fetch_image(mp::FetchType::ImageOnly,
                                                    default_query,
                                                    stub_prepare,
                                                    stub_monitor,
                                                    false,
                                                    std::nullopt,
                                                    save_dir.path()));
    EXPECT_FALSE(download_requested);
    EXPECT_EQ(image.original_release, mpt::default_release_info);
}

TEST_F(LXDImageVault, requests_download_if_image_does_not_exist)
{
    bool download_requested{false};

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _))
        .WillByDefault([&download_requested](auto, auto request, auto) {
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "POST" && url.contains("1.0/images"))
            {
                download_requested = true;
                return new mpt::MockLocalSocketReply(mpt::image_download_task_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    EXPECT_NO_THROW(image_vault.fetch_image(mp::FetchType::ImageOnly,
                                            default_query,
                                            stub_prepare,
                                            stub_monitor,
                                            false,
                                            std::nullopt,
                                            save_dir.path()));
    EXPECT_TRUE(download_requested);
}

TEST_F(LXDImageVault, sets_fingerprint_with_hash_query)
{
    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _))
        .WillByDefault([](auto, auto request, auto outgoingData) {
            outgoingData->open(QIODevice::ReadOnly);
            auto data = outgoingData->readAll();
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "POST" && url.contains("1.0/images"))
            {
                EXPECT_TRUE(data.contains("fingerprint"));
                EXPECT_FALSE(data.contains("alias"));
                return new mpt::MockLocalSocketReply(mpt::image_download_task_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    const mp::Query query{"", "e3b0c44298fc1c1", false, "release", mp::Query::Type::Alias};
    EXPECT_NO_THROW(image_vault.fetch_image(mp::FetchType::ImageOnly,
                                            query,
                                            stub_prepare,
                                            stub_monitor,
                                            false,
                                            std::nullopt,
                                            save_dir.path()));
}

TEST_F(LXDImageVault, download_deletes_and_throws_on_cancel)
{
    bool delete_requested{false};

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _))
        .WillByDefault([&delete_requested](auto, auto request, auto) {
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "POST" && url.contains("1.0/images"))
            {
                return new mpt::MockLocalSocketReply(mpt::image_download_task_data);
            }
            else if (op == "GET" && url.contains("1.0/operations/0a19a412-03d0-4118-bee8-a3095f06d4da"))
            {
                return new mpt::MockLocalSocketReply(mpt::image_downloading_task_data);
            }
            else if (op == "DELETE" && url.contains("1.0/operations/0a19a412-03d0-4118-bee8-a3095f06d4da"))
            {
                delete_requested = true;
                return new mpt::MockLocalSocketReply(mpt::post_no_error_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::ProgressMonitor monitor{[](auto, auto progress) {
        EXPECT_EQ(progress, 25);

        return false;
    }};

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    EXPECT_THROW(image_vault.fetch_image(mp::FetchType::ImageOnly,
                                         default_query,
                                         stub_prepare,
                                         monitor,
                                         false,
                                         std::nullopt,
                                         save_dir.path()),
                 mp::AbortedDownloadException);

    EXPECT_TRUE(delete_requested);
}

TEST_F(LXDImageVault, percent_complete_returns_negative_on_metadata_download)
{
    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _)).WillByDefault([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();

        if (op == "POST" && url.contains("1.0/images"))
        {
            return new mpt::MockLocalSocketReply(mpt::image_download_task_data);
        }
        else if (op == "GET" && url.contains("1.0/operations/0a19a412-03d0-4118-bee8-a3095f06d4da"))
        {
            return new mpt::MockLocalSocketReply(mpt::metadata_downloading_task_data);
        }
        else if (op == "DELETE" && url.contains("1.0/operations/0a19a412-03d0-4118-bee8-a3095f06d4da"))
        {
            return new mpt::MockLocalSocketReply(mpt::post_no_error_data);
        }

        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    mp::ProgressMonitor monitor{[](auto, auto progress) {
        EXPECT_EQ(progress, -1);

        return false;
    }};

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    EXPECT_THROW(image_vault.fetch_image(mp::FetchType::ImageOnly,
                                         default_query,
                                         stub_prepare,
                                         monitor,
                                         false,
                                         std::nullopt,
                                         save_dir.path()),
                 mp::AbortedDownloadException);
}

TEST_F(LXDImageVault, delete_requested_on_instance_remove)
{
    bool delete_requested{false}, wait_requested{false};

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _))
        .WillByDefault([&delete_requested, &wait_requested](auto, auto request, auto) {
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "DELETE" && url.contains("1.0/virtual-machines/pied-piper-valley"))
            {
                delete_requested = true;
                return new mpt::MockLocalSocketReply(mpt::delete_vm_data);
            }
            else if (op == "GET" && url.contains("1.0/operations/1c6265fc-8194-4790-9ab0-3225b0479155"))
            {
                wait_requested = true;
                return new mpt::MockLocalSocketReply(mpt::post_no_error_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    EXPECT_NO_THROW(image_vault.remove(instance_name));
    EXPECT_TRUE(delete_requested);
    EXPECT_TRUE(wait_requested);
}

TEST_F(LXDImageVault, logs_warning_when_removing_nonexistent_instance)
{
    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _)).WillByDefault([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();

        if (op == "DELETE" && url.contains("1.0/virtual-machines/pied-piper-valley"))
        {
            return new mpt::MockLocalSocketReply(mpt::post_no_error_data);
        }

        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    const std::string name{"foo"};
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::warning), mpt::MockLogger::make_cstring_matcher(StrEq("lxd image vault")),
                    mpt::MockLogger::make_cstring_matcher(
                        StrEq(fmt::format("Instance \'{}\' does not exist: not removing", name)))))
        .Times(1);
    EXPECT_NO_THROW(image_vault.remove(name));
}

TEST_F(LXDImageVault, has_record_for_returns_expected_values)
{
    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _)).WillByDefault([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();

        if (op == "GET")
        {
            if (url.contains("1.0/virtual-machines/pied-piper-valley"))
            {
                return new mpt::MockLocalSocketReply(mpt::vm_info_data);
            }
        }

        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    EXPECT_TRUE(image_vault.has_record_for(instance_name));
    EXPECT_FALSE(image_vault.has_record_for("foo"));
}

TEST_F(LXDImageVault, has_record_for_error_logs_message_and_returns_true)
{
    const std::string exception_message{"Cannot connect to socket"};
    const std::string instance_name{"foo"};

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _))
        .WillByDefault([&exception_message](auto...) -> QNetworkReply* {
            throw mp::LocalSocketConnectionException(exception_message);
        });

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::warning), mpt::MockLogger::make_cstring_matcher(StrEq("lxd image vault")),
                    mpt::MockLogger::make_cstring_matcher(StrEq(
                        fmt::format("{} - Unable to determine if \'{}\' exists", exception_message, instance_name)))));

    EXPECT_TRUE(image_vault.has_record_for(instance_name));
}

TEST_F(LXDImageVault, update_image_downloads_new_and_deletes_old_and_logs_expected_message)
{
    bool download_requested{false}, delete_requested{false};

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _))
        .WillByDefault([&download_requested, &delete_requested](auto, auto request, auto) {
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET" && url.contains("1.0/images"))
            {
                return new mpt::MockLocalSocketReply(mpt::image_info_update_source_info);
            }
            else if (op == "POST" && url.contains("1.0/images"))
            {
                download_requested = true;
                return new mpt::MockLocalSocketReply(mpt::image_download_task_data);
            }
            else if (op == "DELETE" &&
                     url.contains("1.0/images/aedb5a84aaf2e4e443e090511156366a2800c26cec1b6a46f44d153c4bf04205"))
            {
                delete_requested = true;
                return new mpt::MockLocalSocketReply(mpt::image_delete_task_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::debug), mpt::MockLogger::make_cstring_matcher(StrEq("lxd image vault")),
                    mpt::MockLogger::make_cstring_matcher(StrEq("Checking for images to update…"))));
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::info), mpt::MockLogger::make_cstring_matcher(StrEq("lxd image vault")),
                    mpt::MockLogger::make_cstring_matcher(StrEq("Updating bionic source image to latest"))));

    image_vault.update_images(mp::FetchType::ImageOnly, stub_prepare, stub_monitor);

    EXPECT_TRUE(download_requested);
    EXPECT_TRUE(delete_requested);
}

TEST_F(LXDImageVault, update_image_not_downloaded_when_no_new_image)
{
    bool download_requested{false};

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _))
        .WillByDefault([&download_requested](auto, auto request, auto) {
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET" && url.contains("1.0/images"))
            {
                return new mpt::MockLocalSocketReply(mpt::image_info_data);
            }
            else if (op == "POST" && url.contains("1.0/images"))
            {
                download_requested = true;
                return new mpt::MockLocalSocketReply(mpt::image_download_task_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    image_vault.update_images(mp::FetchType::ImageOnly, stub_prepare, stub_monitor);

    EXPECT_FALSE(download_requested);
}

TEST_F(LXDImageVault, update_image_no_project_does_not_throw)
{
    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _)).WillByDefault([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();

        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    EXPECT_NO_THROW(image_vault.update_images(mp::FetchType::ImageOnly, stub_prepare, stub_monitor));
}

TEST_F(LXDImageVault, image_update_source_delete_requested_on_expiration)
{
    bool delete_requested{false};

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _))
        .WillByDefault([&delete_requested](auto, auto request, auto) {
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET" && url.contains("1.0/images"))
            {
                return new mpt::MockLocalSocketReply(mpt::image_info_update_source_info);
            }
            else if (op == "DELETE" &&
                     url.contains("1.0/images/aedb5a84aaf2e4e443e090511156366a2800c26cec1b6a46f44d153c4bf04205"))
            {
                delete_requested = true;
                return new mpt::MockLocalSocketReply(mpt::image_delete_task_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::info), mpt::MockLogger::make_cstring_matcher(StrEq("lxd image vault")),
                    mpt::MockLogger::make_cstring_matcher(StrEq("Source image \'bionic\' is expired. Removing it…"))));

    image_vault.prune_expired_images();

    EXPECT_TRUE(delete_requested);
}

TEST_F(LXDImageVault, image_hash_delete_requested_on_expiration)
{
    bool delete_requested{false};

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _))
        .WillByDefault([&delete_requested](auto, auto request, auto) {
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET" && url.contains("1.0/images"))
            {
                return new mpt::MockLocalSocketReply(mpt::image_info_data);
            }
            else if (op == "DELETE" &&
                     url.contains("1.0/images/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"))
            {
                delete_requested = true;
                return new mpt::MockLocalSocketReply(mpt::image_delete_task_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    image_vault.prune_expired_images();

    EXPECT_TRUE(delete_requested);
}

TEST_F(LXDImageVault, prune_uses_last_update_property_on_new_unused_image)
{
    bool delete_requested{false};

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _))
        .WillByDefault([&delete_requested](auto, auto request, auto) {
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET" && url.contains("1.0/images"))
            {
                return new mpt::MockLocalSocketReply(mpt::image_info_unused_updated_image);
            }
            else if (op == "DELETE" &&
                     url.contains("1.0/images/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"))
            {
                delete_requested = true;
                return new mpt::MockLocalSocketReply(mpt::image_delete_task_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    image_vault.prune_expired_images();

    EXPECT_TRUE(delete_requested);
}

TEST_F(LXDImageVault, prune_expired_image_no_project_does_not_throw)
{
    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _)).WillByDefault([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();

        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    EXPECT_NO_THROW(image_vault.prune_expired_images());
}

TEST_F(LXDImageVault, prune_expired_error_logs_warning_does_not_throw)
{
    const std::string exception_message{"Cannot connect to socket"};

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _))
        .WillByDefault([&exception_message](auto...) -> QNetworkReply* {
            throw mp::LocalSocketConnectionException(exception_message);
        });

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::warning), mpt::MockLogger::make_cstring_matcher(StrEq("lxd image vault")),
                    mpt::MockLogger::make_cstring_matcher(StrEq(exception_message))));

    EXPECT_NO_THROW(image_vault.prune_expired_images());
}

TEST_F(LXDImageVault, prune_expired_image_delete_fails_does_no_throw)
{
    bool delete_requested{false};

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _))
        .WillByDefault([&delete_requested](auto, auto request, auto) {
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET" && url.contains("1.0/images"))
            {
                return new mpt::MockLocalSocketReply(mpt::image_info_data);
            }
            else if (op == "DELETE" &&
                     url.contains("1.0/images/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"))
            {
                delete_requested = true;
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    EXPECT_NO_THROW(image_vault.prune_expired_images());

    EXPECT_TRUE(delete_requested);
}

TEST_F(LXDImageVault, custom_image_found_returns_expected_info)
{
    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _)).WillByDefault([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();

        if (op == "GET" && url.contains("1.0/images?recursion=1"))
        {
            return new mpt::MockLocalSocketReply(mpt::image_info_data);
        }

        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    const mp::Query query{"", "snapcraft", false, "release", mp::Query::Type::Alias};
    auto image = image_vault.fetch_image(mp::FetchType::ImageOnly,
                                         query,
                                         stub_prepare,
                                         stub_monitor,
                                         false,
                                         std::nullopt,
                                         save_dir.path());

    EXPECT_EQ(image.id, mpt::lxd_snapcraft_image_id);
    EXPECT_EQ(image.original_release, mpt::snapcraft_release_info);
    EXPECT_EQ(image.release_date, mpt::snapcraft_image_version);
}

TEST_F(LXDImageVault, custom_image_downloads_and_creates_correct_upload)
{
    const std::string content{"This is a fake image!"};
    mpt::TrackingURLDownloader url_downloader{content};
    auto factory = mpt::MockProcessFactory::Inject();
    factory->register_callback([](mpt::MockProcess* process) {
        if (process->program().startsWith("tar"))
        {
            auto tar_args = process->arguments();

            EXPECT_EQ(tar_args.size(), 5);
            QFile output_file{tar_args[1]}, input_file{tar_args[3] + "/" + tar_args[4]};

            output_file.open(QIODevice::WriteOnly);
            input_file.open(QIODevice::ReadOnly);

            output_file.write(input_file.readAll());

            mp::ProcessState exit_state;
            exit_state.exit_code = 0;
            ON_CALL(*process, execute(_)).WillByDefault(Return(exit_state));
        }
    });

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _))
        .WillByDefault([](auto, auto request, auto outgoingData) {
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "POST" && url.contains("1.0/images"))
            {
                auto content_header = request.header(QNetworkRequest::ContentTypeHeader).toString();

                EXPECT_TRUE(content_header.contains("multipart/form-data"));
                EXPECT_TRUE(content_header.contains("boundary"));

                return new mpt::MockLocalSocketReply(mpt::image_upload_task_data);
            }
            else if (op == "GET" && url.contains("1.0/operations/dcce4fda-aab9-4117-89c1-9f42b8e3f4a8"))
            {
                return new mpt::MockLocalSocketReply(mpt::image_upload_task_complete_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVMImageVault image_vault{hosts,    &url_downloader,  mock_network_access_manager.get(),
                                    base_url, cache_dir.path(), mp::days{0}};

    const mp::Query query{"", "custom", false, "release", mp::Query::Type::Alias};
    auto image = image_vault.fetch_image(mp::FetchType::ImageOnly,
                                         query,
                                         stub_prepare,
                                         stub_monitor,
                                         false,
                                         std::nullopt,
                                         save_dir.path());

    EXPECT_EQ(image.id, mpt::lxd_custom_image_id);
    EXPECT_EQ(image.original_release, mpt::custom_release_info);
    EXPECT_EQ(image.release_date, mpt::custom_image_version);
}

TEST_F(LXDImageVault, fetch_image_unable_to_connect_logs_error_and_returns_blank_vmimage)
{
    const std::string exception_message{"Cannot connect to socket"};

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _))
        .WillByDefault([&exception_message](auto...) -> QNetworkReply* {
            throw mp::LocalSocketConnectionException(exception_message);
        });

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::warning), mpt::MockLogger::make_cstring_matcher(StrEq("lxd image vault")),
                    mpt::MockLogger::make_cstring_matcher(
                        StrEq(fmt::format("{} - returning blank image info", exception_message)))));

    auto image = image_vault.fetch_image(mp::FetchType::ImageOnly,
                                         default_query,
                                         stub_prepare,
                                         stub_monitor,
                                         false,
                                         std::nullopt,
                                         save_dir.path());

    EXPECT_TRUE(image.id.empty());
    EXPECT_TRUE(image.original_release.empty());
    EXPECT_TRUE(image.release_date.empty());
}

TEST_F(LXDImageVault, minimum_image_size_returns_expected_size)
{
    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _)).WillByDefault([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();

        if (op == "GET")
        {
            if (url.contains("1.0/images/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"))
            {
                return new mpt::MockLocalSocketReply(mpt::normal_image_info_data);
            }
        }

        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    const auto image_size =
        image_vault.minimum_image_size_for("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    EXPECT_EQ(image_size, mp::MemorySize{"10G"});
}

TEST_F(LXDImageVault, minimum_image_size_large_returns_expected_size)
{
    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _)).WillByDefault([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();

        if (op == "GET")
        {
            if (url.contains("1.0/images/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"))
            {
                return new mpt::MockLocalSocketReply(mpt::large_image_info_data);
            }
        }

        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    const auto image_size =
        image_vault.minimum_image_size_for("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    EXPECT_EQ(image_size, mp::MemorySize{"33345572108"});
}

TEST_F(LXDImageVault, minimum_image_size_throws_when_not_found)
{
    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _)).WillByDefault([](auto...) {
        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    const std::string id{"12345"};
    MP_EXPECT_THROW_THAT(
        image_vault.minimum_image_size_for(id), std::runtime_error,
        mpt::match_what(AllOf(HasSubstr(fmt::format("Cannot retrieve info for image with id \'{}\'", id)))));
}

TEST_F(LXDImageVault, http_based_image_downloads_and_creates_correct_upload)
{
    const std::string content{"This is a fake image!"};
    mpt::TrackingURLDownloader url_downloader{content};
    auto factory = mpt::MockProcessFactory::Inject();
    factory->register_callback([](mpt::MockProcess* process) {
        if (process->program().startsWith("tar"))
        {
            auto tar_args = process->arguments();

            EXPECT_EQ(tar_args.size(), 5);
            QFile output_file{tar_args[1]}, input_file{tar_args[3] + "/" + tar_args[4]};

            output_file.open(QIODevice::WriteOnly);
            input_file.open(QIODevice::ReadOnly);

            output_file.write(input_file.readAll());

            mp::ProcessState exit_state;
            exit_state.exit_code = 0;
            ON_CALL(*process, execute(_)).WillByDefault(Return(exit_state));
        }
    });

    ON_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillByDefault([](auto, auto request, auto outgoingData) {
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "POST" && url.contains("1.0/images"))
            {
                auto content_header = request.header(QNetworkRequest::ContentTypeHeader).toString();

                EXPECT_TRUE(content_header.contains("multipart/form-data"));
                EXPECT_TRUE(content_header.contains("boundary"));

                return new mpt::MockLocalSocketReply(mpt::image_upload_task_data);
            }
            else if (op == "GET" && url.contains("1.0/operations/dcce4fda-aab9-4117-89c1-9f42b8e3f4a8"))
            {
                return new mpt::MockLocalSocketReply(mpt::image_upload_task_complete_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVMImageVault image_vault{hosts,    &url_downloader,  mock_network_access_manager.get(),
                                    base_url, cache_dir.path(), mp::days{0}};

    auto current_time = QDateTime::currentDateTime();

    const std::string download_url{"http://www.foo.com/images/foo.img"};
    const mp::Query query{"", download_url, false, "", mp::Query::Type::HttpDownload};
    auto image = image_vault.fetch_image(mp::FetchType::ImageOnly,
                                         query,
                                         stub_prepare,
                                         stub_monitor,
                                         false,
                                         std::nullopt,
                                         save_dir.path());

    EXPECT_EQ(image.id, "bc5a973bd6f2bef30658fb51177cf5e506c1d60958a4c97813ee26416dc368da");

    auto image_time = QDateTime::fromString(QString::fromStdString(image.release_date), Qt::ISODateWithMs);

    EXPECT_TRUE(image_time >= current_time);

    EXPECT_EQ(url_downloader.downloaded_files.size(), 1);
    EXPECT_EQ(url_downloader.downloaded_urls.size(), 1);
    EXPECT_EQ(url_downloader.downloaded_urls.front().toStdString(), download_url);
}

TEST_F(LXDImageVault, file_based_fetch_copies_image_and_returns_expected_info)
{
    mpt::TempFile file;

    auto factory = mpt::MockProcessFactory::Inject();
    factory->register_callback([](mpt::MockProcess* process) {
        if (process->program().startsWith("tar"))
        {
            auto tar_args = process->arguments();

            EXPECT_EQ(tar_args.size(), 5);
            QFile output_file{tar_args[1]}, input_file{tar_args[3] + "/" + tar_args[4]};

            output_file.open(QIODevice::WriteOnly);
            input_file.open(QIODevice::ReadOnly);

            output_file.write(input_file.readAll());

            mp::ProcessState exit_state;
            exit_state.exit_code = 0;
            ON_CALL(*process, execute(_)).WillByDefault(Return(exit_state));
        }
    });

    ON_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillByDefault([](auto, auto request, auto outgoingData) {
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "POST" && url.contains("1.0/images"))
            {
                auto content_header = request.header(QNetworkRequest::ContentTypeHeader).toString();

                EXPECT_TRUE(content_header.contains("multipart/form-data"));
                EXPECT_TRUE(content_header.contains("boundary"));

                return new mpt::MockLocalSocketReply(mpt::image_upload_task_data);
            }
            else if (op == "GET" && url.contains("1.0/operations/dcce4fda-aab9-4117-89c1-9f42b8e3f4a8"))
            {
                return new mpt::MockLocalSocketReply(mpt::image_upload_task_complete_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    auto current_time = QDateTime::currentDateTime();
    const mp::Query query{"", file.url().toStdString(), false, "", mp::Query::Type::LocalFile};

    auto image = image_vault.fetch_image(mp::FetchType::ImageOnly,
                                         query,
                                         stub_prepare,
                                         stub_monitor,
                                         false,
                                         std::nullopt,
                                         save_dir.path());

    EXPECT_EQ(image.id, "bc5a973bd6f2bef30658fb51177cf5e506c1d60958a4c97813ee26416dc368da");

    auto image_time = QDateTime::fromString(QString::fromStdString(image.release_date), Qt::ISODateWithMs);

    EXPECT_TRUE(image_time >= current_time);
}

TEST_F(LXDImageVault, invalid_local_file_image_throws)
{
    ON_CALL(*mock_network_access_manager, createRequest(_, _, _)).WillByDefault([](auto...) {
        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    const std::string missing_file{"/foo"};
    const mp::Query query{"", fmt::format("file://{}", missing_file), false, "", mp::Query::Type::LocalFile};

    MP_EXPECT_THROW_THAT(image_vault.fetch_image(mp::FetchType::ImageOnly,
                                                 query,
                                                 stub_prepare,
                                                 stub_monitor,
                                                 false,
                                                 std::nullopt,
                                                 save_dir.path()),
                         std::runtime_error,
                         mpt::match_what(StrEq(fmt::format("Custom image `{}` does not exist.", missing_file))));
}

TEST_F(LXDImageVault, updateImagesThrowsOnMissingImage)
{
    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _)).WillByDefault([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();

        if (op == "GET" && url.contains("1.0/images"))
        {
            return new mpt::MockLocalSocketReply(mpt::image_info_update_source_info);
        }

        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    mp::LXDVMImageVault image_vault{hosts,    &stub_url_downloader, mock_network_access_manager.get(),
                                    base_url, cache_dir.path(),     mp::days{0}};

    EXPECT_CALL(host, info_for(_)).WillOnce(Return(std::nullopt));

    EXPECT_THROW(image_vault.update_images(mp::FetchType::ImageOnly, stub_prepare, stub_monitor),
                 mp::ImageNotFoundException);
}

TEST_F(LXDImageVault, lxdImageVaultCloneThrow)
{
    mp::LXDVMImageVault image_vault{hosts,
                                    &stub_url_downloader,
                                    mock_network_access_manager.get(),
                                    base_url,
                                    cache_dir.path(),
                                    mp::days{0}};

    MP_EXPECT_THROW_THAT(image_vault.clone("dummy_src_image_name", "dummy_dest_image_name"),
                         std::runtime_error,
                         mpt::match_what(StrEq("Clone methond is not supported in LXDVMImageVault yet.")));
}
