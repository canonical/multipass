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
#include "tests/windows/power_shell_test.h"

#include <QRegularExpression>

#include <gmock/gmock.h>

#include <stdexcept>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
struct HyperVBackend : public testing::Test
{
    mpt::TempFile dummy_image;
    mpt::TempFile dummy_cloud_init_iso;
    mp::VirtualMachineDescription default_description{2,
                                                      mp::MemorySize{"3M"},
                                                      mp::MemorySize{}, // not used,
                                                      "pied-piper-valley",
                                                      {"default", "", true},
                                                      {},
                                                      "",
                                                      {dummy_image.name(), "", "", "", "", "", "", {}},
                                                      dummy_cloud_init_iso.name()};
    mp::HyperVVirtualMachineFactory backend;
};

TEST_F(HyperVBackend, DISABLED_creates_in_off_state)
{
    mpt::StubVMStatusMonitor stub_monitor;
    auto machine = backend.create_virtual_machine(default_description, stub_monitor);
    ASSERT_THAT(machine.get(), NotNull());
    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));
}

struct HyperVListNetworks : public mpt::PowerShellTest
{
    void SetUp() override
    {
        mpt::PowerShellTest::SetUp(); // This isn't there ATTOW, but could come in the future, so avoid shadowing it
        logger_scope.mock_logger->screen_logs(mpl::Level::warning);
    }

    void simulate_ps_exec_output(const QByteArray& output, bool succeed = true)
    {
        setup([output, succeed](auto* process) {
            InSequence seq;

            auto emit_ready_read = [process] { emit process->ready_read_standard_output(); };
            EXPECT_CALL(*process, start).WillOnce(Invoke(emit_ready_read));
            EXPECT_CALL(*process, read_all_standard_output).WillOnce(Return(output));
            EXPECT_CALL(*process, wait_for_finished).WillOnce(Return(succeed));
        });
    }

    mp::HyperVVirtualMachineFactory backend;
    inline static constexpr auto cmdlet = "Get-VMSwitch";
};

TEST_F(HyperVListNetworks, requests_switches)
{
    setup([](auto* process) { EXPECT_THAT(process->arguments(), Contains(cmdlet)); });

    backend.list_networks();
}

TEST_F(HyperVListNetworks, returns_empty_when_no_switches_found)
{
    simulate_ps_exec_output("");
    EXPECT_THAT(backend.list_networks(), IsEmpty());
}

TEST_F(HyperVListNetworks, throws_on_failure_to_execute_cmdlet)
{
    auto& logger = *logger_scope.mock_logger;
    logger.screen_logs(mpl::Level::warning);
    logger.expect_log(mpl::Level::warning, cmdlet);

    constexpr auto error = "error msg";
    simulate_ps_exec_output(error, false);
    MP_ASSERT_THROW_THAT(backend.list_networks(), std::runtime_error,
                         Property(&std::runtime_error::what, HasSubstr(error)));
}

TEST_F(HyperVListNetworks, throws_on_unexpected_cmdlet_output)
{
    constexpr auto output = "g1bb€r1$h";
    simulate_ps_exec_output(output);
    MP_ASSERT_THROW_THAT(backend.list_networks(), std::runtime_error,
                         Property(&std::runtime_error::what, AllOf(HasSubstr(output), HasSubstr("unexpected"))));
}

struct TestWrongFields : public HyperVListNetworks, public WithParamInterface<std::string>
{
    inline static constexpr auto bad_line_in_output_format = "a,few,\ngood,lines,\n{}\naround,a,\nbad,one,";
};

TEST_P(TestWrongFields, throws_on_output_with_wrong_fields)
{
    simulate_ps_exec_output(QByteArray::fromStdString(fmt::format(bad_line_in_output_format, GetParam())));
    ASSERT_THROW(backend.list_networks(), std::runtime_error);
}

INSTANTIATE_TEST_SUITE_P(HyperVListNetworks, TestWrongFields,
                         Values("too,many,fields,here", "insufficient,fields",
                                "an, internal switch, shouldn't be connected to an external adapter",
                                "nor should a, private, one", "but an, external one should,"));

TEST_F(HyperVListNetworks, returns_as_many_items_as_lines_in_proper_output)
{
    simulate_ps_exec_output("a,b,\nd,e,\ng,h,\nj,k,\n,,\n,m,\njj,external,asdf\n");
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

    simulate_ps_exec_output(QByteArray::fromStdString(fmt::format(output_format, id1, id2, id3)));
    auto id_matcher = [](const auto& expect) { return Field(&mp::NetworkInterfaceInfo::id, expect); };
    EXPECT_THAT(backend.list_networks(), UnorderedElementsAre(id_matcher(id1), id_matcher(id2), id_matcher(id3)));
}

TEST_F(HyperVListNetworks, returns_only_switches)
{
    simulate_ps_exec_output("a,b,\nc,d,\nasdf,internal,\nsdfg,external,dfgh\nfghj,private,");
    EXPECT_THAT(backend.list_networks(), Each(Field(&mp::NetworkInterfaceInfo::type, "switch")));
}

auto make_required_forbidden_regex_matcher(const QRegularExpression& required, const QRegularExpression& forbidden)
{
    return Truly([required, forbidden](const std::string& str) {
        auto qstr = QString::fromStdString(str);
        return qstr.contains(required) && !qstr.contains(forbidden);
    });
}

template <typename Matcher>
auto adapt_to_single_description_matcher(const Matcher& matcher)
{
    return ElementsAre(Field(&mp::NetworkInterfaceInfo::description, matcher));
}

struct TestNonExternalSwitchTypes : public HyperVListNetworks, public WithParamInterface<QString>
{
};

TEST_P(TestNonExternalSwitchTypes, recognizes_switch_type)
{
    const auto& type = GetParam();
    const auto matcher = adapt_to_single_description_matcher(make_required_forbidden_regex_matcher(
        QRegularExpression{type, QRegularExpression::CaseInsensitiveOption},
        QRegularExpression{"external|unknown", QRegularExpression::CaseInsensitiveOption}));

    simulate_ps_exec_output(QByteArray::fromStdString(fmt::format("some switch,{},", type)));
    EXPECT_THAT(backend.list_networks(), matcher);
}

INSTANTIATE_TEST_SUITE_P(HyperVListNetworks, TestNonExternalSwitchTypes, Values("Private", "Internal"));

TEST_F(HyperVListNetworks, recognizes_external_switch)
{
    constexpr auto nic = "some NIC";
    const auto matcher_part =
        make_required_forbidden_regex_matcher(QRegularExpression{"external", QRegularExpression::CaseInsensitiveOption},
                                              QRegularExpression{"unknown", QRegularExpression::CaseInsensitiveOption});
    const auto matcher = adapt_to_single_description_matcher(AllOf(matcher_part, HasSubstr(nic)));

    simulate_ps_exec_output(QByteArray::fromStdString(fmt::format("some switch,external,{}", nic)));
    EXPECT_THAT(backend.list_networks(), matcher);
}

TEST_F(HyperVListNetworks, handles_unknown_switch_types)
{
    constexpr auto type = "Strange";
    const auto matcher_part = make_required_forbidden_regex_matcher(
        QRegularExpression{"unknown", QRegularExpression::CaseInsensitiveOption},
        QRegularExpression{"private|internal|external", QRegularExpression::CaseInsensitiveOption});
    const auto matcher = adapt_to_single_description_matcher(AllOf(matcher_part, HasSubstr(type)));

    simulate_ps_exec_output(QByteArray::fromStdString(fmt::format("Custom Switch,{},", type)));
    EXPECT_THAT(backend.list_networks(), matcher);
}

} // namespace
