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

#ifndef MULTIPASS_BIOMEM_H
#define MULTIPASS_BIOMEM_H

#include <openssl/bio.h>

#include <memory>
#include <string>

namespace multipass
{
class BIOMem
{
public:
    BIOMem();
    BIOMem(const std::string& pem_source);
    std::string as_string() const;
    BIO* get() const;

private:
    const std::unique_ptr<BIO, decltype(BIO_free)*> bio;
};
} // namespace multipass
#endif // MULTIPASS_BIOMEM_H
