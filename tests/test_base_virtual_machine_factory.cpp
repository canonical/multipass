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

#include "common.h"
#include "mock_logger.h"
#include "mock_platform.h"
#include "stub_ssh_key_provider.h"
#include "stub_url_downloader.h"
#include "temp_dir.h"

#include <shared/base_virtual_machine_factory.h>

#include <multipass/network_interface_info.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_status_monitor.h>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct MockBaseFactory : mp::BaseVirtualMachineFactory
{
    MockBaseFactory() : MockBaseFactory{std::make_unique<mp::test::TempDir>()}
    {
    }

    MockBaseFactory(std::unique_ptr<mp::test::TempDir>&& tmp_dir)
        : mp::BaseVirtualMachineFactory{tmp_dir->path()}, tmp_dir{std::move(tmp_dir)}
    {
    }

    MOCK_METHOD(mp::VirtualMachine::UPtr,
                create_virtual_machine,
                (const mp::VirtualMachineDescription&, const mp::SSHKeyProvider&, mp::VMStatusMonitor&),
                (override));
    MOCK_METHOD(mp::VirtualMachine::UPtr,
                create_vm_and_instance_disk_data,
                (const QString&,
                 const mp::VMSpecs&,
                 const mp::VMSpecs&,
                 const std::string&,
                 const std::string&,
                 const mp::VMImage&,
                 const mp::SSHKeyProvider&,
                 mp::VMStatusMonitor&),
                (override));
    MOCK_METHOD(mp::VMImage, prepare_source_image, (const mp::VMImage&), (override));
    MOCK_METHOD(void, prepare_instance_image, (const mp::VMImage&, const mp::VirtualMachineDescription&), (override));
    MOCK_METHOD(void, hypervisor_health_check, (), (override));
    MOCK_METHOD(QString, get_backend_version_string, (), (const, override));
    MOCK_METHOD(void, prepare_networking, (std::vector<mp::NetworkInterface>&), (override));
    MOCK_METHOD(std::vector<mp::NetworkInterfaceInfo>, networks, (), (const, override));
    MOCK_METHOD(std::string, create_bridge_with, (const mp::NetworkInterfaceInfo&), (override));
    MOCK_METHOD(void,
                prepare_interface,
                (mp::NetworkInterface & net, std::vector<mp::NetworkInterfaceInfo>& host_nets),
                (override));
    MOCK_METHOD(void, remove_resources_for_impl, (const std::string&), (override));

    std::string base_create_bridge_with(const mp::NetworkInterfaceInfo& interface)
    {
        return mp::BaseVirtualMachineFactory::create_bridge_with(interface); // protected
    }

    void base_prepare_interface(mp::NetworkInterface& net, std::vector<mp::NetworkInterfaceInfo>& host_nets)
    {
        return mp::BaseVirtualMachineFactory::prepare_interface(net, host_nets); // protected
    }

    std::unique_ptr<mp::test::TempDir> tmp_dir;
};

struct BaseFactory : public Test
{
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
};

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

TEST_F(BaseFactory, networks_throws)
{
    StrictMock<MockBaseFactory> factory;

    ASSERT_THROW(factory.mp::BaseVirtualMachineFactory::networks(), mp::NotImplementedOnThisBackendException);
}

// Ideally, we'd define some unique YAML for each node and test the contents of the ISO image,
// but we'd need a cross-platfrom library to read files in an ISO image and that is beyond scope
// at this time.  Instead, just make sure an ISO image is created and has the expected path.
TEST_F(BaseFactory, creates_cloud_init_iso_image)
{
    MockBaseFactory factory;
    const std::string name{"foo"};
    const YAML::Node metadata{YAML::Load({fmt::format("name: {}", name)})}, vendor_data{metadata}, user_data{metadata},
        network_data{metadata};

    mp::VMImage image;
    image.image_path = QString("%1/%2").arg(factory.tmp_dir->path()).arg(QString::fromStdString(name));

    mp::VirtualMachineDescription vm_desc{2,
                                          mp::MemorySize{"3M"},
                                          mp::MemorySize{}, // not used
                                          name,
                                          "00:16:3e:fe:f2:b9",
                                          {},
                                          "yoda",
                                          image,
                                          "",
                                          metadata,
                                          user_data,
                                          vendor_data,
                                          network_data};

    factory.configure(vm_desc);

    EXPECT_EQ(vm_desc.cloud_init_iso, QString("%1/cloud-init-config.iso").arg(factory.tmp_dir->path()));
    EXPECT_TRUE(QFile::exists(vm_desc.cloud_init_iso));
}

TEST_F(BaseFactory, create_bridge_not_implemented)
{
    StrictMock<MockBaseFactory> factory;

    MP_EXPECT_THROW_THAT(factory.base_create_bridge_with({}), mp::NotImplementedOnThisBackendException,
                         mpt::match_what(HasSubstr("bridge creation")));
}

TEST_F(BaseFactory, prepareNetworkingHasNoObviousEffectByDefault)
{
    MockBaseFactory factory;

    EXPECT_CALL(factory, prepare_networking).WillOnce(Invoke([&factory](auto& nets) {
        factory.mp::BaseVirtualMachineFactory::prepare_networking(nets);
    }));

    std::vector<mp::NetworkInterface> nets{{"asdf", "qwer", true}};
    const auto nets_copy = nets;

    factory.prepare_networking(nets);
    EXPECT_EQ(nets, nets_copy);
}

TEST_F(BaseFactory, prepareInterfaceLeavesUnrecognizedNetworkAlone)
{
    StrictMock<MockBaseFactory> factory;

    auto host_nets = std::vector<mp::NetworkInterfaceInfo>{{"eth0", "ethernet", "asd"}, {"wlan0", "wifi", "asd"}};
    auto extra_net = mp::NetworkInterface{"eth1", "fa:se:ma:c0:12:23", false};
    const auto host_copy = host_nets;
    const auto extra_copy = extra_net;

    factory.base_prepare_interface(extra_net, host_nets);
    EXPECT_EQ(host_nets, host_copy);
    EXPECT_EQ(extra_net, extra_copy);
}

TEST_F(BaseFactory, prepareInterfaceLeavesExistingBridgeAlone)
{
    StrictMock<MockBaseFactory> factory;
    constexpr auto bridge_type = "arbitrary";

    auto [mock_platform, platform_guard] = mpt::MockPlatform::inject();
    EXPECT_CALL(*mock_platform, bridge_nomenclature).WillRepeatedly(Return(bridge_type));

    auto host_nets = std::vector<mp::NetworkInterfaceInfo>{{"br0", bridge_type, "foo"}, {"xyz", bridge_type, "bar"}};
    auto extra_net = mp::NetworkInterface{"xyz", "fake mac", true};
    const auto host_copy = host_nets;
    const auto extra_copy = extra_net;

    factory.base_prepare_interface(extra_net, host_nets);
    EXPECT_EQ(host_nets, host_copy);
    EXPECT_EQ(extra_net, extra_copy);
}

TEST_F(BaseFactory, prepareInterfaceReplacesBridgedNetworkWithCorrespongingBridge)
{
    StrictMock<MockBaseFactory> factory;
    constexpr auto bridge_type = "tunnel";
    constexpr auto bridge = "br";

    auto [mock_platform, platform_guard] = mpt::MockPlatform::inject();
    EXPECT_CALL(*mock_platform, bridge_nomenclature).WillRepeatedly(Return(bridge_type));

    auto host_nets = std::vector<mp::NetworkInterfaceInfo>{{"eth", "ethernet", "already bridged"},
                                                           {"wlan", "wifi", "something else"},
                                                           {bridge, bridge_type, "bridge to eth", {"eth"}},
                                                           {"different", bridge_type, "uninteresting", {"wlan"}}};
    auto extra_net = mp::NetworkInterface{"eth", "fake mac", false};

    const auto host_copy = host_nets;
    auto extra_check = extra_net;
    extra_check.id = bridge;

    factory.base_prepare_interface(extra_net, host_nets);
    EXPECT_EQ(host_nets, host_copy);
    EXPECT_EQ(extra_net, extra_check);
}

TEST_F(BaseFactory, prepareInterfaceCreatesBridgeForUnbridgedNetwork)
{
    StrictMock<MockBaseFactory> factory;
    constexpr auto bridge_type = "gagah";
    constexpr auto bridge = "newbr";

    auto [mock_platform, platform_guard] = mpt::MockPlatform::inject();
    EXPECT_CALL(*mock_platform, bridge_nomenclature).WillRepeatedly(Return(bridge_type));

    auto host_nets = std::vector<mp::NetworkInterfaceInfo>{{"eth", "ethernet", "already bridged"},
                                                           {"wlan", "wifi", "something else"},
                                                           {"br0", bridge_type, "bridge to wlan", {"wlan"}}};
    const auto host_copy = host_nets;

    auto extra_id = "eth";
    auto extra_net = mp::NetworkInterface{extra_id, "maccc", true};
    auto extra_check = extra_net;
    extra_check.id = bridge;

    EXPECT_CALL(factory, create_bridge_with(Field(&mp::NetworkInterfaceInfo::id, Eq(extra_net.id))))
        .WillOnce(Return(bridge));

    factory.base_prepare_interface(extra_net, host_nets);
    EXPECT_EQ(extra_net, extra_check);

    const auto [host_diff, ignore] =
        std::mismatch(host_nets.cbegin(), host_nets.cend(), host_copy.cbegin(), host_copy.cend());
    ASSERT_NE(host_diff, host_nets.cend());
    EXPECT_EQ(host_diff->id, bridge);
    EXPECT_EQ(host_diff->type, bridge_type);
    EXPECT_THAT(host_diff->links, ElementsAre(extra_id));

    host_nets.erase(host_diff);
    EXPECT_EQ(host_nets, host_copy);
}

TEST_F(BaseFactory, prepareNetworkingWithNoExtraNetsHasNoObviousEffect)
{
    MockBaseFactory factory;
    MP_DELEGATE_MOCK_CALLS_ON_BASE(factory, prepare_networking, mp::BaseVirtualMachineFactory);

    std::vector<mp::NetworkInterface> empty;
    factory.prepare_networking(empty);
    EXPECT_THAT(empty, IsEmpty());
}

TEST_F(BaseFactory, prepareNetworkingPreparesEachRequestedNetwork)
{
    constexpr auto bridge_type = "bridge";
    auto [mock_platform, platform_guard] = mpt::MockPlatform::inject();
    EXPECT_CALL(*mock_platform, bridge_nomenclature).WillRepeatedly(Return(bridge_type));

    const auto host_nets = std::vector<mp::NetworkInterfaceInfo>{{"simple", "bridge", "this and that"}};
    const auto tag = mp::NetworkInterface{"updated", "tag", false};

    auto extra_nets = std::vector<mp::NetworkInterface>{
        {"aaa", "alpha", true}, {"bbb", "beta", false}, {"br", "bridge", true}, {"brr", "bridge", false}};
    const auto num_nets = extra_nets.size();

    MockBaseFactory factory;
    EXPECT_CALL(factory, networks).WillOnce(Return(host_nets));
    MP_DELEGATE_MOCK_CALLS_ON_BASE(factory, prepare_networking, mp::BaseVirtualMachineFactory);

    for (auto& net : extra_nets)
        EXPECT_CALL(factory, prepare_interface(Ref(net), Eq(host_nets))).WillOnce(SetArgReferee<0>(tag));

    factory.prepare_networking(extra_nets);
    EXPECT_EQ(extra_nets.size(), num_nets);
    EXPECT_THAT(extra_nets, Each(Eq(tag)));
}

TEST_F(BaseFactory, factoryHasDefaultSuspendSupport)
{
    MockBaseFactory factory;
    EXPECT_NO_THROW(factory.mp::BaseVirtualMachineFactory::require_suspend_support());
}
} // namespace
