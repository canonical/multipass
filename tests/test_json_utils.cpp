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

#include <multipass/json_utils.h>

#include <QFileDevice>
#include <QString>

#include <stdexcept>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
struct TestJsonUtils : public Test
{
    mpt::MockFileOps::GuardedMock guarded_mock_file_ops = mpt::MockFileOps::inject();
    mpt::MockFileOps& mock_file_ops = *guarded_mock_file_ops.first;
    inline static const QString dir = QStringLiteral("a/b/c");
    inline static const QString file_name = QStringLiteral("asd.blag");
    inline static const QString file_path = QStringLiteral("%1/%2").arg(dir, file_name);
    inline static const QString lockfile_path = file_path + ".lock";
    inline static const char* json_text = R"({"a": [1,2,3]})";
    inline static const boost::json::value json = boost::json::parse(json_text);
    inline static const auto expected_stale_lock_time = std::chrono::seconds{10};
    inline static const auto expected_lock_timeout = std::chrono::seconds{10};
    inline static const auto expected_retry_attempts = 10;

    template <typename T>
    static Matcher<T&>
    file_matcher() // not using static template var to workaround bad init order in AppleClang
    {
        static const auto ret = Property(&T::fileName, Eq(file_path));
        return ret;
    }

    template <typename T>
    static Matcher<T&>
    lockfile_matcher() // not using static template var to workaround bad init order in AppleClang
    {
        static const auto ret = Property(&T::fileName, Eq(lockfile_path));
        return ret;
    }
};

TEST_F(TestJsonUtils, writesJsonTransactionally)
{
    auto json_matcher =
        ResultOf([](auto&& text) { return boost::json::parse(std::string_view(text)); }, Eq(json));
    EXPECT_CALL(mock_file_ops,
                setStaleLockTime(lockfile_matcher<QLockFile>(), Eq(expected_stale_lock_time)));
    EXPECT_CALL(mock_file_ops, tryLock(lockfile_matcher<QLockFile>(), Eq(expected_lock_timeout)))
        .WillOnce(Return(true));

    EXPECT_CALL(mock_file_ops, mkpath(Eq(dir), Eq("."))).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, open(file_matcher<QFileDevice>(), _)).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, write(file_matcher<QFileDevice>(), json_matcher))
        .WillOnce(Return(14));
    EXPECT_CALL(mock_file_ops, commit(file_matcher<QSaveFile>())).WillOnce(Return(true));
    EXPECT_NO_THROW(MP_JSONUTILS.write_json(json, file_path));
}

TEST_F(TestJsonUtils, writesJsonTransactionallyEventually)
{
    auto json_matcher =
        ResultOf([](auto&& text) { return boost::json::parse(std::string_view(text)); }, Eq(json));
    EXPECT_CALL(mock_file_ops,
                setStaleLockTime(lockfile_matcher<QLockFile>(), Eq(expected_stale_lock_time)));
    EXPECT_CALL(mock_file_ops, tryLock(lockfile_matcher<QLockFile>(), Eq(expected_lock_timeout)))
        .WillOnce(Return(true));

    EXPECT_CALL(mock_file_ops, mkpath(Eq(dir), Eq("."))).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, open(file_matcher<QFileDevice>(), _))
        .Times(expected_retry_attempts)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(mock_file_ops, write(file_matcher<QFileDevice>(), json_matcher))
        .Times(expected_retry_attempts)
        .WillRepeatedly(Return(14));

    auto commit_called_times = 0;
    EXPECT_CALL(mock_file_ops, commit(file_matcher<QSaveFile>()))
        .Times(expected_retry_attempts)
        .WillRepeatedly(
            InvokeWithoutArgs([&]() { return ++commit_called_times == expected_retry_attempts; }));

    EXPECT_NO_THROW(MP_JSONUTILS.write_json(json, file_path));
}

TEST_F(TestJsonUtils, writeJsonThrowsOnFailureToCreateDirectory)
{
    EXPECT_CALL(mock_file_ops, mkpath).WillOnce(Return(false));
    MP_EXPECT_THROW_THAT(
        MP_JSONUTILS.write_json(json, file_path),
        std::runtime_error,
        mpt::match_what(AllOf(HasSubstr("Could not create"), HasSubstr(dir.toStdString()))));
}

TEST_F(TestJsonUtils, writeJsonThrowsOnFailureToOpenFile)
{
    EXPECT_CALL(mock_file_ops,
                setStaleLockTime(lockfile_matcher<QLockFile>(), Eq(expected_stale_lock_time)));
    EXPECT_CALL(mock_file_ops, tryLock(lockfile_matcher<QLockFile>(), Eq(expected_lock_timeout)))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, mkpath).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, open(_, _)).WillOnce(Return(false));
    MP_EXPECT_THROW_THAT(
        MP_JSONUTILS.write_json(json, file_path),
        std::runtime_error,
        mpt::match_what(AllOf(HasSubstr("Could not open"), HasSubstr(file_path.toStdString()))));
}

TEST_F(TestJsonUtils, writeJsonThrowsOnFailureToWriteFile)
{
    EXPECT_CALL(mock_file_ops,
                setStaleLockTime(lockfile_matcher<QLockFile>(), Eq(expected_stale_lock_time)));
    EXPECT_CALL(mock_file_ops, tryLock(lockfile_matcher<QLockFile>(), Eq(expected_lock_timeout)))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, mkpath).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, open(_, _)).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, write(_, _)).WillOnce(Return(-1));
    MP_EXPECT_THROW_THAT(
        MP_JSONUTILS.write_json(json, file_path),
        std::runtime_error,
        mpt::match_what(AllOf(HasSubstr("Could not write"), HasSubstr(file_path.toStdString()))));
}

TEST_F(TestJsonUtils, writeJsonThrowsOnFailureToAcquireLock)
{
    EXPECT_CALL(mock_file_ops,
                setStaleLockTime(lockfile_matcher<QLockFile>(), Eq(expected_stale_lock_time)));
    EXPECT_CALL(mock_file_ops, tryLock(lockfile_matcher<QLockFile>(), Eq(expected_lock_timeout)))
        .WillOnce(Return(false));
    EXPECT_CALL(mock_file_ops, mkpath).WillOnce(Return(true));

    MP_EXPECT_THROW_THAT(MP_JSONUTILS.write_json(json, file_path),
                         std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("Could not acquire lock"),
                                               HasSubstr(file_path.toStdString()))));
}

TEST_F(TestJsonUtils, writeJsonThrowsOnFailureToCommit)
{
    EXPECT_CALL(mock_file_ops,
                setStaleLockTime(lockfile_matcher<QLockFile>(), Eq(expected_stale_lock_time)));
    EXPECT_CALL(mock_file_ops, tryLock(lockfile_matcher<QLockFile>(), Eq(expected_lock_timeout)))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, mkpath).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, open(_, _))
        .Times(expected_retry_attempts)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(mock_file_ops, write(_, _))
        .Times(expected_retry_attempts)
        .WillRepeatedly(Return(1234));
    EXPECT_CALL(mock_file_ops, commit).Times(expected_retry_attempts).WillRepeatedly(Return(false));
    MP_EXPECT_THROW_THAT(
        MP_JSONUTILS.write_json(json, file_path),
        std::runtime_error,
        mpt::match_what(AllOf(HasSubstr("Could not commit"), HasSubstr(file_path.toStdString()))));
}

class JsonPrettyPrintTest : public TestWithParam<std::string>
{
};

TEST_P(JsonPrettyPrintTest, prettyPrintsCorrectly)
{
    std::string expected = GetParam();
    boost::json::value json = boost::json::parse(expected);
    EXPECT_EQ(mp::pretty_print(json, {.trailing_newline = false}), expected);
}

INSTANTIATE_TEST_SUITE_P(TestJsonUtils,
                         JsonPrettyPrintTest,
                         Values(
                             // Null
                             "null",
                             // Booleans
                             "true",
                             "false",
                             // Numbers
                             "12345",
                             "1.234",
                             // Strings
                             R"("hello there")",
                             // Arrays
                             "[\n]",
                             R"([
    123,
    "hello there"
])",
                             // Objects
                             "{\n}",
                             R"({
    "foo": "bar",
    "one": 1,
    "yes": true
})",
                             // Nested
                             R"({
    "foo": {
        "bar": [
            1,
            2
        ],
        "baz": "quux"
    }
})"));
} // namespace
