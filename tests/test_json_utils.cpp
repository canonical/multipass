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

#include <QDir>
#include <QFileDevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace mpt = multipass::test;
using namespace testing;

namespace
{
struct TestJsonUtils : public Test
{
    mpt::MockFileOps::GuardedMock guarded_mock_file_ops = mpt::MockFileOps::inject();
    mpt::MockFileOps& mock_file_ops = *guarded_mock_file_ops.first;
    inline static const QChar separator = QDir::separator();
    inline static const QString dir = QStringLiteral("a%1b%1c").arg(separator);
    inline static const QString file_name = QStringLiteral("asd.blag");
    inline static const QString file_path = QStringLiteral("%1%2%3").arg(dir, separator, file_name);
    inline static const char* json_text = R"({"a": [1,2,3]})";
    inline static const QJsonObject json = QJsonDocument::fromJson(json_text).object();
    template <typename T>
    inline static Matcher<T&> file_matcher = Property(&T::fileName, Eq(file_path));
};

TEST_F(TestJsonUtils, writesJsonTransactionally)
{
    auto json_matcher =
        ResultOf([](auto&& text) { return QJsonDocument::fromJson(std::forward<decltype(text)>(text), nullptr); },
                 Property(&QJsonDocument::object, Eq(json)));
    EXPECT_CALL(mock_file_ops, mkpath(Eq(dir), Eq("."))).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, open(file_matcher<QFileDevice>, _)).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, write(file_matcher<QFileDevice>, json_matcher)).WillOnce(Return(14));
    EXPECT_CALL(mock_file_ops, commit(file_matcher<QSaveFile>)).WillOnce(Return(true));
    EXPECT_NO_THROW(MP_JSONUTILS.write_json(json, file_path));
}

TEST_F(TestJsonUtils, writeJsonThrowsOnFailureToCreateDirectory)
{
    EXPECT_CALL(mock_file_ops, mkpath).WillOnce(Return(false));
    MP_EXPECT_THROW_THAT(MP_JSONUTILS.write_json(json, file_path),
                         std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("Could not create"), HasSubstr(dir.toStdString()))));
}

} // namespace
