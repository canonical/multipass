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
#include <multipass/vm_specs.h>

#include <QFileDevice>
#include <QString>

#include <cctype>
#include <regex>
#include <stdexcept>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
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

TEST(TestJsonUtils, updatesUniqueIdentifiersOfMetadata)
{
    mp::VMSpecs src_specs = {1,
                             mp::MemorySize::from_bytes(0),
                             mp::MemorySize::from_bytes(0),
                             "01:ff:00:00:00:01",
                             {{"id", "01:ff:00:00:00:02", false}},
                             "username",
                             mp::VirtualMachine::State::off,
                             {},
                             false,
                             {}};
    mp::VMSpecs dst_specs = src_specs;
    dst_specs.default_mac_address = "aa:ff:00:00:00:01";
    dst_specs.extra_interfaces = {{"id", "aa:ff:00:00:00:02", false}};

    boost::json::object src_metadata = {{"arguments",
                                         {"instances/src_vm",
                                          "misc arg",
                                          "don't change src_vm",
                                          "--mac=01:ff:00:00:00:01",
                                          "01:ff:00:00:00:01==01:ff:00:00:00:01",
                                          "--extra_mac=01:ff:00:00:00:02"}}};
    boost::json::object dst_metadata = {{"arguments",
                                         {"instances/dst_vm",
                                          "misc arg",
                                          "don't change src_vm",
                                          "--mac=aa:ff:00:00:00:01",
                                          "aa:ff:00:00:00:01==aa:ff:00:00:00:01",
                                          "--extra_mac=aa:ff:00:00:00:02"}}};

    EXPECT_EQ(update_unique_identifiers_of_metadata(src_metadata,
                                                    src_specs,
                                                    dst_specs,
                                                    "src_vm",
                                                    "dst_vm"),
              dst_metadata);
}

TEST(TestJsonUtils, lookupInArray)
{
    boost::json::value json = {"sam", "max"};
    EXPECT_EQ(mp::lookup_or<std::string>(json, 1, "fallback"), "max");
    EXPECT_EQ(mp::lookup_or<std::string>(json, 1, "fallback", UpcaseContext{}), "MAX");
}

TEST(TestJsonUtils, lookupInArrayFallback)
{
    boost::json::value json = {"sam", "max"};
    EXPECT_EQ(mp::lookup_or<std::string>(json, 2, "fallback"), "fallback");
    // The context doesn't affect the fallback value!
    EXPECT_EQ(mp::lookup_or<std::string>(json, 2, "fallback", UpcaseContext{}), "fallback");
}

TEST(TestJsonUtils, lookupInArrayWrongType)
{
    boost::json::value json = {"sam", "max"};
    EXPECT_THROW(mp::lookup_or<std::string>(json, "max", "fallback"), boost::system::system_error);
    EXPECT_THROW(mp::lookup_or<std::string>(json, "max", "fallback", UpcaseContext{}),
                 boost::system::system_error);
}

TEST(TestJsonUtils, lookupInObject)
{
    boost::json::value json = {{"sam", "canine shamus"}, {"max", "hyperkinetic rabbity thing"}};
    EXPECT_EQ(mp::lookup_or<std::string>(json, "sam", "fallback"), "canine shamus");
    EXPECT_EQ(mp::lookup_or<std::string>(json, "sam", "fallback", UpcaseContext{}),
              "CANINE SHAMUS");
}

TEST(TestJsonUtils, lookupInObjectFallback)
{
    boost::json::value json = {{"sam", "canine shamus"}, {"max", "hyperkinetic rabbity thing"}};
    EXPECT_EQ(mp::lookup_or<std::string>(json, "sybil", "fallback"), "fallback");
    // The context doesn't affect the fallback value!
    EXPECT_EQ(mp::lookup_or<std::string>(json, "sybil", "fallback", UpcaseContext{}), "fallback");
}

TEST(TestJsonUtils, lookupInObjectWrongType)
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

TEST(TestJsonUtils, mapToJsonArrayDoesntRecurse)
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

TEST(TestJsonUtils, sortJsonKeys)
{
    // Force a different sort order for our map.
    using Map = std::map<std::string, std::string, std::greater<>>;
    Map map = {{"4", "four"}, {"3", "three"}, {"2", "two"}, {"1", "one"}};
    boost::json::object json_object = {{"1", "one"}, {"2", "two"}, {"3", "three"}, {"4", "four"}};

    auto json_result = boost::json::value_from(map, mp::SortJsonKeys{});
    EXPECT_EQ(json_result, json_object);
    EXPECT_EQ(serialize(json_result), serialize(json_object));
}

TEST(TestJsonUtils, sortJsonKeysDoesntRecurse)
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
