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
#include "mock_logger.h"
#include "mock_network.h"
#include "temp_dir.h"

#include <multipass/exceptions/aborted_download_exception.h>
#include <multipass/exceptions/download_exception.h>

#include <QTimer>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;

using namespace std::chrono_literals;
using namespace testing;

namespace
{
struct URLDownloader : public Test
{
    URLDownloader()
    {
        mock_network_access_manager = std::make_unique<NiceMock<mpt::MockQNetworkAccessManager>>();

        EXPECT_CALL(*mock_network_manager_factory, make_network_manager(_)).WillOnce([this](auto...) {
            return std::move(mock_network_access_manager);
        });
    };

    mpt::TempDir cache_dir;
    mpt::MockNetworkManagerFactory::GuardedMock attr{mpt::MockNetworkManagerFactory::inject()};
    mpt::MockNetworkManagerFactory* mock_network_manager_factory{attr.first};
    std::unique_ptr<NiceMock<mpt::MockQNetworkAccessManager>> mock_network_access_manager;
    const QUrl fake_url{"https://a.fake.url"};
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
};
} // namespace

TEST_F(URLDownloader, simpleDownloadReturnsExpectedData)
{
    const QByteArray test_data{"The answer to everything is 42."};
    mpt::MockQNetworkReply* mock_reply = new mpt::MockQNetworkReply();

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _)).WillOnce(Return(mock_reply));

    EXPECT_CALL(*mock_reply, readData(_, _))
        .WillOnce([&test_data](char* data, auto) {
            auto data_size{test_data.size()};
            memcpy(data, test_data.constData(), data_size);

            return data_size;
        })
        .WillOnce(Return(0));

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    logger_scope.mock_logger->expect_log(mpl::Level::trace,
                                         fmt::format("Found {} in cache: false", fake_url.toString()));

    mp::URLDownloader downloader(cache_dir.path(), 1s);

    QTimer::singleShot(0, [&mock_reply] { mock_reply->finished(); });

    auto downloaded_data = downloader.download(fake_url);

    EXPECT_EQ(downloaded_data, test_data);
}

TEST_F(URLDownloader, simpleDownloadNetworkTimeoutTriesCache)
{
    mpt::MockQNetworkReply* mock_reply_abort = new mpt::MockQNetworkReply();
    mpt::MockQNetworkReply* mock_reply_cache = new mpt::MockQNetworkReply();

    const QByteArray test_data{"The answer to everything is 42."};

    EXPECT_CALL(*mock_reply_abort, abort()).WillOnce([&mock_reply_abort] { mock_reply_abort->abort_operation(); });

    EXPECT_CALL(*mock_reply_cache, readData(_, _))
        .WillOnce([&test_data](char* data, auto) {
            auto data_size{test_data.size()};
            memcpy(data, test_data.constData(), data_size);

            return data_size;
        })
        .WillOnce(Return(0));

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillOnce(Return(mock_reply_abort))
        .WillOnce([&mock_reply_cache](auto...) {
            QTimer::singleShot(0, [&mock_reply_cache] {
                mock_reply_cache->set_attribute(QNetworkRequest::SourceIsFromCacheAttribute, QVariant(true));
                mock_reply_cache->readyRead();
                mock_reply_cache->finished();
            });
            return mock_reply_cache;
        });

    logger_scope.mock_logger->screen_logs(mpl::Level::error);

    // Add expectations for the new debug log and the warning log
    EXPECT_CALL(*logger_scope.mock_logger,
                log(mpl::Level::debug, _, Property(&multipass::logging::CString::c_str, StartsWith("Qt error"))));

    logger_scope.mock_logger->expect_log(
        mpl::Level::warning,
        fmt::format("Failed to get {}: Operation canceled - trying cache.", fake_url.toString()));
    logger_scope.mock_logger->expect_log(mpl::Level::trace,
                                         fmt::format("Found {} in cache: true", fake_url.toString()));

    mp::URLDownloader downloader(cache_dir.path(), 10ms);

    auto downloaded_data = downloader.download(fake_url);

    EXPECT_EQ(downloaded_data, test_data);
}

TEST_F(URLDownloader, simpleDownloadProxyAuthenticationRequiredAborts)
{
    mpt::MockQNetworkReply* mock_reply = new mpt::MockQNetworkReply();

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _)).WillOnce([&mock_reply](auto...) {
        QTimer::singleShot(0, [&mock_reply] {
            mock_reply->set_error(QNetworkReply::ProxyAuthenticationRequiredError, "Proxy authorization required");
            mock_reply->finished();
        });
        return mock_reply;
    });

    mp::URLDownloader downloader(cache_dir.path(), 10ms);

    MP_EXPECT_THROW_THAT(downloader.download(fake_url), mp::AbortedDownloadException,
                         mpt::match_what(StrEq("Proxy authorization required")));
}

TEST_F(URLDownloader, simpleDownloadAbortAllStopsDownload)
{
    mpt::MockQNetworkReply* mock_reply = new mpt::MockQNetworkReply();

    EXPECT_CALL(*mock_reply, abort()).WillOnce([&mock_reply] { mock_reply->abort_operation(); });

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _)).WillOnce([&mock_reply](auto...) {
        QTimer::singleShot(0, [&mock_reply] { mock_reply->readyRead(); });
        return mock_reply;
    });

    mp::URLDownloader downloader(cache_dir.path(), 10ms);

    downloader.abort_all_downloads();

    MP_EXPECT_THROW_THAT(downloader.download(fake_url), mp::AbortedDownloadException,
                         mpt::match_what(StrEq("Operation canceled")));
}

TEST_F(URLDownloader, fileDownloadNoErrorHasExpectedResults)
{
    mpt::MockQNetworkReply* mock_reply = new mpt::MockQNetworkReply();
    const QByteArray test_data{"This is some data to put in a file when downloaded."};
    const int download_type{-1};

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _)).WillOnce([&mock_reply, &test_data](auto...) {
        QTimer::singleShot(0, [&mock_reply, &test_data] {
            mock_reply->downloadProgress(test_data.size(), test_data.size());
            mock_reply->readyRead();
            mock_reply->finished();
        });
        return mock_reply;
    });

    EXPECT_CALL(*mock_reply, readData(_, _))
        .WillOnce([&test_data](char* data, auto) {
            auto data_size{test_data.size()};
            memcpy(data, test_data.constData(), data_size);

            return data_size;
        })
        .WillRepeatedly(Return(0));

    bool progress_called{false};
    auto progress_monitor = [download_type, &progress_called](int type, int progress) {
        EXPECT_EQ(type, download_type);
        EXPECT_EQ(progress, 100);
        progress_called = true;

        return true;
    };

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    logger_scope.mock_logger->expect_log(mpl::Level::trace,
                                         fmt::format("Found {} in cache: false", fake_url.toString()));

    mp::URLDownloader downloader(cache_dir.path(), 10ms);

    mpt::TempDir file_dir;
    QString download_file{file_dir.path() + "/foo.txt"};

    downloader.download_to(fake_url, download_file, test_data.size(), download_type, progress_monitor);

    EXPECT_TRUE(progress_called);

    QFile test_file{download_file};
    ASSERT_TRUE(test_file.exists());

    test_file.open(QIODevice::ReadOnly);
    auto file_data = test_file.readAll();
    EXPECT_EQ(file_data, test_data);
}

TEST_F(URLDownloader, fileDownloadErrorTriesCache)
{
    mpt::MockQNetworkReply* mock_reply_abort = new mpt::MockQNetworkReply();
    mpt::MockQNetworkReply* mock_reply_cache = new mpt::MockQNetworkReply();
    const QByteArray test_data{"This is some data to put in a file when downloaded."};

    EXPECT_CALL(*mock_reply_abort, abort()).WillOnce([&mock_reply_abort] { mock_reply_abort->abort_operation(); });

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillOnce(Return(mock_reply_abort))
        .WillOnce([&mock_reply_cache, &test_data](auto...) {
            QTimer::singleShot(0, [&mock_reply_cache, &test_data] {
                mock_reply_cache->set_attribute(QNetworkRequest::SourceIsFromCacheAttribute, QVariant(true));
                mock_reply_cache->downloadProgress(test_data.size(), test_data.size());
                mock_reply_cache->readyRead();
                mock_reply_cache->finished();
            });
            return mock_reply_cache;
        });

    EXPECT_CALL(*mock_reply_cache, readData(_, _))
        .WillOnce([&test_data](char* data, auto) {
            auto data_size{test_data.size()};
            memcpy(data, test_data.constData(), data_size);

            return data_size;
        })
        .WillRepeatedly(Return(0));

    auto progress_monitor = [](auto...) { return true; };

    logger_scope.mock_logger->screen_logs(mpl::Level::error);
    logger_scope.mock_logger->expect_log(mpl::Level::trace,
                                         fmt::format("Found {} in cache: true", fake_url.toString()));
    logger_scope.mock_logger->expect_log(
        mpl::Level::warning,
        fmt::format("Failed to get {}: Operation canceled - trying cache.", fake_url.toString()));

    EXPECT_CALL(*logger_scope.mock_logger,
                log(mpl::Level::debug, _, Property(&multipass::logging::CString::c_str, StartsWith("Qt error"))));

    mp::URLDownloader downloader(cache_dir.path(), 10ms);

    mpt::TempDir file_dir;
    QString download_file{file_dir.path() + "/foo.txt"};

    downloader.download_to(fake_url, download_file, test_data.size(), -1, progress_monitor);

    QFile test_file{download_file};
    ASSERT_TRUE(test_file.exists());

    test_file.open(QIODevice::ReadOnly);
    auto file_data = test_file.readAll();
    EXPECT_EQ(file_data, test_data);
}

TEST_F(URLDownloader, fileDownloadMonitorReturnFalseAborts)
{
    mpt::MockQNetworkReply* mock_reply = new mpt::MockQNetworkReply();

    EXPECT_CALL(*mock_reply, abort()).WillOnce([&mock_reply] { mock_reply->abort_operation(); });

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _)).WillOnce([&mock_reply](auto...) {
        QTimer::singleShot(0, [&mock_reply] { mock_reply->downloadProgress(1000, 1000); });
        return mock_reply;
    });

    auto progress_monitor = [](auto...) { return false; };

    mp::URLDownloader downloader(cache_dir.path(), 10ms);

    mpt::TempDir file_dir;
    QString download_file{file_dir.path() + "/foo.txt"};

    MP_EXPECT_THROW_THAT(downloader.download_to(fake_url, download_file, -1, -1, progress_monitor),
                         mp::AbortedDownloadException, mpt::match_what(StrEq("Operation canceled")));

    EXPECT_FALSE(QFile::exists(download_file));
}

TEST_F(URLDownloader, fileDownloadZeroBytesReceivedDoesNotCallMonitor)
{
    mpt::MockQNetworkReply* mock_reply = new mpt::MockQNetworkReply();

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _)).WillOnce([&mock_reply](auto...) {
        QTimer::singleShot(0, [&mock_reply] {
            mock_reply->downloadProgress(0, 1000);
            mock_reply->finished();
        });
        return mock_reply;
    });

    EXPECT_CALL(*mock_reply, readData(_, _)).WillRepeatedly(Return(0));

    bool progress_called{false};
    auto progress_monitor = [&progress_called](auto...) {
        progress_called = true;

        return true;
    };

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    logger_scope.mock_logger->expect_log(mpl::Level::trace,
                                         fmt::format("Found {} in cache: false", fake_url.toString()));

    mp::URLDownloader downloader(cache_dir.path(), 10ms);

    mpt::TempDir file_dir;
    QString download_file{file_dir.path() + "/foo.txt"};

    downloader.download_to(fake_url, download_file, -1, -1, progress_monitor);

    EXPECT_FALSE(progress_called);
}

TEST_F(URLDownloader, fileDownloadAbortAllStopDownload)
{
    mpt::MockQNetworkReply* mock_reply = new mpt::MockQNetworkReply();

    EXPECT_CALL(*mock_reply, abort()).WillOnce([&mock_reply] { mock_reply->abort_operation(); });

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _)).WillOnce([&mock_reply](auto...) {
        QTimer::singleShot(0, [&mock_reply] {
            mock_reply->readyRead();
            mock_reply->finished();
        });
        return mock_reply;
    });

    auto progress_monitor = [](auto...) { return true; };

    mp::URLDownloader downloader(cache_dir.path(), 10ms);
    downloader.abort_all_downloads();

    mpt::TempDir file_dir;
    QString download_file{file_dir.path() + "/foo.txt"};

    MP_EXPECT_THROW_THAT(downloader.download_to(fake_url, download_file, -1, -1, progress_monitor),
                         mp::AbortedDownloadException, mpt::match_what(StrEq("Operation canceled")));
}

TEST_F(URLDownloader, fileDownloadUnknownBytesSetToQueriedSize)
{
    mpt::MockQNetworkReply* mock_reply = new mpt::MockQNetworkReply();

    const QByteArray test_data{"This is some data to put in a file when downloaded."};

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _)).WillOnce([&mock_reply, &test_data](auto...) {
        QTimer::singleShot(0, [&mock_reply, &test_data] {
            mock_reply->downloadProgress(test_data.size(), -1);
            mock_reply->readyRead();
            mock_reply->finished();
        });
        return mock_reply;
    });

    EXPECT_CALL(*mock_reply, readData(_, _))
        .WillOnce([&test_data](char* data, auto) {
            auto data_size{test_data.size()};
            memcpy(data, test_data.constData(), data_size);

            return data_size;
        })
        .WillRepeatedly(Return(0));

    auto progress_monitor = [](auto, int progress) {
        EXPECT_EQ(progress, 100);

        return true;
    };

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    logger_scope.mock_logger->expect_log(mpl::Level::trace,
                                         fmt::format("Found {} in cache: false", fake_url.toString()));

    mp::URLDownloader downloader(cache_dir.path(), 10ms);

    mpt::TempDir file_dir;
    QString download_file{file_dir.path() + "/foo.txt"};

    downloader.download_to(fake_url, download_file, test_data.size(), -1, progress_monitor);
}

TEST_F(URLDownloader, fileDownloadTimeoutDoesNotWriteFile)
{
    mpt::MockQNetworkReply* mock_reply_abort1 = new mpt::MockQNetworkReply();
    mpt::MockQNetworkReply* mock_reply_abort2 = new mpt::MockQNetworkReply();
    bool ready_read_fired{false};

    EXPECT_CALL(*mock_reply_abort1, abort()).WillOnce([&mock_reply_abort1, &ready_read_fired] {
        mock_reply_abort1->abort_operation();

        // Fake data becoming ready after the network timeout
        mock_reply_abort1->readyRead();
        ready_read_fired = true;
    });
    EXPECT_CALL(*mock_reply_abort2, abort()).WillOnce([&mock_reply_abort2] { mock_reply_abort2->abort_operation(); });

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillOnce(Return(mock_reply_abort1))
        .WillOnce(Return(mock_reply_abort2));

    auto progress_monitor = [](auto...) { return true; };

    logger_scope.mock_logger->screen_logs(mpl::Level::error);

    // Expect warning log for the first failed attempt
    EXPECT_CALL(*logger_scope.mock_logger,
                log(mpl::Level::warning,
                    _,
                    Property(&multipass::logging::CString::c_str,
                             HasSubstr(fmt::format("Failed to get {}: Operation canceled - trying cache.",
                                                   fake_url.toString())))));

    // Expect error log for the second failed attempt
    EXPECT_CALL(*logger_scope.mock_logger,
                log(mpl::Level::error,
                    _,
                    Property(&multipass::logging::CString::c_str,
                             HasSubstr(fmt::format("Failed to get {}: Operation canceled", fake_url.toString())))));

    // Expect two debug logs for each failure
    EXPECT_CALL(*logger_scope.mock_logger,
                log(mpl::Level::debug, _, Property(&multipass::logging::CString::c_str, StartsWith("Qt error"))))
        .Times(2);

    mp::URLDownloader downloader(cache_dir.path(), 10ms);

    mpt::TempDir file_dir;
    QString download_file{file_dir.path() + "/foo.txt"};

    EXPECT_THROW(downloader.download_to(fake_url, download_file, -1, -1, progress_monitor), mp::DownloadException);

    EXPECT_TRUE(ready_read_fired);
    EXPECT_FALSE(QFile::exists(download_file));
}

TEST_F(URLDownloader, fileDownloadWriteFailsLogsErrorAndThrows)
{
    const QByteArray test_data{"This is some data to put in a file when downloaded."};
    mpt::MockQNetworkReply* mock_reply = new mpt::MockQNetworkReply();

    EXPECT_CALL(*mock_reply, abort()).WillOnce([&mock_reply] { mock_reply->abort_operation(); });

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _)).WillOnce([&mock_reply](auto...) {
        QTimer::singleShot(0, [&mock_reply] {
            mock_reply->readyRead();
            mock_reply->finished();
        });
        return mock_reply;
    });

    EXPECT_CALL(*mock_reply, readData(_, _))
        .WillOnce([&test_data](char* data, auto) {
            auto data_size{test_data.size()};
            memcpy(data, test_data.constData(), data_size);

            return data_size;
        })
        .WillRepeatedly(Return(0));

    auto progress_monitor = [](auto...) { return true; };

    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, write(_, _)).WillOnce(Return(-1));

    logger_scope.mock_logger->screen_logs(mpl::Level::error);
    logger_scope.mock_logger->expect_log(mpl::Level::error, "error writing image:");

    mp::URLDownloader downloader(cache_dir.path(), 10ms);

    mpt::TempDir file_dir;
    QString download_file{file_dir.path() + "/foo.txt"};

    EXPECT_THROW(downloader.download_to(fake_url, download_file, -1, -1, progress_monitor),
                 mp::AbortedDownloadException);
}

TEST_F(URLDownloader, lastModifiedHeaderReturnsExpectedData)
{
    const QDateTime date_time{QDateTime::currentDateTimeUtc()};
    mpt::MockQNetworkReply* mock_reply = new mpt::MockQNetworkReply();

    mock_reply->set_header(QNetworkRequest::LastModifiedHeader, QVariant(date_time));

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _)).WillOnce(Return(mock_reply));

    mp::URLDownloader downloader(cache_dir.path(), 1s);

    QTimer::singleShot(0, [&mock_reply] { mock_reply->finished(); });

    auto last_modified = downloader.last_modified(fake_url);

    EXPECT_EQ(date_time, last_modified);
}

TEST_F(URLDownloader, lastModifiedHeaderTimeoutThrows)
{
    mpt::MockQNetworkReply* mock_reply = new mpt::MockQNetworkReply();

    EXPECT_CALL(*mock_reply, abort()).WillOnce([&mock_reply] { mock_reply->abort_operation(); });

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _)).WillOnce(Return(mock_reply));

    logger_scope.mock_logger->screen_logs(mpl::Level::error);
    logger_scope.mock_logger->expect_log(
        mpl::Level::error,
        fmt::format("Cannot retrieve headers for {}: Operation canceled", fake_url.toString()));

    // Expectation for debug log
    EXPECT_CALL(*logger_scope.mock_logger,
                log(mpl::Level::debug, _, Property(&multipass::logging::CString::c_str, StartsWith("Qt error"))));
    mp::URLDownloader downloader(cache_dir.path(), 10ms);

    EXPECT_THROW(downloader.last_modified(fake_url), mp::DownloadException);
}

TEST_F(URLDownloader, lastModifiedHeaderErrorThrows)
{
    const QString error_msg{"Host not found"};
    mpt::MockQNetworkReply* mock_reply = new mpt::MockQNetworkReply();

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _)).WillOnce([&mock_reply, &error_msg](auto...) {
        QTimer::singleShot(0, [&mock_reply, &error_msg] {
            mock_reply->set_error(QNetworkReply::HostNotFoundError, error_msg);
            mock_reply->finished();
        });
        return mock_reply;
    });

    logger_scope.mock_logger->screen_logs(mpl::Level::error);
    logger_scope.mock_logger->expect_log(
        mpl::Level::error,
        fmt::format("Cannot retrieve headers for {}: {}", fake_url.toString(), error_msg));

    // Expectation for debug log
    EXPECT_CALL(*logger_scope.mock_logger,
                log(mpl::Level::debug, _, Property(&multipass::logging::CString::c_str, StartsWith("Qt error"))));

    mp::URLDownloader downloader(cache_dir.path(), 1s);

    EXPECT_THROW(downloader.last_modified(fake_url), mp::DownloadException);
}

struct URLConverter : public URLDownloader
{
    template <class Callable>
    decltype(auto) test_function_converts_url(Callable&& function)
    {
        mpt::MockQNetworkReply* mock_reply = new mpt::MockQNetworkReply();
        QTimer::singleShot(0, [&mock_reply] { mock_reply->finished(); });

        ON_CALL(*mock_network_access_manager, createRequest(_, _, _)).WillByDefault(Return(mock_reply));
        ON_CALL(*mock_reply, readData(_, _)).WillByDefault(Return(0));

        EXPECT_CALL(*mock_network_access_manager, createRequest(_, Property(&QNetworkRequest::url, Eq(https_url)), _))
            .WillRepeatedly(Return(mock_reply));

        return function(http_url);
    }

    const QUrl http_url{"http://a.url.net"};
    const QUrl https_url{"https://a.url.net"};
};

TEST_F(URLConverter, downloadHttpUrlBecomesHttps)
{
    test_function_converts_url([this](const QUrl& url) {
        mp::URLDownloader downloader(cache_dir.path(), 1s);
        return downloader.download(url);
    });
}

TEST_F(URLConverter, lastModifiedHttpUrlBecomesHttps)
{
    test_function_converts_url([this](const QUrl& url) {
        mp::URLDownloader downloader(cache_dir.path(), 1s);
        downloader.last_modified(url);
    });
}
