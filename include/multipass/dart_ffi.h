#ifndef MULTIPASS_DART_FFI_H
#define MULTIPASS_DART_FFI_H

// clang-format off
extern "C"
{
// clang-format on
const char* multipass_version();

const char* generate_petname();

const char* get_server_address();

struct KeyCertificatePair
{
    const char* pem_cert;
    const char* pem_priv_key;
};

struct KeyCertificatePair get_cert_pair();

enum SettingResult
{
    Ok,
    KeyNotFound,
    InvalidValue,
    UnexpectedError,
};

const char* settings_file();

enum SettingResult get_setting(const char* key, const char** output);

enum SettingResult set_setting(const char* key, const char* value, const char** output);

int uid();

int gid();

int default_id();

long long memory_in_bytes(const char* value);
}

#endif // MULTIPASS_DART_FFI_H
