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

#endif // MULTIPASS_DART_FFI_H
