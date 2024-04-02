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

#ifndef MULTIPASS_VM_STATUS_MONITOR_H
#define MULTIPASS_VM_STATUS_MONITOR_H

#include "disabled_copy_move.h"
#include "virtual_machine.h"

#include <string>

#include <QJsonObject>

namespace multipass
{
class VMStatusMonitor : private DisabledCopyMove
{
public:
    virtual ~VMStatusMonitor() = default;
    virtual void on_resume() = 0;
    virtual void on_shutdown() = 0;
    virtual void on_suspend() = 0;
    virtual void on_restart(const std::string& name) = 0;
    virtual void persist_state_for(const std::string& name, const VirtualMachine::State& state) = 0;
    virtual void update_metadata_for(const std::string& name, const QJsonObject& metadata) = 0;
    virtual QJsonObject retrieve_metadata_for(const std::string& name) = 0;

protected:
    VMStatusMonitor() = default;
};
}
#endif // MULTIPASS_VM_STATUS_MONITOR_H
