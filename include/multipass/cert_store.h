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

#ifndef MULTIPASS_CERT_STORE_H
#define MULTIPASS_CERT_STORE_H

#include "disabled_copy_move.h"

#include <string>

namespace multipass
{
class CertStore : private DisabledCopyMove
{
public:
    virtual ~CertStore() = default;
    virtual void add_cert(const std::string& pem_cert) = 0;
    virtual std::string PEM_cert_chain() const = 0;
    virtual bool verify_cert(const std::string& pem_cert) = 0;
    virtual bool is_store_empty() = 0;

protected:
    CertStore() = default;
};
} // namespace multipass
#endif // MULTIPASS_CERT_STORE_H
