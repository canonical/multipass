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

#ifndef MULTIPASS_BLUEPRINTS_TEST_LAMBDAS_H
#define MULTIPASS_BLUEPRINTS_TEST_LAMBDAS_H

#include <multipass/client_launch_data.h>
#include <multipass/memory_size.h>
#include <multipass/query.h>
#include <multipass/virtual_machine.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_image.h>

#include "blueprint_test_lambdas.h"
#include "common.h"
#include "stub_virtual_machine.h"
#include "stub_vm_image_vault.h"
#include "temp_dir.h"

namespace mp = multipass;
namespace mpt = multipass::test;

std::function<mp::VMImage(const mp::FetchType&,
                          const mp::Query&,
                          const mp::VMImageVault::PrepareAction&,
                          const mp::ProgressMonitor&,
                          const bool,
                          const std::optional<std::string>,
                          const mp::Path&)>
mpt::fetch_image_lambda(const std::string& release, const std::string& remote, const bool must_have_checksum)
{
    return [&release, &remote, must_have_checksum](const mp::FetchType& fetch_type,
                                                   const mp::Query& query,
                                                   const mp::VMImageVault::PrepareAction& prepare,
                                                   const mp::ProgressMonitor& monitor,
                                                   const bool unlock,
                                                   const std::optional<std::string>& checksum,
                                                   const mp::Path& save_dir) {
        EXPECT_EQ(query.release, release);
        if (remote.empty())
        {
            EXPECT_TRUE(query.remote_name.empty());
        }
        else
        {
            EXPECT_EQ(query.remote_name, remote);
        }

        if (must_have_checksum)
        {
            EXPECT_NE(checksum, std::nullopt);
        }

        return mpt::StubVMImageVault().fetch_image(fetch_type, query, prepare, monitor, unlock, checksum, save_dir);
    };
}

std::function<
    mp::VirtualMachine::UPtr(const mp::VirtualMachineDescription&, const mp::SSHKeyProvider&, mp::VMStatusMonitor&)>
mpt::create_virtual_machine_lambda(const int& num_cores,
                                   const mp::MemorySize& mem_size,
                                   const mp::MemorySize& disk_space,
                                   const std::string& name)
{
    return [&num_cores, &mem_size, &disk_space, &name](const mp::VirtualMachineDescription& vm_desc,
                                                       const mp::SSHKeyProvider&,
                                                       mp::VMStatusMonitor&) {
        EXPECT_EQ(vm_desc.num_cores, num_cores);
        EXPECT_EQ(vm_desc.mem_size, mem_size);
        EXPECT_EQ(vm_desc.disk_space, disk_space);
        if (!name.empty())
        {
            EXPECT_EQ(vm_desc.vm_name, name);
        }

        return std::make_unique<mpt::StubVirtualMachine>();
    };
}

std::function<mp::Query(const std::string&, mp::VirtualMachineDescription&, mp::ClientLaunchData&)>
mpt::fetch_blueprint_for_lambda(const int& num_cores, const mp::MemorySize& mem_size, const mp::MemorySize& disk_space,
                                const std::string& release, const std::string& remote,
                                std::optional<std::pair<std::string, mp::AliasDefinition>> alias,
                                std::optional<std::string> workspace, std::optional<std::string> sha256)
{
    return [&num_cores, &mem_size, &disk_space, &release, &remote, alias, workspace,
            sha256](const auto&, mp::VirtualMachineDescription& vm_desc, mp::ClientLaunchData& l_data) -> mp::Query {
        vm_desc.num_cores = num_cores;
        vm_desc.mem_size = mem_size;
        vm_desc.disk_space = disk_space;
        if (sha256)
            vm_desc.image.id = *sha256;

        if (alias)
            l_data.aliases_to_be_created.emplace(alias->first, alias->second);

        if (workspace)
            l_data.workspaces_to_be_created.push_back(*workspace);

        return {"", release, false, remote, mp::Query::Type::Alias};
    };
}

#endif // MULTIPASS_BLUEPRINTS_TEST_LAMBDAS_H
