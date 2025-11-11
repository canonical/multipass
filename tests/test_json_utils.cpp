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
struct ExtraInterfacesRead : public TestWithParam<std::vector<mp::NetworkInterface>>
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

TEST(TestJsonUtils, givesNulloptOnEmptyExtraInterfaces)
{
    QJsonObject doc;
    doc.insert("some_data", "nothing to see here");

    ASSERT_FALSE(MP_JSONUTILS.read_extra_interfaces(doc).has_value());
}

TEST(TestJsonUtils, throwsOnWrongMac)
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

TEST(TestJsonUtils, updateCloudInitInstanceIdSucceed)
{
    EXPECT_EQ(MP_JSONUTILS.update_cloud_init_instance_id(QJsonValue{"vm1_e_e_e"}, "vm1", "vm2"),
              QJsonValue{"vm2_e_e_e"});
}

struct Animal
{
    std::string name;
    friend bool operator==(const Animal&, const Animal&) = default;
};

void tag_invoke(const boost::json::value_from_tag&, boost::json::value& json, const Animal& animal)
{
    json = {{"name", animal.name}};
}
Animal tag_invoke(const boost::json::value_to_tag<Animal>&, const boost::json::value& json)
{
    return {value_to<std::string>(json.at("name"))};
}

TEST(TestJsonUtils, mapToJsonArray)
{
    std::map<std::string, Animal> map = {{"dog", {"fido"}},
                                         {"goat", {"philipp"}},
                                         {"panda", {"coco"}}};
    boost::json::array json_array = {{{"species", "dog"}, {"name", "fido"}},
                                     {{"species", "goat"}, {"name", "philipp"}},
                                     {{"species", "panda"}, {"name", "coco"}}};

    auto json_result = boost::json::value_from(map, mp::MapAsJsonArray{"species"});
    EXPECT_EQ(json_result, json_array);

    auto map_result =
        value_to<std::map<std::string, Animal>>(json_array, mp::MapAsJsonArray{"species"});
    EXPECT_EQ(map_result, map);
}

TEST(TestJsonUtils, jsonToQString)
{
    boost::json::value json = "hello";
    EXPECT_EQ(value_to<QString>(json), QString("hello"));
}

TEST(TestJsonUtils, qStringToJson)
{
    QString str = "hello";
    auto json = boost::json::value_from(str);
    EXPECT_EQ(json, boost::json::string("hello"));
}

TEST(TestJsonUtils, jsonToQStringList)
{
    boost::json::value json = {"hello", "goodbye"};
    EXPECT_EQ(value_to<QStringList>(json),
              QStringList() << "hello"
                            << "goodbye");
}

TEST(TestJsonUtils, qStringListToJson)
{
    auto list = QStringList() << "hello"
                              << "goodbye";
    auto json = boost::json::value_from(list);
    EXPECT_EQ(json, boost::json::array({"hello", "goodbye"}));
}
} // namespace
