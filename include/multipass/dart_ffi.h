#ifndef MULTIPASS_DART_FFI_H
#define MULTIPASS_DART_FFI_H

// clang-format off
extern "C"
{
// clang-format on
const char* multipass_version();

char* generate_petname();

char* get_server_address();

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

char* settings_file();

enum SettingResult get_setting(char* key, char** output);

enum SettingResult set_setting(char* key, char* value, char** output);

int uid();

int gid();

int default_id();

long long memory_in_bytes(char* value);

const char* human_readable_memory(long long bytes);

long long get_total_disk_size();

char* default_mount_target(char* source);
}

#endif // MULTIPASS_DART_FFI_H
