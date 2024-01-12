#include "multipass/dart_ffi.h"
#include "multipass/cli/client_common.h"
#include "multipass/format.h"
#include "multipass/logging/log.h"
#include "multipass/name_generator.h"
#include "multipass/settings/settings.h"
#include "multipass/version.h"

namespace mp = multipass;
namespace mpc = multipass::client;
namespace mpl = multipass::logging;
namespace mcp = multipass::cli::platform;

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

static std::once_flag initialize_settings_once_flag;

extern "C" enum SettingResult get_setting(const char* key, const char** output)
{
    const QString key_string{key};
    free((void*)key);
    try
    {
        std::call_once(initialize_settings_once_flag, mpc::register_global_settings_handlers);
        const auto value = MP_SETTINGS.get(key_string).toStdString();
        *output = strdup(value.c_str());
        return SettingResult::Ok;
    }
    catch (const mp::UnrecognizedSettingException& e)
    {
        mpl::log(mpl::Level::warning,
                 category,
                 fmt::format("failed retrieving setting with key '{}': {}", key_string, e.what()));
        *output = nullptr;
        return SettingResult::KeyNotFound;
    }
    catch (const std::exception& e)
    {
        mpl::log(mpl::Level::warning,
                 category,
                 fmt::format("failed retrieving setting with key '{}': {}", key_string, e.what()));
        *output = strdup(e.what());
        return SettingResult::UnexpectedError;
    }
    catch (...)
    {
        mpl::log(mpl::Level::warning, category, fmt::format("failed retrieving setting with key '{}'", key_string));
        *output = strdup("unknown error");
        return SettingResult::UnexpectedError;
    }
}

extern "C" enum SettingResult set_setting(const char* key, const char* value, const char** output)
{
    const QString key_string{key};
    free((void*)key);
    const QString value_string{value};
    free((void*)value);
    try
    {
        std::call_once(initialize_settings_once_flag, mpc::register_global_settings_handlers);
        MP_SETTINGS.set(key_string, value_string);
        *output = nullptr;
        return SettingResult::Ok;
    }
    catch (const mp::UnrecognizedSettingException& e)
    {
        mpl::log(mpl::Level::warning,
                 category,
                 fmt::format("failed storing setting with key '{}'='{}': {}", key_string, value_string, e.what()));
        *output = nullptr;
        return SettingResult::KeyNotFound;
    }
    catch (const mp::InvalidSettingException& e)
    {
        mpl::log(mpl::Level::warning,
                 category,
                 fmt::format("failed storing setting with key '{}'='{}': {}", key_string, value_string, e.what()));
        *output = strdup(e.what());
        return SettingResult::InvalidValue;
    }
    catch (const std::exception& e)
    {
        mpl::log(mpl::Level::warning,
                 category,
                 fmt::format("failed storing setting with key '{}'='{}': {}", key_string, value_string, e.what()));
        *output = strdup(e.what());
        return SettingResult::UnexpectedError;
    }
    catch (...)
    {
        mpl::log(mpl::Level::warning,
                 category,
                 fmt::format("failed storing setting with key '{}'='{}'", key_string, value_string));
        *output = strdup("unknown error");
        return SettingResult::UnexpectedError;
    }
}

extern "C" int uid()
{
    return mcp::getuid();
}

extern "C" int gid()
{
    return mcp::getgid();
}

extern "C" int default_id()
{
    return mp::default_id;
}
