/*
 * Copyright (C) 2018-2021 Canonical, Ltd.
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

#include <multipass/client_cert_store.h>
#include <multipass/utils.h>

#include "biomem.h"

#include <openssl/pem.h>
#include <openssl/x509.h>

#include <QDir>
#include <QFile>

#include <fstream>
#include <stdexcept>

namespace mp = multipass;

namespace
{
constexpr auto chain_name = "multipass_client_certs.pem";

auto load_certs_from_file(const multipass::Path& cert_dir)
{
    std::vector<std::string> certs;
    std::string cert;
    auto path = QDir(cert_dir).filePath(chain_name);

    if (QFile::exists(path))
    {
        std::ifstream cert_file{path.toStdString()};
        std::string line;
        while (std::getline(cert_file, line))
        {
            // Re-add the newline to make comparing of cert sent by client easier
            cert.append(line + '\n');

            if (line == "-----END CERTIFICATE-----")
            {
                certs.push_back(cert);
                cert.clear();
            }
        }
    }

    return certs;
}

void validate_certificate(const std::string& pem_cert)
{
    mp::BIOMem bio{pem_cert};
    auto raw_cert = PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr);
    std::unique_ptr<X509, decltype(X509_free)*> x509{raw_cert, X509_free};

    if (raw_cert == nullptr)
        throw std::runtime_error("invalid certificate data");
}
} // namespace

mp::ClientCertStore::ClientCertStore(const multipass::Path& cert_dir)
    : cert_dir{cert_dir}, authenticated_client_certs{load_certs_from_file(cert_dir)}
{
}

void mp::ClientCertStore::add_cert(const std::string& pem_cert)
{
    if (cert_exists(pem_cert))
        return;

    validate_certificate(pem_cert);
    QDir dir{cert_dir};
    QFile file{dir.filePath(chain_name)};
    auto opened = file.open(QIODevice::WriteOnly | QIODevice::Append);
    if (!opened)
        throw std::runtime_error("failed to create file to store certificate");

    size_t written = file.write(pem_cert.data(), pem_cert.size());
    if (written != pem_cert.size())
        throw std::runtime_error("failed to write certificate");

    authenticated_client_certs.push_back(pem_cert);
}

std::string mp::ClientCertStore::PEM_cert_chain() const
{
    QDir dir{cert_dir};
    auto path = dir.filePath(chain_name);
    if (QFile::exists(path))
        return mp::utils::contents_of(path);
    return {};
}

bool mp::ClientCertStore::verify_cert(const std::string& pem_cert)
{
    // The first client to connect will automatically have its certificate stored
    if (authenticated_client_certs.empty())
    {
        add_cert(pem_cert);
        return true;
    }

    return cert_exists(pem_cert);
}

bool mp::ClientCertStore::is_store_empty()
{
    return authenticated_client_certs.empty();
}

bool mp::ClientCertStore::cert_exists(const std::string& pem_cert)
{
    return std::find(authenticated_client_certs.cbegin(), authenticated_client_certs.cend(), pem_cert) !=
           authenticated_client_certs.cend();
}
