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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include "common.h"

#include "rust/cxx.h"
#include <multipass/src/lib.rs.h>

using namespace testing;

// These tests now call the Rust test functions through FFI
TEST(Petname, generatesTheRequestedNumWords)
{
    EXPECT_TRUE(multipass::test_generates_the_requested_num_words());
}

TEST(Petname, usesCustomSeparator)
{
    EXPECT_TRUE(multipass::test_uses_custom_separator());
}

TEST(Petname, generatesTwoTokensByDefault)
{
    EXPECT_TRUE(multipass::test_generates_two_tokens_by_default());
}

TEST(Petname, canGenerateAtLeastHundredUniqueNames)
{
    EXPECT_TRUE(multipass::test_can_generate_at_least_hundred_unique_names());
}

TEST(Petname, ffiIntegrationTest)
{
    EXPECT_TRUE(multipass::test_ffi_integration_test());
}
