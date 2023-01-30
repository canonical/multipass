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

#ifndef MULTIPASS_CERTPROVIDER_H
#define MULTIPASS_CERTPROVIDER_H

#include "disabled_copy_move.h"

#include <string>

namespace multipass
{
class CertProvider : private DisabledCopyMove
{
public:
    virtual ~CertProvider() = default;
    virtual std::string PEM_certificate() const = 0;
    virtual std::string PEM_signing_key() const = 0;

protected:
    CertProvider() = default;
};
} // namespace multipass
#endif // MULTIPASS_CERTPROVIDER_H
