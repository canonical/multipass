/*
 * Copyright (C) 2017 Canonical, Ltd.
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
std::string mock_bin_path();
}
}
#endif // MULTIPASS_TEST_DATA_PATH_H
