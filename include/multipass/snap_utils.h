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

#ifndef MULTIPASS_SNAP_UTILS_H
#define MULTIPASS_SNAP_UTILS_H

#include <QByteArray>

namespace multipass
{
namespace utils
{
bool in_multipass_snap();
QByteArray snap_dir();
QByteArray snap_common_dir();
QByteArray snap_real_home_dir();
QByteArray snap_user_common_dir();
} // namespace utils
} // namespace multipass

#endif // MULTIPASS_SNAP_UTILS_H
