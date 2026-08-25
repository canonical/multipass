#pragma once
#include <stdint.h>
// clang-format off
extern "C"
{
// clang-format on
const char* multipass_version();

char* generate_petname();

char* get_server_address();

enum VsockHostTag : uint32_t
{
    VSOCK_NONE = 0,
    VSOCK_HVSOCK = 1,
    VSOCK_VSOCK = 2,
    VSOCK_USOCK = 3
};

union VsockHostUnion
{
    const char* hvsock_vmid;
    uint32_t vsock_cid;
    const char* usock_addr;
};

struct SSHCoordinatesFfi
{
    char* username;
    char* private_key_as_base64;
    uint32_t port;
    char* tcp_host;
    enum VsockHostTag vsock_host_tag;
    union VsockHostUnion vsock_host;
};

struct KeyCertificatePair
{
    const char* pem_cert;
    const char* pem_priv_key;
};

struct KeyCertificatePair get_cert_pair();

char* get_root_cert();

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

// Connects to the guest over the vsock-family transport in `coordinates`
// (HVSOCK/VSOCK/USOCK) and returns a connected, blocking socket fd. Returns a
// negative value if the transport is unset/unsupported or the connection fails.
int open_vsock_socket(const struct SSHCoordinatesFfi* coordinates);

// Shuts down both directions of an fd from open_vsock_socket so a blocked
// read() sees EOF. The native side handles platform specifics.
void shutdown_socket(int fd);
}
