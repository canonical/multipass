#ifndef MULTIPASS_DART_FFI_H
#define MULTIPASS_DART_FFI_H

extern "C" const char* multipass_version();

extern "C" const char* generate_petname();

extern "C" const char* get_server_address();

extern "C" struct KeyCertificatePair
{
    const char* pem_cert;
    const char* pem_priv_key;
};

extern "C" struct KeyCertificatePair get_cert_pair();

enum SettingResult
{
    Ok,
    KeyNotFound,
    InvalidValue,
    UnexpectedError,
};

extern "C" enum SettingResult get_setting(const char* key, const char** output);

extern "C" enum SettingResult set_setting(const char* key, const char* value, const char** output);

extern "C" int uid();

extern "C" int gid();

extern "C" int default_id();

extern "C" long long memory_in_bytes(const char* value);

#endif // MULTIPASS_DART_FFI_H
