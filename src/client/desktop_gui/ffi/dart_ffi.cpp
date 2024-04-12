#include "multipass/dart_ffi.h"
#include "multipass/cli/client_common.h"
#include "multipass/logging/log.h"
#include "multipass/memory_size.h"
#include "multipass/name_generator.h"
#include "multipass/settings/settings.h"
#include "multipass/version.h"

namespace mp = multipass;
namespace mpc = multipass::client;
namespace mpl = multipass::logging;
namespace mcp = multipass::cli::platform;

constexpr auto category = "dart-ffi";

// clang-format off
extern "C"
{
// clang-format on
const char* multipass_version()
{
    return mp::version_string;
}

const char* generate_petname()
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

const char* get_server_address()
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

struct KeyCertificatePair get_cert_pair()
try
{
    const auto provider = mpc::get_cert_provider();
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

const char* settings_file()
try
{
    const auto file_name = mpc::persistent_settings_filename().toStdString();
    return strdup(file_name.c_str());
}
catch (const std::exception& e)
{
    mpl::log(mpl::Level::warning, category, fmt::format("failed getting settings file: {}", e.what()));
    return nullptr;
}
catch (...)
{
    mpl::log(mpl::Level::warning, category, "failed getting settings file");
    return nullptr;
}

enum SettingResult get_setting(const char* key, const char** output)
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

enum SettingResult set_setting(const char* key, const char* value, const char** output)
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

int uid()
{
    return mcp::getuid();
}

int gid()
{
    return mcp::getgid();
}

int default_id()
{
    return mp::default_id;
}

long long memory_in_bytes(const char* value)
try
{
    std::string string_value{value};
    free((void*)value);
    return mp::in_bytes(string_value);
}
catch (const std::exception& e)
{
    mpl::log(mpl::Level::warning, category, fmt::format("failed converting memory to bytes: {}", e.what()));
    return -1;
}
catch (...)
{
    mpl::log(mpl::Level::warning, category, "failed converting memory to bytes");
    return -1;
}
}
