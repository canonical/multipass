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

#pragma once

#include <filesystem>
#include <io.h>
#include <random>

#include <fmt/format.h>

#define EXPECT_NO_CALL(mock) EXPECT_CALL(mock, Call).Times(0)

namespace multipass::test
{

template <typename CharT>
inline auto trim_whitespace(const CharT* input)
{
    std::basic_string<CharT> str{input};
    str.erase(std::remove_if(str.begin(), str.end(), ::iswspace), str.end());
    return str;
}

/**
 * Create an unique path for a temporary file.
 */
inline auto make_tempfile_path(std::string extension)
{
    static std::mt19937_64 rng{std::random_device{}()};
    struct auto_remove_path
    {

        auto_remove_path(std::filesystem::path p) : path(p)
        {
        }

        ~auto_remove_path() noexcept
        {
            std::error_code ec{};
            // Use the noexcept overload
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

    std::filesystem::path temp_path{};
    std::uint32_t remaining_attempts = 10;
    do
    {
        temp_path = std::filesystem::temp_directory_path() /
                    fmt::format("temp-{:016x}{}", rng(), extension);
        // The generated path is vulnerable to TOCTOU, but it's highly unlikely we'll see a clash.
        // Better handling of this would require creation of a placeholder file, and an atomic swap
        // with the real file.
    } while (std::filesystem::exists(temp_path) && --remaining_attempts);

    if (!remaining_attempts)
    {
        throw std::runtime_error{"Exhausted attempt count for temporary filename generation."};
    }

    return auto_remove_path{temp_path};
}

} // namespace multipass::test
