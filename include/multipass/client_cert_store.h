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

#ifndef MULTIPASS_CLIENT_CERT_STORE_H
#define MULTIPASS_CLIENT_CERT_STORE_H

#include <multipass/cert_store.h>
#include <multipass/path.h>

#include <string>
#include <vector>

namespace multipass
{
class ClientCertStore : public CertStore
{
public:
    explicit ClientCertStore(const multipass::Path& cert_dir);
    void add_cert(const std::string& pem_cert) override;
    std::string PEM_cert_chain() const override;
    bool verify_cert(const std::string& pem_cert) override;
    bool is_store_empty() override;

private:
    bool cert_exists(const std::string& pem_cert);

    Path cert_dir;
    std::vector<std::string> authenticated_client_certs;
};
} // namespace multipass
#endif // MULTIPASS_CLIENT_CERT_STORE_H
