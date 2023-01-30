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

#ifndef MULTIPASS_PROCESS_FACTORY_H
#define MULTIPASS_PROCESS_FACTORY_H

#include "apparmor.h"
#include <multipass/process/process_spec.h>
#include <multipass/singleton.h>

#include <memory>
#include <optional>

#define MP_PROCFACTORY multipass::ProcessFactory::instance()

namespace multipass
{
class Process;

class ProcessFactory : public Singleton<ProcessFactory>
{
public:
    ProcessFactory(const Singleton<ProcessFactory>::PrivatePass&);

    virtual std::unique_ptr<Process> create_process(std::unique_ptr<ProcessSpec>&& process_spec) const;
    std::unique_ptr<Process> create_process(const QString& command, const QStringList& args = QStringList()) const;

private:
    const std::optional<AppArmor> apparmor;
};

} // namespace multipass

#endif // MULTIPASS_PROCESS_FACTORY_H
