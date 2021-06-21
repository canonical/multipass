/*
 * Copyright (C) 2020-2021 Canonical, Ltd.
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

#include "extra_assertions.h"
#include "mock_logger.h"
#include "stub_url_downloader.h"
#include "temp_dir.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

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
    MOCK_METHOD1(prepare_networking, void(std::vector<mp::NetworkInterface>&));
    MOCK_CONST_METHOD0(networks, std::vector<mp::NetworkInterfaceInfo>());
    MOCK_METHOD1(create_bridge_with, std::string(const mp::NetworkInterfaceInfo&));

    void base_prepare_networking_guts(std::vector<mp::NetworkInterface>& extra_interfaces,
                                      const std::string& bridge_type)
    {
        return mp::BaseVirtualMachineFactory::prepare_networking_guts(extra_interfaces, bridge_type); // protected
    }
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
    mpt::TempDir iso_dir;
    const std::string name{"foo"};
    const YAML::Node metadata{YAML::Load({fmt::format("name: {}", name)})}, vendor_data{metadata}, user_data{metadata},
        network_data{metadata};

    mp::VMImage image;
    image.image_path = QString("%1/%2").arg(iso_dir.path()).arg(QString::fromStdString(name));

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

    MockBaseFactory factory;
    factory.configure(vm_desc);

    EXPECT_EQ(vm_desc.cloud_init_iso, QString("%1/cloud-init-config.iso").arg(iso_dir.path()));
    EXPECT_TRUE(QFile::exists(vm_desc.cloud_init_iso));
}

TEST_F(BaseFactory, prepareNetworkingHasNoObviousEffectByDefault)
{
    StrictMock<MockBaseFactory> factory;

    EXPECT_CALL(factory, prepare_networking).WillOnce(Invoke([&factory](auto& nets) {
        factory.mp::BaseVirtualMachineFactory::prepare_networking(nets);
    }));

    std::vector<mp::NetworkInterface> nets{{"asdf", "qwer", true}};
    const auto nets_copy = nets;

    factory.prepare_networking(nets);
    EXPECT_EQ(nets, nets_copy);
}

TEST_F(BaseFactory, prepareNetworkingGutsWithNoExtraNetsHasNoObviousEffect)
{
    StrictMock<MockBaseFactory> factory;

    std::vector<mp::NetworkInterface> empty;
    factory.base_prepare_networking_guts(empty, "asdf");
    EXPECT_THAT(empty, IsEmpty());
}

TEST_F(BaseFactory, prepareNetworkingGutsLeavesUnrecognizedNetworksAlone)
{
    StrictMock<MockBaseFactory> factory;
    EXPECT_CALL(factory, networks).WillOnce(Return(std::vector<mp::NetworkInterfaceInfo>{{"eth0", "ethernet", "asd"}}));

    std::vector<mp::NetworkInterface> extra_nets{{"wlan0", "fa:se:ma:c0:12:23", false},
                                                 {"eth1", "fa:se:ma:c9:45:56", true}};
    const auto extra_copy = extra_nets;

    factory.base_prepare_networking_guts(extra_nets, "bridge");
    EXPECT_EQ(extra_nets, extra_copy);
}

TEST_F(BaseFactory, prepareNetworkingGutsLeavesBridgeTypeNetworksAlone)
{
    StrictMock<MockBaseFactory> factory;
    constexpr auto bridge_type = "arbitrary";
    EXPECT_CALL(factory, networks)
        .WillOnce(
            Return(std::vector<mp::NetworkInterfaceInfo>{{"br0", bridge_type, "foo"}, {"xyz", bridge_type, "bar"}}));

    std::vector<mp::NetworkInterface> extra_nets{{"br0", "false mac", false}, {"xyz", "fake mac", true}};
    const auto extra_copy = extra_nets;

    factory.base_prepare_networking_guts(extra_nets, bridge_type);
    EXPECT_EQ(extra_nets, extra_copy);
}

struct TestBridgePreparation
    : public BaseFactory,
      WithParamInterface<std::tuple<std::vector<mp::NetworkInterface>, std::vector<mp::NetworkInterface>,
                                    std::vector<mp::NetworkInterface>>>
{
    inline static constexpr auto br_type = "tunnel";
    inline static const std::vector<mp::NetworkInterfaceInfo> host_nets{
        {"eth0", "ethernet", "e0"}, {"eth1", "ethernet", "e1"},       {"wlan0", "wifi", "w0"},
        {"wlan1", "wifi", "w1"},    {"br0", br_type, "b0", {"eth0"}}, {"br1", br_type, "b1", {"wlan1"}},
        {"br2", br_type, "empty"}};
    std::vector<mp::NetworkInterface> mix_extra_nets() const // mixes all requested
    {
        std::vector<mp::NetworkInterface> ret;
        const auto& [requested_bridged, requested_unbridged, requested_existing_bridges] = GetParam();

        auto maxim = std::max(requested_bridged.size(), requested_unbridged.size());
        maxim = std::max(maxim, requested_existing_bridges.size());
        for (size_t i = 0; i < maxim; ++i) // alternate to avoid testing only partitioned extra_nets
            for (const auto& requests : {requested_bridged, requested_unbridged, requested_existing_bridges})
                if (i < requests.size())
                    ret.push_back(requests[i]);

        return ret;
    }
};

TEST_P(TestBridgePreparation, prepareNetworkingGutsCreatesBridgesAndReplacesIds)
{
    const auto& [requested_bridged, requested_unbridged, requested_existing_bridges] = GetParam();
    auto extra_nets = mix_extra_nets();
    if (extra_nets.empty()) // this case is covered elsewhere
        return;

    StrictMock<MockBaseFactory> factory;
    EXPECT_CALL(factory, networks).WillOnce(Return(host_nets));

    auto same_id = [](const auto& a, const auto& b) { return a.id == b.id; };
    auto was_unbridged = mpt::HasCorrespondentIn(requested_unbridged, same_id);
    EXPECT_CALL(factory, create_bridge_with(was_unbridged)).WillRepeatedly([](const auto& net) {
        return net.id + "br";
    });

    factory.base_prepare_networking_guts(extra_nets, br_type); // extra_nets updated here

    // Now all predicates and matchers that we need to check extra_nets...
    auto is_same_bridge = [&same_id](const auto& a, const auto& b) { return same_id(a, b) && b.type == br_type; };
    auto is_old_bridge = mpt::HasCorrespondentIn(host_nets, is_same_bridge);
    auto is_unrecognized_network = Not(mpt::HasCorrespondentIn(host_nets, same_id));
    auto is_old_bridge_or_unrecognized = AnyOf(is_old_bridge, is_unrecognized_network);
    auto was_requested = AnyOf(mpt::ContainedIn(requested_unbridged), mpt::ContainedIn(requested_bridged),
                               mpt::ContainedIn(requested_existing_bridges));

    auto is_new_bridge = Field(&mp::NetworkInterface::id, EndsWith("br"));
    auto starts_with_id = [](const auto& a, const auto& b) { return a.id.rfind(b.id) == 0; };
    auto links_to_previously_unbridged = mpt::HasCorrespondentIn(requested_unbridged, starts_with_id);
    auto is_new_bridge_linking_to_previously_unbridged = AllOf(is_new_bridge, links_to_previously_unbridged);

    auto links_to_previously_bridged =
        [bridged_ptr = &requested_bridged](const auto& a) { // ptr works around forbidden structured-binding capture
            auto a_links_to = [&a](const auto& b) { return a.has_link(b.id); };
            return std::any_of(std::cbegin(*bridged_ptr), std::cend(*bridged_ptr), a_links_to);
        };
    auto is_same_bridge_and_links_to_previously_bridged = // use matching element in host_net to check the link
        [&is_same_bridge, &links_to_previously_bridged](const auto& a, const auto& b) {
            return is_same_bridge(a, b) && links_to_previously_bridged(b);
        };
    auto is_old_bridge_linking_to_previously_bridged =
        mpt::HasCorrespondentIn(host_nets, is_same_bridge_and_links_to_previously_bridged);

    EXPECT_EQ(extra_nets.size(),
              requested_bridged.size() + requested_unbridged.size() + requested_existing_bridges.size());

    EXPECT_THAT(extra_nets, Each(AnyOf(AllOf(is_old_bridge_or_unrecognized, was_requested),
                                       is_new_bridge_linking_to_previously_unbridged,
                                       is_old_bridge_linking_to_previously_bridged)));
}

std::vector<std::vector<mp::NetworkInterface>> requested_bridged_possibilities{
    {},
    {{"eth0", "", true}},
    {{"wlan1", "", false}},
    {{"eth0", "", true}, {"wlan1", "", false}},
    {{"eth0", "", true}, {"foo", "ethernet", false}, {"wlan1", "", false}, {"bar", "wifi", true}}};
std::vector<std::vector<mp::NetworkInterface>> requested_unbridged_possibilities{
    {},
    {{"eth1", "", true}},
    {{"wlan0", "", false}},
    {{"eth1", "", true}, {"wlan0", "", false}},
    {{"eth1", "", true}, {"foo", "ethernet", false}, {"wlan0", "", false}, {"bar", "wifi", true}}};
std::vector<std::vector<mp::NetworkInterface>> requested_existing_bridges_possibilities{
    {},
    {{"br0", "", true}},
    {{"br1", "", false}},
    {{"br2", "", true}},
    {{"br0", "", false}, {"br1", "", true}},
    {{"br1", "", true}, {"br2", "", false}},
    {{"br0", "", false}, {"br2", "", false}},
    {{"br0", "", false}, {"br1", "", true}, {"br2", "", false}}};

INSTANTIATE_TEST_SUITE_P(BaseFactory, TestBridgePreparation,
                         Combine(ValuesIn(requested_bridged_possibilities), ValuesIn(requested_unbridged_possibilities),
                                 ValuesIn(requested_existing_bridges_possibilities)));
} // namespace
