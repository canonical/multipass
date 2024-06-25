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

#ifndef MULTIPASS_FILE_READER_H
#define MULTIPASS_FILE_READER_H

#include <QByteArray>
#include <QString>

namespace multipass
{
namespace test
{
QByteArray load(QString path);
QByteArray load_test_file(const char* file_name);
void make_file_with_content(const QString& file_name, const std::string& content = "this is a test file");
}
}
#endif // MULTIPASS_FILE_READER_H
