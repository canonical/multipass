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

#ifndef MULTIPASS_TEST_DATA_PATH_H
#define MULTIPASS_TEST_DATA_PATH_H

#include <QString>

namespace multipass
{
namespace test
{
QString test_data_path();
QString test_data_path_for(const char* file_name);
QString test_data_sub_dir_path(const char* dir_name);
std::string mock_bin_path();
} // namespace test
} // namespace multipass
#endif // MULTIPASS_TEST_DATA_PATH_H
