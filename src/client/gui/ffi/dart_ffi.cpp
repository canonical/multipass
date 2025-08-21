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

#include "multipass/dart_ffi.h"
#include "multipass/cli/client_common.h"
#include "multipass/logging/log.h"
#include "multipass/memory_size.h"
#include "multipass/platform.h"
#include "multipass/settings/settings.h"
#include "multipass/standard_paths.h"
#include "multipass/utils.h"
#include "multipass/version.h"
#include <multipass-petname/src/lib.rs.h>

#include <QStorageInfo>

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

char* generate_petname()
{
    static constexpr auto error = "failed generating petname";
    try
    {
        static rust::Box<multipass::petname::Petname> generator =
            multipass::petname::new_petname(2, "-");
        const auto name = multipass::petname::make_name(*generator);
        return strdup(std::string(name).c_str());
    }
    catch (const std::exception& e)
    {
        mpl::warn(category, "{}: {}", error, e.what());
        return nullptr;
    }
    catch (...)
    {
        mpl::log_message(mpl::Level::warning, category, error);
        return nullptr;
    }
}

char* get_server_address()
{
    static constexpr auto error = "failed retrieving server address";
    try
    {
        const auto address = mpc::get_server_address();
        return strdup(address.c_str());
    }
    catch (const std::exception& e)
    {
        mpl::warn(category, "{}: {}", error, e.what());
        return nullptr;
    }
    catch (...)
    {
        mpl::log_message(mpl::Level::warning, category, error);
        return nullptr;
    }
}

struct KeyCertificatePair get_cert_pair()
{
    static constexpr auto error = "failed retrieving certificate key pair";
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
        mpl::warn(category, "{}: {}", error, e.what());
        return KeyCertificatePair{nullptr, nullptr};
    }
    catch (...)
    {
        mpl::log_message(mpl::Level::warning, category, error);
        return KeyCertificatePair{nullptr, nullptr};
    }
}

char* get_root_cert()
{
    static constexpr auto error = "failed retrieving root certificate";
    try
    {
        const auto cert_path = MP_PLATFORM.get_root_cert_path();
        const auto cert = MP_UTILS.contents_of(QString::fromStdU16String(cert_path.u16string()));
        return strdup(cert.c_str());
    }
    catch (const std::exception& e)
    {
        mpl::warn(category, "{}: {}", error, e.what());
        return nullptr;
    }
    catch (...)
    {
        mpl::warn(category, error);
        return nullptr;
    }
}

static std::once_flag initialize_settings_once_flag;

char* settings_file()
{
    static constexpr auto error = "failed getting settings file";
    try
    {
        const auto file_name = mpc::persistent_settings_filename().toStdString();
        return strdup(file_name.c_str());
    }
    catch (const std::exception& e)
    {
        mpl::warn(category, "{}: {}", error, e.what());
        return nullptr;
    }
    catch (...)
    {
        mpl::log_message(mpl::Level::warning, category, error);
        return nullptr;
    }
}

enum SettingResult get_setting(char* key, char** output)
{
    static constexpr auto error = "failed retrieving setting with key";
    const QString key_string{key};
    free(key);
    try
    {
        std::call_once(initialize_settings_once_flag, mpc::register_global_settings_handlers);
        const auto value = MP_SETTINGS.get(key_string).toStdString();
        *output = strdup(value.c_str());
        return SettingResult::Ok;
    }
    catch (const mp::UnrecognizedSettingException& e)
    {
        mpl::warn(category, "{} '{}': {}", error, key_string, e.what());
        *output = nullptr;
        return SettingResult::KeyNotFound;
    }
    catch (const std::exception& e)
    {
        mpl::warn(category, "{} '{}': {}", error, key_string, e.what());
        *output = strdup(e.what());
        return SettingResult::UnexpectedError;
    }
    catch (...)
    {
        mpl::warn(category, "{} '{}'", error, key_string);
        *output = strdup("unknown error");
        return SettingResult::UnexpectedError;
    }
}

enum SettingResult set_setting(char* key, char* value, char** output)
{
    static constexpr auto error = "failed storing setting with key";
    const QString key_string{key};
    free(key);
    const QString value_string{value};
    free(value);
    try
    {
        std::call_once(initialize_settings_once_flag, mpc::register_global_settings_handlers);
        MP_SETTINGS.set(key_string, value_string);
        *output = nullptr;
        return SettingResult::Ok;
    }
    catch (const mp::UnrecognizedSettingException& e)
    {
        mpl::warn(category, "{} '{}'='{}': {}", error, key_string, value_string, e.what());
        *output = nullptr;
        return SettingResult::KeyNotFound;
    }
    catch (const mp::InvalidSettingException& e)
    {
        mpl::warn(category, "{} '{}'='{}': {}", error, key_string, value_string, e.what());
        *output = strdup(e.what());
        return SettingResult::InvalidValue;
    }
    catch (const std::exception& e)
    {
        mpl::warn(category, "{} '{}'='{}': {}", error, key_string, value_string, e.what());
        *output = strdup(e.what());
        return SettingResult::UnexpectedError;
    }
    catch (...)
    {
        mpl::warn(category, "{} '{}'='{}'", error, key_string, value_string);
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

long long memory_in_bytes(char* value)
{
    static constexpr auto error = "failed converting memory to bytes";
    try
    {
        std::string string_value{value};
        free(value);
        return mp::in_bytes(string_value);
    }
    catch (const std::exception& e)
    {
        mpl::warn(category, "{}: {}", error, e.what());
        return -1;
    }
    catch (...)
    {
        mpl::log_message(mpl::Level::warning, category, error);
        return -1;
    }
}

const char* human_readable_memory(long long bytes)
{
    const auto string =
        mp::MemorySize::from_bytes(bytes).human_readable(/*precision=*/2, /*trim_zeros=*/true);
    return strdup(string.c_str());
}

long long get_total_disk_size()
{
    const auto mp_storage = MP_PLATFORM.multipass_storage_location();
    const auto location = mp_storage.isEmpty()
                              ? MP_STDPATHS.writableLocation(mp::StandardPaths::AppDataLocation)
                              : mp_storage;
    QStorageInfo storageInfo{location};
    return storageInfo.bytesTotal();
}

char* default_mount_target(char* source)
{
    static constexpr auto error = "failed retrieving default mount target";
    try
    {
        const QString q_source{source};
        free(source);
        const auto target = MP_UTILS.default_mount_target(q_source).toStdString();
        return strdup(target.c_str());
    }
    catch (const std::exception& e)
    {
        mpl::warn(category, "{}: {}", error, e.what());
        return nullptr;
    }
    catch (...)
    {
        mpl::log_message(mpl::Level::warning, category, error);
        return nullptr;
    }
}
}
