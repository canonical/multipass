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
 * Authored by: Chris Townsend <christopher.townsend@canonical.com>
 *              Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#pragma once

#include <filesystem>

#include <QByteArray>
#include <QString>

namespace multipass::test
{
QByteArray load(QString path);
QByteArray load_test_file(const char* file_name);
void make_file_with_content(const std::filesystem::path& file_name,
                            const std::string& content = "this is a test file");

template <std::same_as<QString> T> // No type conversion!
void make_file_with_content(const T& file_name, const std::string& content = "this is a test file")
{
    make_file_with_content(file_name.toStdString(), content);
}

} // namespace multipass::test
