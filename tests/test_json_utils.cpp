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
#include <QJsonDocument>
#include <QJsonObject>
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
    inline static const char* json_text = R"({"a": [1,2,3]})";
    inline static const QJsonObject json = QJsonDocument::fromJson(json_text).object();

    template <typename T>
    static Matcher<T&> file_matcher() // not using static template var to workaround bad init order in AppleClang
    {
        static const auto ret = Property(&T::fileName, Eq(file_path));
        return ret;
    }
};

TEST_F(TestJsonUtils, writesJsonTransactionally)
{
    auto json_matcher =
        ResultOf([](auto&& text) { return QJsonDocument::fromJson(std::forward<decltype(text)>(text), nullptr); },
                 Property(&QJsonDocument::object, Eq(json)));
    EXPECT_CALL(mock_file_ops, mkpath(Eq(dir), Eq("."))).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, open(file_matcher<QFileDevice>(), _)).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, write(file_matcher<QFileDevice>(), json_matcher)).WillOnce(Return(14));
    EXPECT_CALL(mock_file_ops, commit(file_matcher<QSaveFile>())).WillOnce(Return(true));
    EXPECT_NO_THROW(MP_JSONUTILS.write_json(json, file_path));
}

TEST_F(TestJsonUtils, writeJsonThrowsOnFailureToCreateDirectory)
{
    EXPECT_CALL(mock_file_ops, mkpath).WillOnce(Return(false));
    MP_EXPECT_THROW_THAT(MP_JSONUTILS.write_json(json, file_path),
                         std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("Could not create"), HasSubstr(dir.toStdString()))));
}

TEST_F(TestJsonUtils, writeJsonThrowsOnFailureToOpenFile)
{
    EXPECT_CALL(mock_file_ops, mkpath).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, open(_, _)).WillOnce(Return(false));
    MP_EXPECT_THROW_THAT(MP_JSONUTILS.write_json(json, file_path),
                         std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("Could not open"), HasSubstr(file_path.toStdString()))));
}

TEST_F(TestJsonUtils, writeJsonThrowsOnFailureToWriteFile)
{
    EXPECT_CALL(mock_file_ops, mkpath).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, open(_, _)).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, write(_, _)).WillOnce(Return(-1));
    MP_EXPECT_THROW_THAT(MP_JSONUTILS.write_json(json, file_path),
                         std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("Could not write"), HasSubstr(file_path.toStdString()))));
}

TEST_F(TestJsonUtils, writeJsonThrowsOnFailureToCommit)
{
    EXPECT_CALL(mock_file_ops, mkpath).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, open(_, _)).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, write(_, _)).WillOnce(Return(1234));
    EXPECT_CALL(mock_file_ops, commit).WillOnce(Return(false));
    MP_EXPECT_THROW_THAT(MP_JSONUTILS.write_json(json, file_path),
                         std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("Could not commit"), HasSubstr(file_path.toStdString()))));
}

struct ExtraInterfacesRead : public TestJsonUtils, public WithParamInterface<std::vector<mp::NetworkInterface>>
{
};

TEST_P(ExtraInterfacesRead, write_and_read_extra_interfaces)
{
    std::vector<mp::NetworkInterface> extra_ifaces = GetParam();

    auto written_ifaces = MP_JSONUTILS.extra_interfaces_to_json_array(extra_ifaces);

    QJsonObject doc;
    doc.insert("extra_interfaces", written_ifaces);

    auto read_ifaces = MP_JSONUTILS.read_extra_interfaces(doc);

    ASSERT_EQ(read_ifaces, extra_ifaces);
}

INSTANTIATE_TEST_SUITE_P(TestJsonUtils,
                         ExtraInterfacesRead,
                         Values(std::vector<mp::NetworkInterface>{{"eth1", "52:54:00:00:00:01", true},
                                                                  {"eth2", "52:54:00:00:00:02", false}},
                                std::vector<mp::NetworkInterface>{}));

TEST_F(TestJsonUtils, gives_nullopt_on_empty_extra_interfaces)
{
    QJsonObject doc;
    doc.insert("some_data", "nothing to see here");

    ASSERT_FALSE(MP_JSONUTILS.read_extra_interfaces(doc).has_value());
}

TEST_F(TestJsonUtils, throws_on_wrong_mac)
{
    std::vector<mp::NetworkInterface> extra_ifaces{mp::NetworkInterface{"eth3", "52:54:00:00:00:0x", true}};

    auto written_ifaces = MP_JSONUTILS.extra_interfaces_to_json_array(extra_ifaces);

    QJsonObject doc;
    doc.insert("extra_interfaces", written_ifaces);

    MP_ASSERT_THROW_THAT(MP_JSONUTILS.read_extra_interfaces(doc),
                         std::runtime_error,
                         mpt::match_what(StrEq("Invalid MAC address 52:54:00:00:00:0x")));
}

TEST_F(TestJsonUtils, updateCloudInitInstanceIdSucceed)
{
    EXPECT_EQ(MP_JSONUTILS.update_cloud_init_instance_id(QJsonValue{"vm1_e_e_e"}, "vm1", "vm2"),
              QJsonValue{"vm2_e_e_e"});
}
} // namespace
