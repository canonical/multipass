#pragma once

#include <stdexcept>
#include <string>

namespace multipass
{

class IntentionalShutdownException : public std::runtime_error
{
public:
    explicit IntentionalShutdownException(const std::string& name)
        : std::runtime_error{
            "Instance '" + name +
            "' stopped intentionally during initialization "
            "(cloud-init power_state directive)"} {}
};

}