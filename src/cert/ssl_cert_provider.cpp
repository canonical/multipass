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
#include <multipass/logging/log.h>
#include <multipass/platform.h>
#include <multipass/ssl_cert_provider.h>
#include <multipass/utils.h>

#include "biomem.h"

#include <openssl/core_names.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509v3.h>

#include <cerrno>
#include <cstring>
#include <memory>
#include <vector>

namespace mp = multipass;
namespace mpl = mp::logging;

namespace
{
constexpr auto kLogCategory = "ssl-cert-provider";
// utility function for checking return code or raw pointer from openssl C-apis
// TODO: constrain T to int or raw pointer once C++20 concepts is available
template <typename T>
void openssl_check(T result, const std::string& errorMessage)
{
    // TODO: expand std::is_pointer_v<T> check to cover all smart pointers as well
    if constexpr (std::is_pointer_v<T>)
    {
        if (result == nullptr)
        {
            throw std::runtime_error(errorMessage);
        }
    }
    else
    {
        if (result <= 0)
        {
            throw std::runtime_error(
                fmt::format("{}, with the error code {}", errorMessage, result));
        }
    }
}

class WritableFile
{
public:
    explicit WritableFile(const QString& file_path) : fp{open_file(file_path)}
    {
    }

    FILE* get() const
    {
        return fp.get();
    }

private:
    // decltype(&fclose) does not preserve these some extra function attributes of fclose, leads to
    // warning and compilation error
    using FilePtr = std::unique_ptr<FILE, int (*)(FILE*)>;
    [[nodiscard]] static FilePtr open_file(const QString& file_path)
    {
        const std::filesystem::path file_path_std{file_path.toStdString()};
        std::filesystem::create_directories(file_path_std.parent_path());
        // make sure the parent directory exist

        const auto raw_fp = fopen(file_path_std.string().c_str(), "wb");
        openssl_check(
            raw_fp,
            fmt::format("failed to open file '{}': {}({})", file_path, strerror(errno), errno));

        return FilePtr{raw_fp, fclose};
    }

    FilePtr fp;
};

class EVPKey
{
public:
    EVPKey() : key{create_key()}
    {
    }

    std::string as_pem() const
    {
        mp::BIOMem mem;
        openssl_check(
            PEM_write_bio_PrivateKey(mem.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr),
            "Failed to export certificate in PEM format");
        return mem.as_string();
    }

    void write(const QString& key_path) const
    {
        const std::filesystem::path key_path_std = key_path.toStdU16String();
        if (std::filesystem::exists(key_path_std))
        {
            // enable fopen in WritableFile with wb mode
            MP_PLATFORM.set_permissions(key_path_std,
                                        std::filesystem::perms::owner_read |
                                            std::filesystem::perms::owner_write);
        }

        WritableFile file{key_path};
        openssl_check(
            PEM_write_PrivateKey(file.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr),
            fmt::format("Failed writing certificate private key to file '{}'", key_path));

        MP_PLATFORM.set_permissions(key_path_std, std::filesystem::perms::owner_read);
    }

    EVP_PKEY* get() const
    {
        return key.get();
    }

private:
    using EVPKeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;

    [[nodiscard]] static EVPKeyPtr create_key()
    {
        std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> ctx(
            EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr),
            EVP_PKEY_CTX_free);

        openssl_check(ctx.get(), "Failed to create EVP_PKEY_CTX");
        openssl_check(EVP_PKEY_keygen_init(ctx.get()), "Failed to initialize key generation");

        // Set EC curve (P-256)
        const std::array<OSSL_PARAM, 2> params = {
            // the 3rd argument is length of the buffer, which is 0 in the case of static buffer
            // like "P-256"
            OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME,
                                             const_cast<char*>("P-256"),
                                             0),
            OSSL_PARAM_construct_end()};

        openssl_check(EVP_PKEY_CTX_set_params(ctx.get(), params.data()),
                      "EVP_PKEY_CTX_set_params() failed");

        // Generate the key
        EVP_PKEY* raw_key = nullptr;
        openssl_check(EVP_PKEY_generate(ctx.get(), &raw_key), "Failed to generate EC key");

        return EVPKeyPtr(raw_key, EVP_PKEY_free);
    }

    EVPKeyPtr key;
};

std::vector<unsigned char> as_vector(const std::string& v)
{
    return {v.begin(), v.end()};
}

void set_random_serial_number(X509* cert)
{
    // OpenSSL recommends a 20-byte (160-bit) serial number
    std::array<uint8_t, 20> serial_bytes{};
    openssl_check(RAND_bytes(serial_bytes.data(), serial_bytes.size()),
                  "Failed to set random bytes\n");
    // Set the highest bit to 0 (unsigned) to ensure it's positive
    serial_bytes[0] &= 0x7F;

    // Convert bytes to an BIGNUM, an arbitrary-precision integer type
    std::unique_ptr<BIGNUM, decltype(&BN_free)> bn(
        BN_bin2bn(serial_bytes.data(), serial_bytes.size(), nullptr),
        BN_free);
    openssl_check(bn.get(), "Failed to convert serial bytes to BIGNUM\n");
    assert(!BN_is_negative(bn.get()));

    // Convert BIGNUM to ASN1_INTEGER and set it as the certificate serial number
    // ASN1 is a standard binary format for encoding data like serial numbers in X.509 certificates
    ASN1_INTEGER* serial = BN_to_ASN1_INTEGER(bn.get(), nullptr);
    openssl_check(serial, "Failed to convert serial bytes to BIGNUM\n");

    // Set the serial number in the certificate
    openssl_check(X509_set_serialNumber(cert, serial), "Failed to set serial number!\n");
}

/**
 * Check whether this certificate is the issuer (signer) of the given certificate.
 *
 * @param [in] signed_cert The certificate to check
 * @return True if this certificate signed @p signed_cert; false otherwise.
 */
bool is_issuer_of(X509& issuer, X509& signed_cert)
{
    // Get the public key of this certificate (issuer)
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pubkey{X509_get_pubkey(&issuer),
                                                               &EVP_PKEY_free};
    openssl_check(pubkey.get(), "Failed to get public key from certificate");
    // Verify that signed_cert is issued by this certificate.
    return X509_verify(&signed_cert, pubkey.get()) == 1;
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
    // generate root, client or signed server certificate, the third one requires the last three
    // arguments populated
    {
        openssl_check(cert.get(), "Failed to allocate x509 cert structure");

        X509_set_version(cert.get(), 2); // 0 index based, 2 amounts to 3

        set_random_serial_number(cert.get());
        X509_gmtime_adj(X509_get_notBefore(cert.get()), 0); // Start time: now

        constexpr std::chrono::seconds one_year = std::chrono::hours{24} * 365;
        constexpr std::chrono::seconds ten_years = one_year * 10;
        const auto valid_duration = cert_type == CertType::Root ? ten_years : one_year;
        X509_gmtime_adj(X509_get_notAfter(cert.get()), valid_duration.count());

        constexpr int APPEND_ENTRY{-1};
        constexpr int ADD_RDN{0};

        const auto country = as_vector("US");
        const auto org = as_vector("Canonical");
        const auto cn =
            as_vector(cert_type == CertType::Root     ? "Multipass Root CA"
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
        X509_NAME_add_entry_by_txt(subject_name,
                                   "O",
                                   MBSTRING_ASC,
                                   org.data(),
                                   org.size(),
                                   APPEND_ENTRY,
                                   ADD_RDN);
        X509_NAME_add_entry_by_txt(subject_name,
                                   "CN",
                                   MBSTRING_ASC,
                                   cn.data(),
                                   cn.size(),
                                   APPEND_ENTRY,
                                   ADD_RDN);

        const auto issuer_name = cert_type == CertType::Server
                                     ? X509_get_subject_name(root_certificate.value().cert.get())
                                     : subject_name;
        X509_set_issuer_name(cert.get(), issuer_name);

        openssl_check(X509_set_pubkey(cert.get(), key.get()),
                      "Failed to set certificate public key");

        const auto& issuer_cert =
            cert_type == CertType::Server ? root_certificate.value().cert : cert;
        // Add X509v3 extensions
        X509V3_CTX ctx{};
        X509V3_set_ctx(&ctx, issuer_cert.get(), cert.get(), nullptr, nullptr, 0);

        // Subject Alternative Name
        if (cert_type == CertType::Server)
        {
            add_extension(ctx, NID_subject_alt_name, ("DNS:" + server_name).c_str());
        }

        // Subject Key Identifier
        add_extension(ctx, NID_subject_key_identifier, "hash");

        // Authority Key Identifier
        add_extension(
            ctx,
            NID_authority_key_identifier,
            (std::string("keyid:always") + (cert_type == CertType::Server ? ",issuer" : ""))
                .c_str());

        // Basic Constraints: critical, CA:TRUE or CA:FALSE
        add_extension(
            ctx,
            NID_basic_constraints,
            (std::string("critical,CA:") + (cert_type == CertType::Root ? "TRUE" : "FALSE"))
                .c_str());

        const auto& signing_key = cert_type == CertType::Server ? *root_certificate_key : key;

        openssl_check(X509_sign(cert.get(), signing_key.get(), EVP_sha256()),
                      "Failed to sign certificate");
    }

    std::string as_pem() const
    {
        mp::BIOMem mem;
        openssl_check(PEM_write_bio_X509(mem.get(), cert.get()),
                      "Failed to write certificate in PEM format");
        return mem.as_string();
    }

    void write(const QString& cert_path) const
    {
        WritableFile file{cert_path};

        openssl_check(PEM_write_X509(file.get(), cert.get()),
                      fmt::format("Failed writing certificate to file '{}'", cert_path));
        const std::filesystem::path cert_path_std = cert_path.toStdU16String();
        MP_PLATFORM.set_permissions(cert_path_std,
                                    std::filesystem::perms::owner_all |
                                        std::filesystem::perms::group_read |
                                        std::filesystem::perms::others_read);
    }

private:
    void add_extension(X509V3_CTX& ctx, int nid, const char* value)
    {
        const std::unique_ptr<X509_EXTENSION, decltype(&X509_EXTENSION_free)> ext(
            X509V3_EXT_conf_nid(nullptr, &ctx, nid, value),
            X509_EXTENSION_free);

        openssl_check(ext.get(), "Failed to create X509 extension");
        openssl_check(X509_add_ext(cert.get(), ext.get(), -1), "Failed to add X509 extension");
    }

    std::unique_ptr<X509, decltype(&X509_free)> cert{X509_new(), X509_free};
};

std::unique_ptr<X509, decltype(&X509_free)> load_cert_from_file(const std::string& path)
{
    std::unique_ptr<FILE, int (*)(FILE*)> file{fopen(path.c_str(), "r"), &fclose};
    if (!file)
        return {nullptr, X509_free};

    return {PEM_read_X509(file.get(), nullptr, nullptr, nullptr), X509_free};
}

mp::SSLCertProvider::KeyCertificatePair make_cert_key_pair(const QDir& cert_dir,
                                                           const std::string& server_name)
{
    const QString prefix =
        server_name.empty() ? "multipass_cert" : QString::fromStdString(server_name);

    const auto priv_key_path = cert_dir.filePath(prefix + "_key.pem");
    const auto cert_path = cert_dir.filePath(prefix + ".pem");

    if (!server_name.empty())
    {
        const std::filesystem::path root_cert_path = MP_PLATFORM.get_root_cert_path();
        if (std::filesystem::exists(root_cert_path) && QFile::exists(priv_key_path) &&
            QFile::exists(cert_path))
        {
            // Ensure that we can load both certificates
            const auto root_cert = load_cert_from_file(root_cert_path.string());
            const auto cert = load_cert_from_file(cert_path.toStdString());

            if (root_cert && cert)
            {
                mpl::debug(kLogCategory,
                           "Certificates for the gRPC server (root: {}, subordinate: {}) are valid "
                           "X.509 files",
                           root_cert_path,
                           cert_path.toStdString());

                // FIXME: Also check the validity period of the certificates to decide if they need
                // to be re-generated

                // Validate root cert is the issuer(signer) of the subordinate certificate
                if (is_issuer_of(*root_cert.get(), *cert.get()))
                {
                    mpl::info(kLogCategory, "Re-using existing certificates for the gRPC server");

                    // Unlike other daemon files, the root certificate needs to be accessible by
                    // everyone
                    MP_PLATFORM.set_permissions(root_cert_path,
                                                std::filesystem::perms::owner_all |
                                                    std::filesystem::perms::group_read |
                                                    std::filesystem::perms::others_read);
                    return {mp::utils::contents_of(cert_path),
                            mp::utils::contents_of(priv_key_path)};
                }

                mpl::warn(kLogCategory,
                          "Existing root certificate (`{}`) is not the signer of the gRPC "
                          "server certificate (`{}`)",
                          root_cert_path,
                          cert_path.toStdString());
            }
            else
            {
                mpl::warn(kLogCategory,
                          "Could not load either of the root (`{}`) or subordinate (`{}`) "
                          "certificates for the gRPC server",
                          root_cert_path,
                          cert_path.toStdString());
            }
        }
        mpl::info(kLogCategory, "Regenerating certificates for the gRPC server");

        const auto priv_root_key_path = cert_dir.filePath(prefix + "_root_key.pem");

        EVPKey root_cert_key{};
        X509Cert root_cert{root_cert_key, X509Cert::CertType::Root};
        root_cert_key.write(priv_root_key_path);
        root_cert.write(root_cert_path.u8string().c_str());

        const EVPKey server_cert_key{};
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
            // FIXME: The client does not respect the log level and this always get printed
            // even on `multipass list`
            // Re-enable it after fixing.
            // mpl::trace(kLogCategory, "Re-using existing certificates for the gRPC client");
            return {mp::utils::contents_of(cert_path), mp::utils::contents_of(priv_key_path)};
        }

        // mpl::trace(kLogCategory, "Regenerating certificates for the gRPC client");
        const EVPKey client_cert_key{};
        const X509Cert client_cert{client_cert_key, X509Cert::CertType::Client};
        client_cert_key.write(priv_key_path);
        client_cert.write(cert_path);

        MP_PLATFORM.set_permissions(priv_key_path.toStdU16String(),
                                    std::filesystem::perms::owner_all |
                                        std::filesystem::perms::group_read |
                                        std::filesystem::perms::others_read);

        return {client_cert.as_pem(), client_cert_key.as_pem()};
    }
}
} // namespace

mp::SSLCertProvider::SSLCertProvider(const multipass::Path& cert_dir,
                                     const std::string& server_name)
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
