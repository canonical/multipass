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

#ifndef MULTIPASS_TEST_WITH_MOCKED_BIN_PATH
#define MULTIPASS_TEST_WITH_MOCKED_BIN_PATH

#include "common.h"
#include "mock_environment_helpers.h"

namespace multipass
{
namespace test
{
struct TestWithMockedBinPath : public testing::Test
{
    void SetUp() override;
    void TearDown() override;
private:
    std::unique_ptr<SetEnvScope> env;
};
} // namespace test
} // namespace multipass

#endif // MULTIPASS_TEST_WITH_MOCKED_BIN_PATH
