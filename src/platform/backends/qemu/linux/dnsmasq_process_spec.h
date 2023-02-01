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

#ifndef MULTIPASS_DNSMASQ_PROCESS_SPEC_H
#define MULTIPASS_DNSMASQ_PROCESS_SPEC_H

#include <multipass/ip_address.h>
#include <multipass/path.h>
#include <multipass/process/process_spec.h>

#include <string>

namespace multipass
{

class DNSMasqProcessSpec : public ProcessSpec
{
public:
    explicit DNSMasqProcessSpec(const Path& data_dir, const QString& bridge_name, const std::string& subnet,
                                const QString& conf_file_path);

    QString program() const override;
    QStringList arguments() const override;
    logging::Level error_log_level() const override;

    QString apparmor_profile() const override;

private:
    const Path data_dir;
    const QString bridge_name;
    const std::string subnet;
    const QString conf_file_path;
};

} // namespace multipass

#endif // MULTIPASS_DNSMASQ_PROCESS_SPEC_H
