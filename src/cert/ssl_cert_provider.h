/*
 * Copyright (C) 2018 Canonical, Ltd.
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

namespace multipass
{
class SSLCertProvider : public CertProvider
{
public:
    std::string certificate_as_base64() const override;
};
} // namespace multipass
#endif // MULTIPASS_SSL_CERT_PROVIDER_H
