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

#ifndef MULTIPASS_BASE_VIRTUAL_MACHINE_H
#define MULTIPASS_BASE_VIRTUAL_MACHINE_H

#include <multipass/exceptions/not_implemented_on_this_backend_exception.h>
#include <multipass/logging/log.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine.h>

#include <fmt/format.h>

#include <QRegularExpression>
#include <QString>

#include <mutex>
#include <shared_mutex>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpu = multipass::utils;

namespace multipass
{
class BaseVirtualMachine : public VirtualMachine
{
public:
    BaseVirtualMachine(VirtualMachine::State state, const std::string& vm_name);
    BaseVirtualMachine(const std::string& vm_name);
    ~BaseVirtualMachine(); // allow composing unique_ptr to fwd-declared Snapshots NOLINT(modernize-use-override)

    std::vector<std::string> get_all_ipv4(const SSHKeyProvider& key_provider) override;
    std::unique_ptr<MountHandler> make_native_mount_handler(const SSHKeyProvider* ssh_key_provider,
                                                            const std::string& target,
                                                            const multipass::VMMount& mount) override
    {
        throw NotImplementedOnThisBackendException("native mounts");
    };

    SnapshotVista view_snapshots() const noexcept override;
    LockingConstSnapshotRef take_snapshot(const VMSpecs& specs, const std::string& name,
                                          const std::string& comment) override;

protected:
    SnapshotMap snapshots;
    Snapshot* head_snapshot = nullptr;
    std::shared_mutex snapshot_mutex; // TODO@snapshots will probably want this to be mutable
    std::mutex transfer_mutex;
};

inline auto multipass::BaseVirtualMachine::view_snapshots() const noexcept -> SnapshotVista
{
    return snapshots;
}

} // namespace multipass

#endif // MULTIPASS_BASE_VIRTUAL_MACHINE_H
