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

#include "biomem.h"

#include <stdexcept>
#include <vector>

namespace mp = multipass;

mp::BIOMem::BIOMem() : bio{BIO_new(BIO_s_mem()), BIO_free}
{
    if (bio == nullptr)
        throw std::runtime_error("Failed to create BIO structure");
}

mp::BIOMem::BIOMem(const std::string& pem_source) : BIOMem()
{
    BIO_write(bio.get(), pem_source.data(), pem_source.size());
}

std::string mp::BIOMem::as_string() const
{
    std::vector<char> pem(BIO_number_written(bio.get()));
    BIO_read(bio.get(), pem.data(), pem.size());
    return {pem.begin(), pem.end()};
}

BIO* mp::BIOMem::get() const
{
    return bio.get();
}
