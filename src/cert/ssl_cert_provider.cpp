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

#include <multipass/format.h>
#include <multipass/ssl_cert_provider.h>
#include <multipass/utils.h>

#include "biomem.h"

#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <QFile>

#include <array>
#include <cerrno>
#include <cstring>
#include <memory>
#include <vector>

namespace mp = multipass;

namespace
{
class WritableFile
{
public:
    explicit WritableFile(const QString& name) : fp{fopen(name.toStdString().c_str(), "wb"), fclose}
    {
        if (fp == nullptr)
            throw std::runtime_error(fmt::format("failed to open file '{}': {}({})", name, strerror(errno), errno));
    }

    FILE* get() const
    {
        return fp.get();
    }

private:
    std::unique_ptr<FILE, std::function<int(FILE*)>> fp;
};

class EVPKey
{
public:
    EVPKey()
    {
        if (key == nullptr)
            throw std::runtime_error("Failed to allocate EVP_PKEY");

        std::unique_ptr<EC_KEY, decltype(EC_KEY_free)*> ec_key(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1),
                                                               EC_KEY_free);
        if (ec_key == nullptr)
            throw std::runtime_error("Failed to allocate ec key structure");

        if (!EC_KEY_generate_key(ec_key.get()))
            throw std::runtime_error("Failed to generate key");

        if (!EVP_PKEY_assign_EC_KEY(key.get(), ec_key.get()))
            throw std::runtime_error("Failed to assign key");

        // EVPKey has ownership now
        ec_key.release();
    }

    std::string as_pem() const
    {
        mp::BIOMem mem;
        auto bytes = PEM_write_bio_PrivateKey(mem.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr);
        if (bytes == 0)
            throw std::runtime_error("Failed to export certificate in PEM format");
        return mem.as_string();
    }

    void write(const QString& name)
    {
        WritableFile file{name};
        if (!PEM_write_PrivateKey(file.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr))
            throw std::runtime_error(fmt::format("Failed writing certificate private key to file '{}'", name));

        QFile::setPermissions(name, QFile::ReadOwner);
    }

    EVP_PKEY* get() const
    {
        return key.get();
    }

private:
    std::unique_ptr<EVP_PKEY, decltype(EVP_PKEY_free)*> key{EVP_PKEY_new(), EVP_PKEY_free};
};

std::vector<unsigned char> as_vector(const std::string& v)
{
    return {v.begin(), v.end()};
}

std::string cn_name_from(const std::string& server_name)
{
    if (server_name.empty())
        return mp::utils::make_uuid().toStdString();
    return server_name;
}

void set_san_name(X509* c, const std::string& server_name)
{
    std::string san_dns = server_name;
    GENERAL_NAMES* gens = sk_GENERAL_NAME_new_null();
    GENERAL_NAME* gen = GENERAL_NAME_new();
    ASN1_IA5STRING* ia5 = ASN1_IA5STRING_new();
    ASN1_STRING_set(ia5, san_dns.data(), san_dns.length());
    GENERAL_NAME_set0_value(gen, GEN_DNS, ia5);
    sk_GENERAL_NAME_push(gens, gen);

    X509_add1_ext_i2d(c, NID_subject_alt_name, gens, 0, X509V3_ADD_DEFAULT);

    GENERAL_NAMES_free(gens);
}

class X509Cert
{
public:
    explicit X509Cert(const EVPKey& key, const std::string& server_name)
    {
        if (x509 == nullptr)
            throw std::runtime_error("Failed to allocate x509 cert structure");

        long big_num{0};
        auto rand_bytes = MP_UTILS.random_bytes(4);
        for (unsigned int i = 0; i < 4u; i++)
            big_num |= rand_bytes[i] << i * 8u;

        X509_set_version(x509.get(), 2);

        ASN1_INTEGER_set(X509_get_serialNumber(x509.get()), big_num);
        X509_gmtime_adj(X509_get_notBefore(x509.get()), 0);
        X509_gmtime_adj(X509_get_notAfter(x509.get()), 31536000L);

        constexpr int APPEND_ENTRY{-1};
        constexpr int ADD_RDN{0};

        auto country = as_vector("US");
        auto org = as_vector("Canonical");
        auto cn = as_vector(cn_name_from(server_name));

        auto name = X509_get_subject_name(x509.get());
        X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, country.data(), country.size(), APPEND_ENTRY, ADD_RDN);
        X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, org.data(), org.size(), APPEND_ENTRY, ADD_RDN);
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, cn.data(), cn.size(), APPEND_ENTRY, ADD_RDN);
        X509_set_issuer_name(x509.get(), name);
        set_san_name(x509.get(), server_name);

        if (!X509_set_pubkey(x509.get(), key.get()))
            throw std::runtime_error("Failed to set certificate public key");

        if (!X509_sign(x509.get(), key.get(), EVP_sha256()))
            throw std::runtime_error("Failed to sign certificate");
    }

    std::string as_pem()
    {
        mp::BIOMem mem;
        auto bytes = PEM_write_bio_X509(mem.get(), x509.get());
        if (bytes == 0)
            throw std::runtime_error("Failed to write certificate in PEM format");
        return mem.as_string();
    }

    void write(const QString& name)
    {
        WritableFile file{name};
        if (!PEM_write_X509(file.get(), x509.get()))
            throw std::runtime_error(fmt::format("Failed writing certificate to file '{}'", name));
    }

private:
    std::unique_ptr<X509, decltype(X509_free)*> x509{X509_new(), X509_free};
};

mp::SSLCertProvider::KeyCertificatePair make_cert_key_pair(const QDir& cert_dir, const std::string& server_name)
{
    QString prefix = server_name.empty() ? "multipass_cert" : QString::fromStdString(server_name);

    auto priv_key_path = cert_dir.filePath(prefix + "_key.pem");
    auto cert_path = cert_dir.filePath(prefix + ".pem");

    if (QFile::exists(priv_key_path) && QFile::exists(cert_path))
    {
        return {mp::utils::contents_of(cert_path), mp::utils::contents_of(priv_key_path)};
    }

    EVPKey key;
    X509Cert cert{key, server_name};

    key.write(priv_key_path);
    cert.write(cert_path);

    return {cert.as_pem(), key.as_pem()};
}

} // namespace

mp::SSLCertProvider::SSLCertProvider(const multipass::Path& cert_dir, const std::string& server_name)
    : key_cert_pair{make_cert_key_pair(cert_dir, server_name)}
{
}

mp::SSLCertProvider::SSLCertProvider(const multipass::Path& data_dir) : SSLCertProvider(data_dir, "")
{
}

std::string mp::SSLCertProvider::PEM_certificate() const
{
    return key_cert_pair.pem_cert;
}

std::string mp::SSLCertProvider::PEM_signing_key() const
{
    return key_cert_pair.pem_priv_key;
}
