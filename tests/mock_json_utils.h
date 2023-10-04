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

#ifndef MULTIPASS_MOCK_JSON_UTILS_H
#define MULTIPASS_MOCK_JSON_UTILS_H

#include "common.h"
#include "mock_singleton_helpers.h"

#include <multipass/json_utils.h>

namespace multipass::test
{
class MockJsonUtils : public JsonUtils
{
public:
    using JsonUtils::JsonUtils;

    MOCK_METHOD(void, write_json, (const QJsonObject&, QString), (const, override));
    MOCK_METHOD(std::string, json_to_string, (const QJsonObject& root), (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockJsonUtils, JsonUtils);
};
} // namespace multipass::test

#endif // MULTIPASS_MOCK_JSON_UTILS_H
