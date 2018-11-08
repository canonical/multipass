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

#ifndef MULTIPASS_DHCP_RELEASE_PROCESS_H
#define MULTIPASS_DHCP_RELEASE_PROCESS_H

#include <multipass/ip_address.h>

#include "process_spec.h"

namespace multipass
{

class DHCPReleaseProcessSpec : public ProcessSpec
{
public:
    explicit DHCPReleaseProcessSpec(const QString& bridge_name, const multipass::IPAddress& ip, const QString& hw_addr);

    QString program() const override;
    QStringList arguments() const override;
    QString apparmor_profile() const override;
    QString identifier() const override;

private:
    const QString bridge_name;
    const multipass::IPAddress ip;
    const QString hw_addr;
};

} // namespace multipass

#endif // MULTIPASS_DHCP_RELEASE_PROCESS_H
