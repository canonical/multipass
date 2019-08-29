/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#ifndef MULTIPASS_PROCESS_SPEC_H
#define MULTIPASS_PROCESS_SPEC_H

#include <QProcessEnvironment>
#include <QString>

#include <multipass/logging/level.h>

namespace multipass
{

class ProcessSpec
{
public:
    ProcessSpec() = default;
    virtual ~ProcessSpec() = default;

    virtual QString program() const = 0;
    virtual QStringList arguments() const;
    virtual QProcessEnvironment environment() const;
    virtual QString working_directory() const;

    virtual logging::Level error_log_level() const;

    virtual QString apparmor_profile() const = 0;
    const QString apparmor_profile_name() const;
    virtual QString identifier() const;
};

} // namespace multipass

#endif // MULTIPASS_PROCESS_SPEC_H
