/*
 * Copyright (C) 2019-2021 Canonical, Ltd.
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

#include <src/platform/backends/shared/linux/backend_utils.h>
#include <src/platform/backends/shared/linux/dbus_wrappers.h>

#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/memory_size.h>

#include <shared/shared_backend_utils.h>

#include "tests/extra_assertions.h"
#include "tests/mock_logger.h"
#include "tests/mock_process_factory.h"
#include "tests/mock_singleton_helpers.h"

#include <QMap>
#include <QVariant>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
const auto success = mp::ProcessState{0, mp::nullopt};
const auto failure = mp::ProcessState{1, mp::nullopt};
const auto crash = mp::ProcessState{mp::nullopt, mp::ProcessState::Error{QProcess::Crashed, "core dumped"}};
const auto null_string_matcher = static_cast<mp::optional<decltype(_)>>(mp::nullopt);

using ImageConversionParamType =
    std::tuple<const char*, const char*, mp::ProcessState, bool, mp::ProcessState, mp::optional<Matcher<std::string>>>;

void simulate_qemuimg_info_with_json(const mpt::MockProcess* process, const QString& expect_img,
                                     const mp::ProcessState& produce_result, const QByteArray& produce_output = {})
{
    ASSERT_EQ(process->program().toStdString(), "qemu-img");

    const auto args = process->arguments();
    ASSERT_EQ(args.size(), 3);

    EXPECT_EQ(args.at(0), "info");
    EXPECT_EQ(args.at(1), "--output=json");
    EXPECT_EQ(args.at(2), expect_img);

    InSequence s;

    EXPECT_CALL(*process, execute).WillOnce(Return(produce_result));
    if (produce_result.completed_successfully())
        EXPECT_CALL(*process, read_all_standard_output).WillOnce(Return(produce_output));
    else if (produce_result.exit_code)
        EXPECT_CALL(*process, read_all_standard_error).WillOnce(Return(produce_output));
    else
        ON_CALL(*process, read_all_standard_error).WillByDefault(Return(produce_output));
}

void simulate_qemuimg_resize(mpt::MockProcess* process, const QString& expect_img, const mp::MemorySize& expect_size,
                             const mp::ProcessState& produce_result)
{
    ASSERT_EQ(process->program().toStdString(), "qemu-img");

    const auto args = process->arguments();
    ASSERT_EQ(args.size(), 3);

    EXPECT_EQ(args.at(0).toStdString(), "resize");
    EXPECT_EQ(args.at(1), expect_img);
    EXPECT_THAT(args.at(2),
                ResultOf([](const auto& val) { return mp::MemorySize{val.toStdString()}; }, Eq(expect_size)));

    EXPECT_CALL(*process, execute(mp::backend::image_resize_timeout)).Times(1).WillOnce(Return(produce_result));
}

void simulate_qemuimg_convert(const mpt::MockProcess* process, const QString& img_path,
                              const QString& expected_img_path, const mp::ProcessState& produce_result)
{
    ASSERT_EQ(process->program().toStdString(), "qemu-img");

    const auto args = process->arguments();
    ASSERT_EQ(args.size(), 6);

    EXPECT_EQ(args.at(0), "convert");
    EXPECT_EQ(args.at(1), "-p");
    EXPECT_EQ(args.at(2), "-O");
    EXPECT_EQ(args.at(3), "qcow2");
    EXPECT_EQ(args.at(4), img_path);
    EXPECT_EQ(args.at(5), expected_img_path);

    EXPECT_CALL(*process, execute).WillOnce(Return(produce_result));
}

template <class Matcher>
void test_image_resizing(const char* img, const mp::MemorySize& img_virtual_size, const mp::MemorySize& requested_size,
                         const mp::ProcessState& qemuimg_resize_result, mp::optional<Matcher> throw_msg_matcher)
{
    auto process_count = 0;
    auto mock_factory_scope = mpt::MockProcessFactory::Inject();

    mock_factory_scope->register_callback([&](mpt::MockProcess* process) {
        ASSERT_LE(++process_count, 1);
        simulate_qemuimg_resize(process, img, requested_size, qemuimg_resize_result);
    });

    if (throw_msg_matcher)
        MP_EXPECT_THROW_THAT(mp::backend::resize_instance_image(requested_size, img), std::runtime_error,
                             mpt::match_what(*throw_msg_matcher));
    else
        mp::backend::resize_instance_image(requested_size, img);

    EXPECT_EQ(process_count, 1);
}

template <class Matcher>
void test_image_conversion(const char* img_path, const char* expected_img_path, const char* qemuimg_info_output,
                           const mp::ProcessState& qemuimg_info_result, bool attempt_convert,
                           const mp::ProcessState& qemuimg_convert_result, mp::optional<Matcher> throw_msg_matcher)
{
    auto process_count = 0;
    auto mock_factory_scope = mpt::MockProcessFactory::Inject();
    const auto expected_final_process_count = attempt_convert ? 2 : 1;

    mock_factory_scope->register_callback([&](mpt::MockProcess* process) {
        ASSERT_LE(++process_count, expected_final_process_count);
        if (process_count == 1)
        {
            auto msg = QByteArray{qemuimg_info_output};

            simulate_qemuimg_info_with_json(process, img_path, qemuimg_info_result, msg);
        }
        else
        {
            simulate_qemuimg_convert(process, img_path, expected_img_path, qemuimg_convert_result);
        }
    });

    if (throw_msg_matcher)
        MP_EXPECT_THROW_THAT(mp::backend::convert_to_qcow_if_necessary(img_path), std::runtime_error,
                             mpt::match_what(*throw_msg_matcher));
    else
        EXPECT_THAT(mp::backend::convert_to_qcow_if_necessary(img_path), Eq(expected_img_path));

    EXPECT_EQ(process_count, expected_final_process_count);
}

struct ImageConversionTestSuite : public TestWithParam<ImageConversionParamType>
{
};

const std::vector<ImageConversionParamType> image_conversion_inputs{
    {"/fake/img/path", "{\n    \"format\": \"qcow2\"\n}", success, false, mp::ProcessState{}, null_string_matcher},
    {"/fake/img/path.qcow2", "{\n    \"format\": \"raw\"\n}", success, true, success, null_string_matcher},
    {"/fake/img/path.qcow2", "not found", failure, false, mp::ProcessState{},
     mp::make_optional(HasSubstr("not found"))},
    {"/fake/img/path.qcow2", "{\n    \"format\": \"raw\"\n}", success, true, failure,
     mp::make_optional(HasSubstr("qemu-img failed"))}};

TEST(BackendUtils, image_resizing_checks_minimum_size_and_proceeds_when_larger)
{
    const auto img = "/fake/img/path";
    const auto min_size = mp::MemorySize{"1G"};
    const auto request_size = mp::MemorySize{"3G"};
    const auto qemuimg_resize_result = success;
    const auto throw_msg_matcher = null_string_matcher;

    test_image_resizing(img, min_size, request_size, qemuimg_resize_result, throw_msg_matcher);
}

TEST(BackendUtils, image_resizing_checks_minimum_size_and_proceeds_when_equal)
{
    const auto img = "/fake/img/path";
    const auto min_size = mp::MemorySize{"1234554321"};
    const auto request_size = min_size;
    const auto qemuimg_resize_result = success;
    const auto throw_msg_matcher = null_string_matcher;

    test_image_resizing(img, min_size, request_size, qemuimg_resize_result, throw_msg_matcher);
}

TEST(BackendUtils, image_resize_detects_resizing_exit_failure_and_throws)
{
    const auto img = "imagine";
    const auto min_size = mp::MemorySize{"100M"};
    const auto request_size = mp::MemorySize{"400M"};
    const auto qemuimg_resize_result = failure;
    const auto throw_msg_matcher = mp::make_optional(HasSubstr("qemu-img failed"));

    test_image_resizing(img, min_size, request_size, qemuimg_resize_result, throw_msg_matcher);
}

TEST(BackendUtils, image_resize_detects_resizing_crash_failure_and_throws)
{
    const auto img = "ubuntu";
    const auto min_size = mp::MemorySize{"100M"};
    const auto request_size = mp::MemorySize{"400M"};
    const auto qemuimg_resize_result = crash;
    const auto throw_msg_matcher =
        mp::make_optional(AllOf(HasSubstr("qemu-img failed"), HasSubstr(crash.failure_message().toStdString())));

    test_image_resizing(img, min_size, request_size, qemuimg_resize_result, throw_msg_matcher);
}

TEST_P(ImageConversionTestSuite, properly_handles_image_conversion)
{
    const auto img_path = "/fake/img/path";
    const auto& [expected_img_path, qemuimg_info_output, qemuimg_info_result, attempt_convert, qemuimg_convert_result,
                 throw_msg_matcher] = GetParam();

    test_image_conversion(img_path, expected_img_path, qemuimg_info_output, qemuimg_info_result, attempt_convert,
                          qemuimg_convert_result, throw_msg_matcher);
}

INSTANTIATE_TEST_SUITE_P(BackendUtils, ImageConversionTestSuite, ValuesIn(image_conversion_inputs));

namespace mp_dbus = mp::backend::dbus;
class MockDBusProvider : public mp_dbus::DBusProvider
{
public:
    using DBusProvider::DBusProvider;
    MOCK_CONST_METHOD0(get_system_bus, const mp_dbus::DBusConnection&());

    MP_MOCK_SINGLETON_BOILERPLATE(MockDBusProvider, DBusProvider);
};

class MockDBusConnection : public mp_dbus::DBusConnection
{
public:
    MockDBusConnection() : mp_dbus::DBusConnection(/* create_bus = */ false)
    {
    }

    MOCK_CONST_METHOD0(is_connected, bool());
    MOCK_CONST_METHOD0(last_error, QDBusError());
    MOCK_CONST_METHOD3(get_interface,
                       std::unique_ptr<mp_dbus::DBusInterface>(const QString&, const QString&, const QString&));
};

class MockDBusInterface : public mp_dbus::DBusInterface
{
public:
    using DBusInterface::DBusInterface;

    MOCK_CONST_METHOD0(is_valid, bool());
    MOCK_CONST_METHOD0(last_error, QDBusError());
    MOCK_CONST_METHOD0(interface, QString());
    MOCK_CONST_METHOD0(path, QString());
    MOCK_CONST_METHOD0(service, QString());
    MOCK_METHOD5(call_impl,
                 QDBusMessage(QDBus::CallMode, const QString&, const QVariant&, const QVariant&, const QVariant&));
};

struct CreateBridgeTest : public Test
{
    void SetUp() override
    {
        logger_scope.mock_logger->screen_logs(mpl::Level::warning);

        // These will accept any number of calls (0..N) but they can still be shadowed
        EXPECT_CALL(*mock_dbus_provider, get_system_bus).WillRepeatedly(ReturnRef(mock_bus));
        EXPECT_CALL(*mock_nm_root, is_valid).WillRepeatedly(Return(true));
        EXPECT_CALL(*mock_nm_settings, is_valid).WillRepeatedly(Return(true));
        EXPECT_CALL(mock_bus, is_connected).WillRepeatedly(Return(true));
    }

    void inject_dbus_interfaces() // this moves the DBus interface mocks, so expectations must be set before calling
    {
        inject_root_interface();
        inject_settings_interface();
    }

    void inject_root_interface() // moves mock, so expectations first please
    {
        EXPECT_CALL(mock_bus, get_interface(Eq("org.freedesktop.NetworkManager"), Eq("/org/freedesktop/NetworkManager"),
                                            Eq("org.freedesktop.NetworkManager")))
            .WillOnce(Return(ByMove(std::move(mock_nm_root))));
    }

    void inject_settings_interface() // moves mock, so expectations first please
    {
        EXPECT_CALL(mock_bus,
                    get_interface(Eq("org.freedesktop.NetworkManager"), Eq("/org/freedesktop/NetworkManager/Settings"),
                                  Eq("org.freedesktop.NetworkManager.Settings")))
            .WillOnce(Return(ByMove(std::move(mock_nm_settings))));
    }

    static auto make_obj_path_reply(const QString& obj_path)
    {
        return QDBusMessage{}.createReply(QVariant::fromValue(QDBusObjectPath{obj_path}));
    }

    static auto make_parent_connection_matcher(const char* child)
    {
        return Truly([child](const QVariant& arg) {
            QMap<QString, QVariantMap> outer_map;
            QMap<QString, QVariantMap>::const_iterator outer_it;
            QVariantMap::const_iterator inner_it;
            QString parent_name = get_bridge_name(child);

            return (outer_map = arg.value<QMap<QString, QVariantMap>>()).size() == 2 &&
                   (outer_it = outer_map.find("connection")) != outer_map.end() && outer_it->size() == 3 &&
                   (inner_it = outer_it->find("id")) != outer_it->end() && inner_it->toString() == parent_name &&
                   (inner_it = outer_it->find("type")) != outer_it->end() && inner_it->toString() == "bridge" &&
                   (inner_it = outer_it->find("autoconnect-slaves")) != outer_it->end() && inner_it->toInt() == 1 &&
                   (outer_it = outer_map.find("bridge")) != outer_map.end() && outer_it->size() == 1 &&
                   (inner_it = outer_it->find("interface-name")) != outer_it->end() &&
                   inner_it->toString() == parent_name;
        });
    }
    static auto make_child_connection_matcher(const char* child)
    {
        return Truly([child](const QVariant& arg) {
            QMap<QString, QVariantMap> outer_map;
            QMap<QString, QVariantMap>::const_iterator outer_it;
            QVariantMap::const_iterator inner_it;
            QString parent_name = get_bridge_name(child);
            QString child_name = parent_name + "-child";

            return (outer_map = arg.value<QMap<QString, QVariantMap>>()).size() == 1 &&
                   (outer_it = outer_map.find("connection")) != outer_map.end() && outer_it->size() == 6 &&
                   (inner_it = outer_it->find("id")) != outer_it->end() && inner_it->toString() == child_name &&
                   (inner_it = outer_it->find("type")) != outer_it->end() && inner_it->toString() == "802-3-ethernet" &&
                   (inner_it = outer_it->find("slave-type")) != outer_it->end() && inner_it->toString() == "bridge" &&
                   (inner_it = outer_it->find("master")) != outer_it->end() && inner_it->toString() == parent_name &&
                   (inner_it = outer_it->find("interface-name")) != outer_it->end() && inner_it->toString() == child &&
                   (inner_it = outer_it->find("autoconnect-priority")) != outer_it->end() && inner_it->toInt() > 0;
        });
    }

    static auto make_object_path_matcher(const char* path)
    {
        return Property(&QVariant::value<QDBusObjectPath>,
                        Property(&QDBusObjectPath::path, mpt::match_qstring(Eq(path))));
    }

    static QString get_bridge_name(const char* child)
    {
        return (QString{"br-"} + child).left(15);
    }

    MockDBusProvider::GuardedMock mock_dbus_injection = MockDBusProvider::inject();
    MockDBusProvider* mock_dbus_provider = mock_dbus_injection.first;
    MockDBusConnection mock_bus{};
    std::unique_ptr<MockDBusInterface> mock_nm_settings = std::make_unique<MockDBusInterface>();
    std::unique_ptr<MockDBusInterface> mock_nm_root = std::make_unique<MockDBusInterface>();
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();

    const QVariant empty{};
};

TEST_F(CreateBridgeTest, creates_and_activates_connections) // success case
{
    static constexpr auto network = "eth1234567890a";
    static constexpr auto child_obj_path = "/an/obj/path/for/child";
    static constexpr auto null_obj_path = "/";

    {
        InSequence seq{};
        EXPECT_CALL(*mock_nm_settings,
                    call_impl(QDBus::Block, Eq("AddConnection"), make_parent_connection_matcher(network), empty, empty))
            .WillOnce(Return(make_obj_path_reply("/a/b/c")));

        EXPECT_CALL(*mock_nm_settings,
                    call_impl(QDBus::Block, Eq("AddConnection"), make_child_connection_matcher(network), empty, empty))
            .WillOnce(Return(make_obj_path_reply(child_obj_path)));

        auto null_obj_matcher = make_object_path_matcher(null_obj_path);
        auto child_obj_matcher = make_object_path_matcher(child_obj_path);
        EXPECT_CALL(*mock_nm_root, call_impl(QDBus::Block, Eq("ActivateConnection"), child_obj_matcher,
                                             null_obj_matcher, null_obj_matcher))
            .WillOnce(Return(make_obj_path_reply("/active/obj/path")));
    }

    inject_dbus_interfaces();
    EXPECT_EQ(mp::backend::create_bridge_with(network), get_bridge_name(network).toStdString());
}

TEST_F(CreateBridgeTest, throws_if_bus_disconnected)
{
    auto msg = QStringLiteral("DBus error msg");
    EXPECT_CALL(mock_bus, is_connected).WillOnce(Return(false));
    EXPECT_CALL(mock_bus, last_error).WillOnce(Return(QDBusError{QDBusError::BadAddress, msg}));

    MP_EXPECT_THROW_THAT(
        mp::backend::create_bridge_with("asdf"), mp::backend::CreateBridgeException,
        mpt::match_what(AllOf(HasSubstr("Could not create bridge"), HasSubstr("Failed to connect to D-Bus system bus"),
                              HasSubstr(msg.toStdString()))));
}

struct CreateBridgeInvalidInterfaceTest : public CreateBridgeTest, WithParamInterface<bool>
{
};

TEST_P(CreateBridgeInvalidInterfaceTest, throws_if_interface_invalid)
{
    bool invalid_root_interface = GetParam(); // otherwise, invalid settings interface
    auto& mock_nm_interface = invalid_root_interface ? mock_nm_root : mock_nm_settings;
    auto msg = QStringLiteral("DBus error msg");
    EXPECT_CALL(*mock_nm_interface, is_valid).WillOnce(Return(false));
    EXPECT_CALL(*mock_nm_interface, last_error).WillOnce(Return(QDBusError{QDBusError::InvalidInterface, msg}));

    inject_root_interface();
    if (!invalid_root_interface)
        inject_settings_interface();

    MP_ASSERT_THROW_THAT(
        mp::backend::create_bridge_with("whatever"), mp::backend::CreateBridgeException,
        mpt::match_what(AllOf(HasSubstr("Could not reach remote D-Bus object"), HasSubstr(msg.toStdString()))));
}

INSTANTIATE_TEST_SUITE_P(CreateBridgeTest, CreateBridgeInvalidInterfaceTest, Values(true, false));

TEST_F(CreateBridgeTest, throws_on_failure_to_create_first_connection)
{
    auto msg = QStringLiteral("Nope");
    auto ifc = QStringLiteral("An interface");
    auto obj = QStringLiteral("An object");
    auto svc = QStringLiteral("A service");

    EXPECT_CALL(*mock_nm_settings, call_impl(_, Eq("AddConnection"), _, _, _))
        .WillOnce(Return(QDBusMessage::createError(QDBusError::AccessDenied, msg)));
    EXPECT_CALL(*mock_nm_settings, interface).WillOnce(Return(ifc));
    EXPECT_CALL(*mock_nm_settings, path).WillOnce(Return(obj));
    EXPECT_CALL(*mock_nm_settings, service).WillOnce(Return(svc));

    inject_dbus_interfaces();
    MP_ASSERT_THROW_THAT(mp::backend::create_bridge_with("umdolita"), mp::backend::CreateBridgeException,
                         mpt::match_what(AllOf(HasSubstr(msg.toStdString()), HasSubstr(ifc.toStdString()),
                                               HasSubstr(obj.toStdString()), HasSubstr(svc.toStdString()))));
}

TEST_F(CreateBridgeTest, throws_on_failure_to_create_second_connection)
{
    const auto msg = QStringLiteral("Still not");
    const auto ifc = QStringLiteral("the interface");
    const auto obj = QStringLiteral("the object");
    const auto svc = QStringLiteral("the service");
    const auto new_connection_path = QStringLiteral("/a/b/c");

    EXPECT_CALL(*mock_nm_settings, call_impl(_, Eq("AddConnection"), _, _, _))
        .WillOnce(Return(make_obj_path_reply(new_connection_path)))
        .WillOnce(Return(QDBusMessage::createError(QDBusError::UnknownMethod, msg)));
    EXPECT_CALL(*mock_nm_settings, interface).WillOnce(Return(ifc));
    EXPECT_CALL(*mock_nm_settings, path).WillOnce(Return(obj));
    EXPECT_CALL(*mock_nm_settings, service).WillOnce(Return(svc));

    inject_dbus_interfaces();

    std::unique_ptr<MockDBusInterface> mock_nm_connection = std::make_unique<MockDBusInterface>();
    EXPECT_CALL(*mock_nm_connection, call_impl(_, Eq("Delete"), empty, empty, empty));
    EXPECT_CALL(mock_bus, get_interface(Eq("org.freedesktop.NetworkManager"), Eq(new_connection_path),
                                        Eq("org.freedesktop.NetworkManager.Settings.Connection")))
        .WillOnce(Return(ByMove(std::move(mock_nm_connection))));

    MP_ASSERT_THROW_THAT(mp::backend::create_bridge_with("abc"), mp::backend::CreateBridgeException,
                         mpt::match_what(AllOf(HasSubstr(msg.toStdString()), HasSubstr(ifc.toStdString()),
                                               HasSubstr(obj.toStdString()), HasSubstr(svc.toStdString()))));
}

TEST_F(CreateBridgeTest, throws_on_failure_to_activate_second_connection)
{
    const auto msg = QStringLiteral("Refusing");
    const auto ifc = QStringLiteral("interface");
    const auto obj = QStringLiteral("object");
    const auto svc = QStringLiteral("service");
    const auto new_connection_path1 = QStringLiteral("/foo");
    const auto new_connection_path2 = QStringLiteral("/bar");

    EXPECT_CALL(*mock_nm_settings, call_impl(_, Eq("AddConnection"), _, _, _))
        .WillOnce(Return(make_obj_path_reply(new_connection_path1)))
        .WillOnce(Return(make_obj_path_reply(new_connection_path2)));

    EXPECT_CALL(*mock_nm_root, call_impl(_, Eq("ActivateConnection"), _, _, _))
        .WillOnce(Return(QDBusMessage::createError(QDBusError::InvalidArgs, msg)));
    EXPECT_CALL(*mock_nm_root, interface).WillOnce(Return(ifc));
    EXPECT_CALL(*mock_nm_root, path).WillOnce(Return(obj));
    EXPECT_CALL(*mock_nm_root, service).WillOnce(Return(svc));

    inject_dbus_interfaces();

    std::unique_ptr<MockDBusInterface> mock_nm_connection1 = std::make_unique<MockDBusInterface>();
    std::unique_ptr<MockDBusInterface> mock_nm_connection2 = std::make_unique<MockDBusInterface>();
    EXPECT_CALL(*mock_nm_connection1, call_impl(_, Eq("Delete"), empty, empty, empty));
    EXPECT_CALL(mock_bus, get_interface(Eq("org.freedesktop.NetworkManager"), Eq(new_connection_path1),
                                        Eq("org.freedesktop.NetworkManager.Settings.Connection")))
        .WillOnce(Return(ByMove(std::move(mock_nm_connection1))));
    EXPECT_CALL(*mock_nm_connection2, call_impl(_, Eq("Delete"), empty, empty, empty));
    EXPECT_CALL(mock_bus, get_interface(Eq("org.freedesktop.NetworkManager"), Eq(new_connection_path2),
                                        Eq("org.freedesktop.NetworkManager.Settings.Connection")))
        .WillOnce(Return(ByMove(std::move(mock_nm_connection2))));

    MP_ASSERT_THROW_THAT(mp::backend::create_bridge_with("kaka"), mp::backend::CreateBridgeException,
                         mpt::match_what(AllOf(HasSubstr(msg.toStdString()), HasSubstr(ifc.toStdString()),
                                               HasSubstr(obj.toStdString()), HasSubstr(svc.toStdString()))));
}

TEST_F(CreateBridgeTest, logs_on_failure_to_rollback)
{
    const auto child_path = QStringLiteral("/child");
    const auto original_error = 255;
    const auto rollback_error = "fail";

    EXPECT_CALL(*mock_nm_settings, call_impl(_, Eq("AddConnection"), _, _, _))
        .WillOnce(Return(make_obj_path_reply("/asdf")))
        .WillOnce(Return(make_obj_path_reply(child_path)));
    EXPECT_CALL(*mock_nm_root, call_impl(_, Eq("ActivateConnection"), _, _, _)).WillOnce(Throw(original_error));

    inject_dbus_interfaces();

    std::unique_ptr<MockDBusInterface> mock_nm_connection1 = std::make_unique<MockDBusInterface>();
    EXPECT_CALL(*mock_nm_connection1, call_impl(_, Eq("Delete"), empty, empty, empty))
        .WillOnce(Throw(std::runtime_error{rollback_error}));
    EXPECT_CALL(mock_bus, get_interface(Eq("org.freedesktop.NetworkManager"), Eq(child_path),
                                        Eq("org.freedesktop.NetworkManager.Settings.Connection")))
        .WillOnce(Return(ByMove(std::move(mock_nm_connection1))));

    logger_scope.mock_logger->expect_log(mpl::Level::error, rollback_error);
    MP_ASSERT_THROW_THAT(mp::backend::create_bridge_with("gigi"), int, Eq(original_error));
}

struct CreateBridgeExceptionTest : public CreateBridgeTest, WithParamInterface<bool>
{
};

TEST_P(CreateBridgeExceptionTest, create_bridge_exception_info)
{
    auto rollback = GetParam();
    static constexpr auto specific_info = "spefic error details";
    auto generic_msg = fmt::format("Could not {} bridge", rollback ? "rollback" : "create");
    EXPECT_THAT((mp::backend::CreateBridgeException{specific_info, QDBusError{}, rollback}),
                mpt::match_what(AllOf(HasSubstr(generic_msg), HasSubstr(specific_info))));
}

TEST_P(CreateBridgeExceptionTest, create_bridge_exception_includes_dbus_cause_when_available)
{
    auto msg = QStringLiteral("DBus error msg");
    QDBusError dbus_error = {QDBusError::Other, msg};
    ASSERT_TRUE(dbus_error.isValid());
    EXPECT_THAT((mp::backend::CreateBridgeException{"detail", dbus_error, GetParam()}),
                mpt::match_what(HasSubstr(msg.toStdString())));
}

TEST_P(CreateBridgeExceptionTest, create_bridge_exception_mentions_unknown_cause_when_unavailable)
{
    QDBusError dbus_error{};
    ASSERT_FALSE(dbus_error.isValid());
    EXPECT_THAT((mp::backend::CreateBridgeException{"detail", dbus_error, GetParam()}),
                mpt::match_what(HasSubstr("unknown cause")));
}

INSTANTIATE_TEST_SUITE_P(CreateBridgeTest, CreateBridgeExceptionTest, Values(true, false));
} // namespace
