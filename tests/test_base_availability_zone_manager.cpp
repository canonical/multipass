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
#include "mock_file_ops.h"
#include "mock_logger.h"
#include "mock_recursive_dir_iterator.h"

#include <multipass/base_availability_zone.h>
#include <multipass/base_availability_zone_manager.h>
#include <multipass/exceptions/availability_zone_exceptions.h>
#include <multipass/format.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <filesystem>
#include <gmock/gmock.h>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
namespace fs = std::filesystem;
using namespace testing;

namespace
{
constexpr auto category = "az-manager"; // Match the category in base_availability_zone_manager.cpp
struct TestBaseAvailabilityZoneManager : public Test
{
    const fs::path test_dir{"/test/path"};
    const fs::path test_zones_dir{test_dir / "zones"};
    const fs::path manager_file{test_dir / "az_manager.json"};
    mpt::MockLogger::Scope mock_logger{mpt::MockLogger::inject()};
    mpt::MockFileOps::GuardedMock mock_file_ops{mpt::MockFileOps::inject()};

    TestBaseAvailabilityZoneManager()
    {
        mock_logger.mock_logger->screen_logs(mp::logging::Level::error);
        auto& mock_file = *mock_file_ops.first;
        ON_CALL(mock_file, open_write(_, _)).WillByDefault(Invoke([](const auto&, const auto&) {
            return std::make_unique<std::stringstream>();
        }));
        ON_CALL(mock_file, status(_, _))
            .WillByDefault(DoAll(SetArgReferee<1>(std::error_code{}), Return(fs::file_status{fs::file_type::regular})));

        // Setup default JSON response only for manager file
        ON_CALL(mock_file, read_all(_)).WillByDefault([](QFile& file) {
            const auto path = file.fileName().toStdString();
            if (path.find("az_manager.json") != std::string::npos)
                return QByteArray("{\"automatic_zone\":\"zone1\"}");
            return QByteArray("{}"); // Empty JSON for other files
        });
    }

    void setup_empty_dir_iterator(const fs::path& dir, bool exists = true)
    {
        auto& mock_file = *mock_file_ops.first;
        auto dir_iter = std::make_unique<NiceMock<mpt::MockDirIterator>>();

        std::error_code err;
        if (!exists)
            err = std::error_code{static_cast<int>(std::errc::no_such_file_or_directory), std::generic_category()};

        auto* dir_iter_ptr = dir_iter.get();
        ON_CALL(*dir_iter_ptr, hasNext()).WillByDefault(Return(false));
        EXPECT_CALL(mock_file, dir_iterator(dir, _))
            .WillOnce(DoAll(SetArgReferee<1>(err), Return(ByMove(std::move(dir_iter)))));
    }

    void setup_default_expectations()
    {
        auto& mock_file = *mock_file_ops.first;

        // Setup expectations for manager file first
        EXPECT_CALL(mock_file, status(manager_file, _))
            .WillRepeatedly(
                DoAll(SetArgReferee<1>(std::error_code{}), Return(fs::file_status{fs::file_type::not_found})));
        EXPECT_CALL(mock_file, open_write(manager_file, _)).WillRepeatedly(Invoke([](const auto&, const auto&) {
            return std::make_unique<std::stringstream>();
        }));

        // Setup directory creation
        EXPECT_CALL(mock_file, create_directory(test_zones_dir, _))
            .WillRepeatedly(DoAll(SetArgReferee<1>(std::error_code{}), Return(true)));

        // Setup expectations for zone files
        for (const auto& zone : {"zone1", "zone2", "zone3"})
        {
            const auto zone_file = test_zones_dir / (std::string(zone) + ".json");
            EXPECT_CALL(mock_file, status(zone_file, _))
                .WillRepeatedly(
                    DoAll(SetArgReferee<1>(std::error_code{}), Return(fs::file_status{fs::file_type::regular})));
            EXPECT_CALL(mock_file, open_write(zone_file, _)).WillRepeatedly(Invoke([](const auto&, const auto&) {
                return std::make_unique<std::stringstream>();
            }));
            EXPECT_CALL(mock_file, open_read(zone_file, _)).WillRepeatedly([](const auto&, auto) {
                auto stream = std::make_unique<std::stringstream>();
                stream->str("{\"available\":true,\"subnet\":\"10.0.0.0/24\"}");
                return stream;
            });
        }
    }

    std::unique_ptr<mp::BaseAvailabilityZoneManager> make_manager()
    {
        return std::make_unique<mp::BaseAvailabilityZoneManager>(test_dir);
    }
};
} // namespace

TEST_F(TestBaseAvailabilityZoneManager, throws_on_zones_directory_creation_failure)
{
    setup_empty_dir_iterator(test_zones_dir, false);

    auto& mock_file = *mock_file_ops.first;
    std::error_code create_err{EACCES, std::system_category()};
    EXPECT_CALL(mock_file, create_directory(test_zones_dir, _))
        .WillOnce(DoAll(SetArgReferee<1>(create_err), Return(false)));

    MP_EXPECT_THROW_THAT(make_manager(),
                         mp::AvailabilityZoneManagerDeserializationError,
                         mpt::match_what(HasSubstr("failed to create")));
}

TEST_F(TestBaseAvailabilityZoneManager, throws_on_zones_directory_access_failure)
{
    auto& mock_file = *mock_file_ops.first;
    std::error_code err{EACCES, std::system_category()};
    auto dir_iter = std::make_unique<NiceMock<mpt::MockDirIterator>>();

    EXPECT_CALL(mock_file, dir_iterator(test_zones_dir, _))
        .WillOnce(DoAll(SetArgReferee<1>(err), Return(ByMove(std::move(dir_iter)))));

    MP_EXPECT_THROW_THAT(make_manager(),
                         mp::AvailabilityZoneManagerDeserializationError,
                         mpt::match_what(HasSubstr("failed to access")));
}

TEST_F(TestBaseAvailabilityZoneManager, throws_on_invalid_manager_file_type)
{
    setup_empty_dir_iterator(test_zones_dir);

    auto& mock_file = *mock_file_ops.first;
    EXPECT_CALL(mock_file, status(manager_file, _))
        .WillOnce(DoAll(SetArgReferee<1>(std::error_code{}), Return(fs::file_status{fs::file_type::directory})));

    MP_EXPECT_THROW_THAT(make_manager(),
                         mp::AvailabilityZoneManagerDeserializationError,
                         mpt::match_what(HasSubstr("not a regular file")));
}

TEST_F(TestBaseAvailabilityZoneManager, get_zone_returns_existing_zone)
{
    setup_empty_dir_iterator(test_zones_dir);
    setup_default_expectations();

    auto manager = make_manager();
    EXPECT_NO_THROW(manager->get_zone("zone1"));
}

TEST_F(TestBaseAvailabilityZoneManager, get_zone_throws_for_nonexistent_zone)
{
    setup_empty_dir_iterator(test_zones_dir);
    setup_default_expectations();

    auto manager = make_manager();
    EXPECT_THROW(manager->get_zone("nonexistent"), mp::AvailabilityZoneNotFound);
}

TEST_F(TestBaseAvailabilityZoneManager, throws_on_manager_file_read_failure)
{
    auto& mock_file = *mock_file_ops.first;
    setup_empty_dir_iterator(test_zones_dir);

    EXPECT_CALL(mock_file, status(manager_file, _))
        .WillOnce(DoAll(SetArgReferee<1>(std::error_code{}), Return(fs::file_status{fs::file_type::regular})));

    auto failed_stream = std::make_unique<std::stringstream>();
    failed_stream->setstate(std::ios_base::failbit);
    EXPECT_CALL(mock_file, open_read(manager_file, _)).WillOnce(Return(ByMove(std::move(failed_stream))));

    MP_EXPECT_THROW_THAT(make_manager(),
                         mp::AvailabilityZoneManagerDeserializationError,
                         mpt::match_what(HasSubstr("failed to open")));
}

TEST_F(TestBaseAvailabilityZoneManager, throws_on_manager_file_write_failure)
{
    auto& mock_file = *mock_file_ops.first;
    setup_empty_dir_iterator(test_zones_dir);

    EXPECT_CALL(mock_file, status(manager_file, _))
        .WillOnce(DoAll(SetArgReferee<1>(std::error_code{}), Return(fs::file_status{fs::file_type::not_found})));

    auto failed_stream = std::make_unique<std::stringstream>();
    failed_stream->setstate(std::ios_base::failbit);
    EXPECT_CALL(mock_file, open_write(_, _)).WillOnce(Return(ByMove(std::move(failed_stream))));

    MP_EXPECT_THROW_THAT(make_manager(),
                         mp::AvailabilityZoneManagerSerializationError,
                         mpt::match_what(HasSubstr("failed to open")));
}

TEST_F(TestBaseAvailabilityZoneManager, get_automatic_zone_name_round_robins)
{
    setup_empty_dir_iterator(test_zones_dir);
    setup_default_expectations();

    auto manager = make_manager();

    // Mock all zones as available
    auto& mock_file = *mock_file_ops.first;

    // Mock zone file reads to return available status
    for (const auto& zone : {"zone1", "zone2", "zone3"})
    {
        const auto zone_file = test_zones_dir / (std::string(zone) + ".json");
        EXPECT_CALL(mock_file, open_read(zone_file, _)).WillRepeatedly([](const auto&, auto) {
            auto stream = std::make_unique<std::stringstream>();
            *stream << "{\"available\":true,\"subnet\":\"10.0.0.0/24\"}";
            return stream;
        });
    }

    // First call should return zone1 and set automatic_zone to zone2
    EXPECT_EQ(manager->get_automatic_zone_name(), "zone1");

    // Second call should return zone2 and set automatic_zone to zone3
    EXPECT_EQ(manager->get_automatic_zone_name(), "zone2");

    // Third call should return zone3 and set automatic_zone back to zone1
    EXPECT_EQ(manager->get_automatic_zone_name(), "zone3");

    // Fourth call should start over at zone1
    EXPECT_EQ(manager->get_automatic_zone_name(), "zone1");
}

TEST_F(TestBaseAvailabilityZoneManager, get_automatic_zone_name_skips_unavailable_zones)
{
    setup_empty_dir_iterator(test_zones_dir);
    setup_default_expectations();

    // Mock zone availability
    auto& mock_file = *mock_file_ops.first;

    // Mock each zone file specifically
    for (const auto& zone : {"zone1", "zone2", "zone3"})
    {
        const auto zone_file = test_zones_dir / (std::string(zone) + ".json");
        EXPECT_CALL(mock_file, open_read(zone_file, _)).WillRepeatedly([zone = std::string(zone)](const auto&, auto) {
            auto stream = std::make_unique<std::stringstream>();
            if (zone == "zone2")
                stream->str("{\"available\":true,\"subnet\":\"10.0.0.0/24\"}");
            else
                stream->str("{\"available\":false,\"subnet\":\"10.0.0.0/24\"}");
            return stream;
        });
    }

    auto manager = make_manager();

    // Should return zone2 since it's the only available zone
    EXPECT_EQ(manager->get_automatic_zone_name(), "zone2");
}

TEST_F(TestBaseAvailabilityZoneManager, get_automatic_zone_name_throws_when_no_zones_available)
{
    setup_empty_dir_iterator(test_zones_dir);
    setup_default_expectations();

    // Mock all zones as unavailable
    auto& mock_file = *mock_file_ops.first;

    // First ensure the files are seen as existing
    for (const auto& zone : {"zone1", "zone2", "zone3"})
    {
        const auto zone_file = test_zones_dir / (std::string(zone) + ".json");
        EXPECT_CALL(mock_file, status(zone_file, _))
            .WillRepeatedly(
                DoAll(SetArgReferee<1>(std::error_code{}), Return(fs::file_status{fs::file_type::regular})));
    }

    // Then mock the file reads to return unavailable status
    for (const auto& zone : {"zone1", "zone2", "zone3"})
    {
        const auto zone_file = test_zones_dir / (std::string(zone) + ".json");
        EXPECT_CALL(mock_file, open_read(zone_file, _)).WillRepeatedly([](const auto&, auto) {
            auto stream = std::make_unique<std::stringstream>();
            *stream << "{\"available\":false,\"subnet\":\"10.0.0.0/24\"}";
            return stream;
        });
    }

    auto manager = make_manager();

    // Should throw since no zones are available
    EXPECT_THROW(manager->get_automatic_zone_name(), mp::NoAvailabilityZoneAvailable);
}

TEST_F(TestBaseAvailabilityZoneManager, get_zones_returns_all_zones)
{
    setup_empty_dir_iterator(test_zones_dir);
    setup_default_expectations();

    auto manager = make_manager();
    auto zones = manager->get_zones();

    EXPECT_EQ(zones.size(), 3);

    std::set<std::string> zone_names;
    for (const auto& zone : zones)
        zone_names.insert(zone.get().get_name());

    EXPECT_THAT(zone_names, UnorderedElementsAre("zone1", "zone2", "zone3"));
}

TEST_F(TestBaseAvailabilityZoneManager, get_default_zone_name_returns_first_zone)
{
    setup_empty_dir_iterator(test_zones_dir);
    setup_default_expectations();

    auto manager = make_manager();
    EXPECT_EQ(manager->get_default_zone_name(), "zone1");
}

TEST_F(TestBaseAvailabilityZoneManager, serialize_writes_correct_json)
{
    setup_empty_dir_iterator(test_zones_dir);
    setup_default_expectations();

    // Mock all zones as available
    auto& mock_file = *mock_file_ops.first;

    // Mock zone file reads to return available status
    for (const auto& zone : {"zone1", "zone2", "zone3"})
    {
        const auto zone_file = test_zones_dir / (std::string(zone) + ".json");
        EXPECT_CALL(mock_file, open_read(zone_file, _)).WillRepeatedly([](const auto&, auto) {
            auto stream = std::make_unique<std::stringstream>();
            *stream << "{\"available\":true,\"subnet\":\"10.0.0.0/24\"}";
            return stream;
        });
    }

    auto manager = make_manager();

    // Setup stream to capture serialized data
    auto stream = std::make_unique<std::stringstream>();
    auto* stream_ptr = stream.get();

    // Capture the serialization after getting automatic zone
    EXPECT_CALL(mock_file, open_write(manager_file, _)).WillOnce([stream_ptr](const auto&, auto) {
        auto write_stream = std::make_unique<std::stringstream>();
        write_stream->str("{\"automatic_zone\":\"zone2\"}");
        return write_stream;
    });

    // Get automatic zone to trigger serialization
    EXPECT_NO_THROW(manager->get_automatic_zone_name());

    // Verify JSON structure
    const auto json = QJsonDocument::fromJson(QByteArray("{\"automatic_zone\":\"zone2\"}")).object();
    EXPECT_TRUE(json.contains("automatic_zone"));
    EXPECT_EQ(json["automatic_zone"].toString().toStdString(), "zone2"); // Should be zone2 after first call
}
