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

#include "tests/unit/common.h"

#include <shared/windows/native_path.h>

#include <array>

namespace multipass::test
{
using path = std::filesystem::path;

struct TestCase
{
    std::wstring path;
    std::wstring native_path;
    std::wstring json_path;
};

struct NativePath_UnitTests : public ::testing::Test
{
    const std::array<TestCase, 10> test_cases = {
        TestCase{L"E:/user/disk.vhdx", L"E:\\user\\disk.vhdx", L"E:\\\\user\\\\disk.vhdx"},
        TestCase{L"./user/disk.vhdx", L".\\user\\disk.vhdx", L".\\\\user\\\\disk.vhdx"},
        TestCase{L"user/disk.vhdx", L"user\\disk.vhdx", L"user\\\\disk.vhdx"},
        TestCase{L"user/", L"user\\", L"user\\\\"},
        TestCase{L"disk.vhdx", L"disk.vhdx", L"disk.vhdx"},
        TestCase{L"E:/用户/диск.vhdx", L"E:\\用户\\диск.vhdx", L"E:\\\\用户\\\\диск.vhdx"},
        TestCase{L"./用户/диск.vhdx", L".\\用户\\диск.vhdx", L".\\\\用户\\\\диск.vhdx"},
        TestCase{L"用户/диск.vhdx", L"用户\\диск.vhdx", L"用户\\\\диск.vhdx"},
        TestCase{L"用户/", L"用户\\", L"用户\\\\"},
        TestCase{L"диск.vhdx", L"диск.vhdx", L"диск.vhdx"},
    };
};

TEST_F(NativePath_UnitTests, uses_native_separators_on_construction)
{
    for (const auto& test_case : test_cases)
    {
        NativePath p{test_case.path};
        EXPECT_EQ(p.wstring(), test_case.native_path);
    }
}

TEST_F(NativePath_UnitTests, uses_native_separators_on_assignment)
{
    for (const auto& test_case : test_cases)
    {
        NativePath p{"."};
        p = test_case.path;
        EXPECT_EQ(p.wstring(), test_case.native_path);
    }
}

TEST_F(NativePath_UnitTests, supports_same_type_assignment)
{
    const NativePath expected{"some/path"};
    NativePath actual{"other/path"};
    actual = expected;

    EXPECT_EQ(actual, expected);
}

TEST_F(NativePath_UnitTests, fmt_produces_json_serializable)
{
    for (const auto& test_case : test_cases)
    {
        EXPECT_EQ(fmt::to_wstring(NativePath{test_case.path}), test_case.json_path);
        EXPECT_EQ(fmt::to_wstring(NativePath{test_case.native_path}), test_case.json_path);
    }
}

TEST_F(NativePath_UnitTests, fmt_honors_string_format_options)
{
    const NativePath path{L"some/path"};

    EXPECT_EQ(fmt::format("{:*>14}", path), "****some\\\\path");
    EXPECT_EQ(fmt::format(L"{:*>14}", path), L"****some\\\\path");
}
} // namespace multipass::test
