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
#include "daemon_test_fixture.h"
#include "mock_cert_provider.h"
#include "mock_image_host.h"
#include "mock_permission_utils.h"
#include "mock_platform.h"
#include "mock_settings.h"
#include "mock_utils.h"
#include "mock_vm_image_vault.h"

#include <src/daemon/daemon.h>
#include <src/daemon/default_vm_image_vault.h>

#include <multipass/constants.h>
#include <multipass/exceptions/download_exception.h>
#include <multipass/format.h>

#include <utility>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

struct DaemonImages : public mpt::DaemonTestFixture
{
    void SetUp() override
    {
        // Let the builder create the real vault implementation.
        config_builder.vault = nullptr;
        config_builder.image_hosts.clear();

        auto* factory = use_a_mock_vm_factory();
        EXPECT_CALL(*factory, create_image_vault).WillRepeatedly(Invoke([](auto&&... args) {
            return std::make_unique<mp::DefaultVMImageVault>(std::forward<decltype(args)>(args)...);
        }));

        EXPECT_CALL(mock_settings, register_handler).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_settings, unregister_handler).Times(AnyNumber());
        EXPECT_CALL(mock_settings, get(Eq(mp::winterm_key))).WillRepeatedly(Return("none"));
        EXPECT_CALL(mock_settings, get(Eq(mp::driver_key))).WillRepeatedly(Return("senna"));
        ON_CALL(mock_utils, contents_of(_)).WillByDefault(Return(mpt::root_cert));
    }

    void TearDown() override
    {
        config_builder.image_hosts.clear();
    }

    template <typename ImageHost, typename... Args>
    ImageHost& add_image_host(Args&&... args)
    {
        auto image_host_ptr = std::make_unique<ImageHost>(std::forward<Args>(args)...);
        auto& image_host = *image_host_ptr;
        config_builder.image_hosts.push_back(std::move(image_host_ptr));
        return image_host;
    }

    std::map<std::string, std::vector<mp::VMImageInfo>> default_images()
    {
        auto mock = mpt::MockImageHost{};
        return {{"release", {mock.mock_bionic_image_info, mock.mock_another_image_info}},
                {"snapcraft", {mock.mock_snapcraft_image_info}},
                {"custom", {mock.mock_custom_image_info}}};
    }

    mp::VMImageInfo make_dummy_info(std::string hash, std::vector<std::string> aliases)
    {
        return mp::VMImageInfo{
            .aliases = std::move(aliases),
            .os = "",
            .release = "",
            .release_title = "",
            .release_codename = "",
            .supported = true,
            .image_location = "",
            .id = std::move(hash),
            .stream_location = "",
            .version = "",
            .size = 0,
            .verify = false,
        };
    }

    mpt::MockPlatform::GuardedMock attr{mpt::MockPlatform::inject<NiceMock>()};
    mpt::MockPlatform* mock_platform = attr.first;

    mpt::MockSettings::GuardedMock mock_settings_injection =
        mpt::MockSettings::inject<StrictMock>();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;

    const mpt::MockPermissionUtils::GuardedMock mock_permission_utils_injection =
        mpt::MockPermissionUtils::inject<NiceMock>();
    mpt::MockPermissionUtils& mock_permission_utils = *mock_permission_utils_injection.first;

    mpt::MockUtils::GuardedMock mock_utils_injection{mpt::MockUtils::inject<NiceMock>()};
    mpt::MockUtils& mock_utils = *mock_utils_injection.first;
};

TEST_F(DaemonImages, blankQueryReturnsFromDefaultRemotes)
{
    add_image_host<NiceMock<mpt::MockBaseImageHost>>(default_images());
    mp::Daemon daemon{config_builder.build()};

    std::stringstream stream;
    send_command({"images"}, stream);

    EXPECT_THAT(stream.str(),
                AllOf(HasSubstr(mpt::default_alias),
                      HasSubstr(mpt::default_release_info),
                      HasSubstr(mpt::another_alias),
                      HasSubstr(mpt::another_release_info)));

    EXPECT_THAT(stream.str(),
                Not(AnyOf(HasSubstr(mpt::custom_alias),
                          HasSubstr(mpt::custom_release_info),
                          HasSubstr(mpt::snapcraft_alias),
                          HasSubstr(mpt::snapcraft_release_info))));

    EXPECT_THAT(stream.str(), Not(HasSubstr("Remote")));
    EXPECT_EQ(total_lines_of_output(stream), 4);
}

TEST_F(DaemonImages, queryWithAllReturnsAllData)
{
    auto& image_host = add_image_host<NiceMock<mpt::MockBaseImageHost>>(default_images());
    EXPECT_CALL(image_host, images_for_remote(_)).Times(3);

    mp::Daemon daemon{config_builder.build()};

    std::stringstream stream;
    send_command({"images", "--all"}, stream);

    EXPECT_THAT(stream.str(),
                AllOf(HasSubstr(mpt::default_alias),
                      HasSubstr(mpt::default_release_info),
                      HasSubstr(mpt::another_alias),
                      HasSubstr(mpt::another_release_info)));

    EXPECT_THAT(stream.str(),
                AllOf(HasSubstr(mpt::default_alias),
                      HasSubstr(mpt::default_release_info),
                      HasSubstr(mpt::another_alias),
                      HasSubstr(mpt::another_release_info),
                      HasSubstr(mpt::custom_alias),
                      HasSubstr(mpt::custom_release_info),
                      HasSubstr(mpt::snapcraft_alias),
                      HasSubstr(mpt::snapcraft_release_info)));

    EXPECT_THAT(stream.str(), HasSubstr("Remote"));
    EXPECT_EQ(total_lines_of_output(stream), 6);
}

TEST_F(DaemonImages, queryForDefaultReturnsExpectedData)
{
    auto& image_host = add_image_host<NiceMock<mpt::MockBaseImageHost>>(default_images());
    EXPECT_CALL(image_host, images_for_remote(_)).Times(1);

    mp::Daemon daemon{config_builder.build()};

    std::stringstream stream;
    send_command({"images", "default"}, stream);

    EXPECT_THAT(stream.str(),
                AllOf(HasSubstr(mpt::default_alias), HasSubstr(mpt::default_release_info)));

    EXPECT_THAT(stream.str(), Not(HasSubstr("Remote")));
    EXPECT_EQ(total_lines_of_output(stream), 3);
}

TEST_F(DaemonImages, unknownQueryReturnsEmpty)
{
    auto& image_host = add_image_host<NiceMock<mpt::MockBaseImageHost>>(default_images());
    EXPECT_CALL(image_host, images_for_remote(_)).Times(1);

    mp::Daemon daemon{config_builder.build()};

    constexpr auto phony_name = "phony";
    std::stringstream stream;
    send_command({"images", phony_name}, stream);

    EXPECT_THAT(stream.str(), HasSubstr("No images found."));
}

TEST_F(DaemonImages, queryWithRemoteReturnsExpectedData)
{
    auto& image_host = add_image_host<NiceMock<mpt::MockBaseImageHost>>(default_images());
    EXPECT_CALL(image_host, images_for_remote(_)).Times(2);

    mp::Daemon daemon{config_builder.build()};

    {
        std::stringstream stream;
        send_command({"images", "release:"}, stream);

        EXPECT_THAT(stream.str(),
                    AllOf(HasSubstr(mpt::default_alias),
                          HasSubstr(mpt::default_release_info),
                          HasSubstr(mpt::another_alias),
                          HasSubstr(mpt::another_release_info)));

        EXPECT_THAT(stream.str(), Not(HasSubstr("Remote")));
        EXPECT_EQ(total_lines_of_output(stream), 4);
    }

    {
        std::stringstream stream;
        send_command({"images", "snapcraft:"}, stream);

        EXPECT_THAT(stream.str(),
                    AllOf(HasSubstr(mpt::snapcraft_alias), HasSubstr(mpt::snapcraft_release_info)));

        EXPECT_THAT(stream.str(), Not(HasSubstr("Remote")));
        EXPECT_EQ(total_lines_of_output(stream), 3);
    }
}

TEST_F(DaemonImages, invalidRemoteName)
{
    auto& image_host = add_image_host<NiceMock<mpt::MockBaseImageHost>>(default_images());
    EXPECT_CALL(image_host, images_for_remote(_)).Times(0);

    constexpr std::string_view remote_name = "nonsense";
    const std::string error_msg = fmt::format("Remote \'{}\' is not found. Please use `multipass "
                                              "images` for supported remotes and images.",
                                              remote_name);

    mp::Daemon daemon{config_builder.build()};

    {
        const std::string full_name = std::string(remote_name) + ":";
        std::stringstream cerr_stream;
        // std::cout is the place holder here.
        send_command({"images", full_name}, std::cout, cerr_stream);

        EXPECT_THAT(cerr_stream.str(), HasSubstr(error_msg));
        EXPECT_EQ(total_lines_of_output(cerr_stream), 1);
    }

    {
        constexpr std::string_view search_string = "default";
        const std::string full_name = std::string(remote_name) + ":" + std::string(search_string);
        std::stringstream cerr_stream;
        // std::cout is the place holder here.
        send_command({"images", full_name}, std::cout, cerr_stream);

        EXPECT_THAT(cerr_stream.str(), HasSubstr(error_msg));
        EXPECT_EQ(total_lines_of_output(cerr_stream), 1);
    }
}

TEST_F(DaemonImages, returnSameResultsWhenFilteredByDifferentAliases)
{
    auto& image_host = add_image_host<NiceMock<mpt::MockBaseImageHost>>();
    image_host.add_image(mp::release_remote, make_dummy_info("image", {"toto", "tata"}));

    mp::Daemon daemon{config_builder.build()};

    std::stringstream s1;
    send_command({"images", "toto"}, s1);

    std::stringstream s2;
    send_command({"images", "tata"}, s2);

    EXPECT_THAT(s1.str(), AllOf(HasSubstr("toto"), HasSubstr("tata")));
    EXPECT_THAT(s2.str(), AllOf(HasSubstr("toto"), HasSubstr("tata")));
    EXPECT_EQ(s1.str(), s2.str());
}

TEST_F(DaemonImages, findWithoutForceUpdateCheckUpdateManifestsCall)
{
    auto& image_host = add_image_host<NiceMock<mpt::MockImageHost>>();
    EXPECT_CALL(image_host, update_manifests(false)).Times(1);

    // only the daemon constructor invoke it once
    const mp::Daemon daemon{config_builder.build()};
    send_command({"images"});
}

TEST_F(DaemonImages,
       updateManifestsThrowTriggersTheFailedCaseEventHandlerOfAsyncPeriodicDownloadTask)
{
    auto& image_host = add_image_host<NiceMock<mpt::MockImageHost>>();

    // daemon constructor which constructs update_manifests_all_task calls this.
    EXPECT_CALL(image_host, update_manifests(false)).WillOnce([]() {
        throw mp::DownloadException{"dummy_url", "dummy_cause"};
    });

    const mp::Daemon daemon{config_builder.build()};

    // need it because mp::Daemon destructor which destructs qfuture and qfuturewatcher does not
    // wait the async task to finish. As a consequence, the event handler is not guaranteed to be
    // called without send_command({"images"});
    send_command({"images"});
}

TEST_F(DaemonImages, findForceUpdateCheckUpdateManifestsCalls)
{
    auto& image_host = add_image_host<NiceMock<mpt::MockImageHost>>();

    // daemon constructor invoke it first and images --force-update invoke it with force flag true
    // after
    const testing::InSequence sequence; // Force the following expectations to occur in order
    EXPECT_CALL(image_host, update_manifests(false)).Times(1);
    EXPECT_CALL(image_host, update_manifests(true)).Times(1);

    const mp::Daemon daemon{config_builder.build()};
    send_command({"images", "--force-update"});
}

TEST_F(DaemonImages, findForceUpdateRemoteCheckUpdateManifestsCalls)
{
    auto& image_host = add_image_host<NiceMock<mpt::MockImageHost>>();

    const testing::InSequence sequence;
    EXPECT_CALL(image_host, all_info_for).Times(0);
    EXPECT_CALL(image_host, update_manifests(false)).Times(1);
    EXPECT_CALL(image_host, update_manifests(true)).Times(1);
    EXPECT_CALL(image_host, all_info_for(Field(&mp::SearchQuery::remote_name, "release"))).Times(1);

    const mp::Daemon daemon{config_builder.build()};
    send_command({"find", "release:", "--force-update"});
}

TEST_F(DaemonImages, findForceUpdateRemoteSearchNameCheckUpdateManifestsCalls)
{
    auto& image_host = add_image_host<NiceMock<mpt::MockImageHost>>();

    const testing::InSequence sequence;
    EXPECT_CALL(image_host, all_info_for).Times(0);
    EXPECT_CALL(image_host, update_manifests(false)).Times(1);
    EXPECT_CALL(image_host, update_manifests(true)).Times(1);
    EXPECT_CALL(image_host,
                all_info_for(AllOf(Field(&mp::SearchQuery::remote_name, "release"),
                                   Field(&mp::SearchQuery::filter, "22.04"))))
        .Times(1);

    const mp::Daemon daemon{config_builder.build()};
    send_command({"find", "release:22.04", "--force-update"});
}
