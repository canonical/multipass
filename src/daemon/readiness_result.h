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

#pragma once

#include <multipass/rpc/multipass.grpc.pb.h>

#include <fmt/format.h>
#include <grpcpp/support/status.h>

#include <algorithm>
#include <cassert>
#include <iterator>
#include <ranges>
#include <string>
#include <vector>

namespace multipass
{
struct ReadinessResult
{
    std::string instance_name;
    ReadinessError::ErrorCode error;
    std::string message;
};

namespace detail
{
inline grpc::StatusCode grpc_status_code_for_readiness(const std::vector<ReadinessResult>& results,
                                                       const std::string& start_errors)
{
    // For now INVALID_ARGUMENT is the generic gRPC error status returned when an error happens
    if (!start_errors.empty())
        return grpc::StatusCode::INVALID_ARGUMENT;

    const auto is_ok = [](const ReadinessResult& result) {
        return result.error == ReadinessError::OK;
    };

    const auto is_timeout_or_ok = [is_ok](const ReadinessResult& result) {
        return result.error == ReadinessError::SSH_TIMEOUT ||
               result.error == ReadinessError::CLOUD_INIT_TIMEOUT || is_ok(result);
    };

    if (std::ranges::all_of(results, is_ok))
        return grpc::StatusCode::OK;

    // Return DEADLINE_EXCEEDED if all error states are timeout related
    if (std::ranges::all_of(results, is_timeout_or_ok))
        return grpc::StatusCode::DEADLINE_EXCEEDED;

    return grpc::StatusCode::INVALID_ARGUMENT;
}

template <typename Reply>
std::string grpc_error_details_for_readiness(const std::vector<ReadinessResult>&)
{
    return {};
}

template <>
inline std::string
grpc_error_details_for_readiness<LaunchReply>(const std::vector<ReadinessResult>& results)
{
    assert(results.size() == 1);

    LaunchError launch_error;
    launch_error.set_readiness_error(results.at(0).error);

    return launch_error.SerializeAsString();
}

template <>
inline std::string
grpc_error_details_for_readiness<StartReply>(const std::vector<ReadinessResult>& results)
{
    StartError start_error;

    auto& readiness_errors = *start_error.mutable_readiness_errors();
    for (const auto& result : results)
        if (result.error != ReadinessError::OK)
            readiness_errors[result.instance_name] = result.error;

    return start_error.SerializeAsString();
}

inline std::string grpc_error_message_for_readiness(const std::vector<ReadinessResult>& results,
                                                    const std::string& start_errors)
{
    fmt::memory_buffer collective_errors;
    fmt::format_to(std::back_inserter(collective_errors), "The following errors occurred:");

    if (!start_errors.empty())
        fmt::format_to(std::back_inserter(collective_errors), "\n{}", start_errors);

    for (const auto& result : results)
        if (result.error != ReadinessError::OK)
            fmt::format_to(std::back_inserter(collective_errors), "\n{}", result.message);

    return fmt::to_string(collective_errors);
}
} // namespace detail

template <typename Reply>
grpc::Status grpc_status_for_readiness(const std::vector<ReadinessResult>& results,
                                       const std::string& start_errors)
{
    const auto status_code = detail::grpc_status_code_for_readiness(results, start_errors);
    if (status_code == grpc::StatusCode::OK)
        return grpc::Status::OK;

    return {status_code,
            detail::grpc_error_message_for_readiness(results, start_errors),
            detail::grpc_error_details_for_readiness<Reply>(results)};
}
} // namespace multipass
