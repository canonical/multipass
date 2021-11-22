/*
 * Copyright (C) 2021 Canonical, Ltd.
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
    MOCK_METHOD1(add_cert, void(const std::string&));
    MOCK_CONST_METHOD0(PEM_cert_chain, std::string());
    MOCK_METHOD1(verify_cert, bool(const std::string&));
    MOCK_METHOD0(is_store_empty, bool());
};
} // namespace multipass::test
#endif // MULTIPASS_MOCK_CERT_STORE_H
