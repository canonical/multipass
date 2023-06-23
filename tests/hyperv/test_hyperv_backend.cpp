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

#include "tests/common.h"
#include "tests/mock_logger.h"
#include "tests/mock_platform.h"
#include "tests/mock_process_factory.h"
#include "tests/stub_ssh_key_provider.h"
#include "tests/stub_status_monitor.h"
#include "tests/temp_file.h"
#include "tests/windows/powershell_test_helper.h"

#include <src/platform/backends/hyperv/hyperv_virtual_machine_factory.h>

#include <multipass/format.h>
#include <multipass/memory_size.h>
#include <multipass/platform.h>
#include <multipass/virtual_machine.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/virtual_machine_factory.h>

#include <QRegularExpression>

#include <stdexcept>
#include <tuple>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;

namespace multipass::test
{
struct HyperVNetworkAccessor
{
    explicit HyperVNetworkAccessor(HyperVVirtualMachineFactory& backend) : backend{backend}
    {
    }

    static auto get_switches(const std::vector<mp::NetworkInterfaceInfo>& adapters)
    {
        return mp::HyperVVirtualMachineFactory::get_switches(adapters); // private, but we're friends
    }

    static auto get_adapters()
    {
        return mp::HyperVVirtualMachineFactory::get_adapters(); // private, but we're friends
    }

    std::string create_bridge_with(const mp::NetworkInterfaceInfo& interface)
    {
        return backend.create_bridge_with(interface); // protected, but we're friends
    }

    HyperVVirtualMachineFactory& backend;
};
} // namespace multipass::test

namespace
{
struct HyperVBackend : public Test
{
    using RunSpec = mpt::PowerShellTestHelper::RunSpec;

    void SetUp() override
    {
        logger_scope.mock_logger->screen_logs(mpl::Level::warning);
    }

    void TearDown() override
    {
        ASSERT_TRUE(ps_helper.was_ps_run());
    }

    std::vector<RunSpec> standard_ps_run_sequence(const std::vector<RunSpec>& network_runs = {default_network_run})
    {
        std::vector<RunSpec> ret = base_ctor_runs;
        ret.insert(std::end(ret), std::cbegin(network_runs), std::cend(network_runs));

        auto failing_run_it = std::find_if(std::cbegin(network_runs), std::cend(network_runs),
                                           [](const auto& run) { return !run.will_return; });
        if (failing_run_it == std::cend(network_runs)) // if the ctor succeeds
            ret.emplace_back(min_dtor_run); // network runs are executed in the ctor, so if they fail the object is
                                            // never constructed and no dtor is called

        return ret;
    }

    inline static const std::vector<RunSpec> base_ctor_runs = {
        {"Get-VM", "", false}, {"Get-VMSwitch"},   {"New-VM"},      {"-EnableSecureBoot Off"},
        {"Set-VMProcessor"},   {"Add-VMDvdDrive"}, {"Set-VMMemory"}};
    inline static const RunSpec default_network_run = {"Set-VMNetworkAdapter"};
    inline static const RunSpec min_dtor_run = {"-ExpandProperty State", "Off"};

    mpt::TempFile dummy_image;
    mpt::TempFile dummy_cloud_init_iso;
    mp::VirtualMachineDescription default_description{2,
                                                      mp::MemorySize{"3M"},
                                                      mp::MemorySize{}, // not used,
                                                      "pied-piper-valley",
                                                      "ba:ba:ca:ca:ca:ba",
                                                      {},
                                                      "",
                                                      {dummy_image.name(), "", "", "", "", {}},
                                                      dummy_cloud_init_iso.name()};
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
    mpt::PowerShellTestHelper ps_helper;
    mp::HyperVVirtualMachineFactory backend;
    mpt::StubVMStatusMonitor stub_monitor;
};

TEST_F(HyperVBackend, creates_in_off_state)
{
    ps_helper.setup_mocked_run_sequence(standard_ps_run_sequence());

    auto machine = backend.create_virtual_machine(default_description, stub_monitor);
    ASSERT_THAT(machine.get(), NotNull());
    EXPECT_THAT(machine->state, Eq(mp::VirtualMachine::State::off));
}

TEST_F(HyperVBackend, sets_mac_address_on_default_network_adapter)
{
    auto network_run = RunSpec{fmt::format("Set-VMNetworkAdapter -VMName {} -StaticMacAddress \"{}\"",
                                           default_description.vm_name, default_description.default_mac_address)};
    ps_helper.setup_mocked_run_sequence(standard_ps_run_sequence({network_run}));

    backend.create_virtual_machine(default_description, stub_monitor);
}

TEST_F(HyperVBackend, throws_on_failure_to_setup_default_network_adapter)
{
    auto run = default_network_run;
    run.will_return = false;

    ps_helper.setup_mocked_run_sequence(standard_ps_run_sequence({run}));

    MP_EXPECT_THROW_THAT(backend.create_virtual_machine(default_description, stub_monitor), std::runtime_error,
                         Property(&std::runtime_error::what, HasSubstr("default adapter")));
}

TEST_F(HyperVBackend, adds_extra_network_adapters)
{
    default_description.extra_interfaces = {
        {"switchA", "55:66:44:77:33:88"}, {"switchB", "15:16:14:17:13:18"}, {"switchC", "5e:6f:4e:7f:3e:8f"}};

    auto network_runs = std::vector<RunSpec>{default_network_run};
    for (const auto& iface : default_description.extra_interfaces)
    {
        network_runs.push_back({fmt::format("Get-VMSwitch -Name \"{}\"", iface.id)});
        network_runs.push_back(
            {fmt::format("Add-VMNetworkAdapter -VMName {} -SwitchName \"{}\" -StaticMacAddress \"{}\"",
                         default_description.vm_name, iface.id, iface.mac_address)});
    };

    ps_helper.setup_mocked_run_sequence(standard_ps_run_sequence(std::move(network_runs)));

    backend.create_virtual_machine(default_description, stub_monitor);
}

TEST_F(HyperVBackend, throws_on_failure_to_detect_switch_from_extra_interface)
{
    auto extra_iface = mp::NetworkInterface{"MissingSwitch", "55:66:44:77:33:88"};
    default_description.extra_interfaces.push_back(extra_iface);

    auto failing_cmd = fmt::format("Get-VMSwitch -Name \"{}\"", extra_iface.id);
    ps_helper.setup_mocked_run_sequence(standard_ps_run_sequence({default_network_run, {failing_cmd, "", false}}));

    MP_EXPECT_THROW_THAT(
        backend.create_virtual_machine(default_description, stub_monitor), std::runtime_error,
        Property(&std::runtime_error::what, AllOf(HasSubstr("Could not find"), HasSubstr(extra_iface.id))));
}

TEST_F(HyperVBackend, throws_on_failure_to_add_extra_interface)
{
    auto extra_iface = mp::NetworkInterface{"SuperPriviledgedSwitch", "55:66:44:77:33:88"};
    default_description.extra_interfaces.push_back(extra_iface);

    auto failing_cmd = fmt::format("Add-VMNetworkAdapter -VMName {} -SwitchName \"{}\" -StaticMacAddress \"{}\"",
                                   default_description.vm_name, extra_iface.id, extra_iface.mac_address);

    ps_helper.setup_mocked_run_sequence(
        standard_ps_run_sequence({default_network_run, {"Get-VMSwitch"}, {failing_cmd, "", false}}));

    MP_EXPECT_THROW_THAT(
        backend.create_virtual_machine(default_description, stub_monitor), std::runtime_error,
        Property(&std::runtime_error::what, AllOf(HasSubstr("Could not setup"), HasSubstr(extra_iface.id))));
}

TEST_F(HyperVBackend, createBridgeRequestsNewSwitch)
{
    const mp::NetworkInterfaceInfo net{"asdf", "ethernet", "The asdf net"};

    ps_helper.setup(
        [&net](auto* process) {
            EXPECT_THAT(process->arguments(),
                        IsSupersetOf({mpt::match_qstring("New-VMSwitch"), mpt::match_qstring(HasSubstr("ExtSwitch")),
                                      mpt::match_qstring(HasSubstr(net.id))}));
            EXPECT_CALL(*process, wait_for_finished).WillOnce(Return(true));
        },
        /* auto_exit = */ false);

    EXPECT_ANY_THROW(mpt::HyperVNetworkAccessor{backend}.create_bridge_with(net));
}

TEST_F(HyperVBackend, createBridgeReturnsNewSwitchName)
{
    const mp::NetworkInterfaceInfo net{"w1", "wifi", "Wireless network"};
    const auto switch_name = fmt::format("ExtSwitch ({})", net.id);
    ps_helper.mock_ps_exec(QByteArray::fromStdString(switch_name));
    EXPECT_THAT(mpt::HyperVNetworkAccessor{backend}.create_bridge_with(net), Eq(switch_name));
}

TEST_F(HyperVBackend, createBridgeThrowsOnNameMismatch)
{
    const auto bad = "wrong";
    ps_helper.mock_ps_exec(bad);

    MP_EXPECT_THROW_THAT(mpt::HyperVNetworkAccessor{backend}.create_bridge_with({"lagwagon", "wifi", "duh"}),
                         std::runtime_error, mpt::match_what(HasSubstr(bad)));
}

TEST_F(HyperVBackend, createBridgeThrowsOnProcessFailure)
{
    const mp::NetworkInterfaceInfo net{"rerere", "ethernet", "lilo"};
    ps_helper.mock_ps_exec(QByteArray::fromStdString(fmt::format("ExtSwitch ({})", net.id)), /* succeed = */ false);

    logger_scope.mock_logger->expect_log(mpl::Level::warning, "Process failed");
    MP_EXPECT_THROW_THAT(mpt::HyperVNetworkAccessor{backend}.create_bridge_with(net), std::runtime_error,
                         mpt::match_what(HasSubstr("Could not create external switch")));
}

TEST_F(HyperVBackend, createBridgeIncludesErrorMsgInException)
{
    const auto error = "Bad Astronaut";
    ps_helper.mock_ps_exec(error, /* succeed = */ false);

    logger_scope.mock_logger->expect_log(mpl::Level::warning, "Process failed");
    MP_EXPECT_THROW_THAT(mpt::HyperVNetworkAccessor{backend}.create_bridge_with({"Needle", "wifi", "in the hay"}),
                         std::runtime_error, mpt::match_what(HasSubstr(error)));
}

struct CheckFineSuite : public HyperVBackend, public WithParamInterface<std::vector<mpt::PowerShellTestHelper::RunSpec>>
{
};

TEST_P(CheckFineSuite, CheckDoesntThrow)
{
    ps_helper.setup_mocked_run_sequence(GetParam());
    EXPECT_NO_THROW(backend.hypervisor_health_check());
}

INSTANTIATE_TEST_SUITE_P(
    HyperVBackend, CheckFineSuite,
    // Common case, vmms running
    Values(std::vector<mpt::PowerShellTestHelper::RunSpec>{{"CurrentMajorVersionNumber", "10"},
                                                           {"ReleaseId", "1803"},
                                                           {"HypervisorPresent", "True"},
                                                           {"Microsoft-Hyper-V", "Enabled"},
                                                           {"Microsoft-Hyper-V-Hypervisor", "Enabled"},
                                                           {"vmms", "Running"}},
           // Common case, vmms needs to be started
           std::vector<mpt::PowerShellTestHelper::RunSpec>{{"CurrentMajorVersionNumber", "10"},
                                                           {"ReleaseId", "1803"},
                                                           {"HypervisorPresent", "True"},
                                                           {"Microsoft-Hyper-V", "Enabled"},
                                                           {"Microsoft-Hyper-V-Hypervisor", "Enabled"},
                                                           {"vmms", "Stopped"},
                                                           {"Get-Service", "Automatic"},
                                                           {"Start-Service"}},
           // New ReleaseId format
           std::vector<mpt::PowerShellTestHelper::RunSpec>{{"CurrentMajorVersionNumber", "10"},
                                                           {"ReleaseId", "21H2"},
                                                           {"HypervisorPresent", "True"},
                                                           {"Microsoft-Hyper-V", "Enabled"},
                                                           {"Microsoft-Hyper-V-Hypervisor", "Enabled"},
                                                           {"vmms", "Running"}},
           // Windows 11, no need to check ReleaseId
           std::vector<mpt::PowerShellTestHelper::RunSpec>{{"CurrentMajorVersionNumber", "11"},
                                                           {"HypervisorPresent", "True"},
                                                           {"Microsoft-Hyper-V", "Enabled"},
                                                           {"Microsoft-Hyper-V-Hypervisor", "Enabled"},
                                                           {"vmms", "Running"}}));

struct CheckBadSuite : public HyperVBackend, public WithParamInterface<std::vector<mpt::PowerShellTestHelper::RunSpec>>
{
};

TEST_P(CheckBadSuite, CheckThrows)
{
    ps_helper.setup_mocked_run_sequence(GetParam());
    EXPECT_THROW(backend.hypervisor_health_check(), std::runtime_error);
}

INSTANTIATE_TEST_SUITE_P(
    HyperVBackend, CheckBadSuite,
    // Windows 7
    Values(std::vector<mpt::PowerShellTestHelper::RunSpec>{{"CurrentMajorVersionNumber", "7"}},
           // Windows 10, too old
           std::vector<mpt::PowerShellTestHelper::RunSpec>{{"CurrentMajorVersionNumber", "10"}, {"ReleaseId", "1802"}},
           // vmms service fails to start
           std::vector<mpt::PowerShellTestHelper::RunSpec>{{"CurrentMajorVersionNumber", "11"},
                                                           {"HypervisorPresent", "True"},
                                                           {"Microsoft-Hyper-V", "Enabled"},
                                                           {"Microsoft-Hyper-V-Hypervisor", "Enabled"},
                                                           {"vmms", "Stopped"},
                                                           {"Get-Service", "Automatic"},
                                                           {"Start-Service", "", false}},
           // vmms service disabled
           std::vector<mpt::PowerShellTestHelper::RunSpec>{{"CurrentMajorVersionNumber", "11"},
                                                           {"HypervisorPresent", "True"},
                                                           {"Microsoft-Hyper-V", "Enabled"},
                                                           {"Microsoft-Hyper-V-Hypervisor", "Enabled"},
                                                           {"vmms", "Stopped"},
                                                           {"Get-Service", "Disabled"}}));
struct HyperVNetworks : public Test
{
    void SetUp() override
    {
        logger_scope.mock_logger->screen_logs(mpl::Level::warning);
    }

    std::map<std::string, mp::NetworkInterfaceInfo> network_map_from_vector(std::vector<mp::NetworkInterfaceInfo> nets)
    {
        std::map<std::string, mp::NetworkInterfaceInfo> ret;
        for (const auto& net : nets)
        {
            auto id = net.id; // avoid param evaluation order issues - we'd need the copy anyway
            ret.emplace(std::move(id), std::move(net));
        }

        return ret;
    }

    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
    mpt::MockPlatform::GuardedMock attr = mpt::MockPlatform::inject<NiceMock>();
    mpt::MockPlatform* mock_platform = attr.first;
    mp::HyperVVirtualMachineFactory backend;
};

struct HyperVNetworksPS : public HyperVNetworks
{
    void TearDown() override
    {
        ASSERT_TRUE(ps_helper.was_ps_run());
    }

    mpt::PowerShellTestHelper ps_helper;
    inline static constexpr auto cmdlet = "Get-VMSwitch";
};

TEST_F(HyperVNetworksPS, requestsSwitches)
{
    ps_helper.setup(
        [](auto* process) {
            EXPECT_THAT(process->arguments(), Contains(cmdlet));
            EXPECT_CALL(*process, wait_for_finished).WillOnce(Return(true));
        },
        /* auto_exit = */ false);

    backend.networks();
}

TEST_F(HyperVNetworksPS, requestsPlatformInterfaces)
{
    ps_helper.mock_ps_exec("");
    EXPECT_CALL(*mock_platform, get_network_interfaces_info).Times(1);
    backend.networks();
}

TEST_F(HyperVNetworksPS, joinsSwitchesAndAdapters)
{
    ps_helper.mock_ps_exec("switch,External, a switch,\n");
    EXPECT_CALL(*mock_platform, get_network_interfaces_info)
        .WillOnce(Return(network_map_from_vector({{"wlan", "wifi", "wireless"}, {"eth", "ethernet", "wired"}})));

    auto got_nets = backend.networks();
    EXPECT_THAT(got_nets, SizeIs(3));
    EXPECT_THAT(got_nets, Contains(Field(&mp::NetworkInterfaceInfo::type, Eq("wifi"))));
    EXPECT_THAT(got_nets, Contains(Field(&mp::NetworkInterfaceInfo::type, Eq("switch"))));
    EXPECT_THAT(got_nets, Contains(Field(&mp::NetworkInterfaceInfo::type, Eq("ethernet"))));
}

TEST_F(HyperVNetworksPS, throwsOnFailureToExecuteCmdlet)
{
    auto& logger = *logger_scope.mock_logger;
    logger.screen_logs(mpl::Level::warning);
    logger.expect_log(mpl::Level::warning, "Process failed");

    constexpr auto error = "error msg";
    ps_helper.mock_ps_exec(error, false);
    MP_ASSERT_THROW_THAT(backend.networks(), std::runtime_error, Property(&std::runtime_error::what, HasSubstr(error)));
}

TEST_F(HyperVNetworksPS, throwsOnUnexpectedCmdletOutput)
{
    constexpr auto output = "g1bb€r1$h";
    ps_helper.mock_ps_exec(output);
    MP_ASSERT_THROW_THAT(backend.networks(), std::runtime_error,
                         Property(&std::runtime_error::what, AllOf(HasSubstr(output), HasSubstr("unexpected"))));
}

struct TestWrongNumFields : public HyperVNetworksPS, public WithParamInterface<std::string>
{
    inline static constexpr auto bad_line_in_output_format = "a,few,,\ngood,lines,,\n{}\naround,a,,\nbad,one,,";
};

TEST_P(TestWrongNumFields, throwsOnOutputWithWrongFields)
{
    ps_helper.mock_ps_exec(QByteArray::fromStdString(fmt::format(bad_line_in_output_format, GetParam())));
    ASSERT_THROW(backend.networks(), std::runtime_error);
}

INSTANTIATE_TEST_SUITE_P(HyperVNetworksPS, TestWrongNumFields,
                         Values("too,many,fields,in,here", "insufficient,fields"));

struct TestNonExternalSwitchesWithLinks : public HyperVNetworksPS, public WithParamInterface<std::string>
{
    inline static constexpr auto bad_line_in_output_format = TestWrongNumFields::bad_line_in_output_format;
};

TEST_P(TestNonExternalSwitchesWithLinks, throwsOnNonExternalSwitchWithLink)
{
    constexpr auto link_description = "foo bar net";
    EXPECT_CALL(*mock_platform, get_network_interfaces_info)
        .WillOnce(Return(network_map_from_vector({{"eth", "ethernet", link_description}})));

    auto switch_type = GetParam();
    auto switch_line = fmt::format("a switch,{},{},", switch_type, link_description);
    ps_helper.mock_ps_exec(QByteArray::fromStdString(fmt::format(bad_line_in_output_format, switch_line)));

    MP_ASSERT_THROW_THAT(backend.networks(), std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("Unexpected"), HasSubstr("non-external"))));
}

INSTANTIATE_TEST_SUITE_P(HyperVNetworksPS, TestNonExternalSwitchesWithLinks, Values("internal", "private"));

template <typename Str>
QRegularExpression make_case_insensitive_regex(Str&& str)
{
    return QRegularExpression{std::forward<Str>(str), QRegularExpression::CaseInsensitiveOption};
}

template <typename Str1, typename Str2>
auto make_required_forbidden_regex_matcher(Str1&& required, Str2&& forbidden)
{
    return Truly([required = make_case_insensitive_regex(std::forward<Str1>(required)),
                  forbidden = make_case_insensitive_regex(std::forward<Str2>(forbidden))](const std::string& str) {
        auto qstr = QString::fromStdString(str);
        return qstr.contains(required) && !qstr.contains(forbidden);
    });
}

template <typename Matcher>
auto adapt_to_single_description_matcher(Matcher&& matcher)
{
    return ElementsAre(Field(&mp::NetworkInterfaceInfo::description, std::forward<Matcher>(matcher)));
}

struct TestNonExternalSwitchTypes : public HyperVNetworksPS, public WithParamInterface<QString>
{
};

TEST_P(TestNonExternalSwitchTypes, recognizes_switch_type)
{
    const auto& type = GetParam();
    const auto matcher =
        adapt_to_single_description_matcher(make_required_forbidden_regex_matcher(type, "external|unknown"));

    ps_helper.mock_ps_exec(QByteArray::fromStdString(fmt::format("some switch,{},,", type)));
    EXPECT_THAT(backend.networks(), matcher);
}

INSTANTIATE_TEST_SUITE_P(HyperVNetworksPS, TestNonExternalSwitchTypes, Values("Private", "Internal"));

TEST_F(HyperVNetworksPS, recognizesExternalSwitch)
{
    const auto name = "some switch";

    const auto id_matcher = Field(&mp::NetworkInterfaceInfo::id, name);
    const auto type_matcher = Field(&mp::NetworkInterfaceInfo::type, "switch");
    const auto desc_matcher =
        Field(&mp::NetworkInterfaceInfo::description, make_required_forbidden_regex_matcher("external", "unknown"));
    const auto switch_matcher = AllOf(id_matcher, type_matcher, desc_matcher);

    ps_helper.mock_ps_exec(QByteArray::fromStdString(fmt::format("{},external,fake nic,", name)));
    EXPECT_THAT(backend.networks(), ElementsAre(switch_matcher));
}

TEST_F(HyperVNetworksPS, handlesUnknownSwitchTypes)
{
    constexpr auto type = "Strange";
    const auto matcher = adapt_to_single_description_matcher(
        AllOf(make_required_forbidden_regex_matcher("unknown", "private|internal|external"), HasSubstr(type)));

    ps_helper.mock_ps_exec(QByteArray::fromStdString(fmt::format("Custom Switch,{},,", type)));
    EXPECT_THAT(backend.networks(), matcher);
}

TEST_F(HyperVNetworksPS, includesSwitchLinksToKnownAdapters)
{
    mp::NetworkInterfaceInfo net_a{"a", "ethernet", "an a a aaa"};
    mp::NetworkInterfaceInfo net_b{"b", "wifi", "a bbb b b"};
    mp::NetworkInterfaceInfo net_c{"c", "ethernet", "a c cc cc"};

    EXPECT_CALL(*mock_platform, get_network_interfaces_info)
        .WillOnce(Return(network_map_from_vector({net_a, net_b, net_c})));
    ps_helper.mock_ps_exec(QByteArray::fromStdString(fmt::format(
        "switch_{},external,{},\nswitch_{},external,{},", net_b.id, net_b.description, net_c.id, net_c.description)));

    auto match = [](const auto& expect_link) {
        return AllOf(Field(&mp::NetworkInterfaceInfo::id, EndsWith(expect_link)),
                     Field(&mp::NetworkInterfaceInfo::links, ElementsAre(expect_link)));
    };
    EXPECT_THAT(backend.networks(), IsSupersetOf({match(net_b.id), match(net_c.id)}));
}

struct TestSwitchUnsupportedLinks : public HyperVNetworksPS,
                                    public WithParamInterface<std::vector<mp::NetworkInterfaceInfo>>
{
};

TEST_P(TestSwitchUnsupportedLinks, excludesSwitchLinksToUnknownAdapters)
{
    EXPECT_CALL(*mock_platform, get_network_interfaces_info).WillOnce(Return(network_map_from_vector(GetParam())));
    ps_helper.mock_ps_exec("switchin,external,crazy adapter,\nnope,external,nope,");

    auto switch_matcher =
        AllOf(Field(&mp::NetworkInterfaceInfo::type, Eq("switch")), Field(&mp::NetworkInterfaceInfo::links, IsEmpty()));
    EXPECT_THAT(backend.networks(), IsSupersetOf({switch_matcher, switch_matcher})); // expect two such switches
}

TEST_P(TestSwitchUnsupportedLinks, omitsUnsupportedAdapterFromExternalSwitchDescription)
{
    EXPECT_CALL(*mock_platform, get_network_interfaces_info).WillOnce(Return(network_map_from_vector(GetParam())));
    const auto desc_matcher =
        Field(&mp::NetworkInterfaceInfo::description,
              make_required_forbidden_regex_matcher("^(?=.*switch)(?=.*external)", "via|internal|private"));

    ps_helper.mock_ps_exec("some switch,external,some unknown NIC,");
    EXPECT_THAT(backend.networks(), Contains(desc_matcher));
}

INSTANTIATE_TEST_SUITE_P(HyperVNetworksPS, TestSwitchUnsupportedLinks,
                         Values(std::vector<mp::NetworkInterfaceInfo>{},
                                std::vector<mp::NetworkInterfaceInfo>{{"nic", "wifi", "a wifi"},
                                                                      {"eth", "ethernet", "an ethernet"}},
                                std::vector<mp::NetworkInterfaceInfo>{{"nic", "crazy_type", "some unknown NIC"},
                                                                      {"eth", "ethernet", "an ethernet"}}));

TEST_F(HyperVNetworksPS, includesSupportedAdapterInExternalSwitchDescription)
{
    mp::NetworkInterfaceInfo wifi{"wlan", "wifi", "A Wifi NIC"};
    mp::NetworkInterfaceInfo eth{"Ethernet", "ethernet", "An Ethernet NIC"};
    mp::NetworkInterfaceInfo other{"Quantumwire", "quantum wire", "Future tech"};
    EXPECT_CALL(*mock_platform, get_network_interfaces_info)
        .WillOnce(Return(network_map_from_vector({wifi, eth, other})));

    auto match = [](const auto& expect_link) {
        return Field(&mp::NetworkInterfaceInfo::description,
                     AllOf(make_required_forbidden_regex_matcher("external", "unknown"), HasSubstr(expect_link)));
    };

    ps_helper.mock_ps_exec(QByteArray::fromStdString(
        fmt::format("some switch,external,{},\nanother,external,{},", eth.description, wifi.description)));
    EXPECT_THAT(backend.networks(), IsSupersetOf({match(wifi.id), match(eth.id)}));
}

TEST_F(HyperVNetworksPS, includesExistingNotesInSwitchDescription)
{
    constexpr auto notes = "custom notes";
    constexpr auto adapter_id = "Ethernet";
    constexpr auto output_format = "PrivSwitch,Private,,{0}\n"
                                   "IntSwitch,Internal,,{0}\n"
                                   "ExtSwitchA,External,,{0}\n"
                                   "ExtSwitchB,External,{1},{0}\n";
    ps_helper.mock_ps_exec(QByteArray::fromStdString(fmt::format(output_format, notes, adapter_id)));

    EXPECT_CALL(*mock_platform, get_network_interfaces_info)
        .WillOnce(Return(network_map_from_vector({mp::NetworkInterfaceInfo{adapter_id, "ethernet", "eth adapter"}})));

    auto matchers = std::vector{4, Field(&mp::NetworkInterfaceInfo::description, HasSubstr(notes))};
    matchers.push_back(Field(&mp::NetworkInterfaceInfo::id, Eq(adapter_id)));
    EXPECT_THAT(backend.networks(), UnorderedElementsAreArray(matchers));
}

struct TestAdapterAuthorization : public HyperVNetworksPS, public WithParamInterface<mp::NetworkInterfaceInfo>
{
};

TEST_P(TestAdapterAuthorization, requiresNoAuthorizationForKnownAdaptersInSwitches)
{
    const auto& net = GetParam();
    EXPECT_CALL(*mock_platform, get_network_interfaces_info).WillOnce(Return(network_map_from_vector({net})));

    ps_helper.mock_ps_exec(QByteArray::fromStdString(fmt::format("switch,external,{},\n", net.description)));
    EXPECT_THAT(backend.networks(), Contains(AllOf(Field(&mp::NetworkInterfaceInfo::id, net.id),
                                                   Field(&mp::NetworkInterfaceInfo::needs_authorization, false))));
}

TEST_P(TestAdapterAuthorization, requiresAuthorizationForKnownAdaptersInNoSwitches)
{
    const auto& net = GetParam();
    EXPECT_CALL(*mock_platform, get_network_interfaces_info).WillOnce(Return(network_map_from_vector({net})));

    ps_helper.mock_ps_exec("switch,external,unknown adapter,\n");
    EXPECT_THAT(backend.networks(), Contains(AllOf(Field(&mp::NetworkInterfaceInfo::id, net.id),
                                                   Field(&mp::NetworkInterfaceInfo::needs_authorization, true))));
}

TEST_P(TestAdapterAuthorization, requiresNoAuthorizationForSwitches)
{
    const auto& net = GetParam();
    EXPECT_CALL(*mock_platform, get_network_interfaces_info).WillOnce(Return(network_map_from_vector({net})));

    constexpr auto ps_output = "ext_switch_a,external,unknown adapter,\next_switch_a,external,{},"
                               "\nint_switch,internal,,\npriv_switch,private,,\n";
    ps_helper.mock_ps_exec(QByteArray::fromStdString(fmt::format(ps_output, net.description)));

    const auto networks = backend.networks();
    EXPECT_THAT(std::count_if(networks.cbegin(), networks.cend(), [](const auto& x) { return x.type == "switch"; }), 4);
    EXPECT_THAT(networks, Each(AnyOf(Field(&mp::NetworkInterfaceInfo::type, Ne("switch")),
                                     Field(&mp::NetworkInterfaceInfo::needs_authorization, false))));
}

INSTANTIATE_TEST_SUITE_P(HyperVNetworkPS, TestAdapterAuthorization,
                         Values(mp::NetworkInterfaceInfo{"abc", "ethernet", "An adapter", {}, false},
                                mp::NetworkInterfaceInfo{"def", "wifi", "Another adapter", {}, true},
                                mp::NetworkInterfaceInfo{"ghi", "ethernet", "Yet another", {"x", "y", "z"}, false},
                                mp::NetworkInterfaceInfo{"jkl", "wifi", "And a final one", {"w"}, true}));

TEST_F(HyperVNetworksPS, getSwitchesReturnsEmptyWhenNoSwitchesFound)
{
    ps_helper.mock_ps_exec("");
    EXPECT_THAT(mpt::HyperVNetworkAccessor::get_switches({}), IsEmpty());
}

TEST_F(HyperVNetworksPS, getSwitchesReturnsAsManyItemsAsLinesInProperOutput)
{
    ps_helper.mock_ps_exec("a,b,,\nd,e,,\ng,h,,\nj,k,,\n,,,\n,m,,\njj,external,asdf,\n");
    EXPECT_THAT(mpt::HyperVNetworkAccessor::get_switches({{}}), SizeIs(7));
}

TEST_F(HyperVNetworksPS, getSwitchesReturnsProvidedInterfaceIds)
{
    constexpr auto id1 = "\"toto\"";
    constexpr auto id2 = " te et te";
    constexpr auto id3 = "\"ti\"-+%ti\t";
    constexpr auto output_format = "{},Private,,\n"
                                   "{},Internal,,\n"
                                   "{},External,adapter description,\n";

    ps_helper.mock_ps_exec(QByteArray::fromStdString(fmt::format(output_format, id1, id2, id3)));
    auto id_matcher = [](const auto& expect) { return Field(&mp::NetworkInterfaceInfo::id, expect); };
    EXPECT_THAT(mpt::HyperVNetworkAccessor::get_switches({{}, {}}),
                UnorderedElementsAre(id_matcher(id1), id_matcher(id2), id_matcher(id3)));
}

TEST_F(HyperVNetworksPS, getSwitchesReturnsOnlySwitches)
{
    ps_helper.mock_ps_exec("a,b,,\nc,d,,\nasdf,internal,,\nsdfg,external,dfgh,\nfghj,private,,");
    EXPECT_THAT(mpt::HyperVNetworkAccessor::get_switches({}), Each(Field(&mp::NetworkInterfaceInfo::type, "switch")));
}

TEST_F(HyperVNetworks, getAdaptersReturnsEthernetAndWifi)
{
    mp::NetworkInterfaceInfo strange{"strange", "strangewire", "waka waka"};
    mp::NetworkInterfaceInfo weird{"weird", "future tech", "wika wika"};
    mp::NetworkInterfaceInfo unknown{"virtio", "unknown", "wuka wuka"};
    mp::NetworkInterfaceInfo eth1{"eth1", "ethernet", "ethththth"};
    mp::NetworkInterfaceInfo eth2{"eth2", "ethernet", "ethththth"};
    mp::NetworkInterfaceInfo wifi1{"wireless1", "wifi", "wiiiiiii"};
    mp::NetworkInterfaceInfo wifi2{"wireless2", "wifi", "wiiiiiii"};

    EXPECT_CALL(*mock_platform, get_network_interfaces_info)
        .WillOnce(Return(network_map_from_vector({strange, eth1, unknown, wifi1, eth2, weird, wifi2})));

    auto got_nets = mpt::HyperVNetworkAccessor::get_adapters();
    EXPECT_EQ(got_nets.size(), 4);

    auto same_net = [](const mp::NetworkInterfaceInfo& a, const mp::NetworkInterfaceInfo& b) {
        return make_tuple(a.id, a.type, a.description) == make_tuple(b.id, b.type, b.description);
    };

    for (const auto& expected_net : {eth1, eth2, wifi1, wifi2})
        EXPECT_TRUE(std::any_of(got_nets.begin(), got_nets.end(), [&expected_net, &same_net](const auto& got_net) {
            return same_net(got_net, expected_net);
        }));
}

} // namespace
