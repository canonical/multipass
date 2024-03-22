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

#ifndef MULTIPASS_BLUEPRINT_TEST_LAMBDAS_H
#define MULTIPASS_BLUEPRINT_TEST_LAMBDAS_H

#include <multipass/alias_definition.h>
#include <multipass/fetch_type.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_image_vault.h>
#include <multipass/vm_status_monitor.h>

#include <optional>
#include <string>

namespace multipass
{

class Query;
class VMImage;
struct ClientLaunchData;
class MemorySize;
class SSHKeyProvider;

namespace test
{

std::function<VMImage(const FetchType&,
                      const Query&,
                      const VMImageVault::PrepareAction&,
                      const ProgressMonitor&,
                      const bool,
                      const std::optional<std::string>,
                      const multipass::Path&)>
fetch_image_lambda(const std::string& release, const std::string& remote, const bool must_have_checksum = false);

std::function<VirtualMachine::UPtr(const VirtualMachineDescription&, const SSHKeyProvider&, VMStatusMonitor&)>
create_virtual_machine_lambda(const int& num_cores,
                              const MemorySize& mem_size,
                              const MemorySize& disk_space,
                              const std::string& name);

std::function<Query(const std::string&, VirtualMachineDescription&, ClientLaunchData&)> fetch_blueprint_for_lambda(
    const int& num_cores, const MemorySize& mem_size, const MemorySize& disk_space, const std::string& release,
    const std::string& remote, std::optional<std::pair<std::string, AliasDefinition>> alias = std::nullopt,
    std::optional<std::string> workspace = std::nullopt, std::optional<std::string> sha256 = std::nullopt);

} // namespace test
} // namespace multipass

#endif // MULTIPASS_BLUEPRINT_TEST_LAMBDAS_H
