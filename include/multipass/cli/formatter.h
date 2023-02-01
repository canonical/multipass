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

#ifndef MULTIPASS_FORMATTER_H
#define MULTIPASS_FORMATTER_H

#include <multipass/disabled_copy_move.h>
#include <multipass/rpc/multipass.grpc.pb.h>

#include <multipass/cli/alias_dict.h>
#include <multipass/cli/client_platform.h>

#include <string>

namespace multipass
{
constexpr auto default_id_str = "default";

class Formatter : private DisabledCopyMove
{
public:
    virtual ~Formatter() = default;
    virtual std::string format(const InfoReply& reply) const = 0;
    virtual std::string format(const ListReply& reply) const = 0;
    virtual std::string format(const NetworksReply& reply) const = 0;
    virtual std::string format(const FindReply& reply) const = 0;
    virtual std::string format(const VersionReply& reply, const std::string& client_version) const = 0;
    virtual std::string format(const AliasDict& aliases) const = 0;

protected:
    Formatter() = default;

    template <class D>
    std::map<typename D::key_type, typename D::mapped_type> sort_dict(const D& unsorted_dict) const
    {
        return std::map<typename D::key_type, typename D::mapped_type>(unsorted_dict.cbegin(), unsorted_dict.cend());
    }
};
}
#endif // MULTIPASS_FORMATTER_H
