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

#include <regex>
#include <stdexcept>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
TEST(TestJsonUtils, updateCloudInitInstanceIdSucceed)
{
    EXPECT_EQ(MP_JSONUTILS.update_cloud_init_instance_id(QJsonValue{"vm1_e_e_e"}, "vm1", "vm2"),
              QJsonValue{"vm2_e_e_e"});
}

TEST(TestJsonUtils, lookupInArray)
{
    boost::json::value json = {"sam", "max"};
    EXPECT_EQ(mp::lookup_or<std::string>(json, 1, "fallback"), "max");
}

TEST(TestJsonUtils, lookupInArrayFallback)
{
    boost::json::value json = {"sam", "max"};
    EXPECT_EQ(mp::lookup_or<std::string>(json, 2, "fallback"), "fallback");
}

TEST(TestJsonUtils, lookupInArrayWrongType)
{
    boost::json::value json = {"sam", "max"};
    EXPECT_THROW(mp::lookup_or<std::string>(json, "max", "fallback"), boost::system::system_error);
}

TEST(TestJsonUtils, lookupInObject)
{
    boost::json::value json = {{"sam", "canine shamus"}, {"max", "hyperkinetic rabbity thing"}};
    EXPECT_EQ(mp::lookup_or<std::string>(json, "sam", "fallback"), "canine shamus");
}

TEST(TestJsonUtils, lookupInObjectFallback)
{
    boost::json::value json = {{"sam", "canine shamus"}, {"max", "hyperkinetic rabbity thing"}};
    EXPECT_EQ(mp::lookup_or<std::string>(json, "sybil", "fallback"), "fallback");
}

TEST(TestJsonUtils, lookupInObjectWrongType)
{
    boost::json::value json = {{"sam", "canine shamus"}, {"max", "hyperkinetic rabbity thing"}};
    EXPECT_THROW(mp::lookup_or<std::string>(json, 1, "fallback"), boost::system::system_error);
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
    // Replace all runs of 4 spaces with 2 spaces. This isn't assumes there are no runs of 4 spaces
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
