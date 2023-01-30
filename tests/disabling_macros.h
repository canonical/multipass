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

#ifndef MULTIPASS_DISABLING_MACROS_H
#define MULTIPASS_DISABLING_MACROS_H

// Macros to disable tests - prepend to test or test suite names
// Use like this: TEST_F(TestSuite, DISABLE_ON_XXX(test_name))
// See https://github.com/google/googletest/blob/master/googletest/docs/advanced.md#temporarily-disabling-tests

#ifdef MULTIPASS_PLATFORM_WINDOWS

#define DISABLE_ON_WINDOWS(test_name) DISABLED_##test_name
#define DISABLE_ON_WINDOWS_AND_MACOS(test_name) DISABLED_##test_name
#define DISABLE_ON_MACOS(test_name) test_name

#elif MULTIPASS_PLATFORM_APPLE

#define DISABLE_ON_WINDOWS(test_name) test_name
#define DISABLE_ON_WINDOWS_AND_MACOS(test_name) DISABLED_##test_name
#define DISABLE_ON_MACOS(test_name) DISABLED_##test_name

#else // Linux

#define DISABLE_ON_WINDOWS(test_name) test_name
#define DISABLE_ON_WINDOWS_AND_MACOS(test_name) test_name
#define DISABLE_ON_MACOS(test_name) test_name

#endif

#endif // MULTIPASS_DISABLING_MACROS_H
