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

#ifndef MULTIPASS_SSL_CERT_PROVIDER_H
#define MULTIPASS_SSL_CERT_PROVIDER_H

#include <multipass/cert_provider.h>
#include <multipass/path.h>

#include <string>

namespace multipass
{
class SSLCertProvider : public CertProvider
{
public:
    struct KeyCertificatePair
    {
        std::string pem_cert;
        std::string pem_priv_key;
    };

    explicit SSLCertProvider(const Path& data_dir);
    SSLCertProvider(const Path& data_dir, const std::string& server_name);
    std::string PEM_certificate() const override;
    std::string PEM_signing_key() const override;

private:
    KeyCertificatePair key_cert_pair;
};
} // namespace multipass
#endif // MULTIPASS_SSL_CERT_PROVIDER_H
