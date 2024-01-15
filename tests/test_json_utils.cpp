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
};

TEST_F(TestJsonUtils, writeJsonCreatesDirectory)
{
    const auto separator = QDir::separator();
    auto dir = QString{"a%1b%1c"}.arg(separator);
    auto file_name = "asd.blag";
    auto file_path = QString{"%1%2%3"}.arg(dir, separator, file_name);
    QJsonObject json{};

    EXPECT_CALL(mock_file_ops, mkpath(Eq(dir), Eq("."))).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, open(A<QFileDevice&>(), _)).WillOnce(Throw(std::runtime_error{"intentional"}));
    EXPECT_ANY_THROW(MP_JSONUTILS.write_json(json, file_path));
}

} // namespace
