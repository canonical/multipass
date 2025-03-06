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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <multipass/logging/level.h>
#include <multipass/logging/standard_logger.h>
namespace mpl = multipass::logging;

using uut_t = mpl::StandardLogger;

struct standard_logger_tests : ::testing::Test
{
};

auto make_mock_file()
{
    // Ideally, tmpfile() should be replaced with something in-memory
    // like open_memstream(), or a pipe but that'd mean per-platform
    // implementation.
    return std::unique_ptr<FILE, int (*)(FILE*)>{tmpfile(), &fclose};
}

auto read_mock_file(FILE* file)
{

    fseek(file, 0, SEEK_END);
    const auto sz = ftell(file);
    std::string content(sz, '\0');
    // Flush and rewind the temporary file.
    fflush(file);
    rewind(file);
    fread(&content[0], sizeof(char), sz, file);
    return content;
}

TEST_F(standard_logger_tests, call_log)
{
    const auto& mock_stderr = make_mock_file();
    uut_t logger{mpl::Level::debug, mock_stderr.get()};
    logger.log(mpl::Level::debug, "cat", "msg");
    const auto content = read_mock_file(mock_stderr.get());
    ASSERT_THAT(content, testing::HasSubstr("[debug] [cat] msg"));
}

TEST_F(standard_logger_tests, call_log_filtered)
{
    const auto& mock_stderr = make_mock_file();
    uut_t logger{mpl::Level::debug, mock_stderr.get()};
    logger.log(mpl::Level::trace, "cat", "msg");
    const auto content = read_mock_file(mock_stderr.get());
    ASSERT_TRUE(content.empty());
}
