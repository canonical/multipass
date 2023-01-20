/*
 * Copyright (C) 2023 Canonical, Ltd.
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
 */

#ifndef MULTIPASS_COMMON_CALLBACKS_H
#define MULTIPASS_COMMON_CALLBACKS_H

#include "animated_spinner.h"

#include <grpc++/grpc++.h>

namespace multipass
{
template <typename Request, typename Reply>
auto make_logging_spinner_callback(AnimatedSpinner& spinner, std::ostream& stream)
{
    return [&spinner, &stream](const Reply& reply, grpc::ClientReaderWriterInterface<Request, Reply>*) {
        if (!reply.log_line().empty())
        {
            spinner.print(stream, reply.log_line());
        }
    };
}
} // namespace multipass

#endif // MULTIPASS_COMMON_CALLBACKS_H
