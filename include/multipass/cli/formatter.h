/*
 * Copyright (C) 2018-2020 Canonical, Ltd.
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

#ifndef MULTIPASS_FORMATTER_H
#define MULTIPASS_FORMATTER_H

#include <multipass/rpc/multipass.grpc.pb.h>

#include <multipass/cli/client_platform.h>

#include <string>

namespace multipass
{
constexpr auto default_id_str = "default";

class Formatter
{
public:
    virtual ~Formatter() = default;
    virtual std::string format(const InfoReply& reply) const = 0;
    virtual std::string format(const ListReply& reply) const = 0;
    virtual std::string format(const ListNetworksReply& reply) const = 0;
    virtual std::string format(const FindReply& reply) const = 0;

protected:
    Formatter() = default;
    Formatter(const Formatter&) = delete;
    Formatter& operator=(const Formatter&) = delete;
};
}
#endif // MULTIPASS_FORMATTER_H
