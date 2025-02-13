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
        ON_CALL(mock_file, open_write(_, _))
            .WillByDefault(Invoke([](const auto&, const auto&) { return std::make_unique<std::stringstream>(); }));
        ON_CALL(mock_file, open_read(_, _))
            .WillByDefault(Invoke([](const auto&, const auto&) { return std::make_unique<std::stringstream>(); }));
        ON_CALL(mock_file, status(_, _))
            .WillByDefault(DoAll(SetArgReferee<1>(std::error_code{}), Return(fs::file_status{fs::file_type::not_found})));

        // Setup default empty JSON responses for manager and zones
        ON_CALL(mock_file, read_all(_)).WillByDefault(Return(QByteArray("{\"automatic_zone\":\"zone1\"}")));
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
            .WillRepeatedly(DoAll(SetArgReferee<1>(std::error_code{}), Return(fs::file_status{fs::file_type::not_found})));
        EXPECT_CALL(mock_file, open_write(manager_file, _))
            .WillRepeatedly(Invoke([](const auto&, const auto&) { return std::make_unique<std::stringstream>(); }));
        
        // Setup directory creation
        EXPECT_CALL(mock_file, create_directory(test_zones_dir, _))
            .WillRepeatedly(DoAll(SetArgReferee<1>(std::error_code{}), Return(true)));
        
        // Setup expectations for zone files
        for (const auto& zone : {"zone1", "zone2", "zone3"}) {
            const auto zone_file = test_zones_dir / (std::string(zone) + ".json");
            EXPECT_CALL(mock_file, status(zone_file, _))
                .WillRepeatedly(DoAll(SetArgReferee<1>(std::error_code{}), Return(fs::file_status{fs::file_type::regular})));
            EXPECT_CALL(mock_file, open_write(zone_file, _))
                .WillRepeatedly(Invoke([](const auto&, const auto&) { return std::make_unique<std::stringstream>(); }));
            EXPECT_CALL(mock_file, open_read(zone_file, _))
                .WillRepeatedly(Invoke([](const auto&, const auto&) { return std::make_unique<std::stringstream>(); }));
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
