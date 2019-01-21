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

#ifndef MULTIPASS_APPARMOR_PROCESS_FACTORY_H
#define MULTIPASS_APPARMOR_PROCESS_FACTORY_H

#include <multipass/process_factory.h>
#include "apparmored_process_spec.h"
#include "apparmor.h"

namespace multipass
{

class ApparmoredProcessFactory : public ProcessFactory
{
public:
    ApparmoredProcessFactory();
    virtual ~ApparmoredProcessFactory() = default;

    // FIXME
    //std::unique_ptr<Process> create_process(std::unique_ptr<multipass::ProcessSpec> &&process_spec) const override;

    std::unique_ptr<Process> create_process(std::unique_ptr<multipass::ApparmoredProcessSpec> &&process_spec) const;

private:
    const AppArmor apparmor;
};

} // namespace multipass

#endif // MULTIPASS_APPARMOR_PROCESS_FACTORY_H
