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

#include <src/platform/update/new_release_monitor.h>

#include <QEventLoop>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>

#include <chrono>

namespace mp = multipass;

using namespace testing;

namespace
{
const auto timeout = std::chrono::milliseconds{250};
const QString json_template = R"END({
  "prefix_field": "foo",
  "release_url": "%1",
  "infix_field": "bar",
  "version": "%2",
  "suffix_field": "baz"
}
)END";

class StubUpdateJson
{
public:
    StubUpdateJson(QString version, QString url)
    {
        json_file.open();
        json_file.write(json_template.arg(url).arg(version).toUtf8());
        json_file.close();
    }

    QByteArray url() const
    {
        return QUrl::fromLocalFile(json_file.fileName()).toEncoded();
    }

private:
    QTemporaryFile json_file;
};

auto check_for_new_release(QString currentVersion, QString newVersion, QString newVersionUrl = "")
{
    QEventLoop e;
    StubUpdateJson json(newVersion, newVersionUrl);

    mp::NewReleaseMonitor monitor(currentVersion, std::chrono::hours(1), json.url());
    QTimer::singleShot(timeout, &e, &QEventLoop::quit); // TODO replace with a thread sync mechanism (e.g. condition)
    e.exec();

    return monitor.get_new_release();
}

} // namespace

TEST(NewReleaseMonitor, checks_new_release)
{
    auto new_release = check_for_new_release("0.1.0", "0.2.0", "https://something_unique.com");

    ASSERT_TRUE(new_release);
    EXPECT_EQ("0.2.0", new_release->version.toStdString());
    EXPECT_EQ("https://something_unique.com", new_release->url.toString().toStdString());
}

TEST(NewReleaseMonitor, checks_new_release_when_nothing_new)
{
    auto new_release = check_for_new_release("0.2.1", "0.2.1");

    EXPECT_FALSE(new_release);
}

TEST(NewReleaseMonitor, checks_new_release_when_newer_than_available)
{
    auto new_release = check_for_new_release("0.3.0", "0.2.0");

    EXPECT_FALSE(new_release);
}

TEST(NewReleaseMonitor, checks_new_release_when_download_fails)
{
    QEventLoop e;
    StubUpdateJson json("0.2.0", "https://something_unique.com");

    mp::NewReleaseMonitor monitor("0.1.0", std::chrono::hours(1), "file:///does/not/exist");
    QTimer::singleShot(timeout, &e, &QEventLoop::quit);
    e.exec();

    auto new_release = monitor.get_new_release();
    EXPECT_FALSE(new_release);
}

// Just double-checking that the SemVer library applies the ordering we expect for prerelease strings.
TEST(NewReleaseMonitor, dev_prerelease_ordering_correct)
{
    auto new_release = check_for_new_release("0.6.0", "0.6.0-dev.238+g5c642f4");

    EXPECT_FALSE(new_release);
}

TEST(NewReleaseMonitor, dev_prerelease_ordering_correct1)
{
    auto new_release = check_for_new_release("0.6.0-dev.238+g5c642f4", "0.6.0");

    ASSERT_TRUE(new_release);
    EXPECT_EQ("0.6.0", new_release->version.toStdString());
}

TEST(NewReleaseMonitor, rc_prerelease_ordering_correct)
{
    auto new_release = check_for_new_release("0.6.0", "0.6.0-rc.238+g5c642f4");

    EXPECT_FALSE(new_release);
}

TEST(NewReleaseMonitor, rc_prerelease_ordering_correct1)
{
    auto new_release = check_for_new_release("0.6.0-rc.238+g5c642f4", "0.6.0");

    EXPECT_TRUE(new_release);
}

TEST(NewReleaseMonitor, dev_rc_release_ordering_correct)
{
    auto new_release = check_for_new_release("0.6.0-rc.238+g3245235.win", "0.6.0-dev.238+g5c642f4");

    EXPECT_FALSE(new_release);
}

TEST(NewReleaseMonitor, dev_rc_release_ordering_correct1)
{
    auto new_release = check_for_new_release("0.6.0-dev.238+g3245235.win", "0.6.0-rc.238+g5c642f4");

    EXPECT_TRUE(new_release);
}
