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

#ifndef MULTIPASS_TESTS_STUB_CERT_STORE_H
#define MULTIPASS_TESTS_STUB_CERT_STORE_H

#include <multipass/cert_store.h>

namespace multipass
{
namespace test
{
class StubCertStore : public CertStore
{
public:
    void add_cert(const std::string& pem_cert) override
    {
    }

    std::string PEM_cert_chain() const override
    {
        return {};
    }

    bool verify_cert(const std::string& pem_cert) override
    {
        return true;
    }

    bool empty() override
    {
        return true;
    }
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_TESTS_STUB_CERT_STORE_H
