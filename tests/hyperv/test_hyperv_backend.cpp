/*
 * Copyright (C) 2017-2020 Canonical, Ltd.
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

#include <multipass/format.h>
#include <multipass/memory_size.h>
#include <multipass/platform.h>
#include <multipass/virtual_machine.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/virtual_machine_factory.h>
#include <src/platform/backends/hyperv/hyperv_virtual_machine_factory.h>

#include "tests/extra_assertions.h"
#include "tests/mock_logger.h"
#include "tests/mock_process_factory.h"
#include "tests/stub_ssh_key_provider.h"
#include "tests/stub_status_monitor.h"
#include "tests/temp_file.h"
#include "tests/windows/powershell_test_helper.h"

#include <QRegularExpression>

#include <gmock/gmock.h>

#include <stdexcept>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;

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
        {"Get-VM", "", false}, {"Get-VMSwitch"}, {"New-VM"}, {"Set-VMProcessor"}, {"Add-VMDvdDrive"}};
    inline static const RunSpec default_network_run = {"Set-VMNetworkAdapter"};
    inline static const RunSpec min_dtor_run = {"-ExpandProperty State", "Off"};

    mpt::TempFile dummy_image;
    mpt::TempFile dummy_cloud_init_iso;
    mp::VirtualMachineDescription default_description{2,
                                                      mp::MemorySize{"3M"},
                                                      mp::MemorySize{}, // not used,
                                                      "pied-piper-valley",
                                                      {"default", "ba:ba:ca:ca:ca:ba", true},
                                                      {},
                                                      "",
                                                      {dummy_image.name(), "", "", "", "", "", "", {}},
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
    // TODO we only honor default_interface.mac_address, the rest is ignored, so should probably receive only the mac
    auto network_run =
        RunSpec{fmt::format("Set-VMNetworkAdapter -VMName {} -StaticMacAddress \"{}\"", default_description.vm_name,
                            default_description.default_interface.mac_address)};
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

struct HyperVListNetworks : public Test
{
    void SetUp() override
    {
        logger_scope.mock_logger->screen_logs(mpl::Level::warning);
    }

    void TearDown() override
    {
        ASSERT_TRUE(ps_helper.was_ps_run());
    }

    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
    mp::HyperVVirtualMachineFactory backend;
    mpt::PowerShellTestHelper ps_helper;
    inline static constexpr auto cmdlet = "Get-VMSwitch";
};

TEST_F(HyperVListNetworks, requests_switches)
{
    ps_helper.setup(
        [](auto* process) {
            EXPECT_THAT(process->arguments(), Contains(cmdlet));
            EXPECT_CALL(*process, wait_for_finished).WillOnce(Return(true));
        },
        /* auto_exit = */ false);

    backend.list_networks();
}

TEST_F(HyperVListNetworks, returns_empty_when_no_switches_found)
{
    ps_helper.mock_ps_exec("");
    EXPECT_THAT(backend.list_networks(), IsEmpty());
}

TEST_F(HyperVListNetworks, throws_on_failure_to_execute_cmdlet)
{
    auto& logger = *logger_scope.mock_logger;
    logger.screen_logs(mpl::Level::warning);
    logger.expect_log(mpl::Level::warning, cmdlet);

    constexpr auto error = "error msg";
    ps_helper.mock_ps_exec(error, false);
    MP_ASSERT_THROW_THAT(backend.list_networks(), std::runtime_error,
                         Property(&std::runtime_error::what, HasSubstr(error)));
}

TEST_F(HyperVListNetworks, throws_on_unexpected_cmdlet_output)
{
    constexpr auto output = "g1bb€r1$h";
    ps_helper.mock_ps_exec(output);
    MP_ASSERT_THROW_THAT(backend.list_networks(), std::runtime_error,
                         Property(&std::runtime_error::what, AllOf(HasSubstr(output), HasSubstr("unexpected"))));
}

struct TestWrongFields : public HyperVListNetworks, public WithParamInterface<std::string>
{
    inline static constexpr auto bad_line_in_output_format = "a,few,\ngood,lines,\n{}\naround,a,\nbad,one,";
};

TEST_P(TestWrongFields, throws_on_output_with_wrong_fields)
{
    ps_helper.mock_ps_exec(QByteArray::fromStdString(fmt::format(bad_line_in_output_format, GetParam())));
    ASSERT_THROW(backend.list_networks(), std::runtime_error);
}

INSTANTIATE_TEST_SUITE_P(HyperVListNetworks, TestWrongFields,
                         Values("too,many,fields,here", "insufficient,fields",
                                "an, internal switch, shouldn't be connected to an external adapter",
                                "nor should a, private, one", "but an, external one should,"));

TEST_F(HyperVListNetworks, returns_as_many_items_as_lines_in_proper_output)
{
    ps_helper.mock_ps_exec("a,b,\nd,e,\ng,h,\nj,k,\n,,\n,m,\njj,external,asdf\n");
    EXPECT_THAT(backend.list_networks(), SizeIs(7));
}

TEST_F(HyperVListNetworks, returns_provided_interface_ids)
{
    constexpr auto id1 = "\"toto\"";
    constexpr auto id2 = " te et te";
    constexpr auto id3 = "\"ti\"-+%ti\t";
    constexpr auto output_format = "\"{}\",\"Private\",\n"
                                   "\"{}\",\"Internal\",\n"
                                   "\"{}\",\"External\",\"adapter description\"\n";

    ps_helper.mock_ps_exec(QByteArray::fromStdString(fmt::format(output_format, id1, id2, id3)));
    auto id_matcher = [](const auto& expect) { return Field(&mp::NetworkInterfaceInfo::id, expect); };
    EXPECT_THAT(backend.list_networks(), UnorderedElementsAre(id_matcher(id1), id_matcher(id2), id_matcher(id3)));
}

TEST_F(HyperVListNetworks, returns_only_switches)
{
    ps_helper.mock_ps_exec("a,b,\nc,d,\nasdf,internal,\nsdfg,external,dfgh\nfghj,private,");
    EXPECT_THAT(backend.list_networks(), Each(Field(&mp::NetworkInterfaceInfo::type, "switch")));
}

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

struct TestNonExternalSwitchTypes : public HyperVListNetworks, public WithParamInterface<QString>
{
};

TEST_P(TestNonExternalSwitchTypes, recognizes_switch_type)
{
    const auto& type = GetParam();
    const auto matcher =
        adapt_to_single_description_matcher(make_required_forbidden_regex_matcher(type, "external|unknown"));

    ps_helper.mock_ps_exec(QByteArray::fromStdString(fmt::format("some switch,{},", type)));
    EXPECT_THAT(backend.list_networks(), matcher);
}

INSTANTIATE_TEST_SUITE_P(HyperVListNetworks, TestNonExternalSwitchTypes, Values("Private", "Internal"));

TEST_F(HyperVListNetworks, recognizes_external_switch)
{
    constexpr auto nic = "some NIC";
    const auto matcher = adapt_to_single_description_matcher(
        AllOf(make_required_forbidden_regex_matcher("external", "unknown"), HasSubstr(nic)));

    ps_helper.mock_ps_exec(QByteArray::fromStdString(fmt::format("some switch,external,{}", nic)));
    EXPECT_THAT(backend.list_networks(), matcher);
}

TEST_F(HyperVListNetworks, handles_unknown_switch_types)
{
    constexpr auto type = "Strange";
    const auto matcher = adapt_to_single_description_matcher(
        AllOf(make_required_forbidden_regex_matcher("unknown", "private|internal|external"), HasSubstr(type)));

    ps_helper.mock_ps_exec(QByteArray::fromStdString(fmt::format("Custom Switch,{},", type)));
    EXPECT_THAT(backend.list_networks(), matcher);
}

} // namespace
