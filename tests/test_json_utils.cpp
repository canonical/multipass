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

#include <cctype>
#include <regex>
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

struct UpcaseContext
{
};

std::string tag_invoke(const boost::json::value_to_tag<std::string>&,
                       const boost::json::value& json,
                       const UpcaseContext&)
{
    std::string s = value_to<std::string>(json);
    for (auto&& i : s)
        i = std::toupper(i);
    return s;
}

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

TEST_F(TestJsonUtils, lookupInArray)
{
    boost::json::value json = {"sam", "max"};
    EXPECT_EQ(mp::lookup_or<std::string>(json, 1, "fallback"), "max");
    EXPECT_EQ(mp::lookup_or<std::string>(json, 1, "fallback", UpcaseContext{}), "MAX");
}

TEST_F(TestJsonUtils, lookupInArrayFallback)
{
    boost::json::value json = {"sam", "max"};
    EXPECT_EQ(mp::lookup_or<std::string>(json, 2, "fallback"), "fallback");
    // The context doesn't affect the fallback value!
    EXPECT_EQ(mp::lookup_or<std::string>(json, 2, "fallback", UpcaseContext{}), "fallback");
}

TEST_F(TestJsonUtils, lookupInArrayWrongType)
{
    boost::json::value json = {"sam", "max"};
    EXPECT_THROW(mp::lookup_or<std::string>(json, "max", "fallback"), boost::system::system_error);
    EXPECT_THROW(mp::lookup_or<std::string>(json, "max", "fallback", UpcaseContext{}),
                 boost::system::system_error);
}

TEST_F(TestJsonUtils, lookupInObject)
{
    boost::json::value json = {{"sam", "canine shamus"}, {"max", "hyperkinetic rabbity thing"}};
    EXPECT_EQ(mp::lookup_or<std::string>(json, "sam", "fallback"), "canine shamus");
    EXPECT_EQ(mp::lookup_or<std::string>(json, "sam", "fallback", UpcaseContext{}),
              "CANINE SHAMUS");
}

TEST_F(TestJsonUtils, lookupInObjectFallback)
{
    boost::json::value json = {{"sam", "canine shamus"}, {"max", "hyperkinetic rabbity thing"}};
    EXPECT_EQ(mp::lookup_or<std::string>(json, "sybil", "fallback"), "fallback");
    // The context doesn't affect the fallback value!
    EXPECT_EQ(mp::lookup_or<std::string>(json, "sybil", "fallback", UpcaseContext{}), "fallback");
}

TEST_F(TestJsonUtils, lookupInObjectWrongType)
{
    boost::json::value json = {{"sam", "canine shamus"}, {"max", "hyperkinetic rabbity thing"}};
    EXPECT_THROW(mp::lookup_or<std::string>(json, 1, "fallback"), boost::system::system_error);
    EXPECT_THROW(mp::lookup_or<std::string>(json, 1, "fallback", UpcaseContext{}),
                 boost::system::system_error);
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

TEST_F(TestJsonUtils, mapToJsonArray)
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

TEST_F(TestJsonUtils, mapToJsonArrayDoesntRecurse)
{
    // MapAsJsonArray should apply only to the top-level `std::map`, but not the inner `std::map`.
    using MapOfMap = std::map<std::string, std::map<std::string, Animal>>;
    MapOfMap map_of_map = {{"pet", {{"dog", {"fido"}}, {"goat", {"philipp"}}}},
                           {"wild", {{"panda", {"coco"}}}}};

    boost::json::array json_array = {
        {{"_where", "pet"}, {"dog", {{"name", "fido"}}}, {"goat", {{"name", "philipp"}}}},
        {{"_where", "wild"}, {"panda", {{"name", "coco"}}}}};

    auto json_result = boost::json::value_from(map_of_map, mp::MapAsJsonArray{"_where"});
    EXPECT_EQ(json_result, json_array);

    auto map_result = value_to<MapOfMap>(json_array, mp::MapAsJsonArray{"_where"});
    EXPECT_EQ(map_result, map_of_map);
}

TEST_F(TestJsonUtils, sortJsonKeys)
{
    // Force a different sort order for our map.
    using Map = std::map<std::string, std::string, std::greater<>>;
    Map map = {{"4", "four"}, {"3", "three"}, {"2", "two"}, {"1", "one"}};
    boost::json::object json_object = {{"1", "one"}, {"2", "two"}, {"3", "three"}, {"4", "four"}};

    auto json_result = boost::json::value_from(map, mp::SortJsonKeys{});
    EXPECT_EQ(json_result, json_object);
    EXPECT_EQ(serialize(json_result), serialize(json_object));
}

TEST_F(TestJsonUtils, sortJsonKeysDoesntRecurse)
{
    // Force a different sort order for our maps.
    using InnerMap = std::map<std::string, std::string, std::greater<>>;
    using MapOfMap = std::map<std::string, InnerMap, std::greater<>>;
    InnerMap inner = {{"4", "four"}, {"3", "three"}, {"2", "two"}, {"1", "one"}};
    MapOfMap map_of_map = {{"4", inner}, {"3", inner}, {"2", inner}, {"1", inner}};

    boost::json::object json_inner = {{"1", "one"}, {"2", "two"}, {"3", "three"}, {"4", "four"}};
    boost::json::object json_object = {{"1", json_inner},
                                       {"2", json_inner},
                                       {"3", json_inner},
                                       {"4", json_inner}};
    auto json_result = boost::json::value_from(map_of_map, mp::SortJsonKeys{});
    EXPECT_EQ(json_result, json_object);
    // SortJsonKeys should apply only to the top-level map, but not the inner map.
    EXPECT_NE(serialize(json_result), serialize(json_object));
}

class JsonPrettyPrintTest : public TestWithParam<std::string>
{
};

TEST_P(JsonPrettyPrintTest, prettyPrintsCorrectly)
{
    std::string expected = GetParam();
    boost::json::value json = boost::json::parse(expected);
    EXPECT_EQ(mp::pretty_print(json), expected + "\n");
}

TEST_P(JsonPrettyPrintTest, prettyPrintsNoTrailingNewlineCorrectly)
{
    std::string expected = GetParam();
    boost::json::value json = boost::json::parse(expected);
    EXPECT_EQ(mp::pretty_print(json, {.trailing_newline = false}), expected);
}

TEST_P(JsonPrettyPrintTest, prettyPrintsCustomIndentCorrectly)
{
    std::string input = GetParam();
    boost::json::value json = boost::json::parse(input);
    // Replace all runs of 4 spaces with 2 spaces. NOTE: This assumes there are no runs of 4 spaces
    // in the actual data.
    auto expected = std::regex_replace(input, std::regex("    "), "  ") + "\n";
    EXPECT_EQ(mp::pretty_print(json, {.indent = 2}), expected);
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
                             "-12345",
                             "1.234",
                             // Strings
                             "\"hello there\"",
                             "\"some\\nnewlines\\n\"",
                             // Arrays
                             "[\n]",
                             ("[\n"
                              "    123,\n"
                              "    \"hello there\"\n"
                              "]"),
                             // Objects
                             "{\n}",
                             ("{\n"
                              "    \"foo\": \"bar\",\n"
                              "    \"one\": 1,\n"
                              "    \"yes\": true\n"
                              "}"),
                             // Nested
                             ("[\n"
                              "    [\n"
                              "        1,\n"
                              "        2\n"
                              "    ]\n"
                              "]"),
                             ("{\n"
                              "    \"foo\": {\n"
                              "        \"bar\": true\n"
                              "    }\n"
                              "}"),
                             ("[\n"
                              "    {\n"
                              "        \"foo\": [\n"
                              "            1,\n"
                              "            2\n"
                              "        ]\n"
                              "    }\n"
                              "]"),
                             ("{\n"
                              "    \"foo\": {\n"
                              "        \"bar\": [\n"
                              "            1,\n"
                              "            2\n"
                              "        ],\n"
                              "        \"baz\": \"quux\"\n"
                              "    }\n"
                              "}")));

TEST_F(TestJsonUtils, jsonToQString)
{
    boost::json::value json = "hello";
    EXPECT_EQ(value_to<QString>(json), QString("hello"));
}

TEST_F(TestJsonUtils, qStringToJson)
{
    QString str = "hello";
    auto json = boost::json::value_from(str);
    EXPECT_EQ(json, boost::json::string("hello"));
}

TEST_F(TestJsonUtils, jsonToQStringList)
{
    boost::json::value json = {"hello", "goodbye"};
    EXPECT_EQ(value_to<QStringList>(json),
              QStringList() << "hello"
                            << "goodbye");
}

TEST_F(TestJsonUtils, qStringListToJson)
{
    auto list = QStringList() << "hello"
                              << "goodbye";
    auto json = boost::json::value_from(list);
    EXPECT_EQ(json, boost::json::array({"hello", "goodbye"}));
}
} // namespace
