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

#pragma once

#include "mock_cert_provider.h"
#include "mock_daemon_rpc.h"
#include "mock_platform.h"
#include "mock_settings.h"
#include "mock_ssh_test_fixture.h"
#include "mock_utils.h"
#include "stub_cert_store.h"
#include "stub_logger.h"

#include <multipass/logging/multiplexing_logger.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <sstream>

namespace multipass
{
class Terminal;
}

namespace multipass::test
{

struct Client : public ::testing::Test
{
    Client();

    void SetUp() override;
    void TearDown() override;

    int setup_client_and_run(const std::vector<std::string>& command, Terminal& term);

    int send_command(const std::vector<std::string>& command,
                     std::ostream& cout = trash_stream,
                     std::ostream& cerr = trash_stream,
                     std::istream& cin = trash_stream);

    template <typename Str1, typename Str2>
    std::string keyval_arg(Str1&& key, Str2&& val)
    {
        return fmt::format("{}={}", std::forward<Str1>(key), std::forward<Str2>(val));
    }

    std::string get_setting(const std::initializer_list<std::string>& args);
    std::string get_setting(const std::string& key);

    static auto make_mount_matcher(const std::string_view fake_source,
                                   const std::string_view fake_target,
                                   const std::string_view instance_name)
    {
        const auto automount_source_matcher = Property(&MountRequest::source_path,
                                                       StrEq(fake_source));

        const auto target_instance_matcher = Property(&TargetPathInfo::instance_name,
                                                      StrEq(instance_name));
        const auto target_path_matcher = Property(&TargetPathInfo::target_path, StrEq(fake_target));
        const auto target_info_matcher = AllOf(target_instance_matcher, target_path_matcher);
        const auto automount_target_matcher = Property(
            &MountRequest::target_paths,
            AllOf(Contains(target_info_matcher), SizeIs(1)));

        return AllOf(automount_source_matcher, automount_target_matcher);
    }

    static auto make_mount_id_mappings_matcher(const id_mappings& expected_uid_mappings,
                                               const id_mappings& expected_gid_mappings)
    {
        auto compare_uid_map = [expected_uid_mappings](auto request_uid_mappings) {
            auto mappings_size = expected_uid_mappings.size();
            if (mappings_size != (decltype(mappings_size))request_uid_mappings.size())
                return false;

            for (decltype(mappings_size) i = 0; i < mappings_size; ++i)
                if (expected_uid_mappings[i].first != request_uid_mappings.at(i).host_id() ||
                    expected_uid_mappings[i].second != request_uid_mappings.at(i).instance_id())
                    return false;

            return true;
        };

        const auto uid_map_matcher = Property(
            &MountRequest::mount_maps,
            Property(&MountMaps::uid_mappings, ResultOf(compare_uid_map, true)));

        auto compare_gid_map = [expected_gid_mappings](auto request_gid_mappings) {
            auto mappings_size = expected_gid_mappings.size();
            if (mappings_size != (decltype(mappings_size))request_gid_mappings.size())
                return false;

            for (decltype(mappings_size) i = 0; i < mappings_size; ++i)
                if (expected_gid_mappings[i].first != request_gid_mappings.at(i).host_id() ||
                    expected_gid_mappings[i].second != request_gid_mappings.at(i).instance_id())
                    return false;

            return true;
        };

        const auto gid_map_matcher = Property(
            &MountRequest::mount_maps,
            Property(&MountMaps::gid_mappings, ResultOf(compare_gid_map, true)));

        return AllOf(uid_map_matcher, gid_map_matcher);
    }

    auto make_launch_instance_matcher(const std::string& instance_name)
    {
        return Property(&LaunchRequest::instance_name, StrEq(instance_name));
    }

    auto make_ssh_info_instance_matcher(const std::string& instance_name)
    {
        return Property(&SSHInfoRequest::instance_name, ElementsAre(StrEq(instance_name)));
    }

    template <typename RequestType, typename Matcher>
    auto make_instances_matcher(const Matcher& instances_matcher)
    {
        return Property(&RequestType::instance_names,
                        Property(&InstanceNames::instance_name, instances_matcher));
    }

    template <typename RequestType, typename Sequence>
    auto make_instances_sequence_matcher(const Sequence& seq)
    {
        return make_instances_matcher<RequestType>(ElementsAreArray(seq));
    }

    template <typename RequestType, int size>
    auto make_instance_in_repeated_field_matcher(const std::string& instance_name)
    {
        static_assert(size > 0, "size must be positive");
        return make_instances_matcher<RequestType>(
            AllOf(Contains(StrEq(instance_name)), SizeIs(size)));
    }

    template <typename RequestType>
    auto make_request_verbosity_matcher(
        decltype(std::declval<RequestType>().verbosity_level()) verbosity)
    {
        return Property(&RequestType::verbosity_level, Eq(verbosity));
    }

    template <typename RequestType>
    auto make_request_timeout_matcher(decltype(std::declval<RequestType>().timeout()) timeout)
    {
        return Property(&RequestType::timeout, Eq(timeout));
    }

    void aux_set_cmd_rejects_bad_val(const char* key, const char* val);

    auto make_fill_listreply(std::vector<InstanceStatus_Status> statuses)
    {
        return [statuses](Unused, grpc::ServerReaderWriter<ListReply, ListRequest>* response) {
            ListReply list_reply;

            for (InstanceStatus_Status status : statuses)
            {
                auto list_entry = list_reply.mutable_instance_list()->add_instances();
                list_entry->mutable_instance_status()->set_status(status);
            }

            response->Write(list_reply);

            return grpc::Status{};
        };
    }

    std::string negate_flag_string(const std::string& orig);

    template <typename ReplyType, typename RequestType, typename Matcher>
    auto check_request_and_return(const Matcher& matcher,
                                  const grpc::Status& status,
                                  const ReplyType& reply = ReplyType{})
    {
        return [&matcher, &status, reply = std::move(reply)](
                   grpc::ServerReaderWriter<ReplyType, RequestType>* server) {
            RequestType request;
            server->Read(&request);

            EXPECT_THAT(request, matcher);

            server->Write(std::move(reply));
            return status;
        };
    }

    std::string server_address;

    static std::unique_ptr<NiceMock<MockCertProvider>> get_client_cert_provider();

    std::unique_ptr<NiceMock<MockCertProvider>> daemon_cert_provider =
        std::make_unique<NiceMock<MockCertProvider>>();
    const MockPlatform::GuardedMock platform_attr = MockPlatform::inject<NiceMock>();
    const MockPlatform* mock_platform = platform_attr.first;
    const MockUtils::GuardedMock utils_attr = MockUtils::inject<NiceMock>();
    const MockUtils* mock_utils = utils_attr.first;

    StubCertStore cert_store;

    StrictMock<MockDaemonRpc> mock_daemon{
        server_address,
        *daemon_cert_provider,
        &cert_store,
        std::make_shared<logging::MultiplexingLogger>(
            std::make_unique<StubLogger>())}; // strict to fail on unexpected calls and play
                                              // well with sharing
    MockSettings::GuardedMock mock_settings_injection = MockSettings::inject();
    MockSettings& mock_settings = *mock_settings_injection.first;
    inline static std::stringstream
        trash_stream; // this may have contents (that we don't care about)
    static constexpr char petenv_name[] = "the-petenv";
    const grpc::Status ok{};

    MockSSHTestFixture mock_ssh_test_fixture;
};

} // namespace multipass::test
