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
};

TEST_F(TestJsonUtils, readObjectFromFileReadsFromFile)
{
    auto filedata = std::make_unique<std::stringstream>();
    *filedata << "{ \"test\": 123 }";

    EXPECT_CALL(mock_file_ops, open_read(_, _)).WillOnce(Return(std::move(filedata)));
    const auto json = MP_JSONUTILS.read_object_from_file(":)");

    ASSERT_NE(json.find("test"), json.end());
    ASSERT_TRUE(json.find("test").value().isDouble());
    EXPECT_EQ(json.find("test").value().toInt(), 123);
}

TEST_F(TestJsonUtils, readObjectFromFileThrowsOnFailbit)
{
    auto filedata = std::make_unique<std::stringstream>();
    filedata->setstate(std::ios_base::failbit);

    EXPECT_CALL(mock_file_ops, open_read(_, _)).WillOnce(Return(std::move(filedata)));

    EXPECT_THROW(MP_JSONUTILS.read_object_from_file(":("), std::ios_base::failure);
}

TEST_F(TestJsonUtils, readObjectFromFileThrowsOnBadbit)
{
    auto filedata = std::make_unique<std::stringstream>();
    filedata->setstate(std::ios_base::badbit);

    EXPECT_CALL(mock_file_ops, open_read(_, _)).WillOnce(Return(std::move(filedata)));

    EXPECT_THROW(MP_JSONUTILS.read_object_from_file(":("), std::ios_base::failure);
}

struct ExtraInterfacesRead : public TestJsonUtils,
                             public WithParamInterface<std::vector<mp::NetworkInterface>>
{
};

TEST_P(ExtraInterfacesRead, writeAndReadExtraInterfaces)
{
    std::vector<mp::NetworkInterface> extra_ifaces = GetParam();

    auto written_ifaces = MP_JSONUTILS.extra_interfaces_to_json_array(extra_ifaces);

    QJsonObject doc;
    doc.insert("extra_interfaces", written_ifaces);

    auto read_ifaces = MP_JSONUTILS.read_extra_interfaces(doc);

    ASSERT_EQ(read_ifaces, extra_ifaces);
}

INSTANTIATE_TEST_SUITE_P(
    TestJsonUtils,
    ExtraInterfacesRead,
    Values(std::vector<mp::NetworkInterface>{{"eth1", "52:54:00:00:00:01", true},
                                             {"eth2", "52:54:00:00:00:02", false}},
           std::vector<mp::NetworkInterface>{}));

TEST_F(TestJsonUtils, givesNulloptOnEmptyExtraInterfaces)
{
    QJsonObject doc;
    doc.insert("some_data", "nothing to see here");

    ASSERT_FALSE(MP_JSONUTILS.read_extra_interfaces(doc).has_value());
}

TEST_F(TestJsonUtils, throwsOnWrongMac)
{
    std::vector<mp::NetworkInterface> extra_ifaces{
        mp::NetworkInterface{"eth3", "52:54:00:00:00:0x", true}};

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
