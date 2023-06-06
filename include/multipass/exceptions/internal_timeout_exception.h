//
// Created by ricab on 06-06-2023.
//

#ifndef MULTIPASS_INTERNAL_TIMEOUT_EXCEPTION_H
#define MULTIPASS_INTERNAL_TIMEOUT_EXCEPTION_H

#include <chrono>
#include <stdexcept>
#include <string>

#include <multipass/format.h>

namespace multipass
{

class InternalTimeoutException : public std::runtime_error
{
public:
    InternalTimeoutException(const std::string& action, std::chrono::milliseconds timeout)
        : std::runtime_error{fmt::format("Could not {} within {}ms", action, timeout.count())}
    {
    }
};

} // namespace multipass

#endif // MULTIPASS_INTERNAL_TIMEOUT_EXCEPTION_H
