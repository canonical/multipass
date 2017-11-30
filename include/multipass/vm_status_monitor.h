/*
 * Copyright (C) 2017 Canonical, Ltd.
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

#ifndef MULTIPASS_VM_STATUS_MONITOR_H
#define MULTIPASS_VM_STATUS_MONITOR_H
namespace multipass
{
class VMStatusMonitor
{
public:
    virtual ~VMStatusMonitor() = default;
    virtual void on_resume() = 0;
    virtual void on_stop() = 0;
    virtual void on_shutdown() = 0;

protected:
    VMStatusMonitor() = default;
    VMStatusMonitor(const VMStatusMonitor&) = delete;
    VMStatusMonitor& operator=(const VMStatusMonitor&) = delete;
};
}
#endif // MULTIPASS_VM_STATUS_MONITOR_H
