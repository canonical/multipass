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

#include <multipass/constants.h>
#include <multipass/rpc/multipass.grpc.pb.h>

#include <algorithm>
#include <string>

namespace multipass
{
class Formatter;

namespace format
{
std::string status_string_for(const InstanceStatus& status);
Formatter* formatter_for(const std::string& format);

template <typename Instances>
Instances sorted(const Instances& instances);

} // namespace format
}

template <typename Instances>
Instances multipass::format::sorted(const Instances& instances)
{
    auto ret = instances;
    std::partial_sort(std::begin(ret), std::next(std::begin(ret)), std::end(ret),
                      [](const auto& a, const auto& /*unused*/) { return a.name() == multipass::petenv_name; });

    return ret;
}
#endif // MULTIPASS_FORMAT_UTILS_H
