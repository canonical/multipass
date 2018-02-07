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

#ifndef MULTIPASS_FORMATTED_OUTPUT_H
#define MULTIPASS_FORMATTED_OUTPUT_H

#include <multipass/rpc/multipass.grpc.pb.h>

#include <string>

namespace multipass
{
class FormattedOutput
{
public:
    virtual ~FormattedOutput() = default;
    virtual std::string process_info(InfoReply& reply) const = 0;
    virtual std::string process_list(ListReply& reply) const = 0;

protected:
    FormattedOutput() = default;
    FormattedOutput(const FormattedOutput&) = delete;
    FormattedOutput& operator=(const FormattedOutput&) = delete;
};
}
#endif // MULTIPASS_FORMATTED_OUTPUT_H
