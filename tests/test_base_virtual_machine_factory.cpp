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

#include <multipass/virtual_machine_description.h>
#include <multipass/vm_status_monitor.h>
#include <shared/base_virtual_machine_factory.h>

#include "mock_logger.h"

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
    BaseFactory()
    {
        mpl::set_logger(logger);
    }

    std::shared_ptr<NiceMock<mpt::MockLogger>> logger = std::make_shared<NiceMock<mpt::MockLogger>>();
};
} // namespace

TEST_F(BaseFactory, returns_image_only_fetch_type)
{
    MockBaseFactory factory;

    EXPECT_EQ(factory.fetch_type(), mp::FetchType::ImageOnly);
}

TEST_F(BaseFactory, configure_logs_trace_message)
{
    const std::string name{"foo"};
    MockBaseFactory factory;

    EXPECT_CALL(*logger, log(Eq(mpl::Level::trace), mpt::MockLogger::make_cstring_matcher(StrEq("base factory")),
                             mpt::MockLogger::make_cstring_matcher(
                                 StrEq(fmt::format("No driver configuration for \"{}\"", name)))));

    YAML::Node node;

    factory.configure(name, node, node);
}

TEST_F(BaseFactory, dir_name_returns_empty_string)
{
    MockBaseFactory factory;

    const auto dir_name = factory.get_backend_directory_name();

    EXPECT_TRUE(dir_name.isEmpty());
}
