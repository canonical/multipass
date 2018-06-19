/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include "ssl_cert_provider.h"

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include <memory>
#include <vector>

namespace mp = multipass;

namespace
{
struct EVPKeyDeleter
{
    void operator()(EVP_PKEY* key)
    {
        EVP_PKEY_free(key);
    }
};

struct RSADeleter
{
    void operator()(RSA* rsa)
    {
        RSA_free(rsa);
    }
};

struct BIGNUMDeleter
{
    void operator()(BIGNUM* num)
    {
        BN_free(num);
    }
};

struct X509Deleter
{
    void operator()(X509* x509)
    {
        X509_free(x509);
    }
};

struct BIODeleter
{
    void operator()(BIO* bio)
    {
        BIO_free(bio);
    }
};

auto make_key()
{
    std::unique_ptr<EVP_PKEY, EVPKeyDeleter> ec_pkey{EVP_PKEY_new()};
    if (ec_pkey == nullptr)
        throw std::runtime_error("Failed to allocate private key structure");

    std::unique_ptr<RSA, RSADeleter> rsa{RSA_new()};
    if (rsa == nullptr)
        throw std::runtime_error("Failed to allocate RSA structure");

    std::unique_ptr<BIGNUM, BIGNUMDeleter> value{BN_new()};
    if (value == nullptr)
        throw std::runtime_error("Failed to allocate BIGNUM structure");

    if (!BN_set_word(value.get(), RSA_F4))
        throw std::runtime_error("Failed to set BIGNUM to RSA_F4");

    if (!RSA_generate_key_ex(rsa.get(), 2048, value.get(), nullptr))
        throw std::runtime_error("Failed to generate RSA key");

    if (!EVP_PKEY_assign_RSA(ec_pkey.get(), rsa.release()))
        throw std::runtime_error("Unable to assign rsa key");

    return ec_pkey;
}

auto make_certificate(EVP_PKEY* key)
{
    std::unique_ptr<X509, X509Deleter> x509_up{X509_new()};
    if (x509_up == nullptr)
        throw std::runtime_error("Failed to allocate x509 structure");

    auto x509 = x509_up.get();

    ASN1_INTEGER_set(X509_get_serialNumber(x509), 42);
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), 31536000L);
    X509_set_pubkey(x509, key);

    X509_NAME * name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(name, "C",  MBSTRING_ASC, (unsigned char *)"CA",        -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O",  MBSTRING_ASC, (unsigned char *)"Canonical", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char *)"localhost", -1, -1, 0);
    X509_set_issuer_name(x509, name);

    if(!X509_sign(x509, key, EVP_sha256()))
        throw std::runtime_error("Failed to sign certificate");

    return x509_up;
}

std::string to_pem(X509* cert)
{
    std::unique_ptr<BIO, BIODeleter> bio{BIO_new(BIO_s_mem())};
    if (bio == nullptr)
        throw std::runtime_error("Failed to create BIO structure");

    auto bytes = PEM_write_bio_X509(bio.get(), cert);
    if (bytes == 0)
        throw std::runtime_error("Failed to write certificate in PEM format");

    std::vector<char> pem(bio->num_write);

    BIO_read(bio.get(), pem.data(), pem.size());

    return {pem.begin(), pem.end()};
}

} // namespace

std::string mp::SSLCertProvider::certificate_as_base64() const
{
    auto key = make_key();
    auto cert = make_certificate(key.get());
    return to_pem(cert.get());
}
