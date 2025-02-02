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
        if (x509 == nullptr)
            throw std::runtime_error("Failed to allocate x509 cert structure");

        long big_num{0};
        const auto rand_bytes = MP_UTILS.random_bytes(4);
        for (unsigned int i = 0; i < 4u; i++)
            big_num |= rand_bytes[i] << i * 8u;

        X509_set_version(x509.get(), 2); // X.509 v3

        ASN1_INTEGER_set(X509_get_serialNumber(x509.get()), big_num);
        X509_gmtime_adj(X509_get_notBefore(x509.get()), 0); // Start time: now
        const long valid_duration_sec = cert_type == CertType::Root ? 3650L * 24L * 60L * 60L : 365L * 24L * 60L * 60L;
        // 10 years for root certicicate and 1 year for server and client certificate
        X509_gmtime_adj(X509_get_notAfter(x509.get()), valid_duration_sec);

        constexpr int APPEND_ENTRY{-1};
        constexpr int ADD_RDN{0};

        const auto country = as_vector("US");
        const auto org = as_vector("Canonical");
        const auto cn = as_vector(cert_type == CertType::Root     ? "Multipass Root CA"
                                  : cert_type == CertType::Client ? mp::utils::make_uuid().toStdString()
                                                                  : server_name);
        const auto subject_name = X509_get_subject_name(x509.get());
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
            cert_type == CertType::Server ? X509_get_subject_name(root_certificate.value().x509.get()) : subject_name;
        X509_set_issuer_name(x509.get(), issuer_name);

        if (!X509_set_pubkey(x509.get(), key.get()))
            throw std::runtime_error("Failed to set certificate public key");

        const auto& issuer_x509 = cert_type == CertType::Server ? root_certificate.value().x509 : x509;
        // Add X509v3 extensions
        X509V3_CTX ctx;
        X509V3_set_ctx(&ctx, issuer_x509.get(), x509.get(), NULL, NULL, 0);

        // wrap into function or struct
        X509_EXTENSION* ext;
        // Subject Alternative Name
        if (cert_type == CertType::Server)
        {
            ext = X509V3_EXT_conf_nid(NULL, &ctx, NID_subject_alt_name, ("DNS:" + server_name).c_str());
            X509_add_ext(x509.get(), ext, -1);
            X509_EXTENSION_free(ext);
        }

        // Subject Key Identifier
        ext = X509V3_EXT_conf_nid(NULL, &ctx, NID_subject_key_identifier, "hash");
        X509_add_ext(x509.get(), ext, -1);
        X509_EXTENSION_free(ext);

        // Authority Key Identifier
        const std::string is_from_issuer = cert_type == CertType::Server ? ",issuer" : "";
        ext = X509V3_EXT_conf_nid(NULL, &ctx, NID_authority_key_identifier, ("keyid:always" + is_from_issuer).c_str());
        X509_add_ext(x509.get(), ext, -1);
        X509_EXTENSION_free(ext);

        // Basic Constraints: critical, CA:TRUE or CA:FALSE
        const std::string is_ca = cert_type == CertType::Root ? "TRUE" : "FALSE";
        ext = X509V3_EXT_conf_nid(NULL, &ctx, NID_basic_constraints, ("critical,CA:" + is_ca).c_str());
        X509_add_ext(x509.get(), ext, -1);
        X509_EXTENSION_free(ext);

        const auto& signing_key = cert_type == CertType::Server ? *root_certificate_key : key;
        if (!X509_sign(x509.get(), signing_key.get(), EVP_sha256()))
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
        const auto priv_root_key_path = cert_dir.filePath(prefix + "_root_key.pem");
        const std::filesystem::path root_cert_path = MP_PLATFORM.get_root_cert_path();

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
