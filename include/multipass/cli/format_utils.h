/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#ifndef MULTIPASS_FORMAT_UTILS_H
#define MULTIPASS_FORMAT_UTILS_H

#include <multipass/rpc/multipass.grpc.pb.h>

#include <string>

namespace multipass
{
namespace format
{
std::string status_string(const InstanceStatus& status);
}
}
#endif // MULTIPASS_FORMAT_UTILS_H
