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

#ifndef MULTIPASS_MOCK_CERT_STORE_H
#define MULTIPASS_MOCK_CERT_STORE_H

#include <multipass/cert_store.h>

namespace multipass::test
{
struct MockCertStore : public CertStore
{
    MOCK_METHOD(void, add_cert, (const std::string&), (override));
    MOCK_METHOD(std::string, PEM_cert_chain, (), (const, override));
    MOCK_METHOD(bool, verify_cert, (const std::string&), (override));
    MOCK_METHOD(bool, empty, (), (override));
};
} // namespace multipass::test
#endif // MULTIPASS_MOCK_CERT_STORE_H
