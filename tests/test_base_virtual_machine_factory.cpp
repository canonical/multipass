/*
 * Copyright (C) 2020 Canonical, Ltd.
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

#include <multipass/network_interface_info.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_status_monitor.h>
#include <shared/base_virtual_machine_factory.h>

#include "mock_logger.h"
#include "stub_url_downloader.h"
#include "temp_dir.h"

#include <gmock/gmock.h>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct MockBaseFactory : mp::BaseVirtualMachineFactory
{
    MOCK_METHOD2(create_virtual_machine,
                 mp::VirtualMachine::UPtr(const mp::VirtualMachineDescription&, mp::VMStatusMonitor&));
    MOCK_METHOD1(remove_resources_for, void(const std::string&));
    MOCK_METHOD1(prepare_source_image, mp::VMImage(const mp::VMImage&));
    MOCK_METHOD2(prepare_instance_image, void(const mp::VMImage&, const mp::VirtualMachineDescription&));
    MOCK_METHOD0(hypervisor_health_check, void());
    MOCK_METHOD0(get_backend_version_string, QString());
};

struct BaseFactory : public Test
{
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
};
} // namespace

TEST_F(BaseFactory, returns_image_only_fetch_type)
{
    MockBaseFactory factory;

    EXPECT_EQ(factory.fetch_type(), mp::FetchType::ImageOnly);
}

TEST_F(BaseFactory, dir_name_returns_empty_string)
{
    MockBaseFactory factory;

    const auto dir_name = factory.get_backend_directory_name();

    EXPECT_TRUE(dir_name.isEmpty());
}

TEST_F(BaseFactory, create_image_vault_returns_default_vault)
{
    mpt::StubURLDownloader stub_downloader;
    mpt::TempDir cache_dir;
    mpt::TempDir data_dir;
    std::vector<mp::VMImageHost*> hosts;
    MockBaseFactory factory;

    auto vault = factory.create_image_vault(hosts, &stub_downloader, cache_dir.path(), data_dir.path(), mp::days{0});

    EXPECT_TRUE(dynamic_cast<mp::DefaultVMImageVault*>(vault.get()));
}

TEST_F(BaseFactory, list_networks_throws)
{
    MockBaseFactory factory;

    ASSERT_THROW(factory.list_networks(), mp::NotImplementedOnThisBackendException);
}
