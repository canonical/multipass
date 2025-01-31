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
#include <multipass/platform.h>
#include <multipass/ssl_cert_provider.h>
#include <multipass/utils.h>

#include "biomem.h"

#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <QFile>

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
    explicit WritableFile(const QString& file_path) : fp{fopen(file_path.toStdString().c_str(), "wb"), fclose}
    {
        if (fp == nullptr)
            throw std::runtime_error(
                fmt::format("failed to open file '{}': {}({})", file_path, strerror(errno), errno));
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

    void write(const QString& key_path) const
    {
        WritableFile file{key_path};
        if (!PEM_write_PrivateKey(file.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr))
            throw std::runtime_error(fmt::format("Failed writing certificate private key to file '{}'", key_path));

        QFile::setPermissions(key_path, QFile::ReadOwner);
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

        if (!X509_set_pubkey(x509.get(), key.get()))
            throw std::runtime_error("Failed to set certificate public key");

        if (!X509_sign(x509.get(), key.get(), EVP_sha256()))
            throw std::runtime_error("Failed to sign certificate");
    }

    explicit X509Cert(const EVPKey& key) // generate root certificate only
    {
        if (x509 == nullptr)
            throw std::runtime_error("Failed to allocate x509 cert structure");

        long big_num{0};
        const auto rand_bytes = MP_UTILS.random_bytes(4);
        for (unsigned int i = 0; i < 4u; i++)
            big_num |= rand_bytes[i] << i * 8u;

        X509_set_version(x509.get(), 2); // X.509 v3

        ASN1_INTEGER_set(X509_get_serialNumber(x509.get()), big_num);
        X509_gmtime_adj(X509_get_notBefore(x509.get()), 0);                      // Start time: now
        X509_gmtime_adj(X509_get_notAfter(x509.get()), 3650L * 24L * 60L * 60L); // Valid for 10 years

        constexpr int APPEND_ENTRY{-1};
        constexpr int ADD_RDN{0};

        const auto country = as_vector("US");
        const auto org = as_vector("Canonical");
        const auto cn = as_vector("Multipass Root CA");

        const auto name = X509_get_subject_name(x509.get());
        X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, country.data(), country.size(), APPEND_ENTRY, ADD_RDN);
        X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, org.data(), org.size(), APPEND_ENTRY, ADD_RDN);
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, cn.data(), cn.size(), APPEND_ENTRY, ADD_RDN);
        X509_set_issuer_name(x509.get(), name);

        if (!X509_set_pubkey(x509.get(), key.get()))
            throw std::runtime_error("Failed to set certificate public key");

        // Add X509v3 extensions
        X509V3_CTX ctx;
        X509V3_set_ctx(&ctx, x509.get(), x509.get(), NULL, NULL, 0);

        // wrap into function or struct
        X509_EXTENSION* ext;
        // Subject Key Identifier
        ext = X509V3_EXT_conf_nid(NULL, &ctx, NID_subject_key_identifier, "hash");
        X509_add_ext(x509.get(), ext, -1);
        X509_EXTENSION_free(ext);

        // Authority Key Identifier
        ext = X509V3_EXT_conf_nid(NULL, &ctx, NID_authority_key_identifier, "keyid:always");
        X509_add_ext(x509.get(), ext, -1);
        X509_EXTENSION_free(ext);

        // Basic Constraints: critical, CA:TRUE
        ext = X509V3_EXT_conf_nid(NULL, &ctx, NID_basic_constraints, "critical,CA:TRUE");
        X509_add_ext(x509.get(), ext, -1);
        X509_EXTENSION_free(ext);

        if (!X509_sign(x509.get(), key.get(), EVP_sha256()))
            throw std::runtime_error("Failed to sign certificate");
    }

    X509Cert(const EVPKey& root_certificate_key,
             const X509Cert& root_certificate,
             const EVPKey& server_key,
             const std::string& server_name)
    // generate signed server certificates
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
        X509_gmtime_adj(X509_get_notAfter(x509.get()), 31536000L); // Valid for 10 years

        constexpr int APPEND_ENTRY{-1};
        constexpr int ADD_RDN{0};

        const auto country = as_vector("US");
        const auto org = as_vector("Canonical");
        const auto cn = as_vector(cn_name_from(server_name));

        const auto server_certificate_name = X509_get_subject_name(x509.get());
        X509_NAME_add_entry_by_txt(server_certificate_name,
                                   "C",
                                   MBSTRING_ASC,
                                   country.data(),
                                   country.size(),
                                   APPEND_ENTRY,
                                   ADD_RDN);
        X509_NAME_add_entry_by_txt(server_certificate_name,
                                   "O",
                                   MBSTRING_ASC,
                                   org.data(),
                                   org.size(),
                                   APPEND_ENTRY,
                                   ADD_RDN);
        X509_NAME_add_entry_by_txt(server_certificate_name,
                                   "CN",
                                   MBSTRING_ASC,
                                   cn.data(),
                                   cn.size(),
                                   APPEND_ENTRY,
                                   ADD_RDN);
        // Set issuer name (from root certificate)
        X509_set_issuer_name(x509.get(), X509_get_subject_name(root_certificate.x509.get()));

        if (!X509_set_pubkey(x509.get(), server_key.get()))
            throw std::runtime_error("Failed to set certificate public key");

        // Add X509v3 extensions
        X509V3_CTX ctx;
        X509V3_set_ctx(&ctx, root_certificate.x509.get(), x509.get(), NULL, NULL, 0);

        // wrap into function or struct

        X509_EXTENSION* ext;
        // Subject Alternative Name
        ext = X509V3_EXT_conf_nid(NULL, &ctx, NID_subject_alt_name, ("DNS:" + server_name).c_str());
        X509_add_ext(x509.get(), ext, -1);
        X509_EXTENSION_free(ext);
        // Subject Key Identifier
        ext = X509V3_EXT_conf_nid(NULL, &ctx, NID_subject_key_identifier, "hash");
        X509_add_ext(x509.get(), ext, -1);
        X509_EXTENSION_free(ext);

        // Authority Key Identifier
        ext = X509V3_EXT_conf_nid(NULL, &ctx, NID_authority_key_identifier, "keyid:always,issuer");
        X509_add_ext(x509.get(), ext, -1);
        X509_EXTENSION_free(ext);

        // Basic Constraints: critical, CA:FALSE
        ext = X509V3_EXT_conf_nid(NULL, &ctx, NID_basic_constraints, "critical,CA:FALSE");
        X509_add_ext(x509.get(), ext, -1);
        X509_EXTENSION_free(ext);

        if (!X509_sign(x509.get(), root_certificate_key.get(), EVP_sha256()))
            throw std::runtime_error("Failed to sign certificate");
    }

    std::string as_pem() const
    {
        mp::BIOMem mem;
        auto bytes = PEM_write_bio_X509(mem.get(), x509.get());
        if (bytes == 0)
            throw std::runtime_error("Failed to write certificate in PEM format");
        return mem.as_string();
    }

    void write(const QString& cert_path) const
    {
        WritableFile file{cert_path};
        if (!PEM_write_X509(file.get(), x509.get()))
            throw std::runtime_error(fmt::format("Failed writing certificate to file '{}'", cert_path));
    }

private:
    std::unique_ptr<X509, decltype(X509_free)*> x509{X509_new(), X509_free};
};

mp::SSLCertProvider::KeyCertificatePair make_cert_key_pair(const QDir& cert_dir, const std::string& server_name)
{
    const QString prefix = server_name.empty() ? "multipass_cert" : QString::fromStdString(server_name);

    const auto priv_key_path = cert_dir.filePath(prefix + "_key.pem");
    const auto cert_path = cert_dir.filePath(prefix + ".pem");

    if (QFile::exists(priv_key_path) && QFile::exists(cert_path))
    {
        return {mp::utils::contents_of(cert_path), mp::utils::contents_of(priv_key_path)};
    }

    if (!server_name.empty())
    {
        const EVPKey root_cert_key;
        const auto priv_root_key_path = cert_dir.filePath(prefix + "_root_key.pem");
        const std::filesystem::path root_cert_path = MP_PLATFORM.get_root_cert_path();

        const X509Cert root_cert{root_cert_key};
        root_cert_key.write(priv_root_key_path);
        root_cert.write(root_cert_path.u8string().c_str());

        const EVPKey server_cert_key;
        const X509Cert signed_server_cert{root_cert_key, root_cert, server_cert_key, server_name};
        server_cert_key.write(priv_key_path);
        signed_server_cert.write(cert_path);
        return {signed_server_cert.as_pem(), server_cert_key.as_pem()};
    }
    else
    {
        const EVPKey client_cert_key;
        const X509Cert client_cert{client_cert_key, server_name};
        client_cert_key.write(priv_key_path);
        client_cert.write(cert_path);

        return {client_cert.as_pem(), client_cert_key.as_pem()};
    }
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
