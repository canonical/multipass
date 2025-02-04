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

#include <openssl/core_names.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>

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
        std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> ctx(
            EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr),
            EVP_PKEY_CTX_free);

        if (!ctx || EVP_PKEY_keygen_init(ctx.get()) <= 0)
        {
            throw std::runtime_error("Failed to initialize key generation");
        }

        // Set EC curve (P-256)
        const std::array<OSSL_PARAM, 2> params = {
            // the 3rd argument is length of the buffer, which is 0 in the case of static buffer like "P-256"
            OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, const_cast<char*>("P-256"), 0),
            OSSL_PARAM_construct_end()};
        EVP_PKEY_CTX_set_params(ctx.get(), params.data());

        // Generate the key
        EVP_PKEY* raw_key = nullptr;
        if (EVP_PKEY_generate(ctx.get(), &raw_key) <= 0)
        {
            throw std::runtime_error("Failed to generate EC key");
        }

        // Assign generated key to unique_ptr for RAII management
        key.reset(raw_key);
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
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key{nullptr, EVP_PKEY_free};
};

std::vector<unsigned char> as_vector(const std::string& v)
{
    return {v.begin(), v.end()};
}

class X509Cert
{
public:
    enum class CertType
    {
        Root,
        Client,
        Server
    };

    explicit X509Cert(const EVPKey& key,
                      CertType cert_type,
                      const std::string& server_name = "",
                      const std::optional<EVPKey>& root_certificate_key = std::nullopt,
                      const std::optional<X509Cert>& root_certificate = std::nullopt)
    // generate root, client or signed server certificate, the third one requires the last three arguments populated
    {
        if (cert == nullptr)
            throw std::runtime_error("Failed to allocate x509 cert structure");

        long big_num{0};
        const auto rand_bytes = MP_UTILS.random_bytes(4);
        for (unsigned int i = 0; i < 4u; i++)
            big_num |= rand_bytes[i] << i * 8u;

        X509_set_version(cert.get(), 2); // X.509 v3

        ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), big_num);
        X509_gmtime_adj(X509_get_notBefore(cert.get()), 0); // Start time: now
        const long valid_duration_sec = cert_type == CertType::Root ? 3650L * 24L * 60L * 60L : 365L * 24L * 60L * 60L;
        // 10 years for root certicicate and 1 year for server and client certificate
        X509_gmtime_adj(X509_get_notAfter(cert.get()), valid_duration_sec);

        constexpr int APPEND_ENTRY{-1};
        constexpr int ADD_RDN{0};

        const auto country = as_vector("US");
        const auto org = as_vector("Canonical");
        const auto cn = as_vector(cert_type == CertType::Root     ? "Multipass Root CA"
                                  : cert_type == CertType::Client ? mp::utils::make_uuid().toStdString()
                                                                  : server_name);
        const auto subject_name = X509_get_subject_name(cert.get());
        X509_NAME_add_entry_by_txt(subject_name,
                                   "C",
                                   MBSTRING_ASC,
                                   country.data(),
                                   country.size(),
                                   APPEND_ENTRY,
                                   ADD_RDN);
        X509_NAME_add_entry_by_txt(subject_name, "O", MBSTRING_ASC, org.data(), org.size(), APPEND_ENTRY, ADD_RDN);
        X509_NAME_add_entry_by_txt(subject_name, "CN", MBSTRING_ASC, cn.data(), cn.size(), APPEND_ENTRY, ADD_RDN);

        const auto issuer_name =
            cert_type == CertType::Server ? X509_get_subject_name(root_certificate.value().cert.get()) : subject_name;
        X509_set_issuer_name(cert.get(), issuer_name);

        if (!X509_set_pubkey(cert.get(), key.get()))
            throw std::runtime_error("Failed to set certificate public key");

        const auto& issuer_cert = cert_type == CertType::Server ? root_certificate.value().cert : cert;
        // Add X509v3 extensions
        X509V3_CTX ctx;
        X509V3_set_ctx(&ctx, issuer_cert.get(), cert.get(), nullptr, nullptr, 0);

        // Subject Alternative Name
        if (cert_type == CertType::Server)
        {
            add_extension(ctx, NID_subject_alt_name, ("DNS:" + server_name).c_str());
        }

        // Subject Key Identifier
        add_extension(ctx, NID_subject_key_identifier, "hash");

        // Authority Key Identifier
        add_extension(ctx,
                      NID_authority_key_identifier,
                      (std::string("keyid:always") + (cert_type == CertType::Server ? ",issuer" : "")).c_str());

        // Basic Constraints: critical, CA:TRUE or CA:FALSE
        add_extension(ctx,
                      NID_basic_constraints,
                      (std::string("critical,CA:") + (cert_type == CertType::Root ? "TRUE" : "FALSE")).c_str());

        const auto& signing_key = cert_type == CertType::Server ? *root_certificate_key : key;
        if (!X509_sign(cert.get(), signing_key.get(), EVP_sha256()))
            throw std::runtime_error("Failed to sign certificate");
    }

    std::string as_pem() const
    {
        mp::BIOMem mem;
        auto bytes = PEM_write_bio_X509(mem.get(), cert.get());
        if (bytes == 0)
            throw std::runtime_error("Failed to write certificate in PEM format");
        return mem.as_string();
    }

    void write(const QString& cert_path) const
    {
        WritableFile file{cert_path};
        if (!PEM_write_X509(file.get(), cert.get()))
            throw std::runtime_error(fmt::format("Failed writing certificate to file '{}'", cert_path));
    }

private:
    void add_extension(X509V3_CTX& ctx, int nid, const char* value)
    {
        const std::unique_ptr<X509_EXTENSION, decltype(&X509_EXTENSION_free)> ext(
            X509V3_EXT_conf_nid(nullptr, &ctx, nid, value),
            X509_EXTENSION_free);

        if (!ext)
        {
            throw std::runtime_error("Failed to create X509 extension");
        }

        if (!X509_add_ext(cert.get(), ext.get(), -1))
        {
            throw std::runtime_error("Failed to add X509 extension");
        }
    }

    std::unique_ptr<X509, decltype(&X509_free)> cert{X509_new(), X509_free};
};

mp::SSLCertProvider::KeyCertificatePair make_cert_key_pair(const QDir& cert_dir, const std::string& server_name)
{
    const QString prefix = server_name.empty() ? "multipass_cert" : QString::fromStdString(server_name);

    const auto priv_key_path = cert_dir.filePath(prefix + "_key.pem");
    const auto cert_path = cert_dir.filePath(prefix + ".pem");

    if (!server_name.empty())
    {
        const std::filesystem::path root_cert_path = MP_PLATFORM.get_root_cert_path();
        if (std::filesystem::exists(root_cert_path) && QFile::exists(priv_key_path) && QFile::exists(cert_path))
        {
            return {mp::utils::contents_of(cert_path), mp::utils::contents_of(priv_key_path)};
        }

        const auto priv_root_key_path = cert_dir.filePath(prefix + "_root_key.pem");

        EVPKey root_cert_key;
        X509Cert root_cert{root_cert_key, X509Cert::CertType::Root};
        root_cert_key.write(priv_root_key_path);
        root_cert.write(root_cert_path.u8string().c_str());

        const EVPKey server_cert_key;
        const X509Cert signed_server_cert{server_cert_key,
                                          X509Cert::CertType::Server,
                                          server_name,
                                          std::move(root_cert_key),
                                          std::move(root_cert)};
        server_cert_key.write(priv_key_path);
        signed_server_cert.write(cert_path);
        return {signed_server_cert.as_pem(), server_cert_key.as_pem()};
    }
    else
    {
        if (QFile::exists(priv_key_path) && QFile::exists(cert_path))
        {
            return {mp::utils::contents_of(cert_path), mp::utils::contents_of(priv_key_path)};
        }

        const EVPKey client_cert_key;
        const X509Cert client_cert{client_cert_key, X509Cert::CertType::Client};
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

std::string mp::SSLCertProvider::PEM_certificate() const
{
    return key_cert_pair.pem_cert;
}

std::string mp::SSLCertProvider::PEM_signing_key() const
{
    return key_cert_pair.pem_priv_key;
}
