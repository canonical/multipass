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

#ifndef MULTIPASS_TESTS_HYPERV_API_HYPERV_TEST_UTILS_H
#define MULTIPASS_TESTS_HYPERV_API_HYPERV_TEST_UTILS_H

#include <filesystem>
#include <io.h>

#define EXPECT_NO_CALL(mock) EXPECT_CALL(mock, Call).Times(0)

namespace multipass::test
{

inline auto trim_whitespace(const wchar_t* input)
{
    std::wstring str{input};
    str.erase(std::remove_if(str.begin(), str.end(), ::iswspace), str.end());
    return str;
}

inline auto make_tempfile_path(std::string extension)
{

    struct auto_remove_path
    {

        auto_remove_path(std::filesystem::path p) : path(p)
        {
        }

        ~auto_remove_path()
        {
            std::error_code ec{};
            std::filesystem::remove(path, ec);
        }

        operator const std::filesystem::path&() const& noexcept
        {
            return path;
        }

        operator const std::filesystem::path&() const&& noexcept = delete;

    private:
        const std::filesystem::path path;
    };
    char pattern[] = "temp-XXXXXX";
    _mktemp_s(pattern);
    std::string f = pattern;
    f.append(extension);
    return auto_remove_path{std::filesystem::temp_directory_path() / f};
}

} // namespace multipass::test

#endif
