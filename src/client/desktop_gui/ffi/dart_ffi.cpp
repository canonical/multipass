#include "multipass/dart_ffi.h"
#include "multipass/cli/client_common.h"
#include "multipass/format.h"
#include "multipass/logging/log.h"
#include "multipass/name_generator.h"
#include "multipass/version.h"

namespace mp = multipass;
namespace mpc = multipass::client;
namespace mpl = multipass::logging;

constexpr auto category = "dart-ffi";

extern "C" const char* multipass_version()
{
    return mp::version_string;
}

extern "C" const char* generate_petname()
try
{
    static mp::NameGenerator::UPtr generator = mp::make_default_name_generator();
    const auto name = generator->make_name();
    return strdup(name.c_str());
}
catch (const std::exception& e)
{
    mpl::log(mpl::Level::warning, category, fmt::format("failed generating petname: {}", e.what()));
    return nullptr;
}
catch (...)
{
    mpl::log(mpl::Level::warning, category, "failed generating petname");
    return nullptr;
}

extern "C" const char* get_server_address()
try
{
    const auto address = mpc::get_server_address();
    return strdup(address.c_str());
}
catch (const std::exception& e)
{
    mpl::log(mpl::Level::warning, category, fmt::format("failed retrieving server address: {}", e.what()));
    return nullptr;
}
catch (...)
{
    mpl::log(mpl::Level::warning, category, "failed retrieving server address");
    return nullptr;
}

extern "C" struct KeyCertificatePair get_cert_pair()
try
{
    const auto provider = mpc::get_cert_provider(mpc::get_server_address());
    const auto cert = provider->PEM_certificate();
    const auto key = provider->PEM_signing_key();
    struct KeyCertificatePair pair
    {
    };
    pair.pem_cert = strdup(cert.c_str());
    pair.pem_priv_key = strdup(key.c_str());
    return pair;
}
catch (const std::exception& e)
{
    mpl::log(mpl::Level::warning, category, fmt::format("failed retrieving certificate key pair: {}", e.what()));
    return KeyCertificatePair{nullptr, nullptr};
}
catch (...)
{
    mpl::log(mpl::Level::warning, category, "failed retrieving certificate key pair");
    return KeyCertificatePair{nullptr, nullptr};
}
