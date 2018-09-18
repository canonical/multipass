/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#ifndef MULTIPASS_STUB_STATUS_MONITOR_H
#define MULTIPASS_STUB_STATUS_MONITOR_H

#include <multipass/vm_status_monitor.h>

namespace multipass
{
namespace test
{
struct StubVMStatusMonitor : public multipass::VMStatusMonitor
{
    void on_resume() override{};
    void on_stop() override{};
    void on_shutdown() override{};
    void on_suspend() override{};
    void on_restart(const std::string& name) override{};
    void persist_state_for(const std::string& name) override{};
};
}
}
#endif // MULTIPASS_STUB_STATUS_MONITOR_H
