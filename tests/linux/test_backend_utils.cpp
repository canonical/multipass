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
#include "tests/mock_backend_utils.h"
#include "tests/mock_file_ops.h"
#include "tests/mock_logger.h"
#include "tests/mock_process_factory.h"
#include "tests/mock_singleton_helpers.h"
#include "tests/mock_utils.h"

#include <shared/shared_backend_utils.h>
#include <src/platform/backends/shared/linux/backend_utils.h>
#include <src/platform/backends/shared/linux/dbus_wrappers.h>

#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/memory_size.h>

#include <QMap>
#include <QVariant>

#include <fcntl.h>
#include <sys/ioctl.h>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
namespace mp_dbus = mp::backend::dbus;
class MockDBusProvider : public mp_dbus::DBusProvider
{
public:
    using DBusProvider::DBusProvider;
    MOCK_METHOD(const mp_dbus::DBusConnection&, get_system_bus, (), (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockDBusProvider, DBusProvider);
};

class MockDBusConnection : public mp_dbus::DBusConnection
{
public:
    MockDBusConnection() : mp_dbus::DBusConnection(/* create_bus = */ false)
    {
    }

    MOCK_METHOD(bool, is_connected, (), (const, override));
    MOCK_METHOD(QDBusError, last_error, (), (const, override));
    MOCK_METHOD(std::unique_ptr<mp_dbus::DBusInterface>, get_interface,
                (const QString&, const QString&, const QString&), (const, override));
};

class MockDBusInterface : public mp_dbus::DBusInterface
{
public:
    using DBusInterface::DBusInterface;

    MOCK_METHOD(bool, is_valid, (), (const, override));
    MOCK_METHOD(QDBusError, last_error, (), (const, override));
    MOCK_METHOD(QString, interface, (), (const, override));
    MOCK_METHOD(QString, path, (), (const, override));
    MOCK_METHOD(QString, service, (), (const, override));
    MOCK_METHOD(QDBusMessage, call_impl,
                (QDBus::CallMode, const QString&, const QVariant&, const QVariant&, const QVariant&), (override));
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
    EXPECT_EQ(MP_BACKEND.create_bridge_with(network), get_bridge_name(network).toStdString());
}

TEST_F(CreateBridgeTest, throws_if_bus_disconnected)
{
    auto msg = QStringLiteral("DBus error msg");
    EXPECT_CALL(mock_bus, is_connected).WillOnce(Return(false));
    EXPECT_CALL(mock_bus, last_error).WillOnce(Return(QDBusError{QDBusError::BadAddress, msg}));

    MP_EXPECT_THROW_THAT(
        MP_BACKEND.create_bridge_with("asdf"), mp::backend::CreateBridgeException,
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
        MP_BACKEND.create_bridge_with("whatever"), mp::backend::CreateBridgeException,
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
    MP_ASSERT_THROW_THAT(MP_BACKEND.create_bridge_with("umdolita"), mp::backend::CreateBridgeException,
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

    MP_ASSERT_THROW_THAT(MP_BACKEND.create_bridge_with("abc"), mp::backend::CreateBridgeException,
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

    MP_ASSERT_THROW_THAT(MP_BACKEND.create_bridge_with("kaka"), mp::backend::CreateBridgeException,
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
    MP_ASSERT_THROW_THAT(MP_BACKEND.create_bridge_with("gigi"), int, Eq(original_error));
}

struct CreateBridgeExceptionTest : public CreateBridgeTest, WithParamInterface<bool>
{
};

TEST_P(CreateBridgeExceptionTest, create_bridge_exception_info)
{
    auto rollback = GetParam();
    static constexpr auto specific_info = "specific error details";
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

TEST(LinuxBackendUtils, check_for_kvm_support_no_error_does_not_throw)
{
    auto [mock_file_ops, file_ops_guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, exists(A<const QFile&>())).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, open(_, _)).WillOnce(Return(true));

    EXPECT_NO_THROW(MP_BACKEND.check_for_kvm_support());
}

TEST(LinuxBackendUtils, check_for_kvm_support_does_not_exist_throws_expected_error)
{
    auto [mock_file_ops, file_ops_guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, exists(A<const QFile&>())).WillOnce(Return(false));

    MP_EXPECT_THROW_THAT(MP_BACKEND.check_for_kvm_support(), std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("KVM support is not enabled on this machine."),
                                               HasSubstr("Please ensure the following:"))));
}

TEST(LinuxBackendUtils, check_for_kvm_support_no_read_write_throws_expected_error)
{
    auto [mock_file_ops, file_ops_guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, exists(A<const QFile&>())).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, open(_, _)).WillOnce(Return(false));

    MP_EXPECT_THROW_THAT(
        MP_BACKEND.check_for_kvm_support(), std::runtime_error,
        mpt::match_what(StrEq("The KVM device cannot be opened for reading and writing.\nPlease "
                              "ensure the Snap KVM interface is connected by issuing:\n$ snap connect multipass:kvm")));
}

TEST(LinuxBackendUtils, check_kvm_in_use_no_failure_does_not_throw)
{
    auto [mock_linux_syscalls, guard] = mpt::MockLinuxSysCalls::inject();

    EXPECT_CALL(*mock_linux_syscalls, close(_)).WillRepeatedly(Return(0));
    EXPECT_CALL(*mock_linux_syscalls, ioctl(_, _, _)).WillOnce(Return(1));
    EXPECT_CALL(*mock_linux_syscalls, open(_, _)).WillOnce(Return(1));

    EXPECT_NO_THROW(MP_BACKEND.check_if_kvm_is_in_use());
}

TEST(LinuxBackendUtils, check_kvm_in_use_fails_throws_expected_message)
{
    auto [mock_linux_syscalls, guard] = mpt::MockLinuxSysCalls::inject();

    EXPECT_CALL(*mock_linux_syscalls, close(_)).WillRepeatedly(Return(0));
    EXPECT_CALL(*mock_linux_syscalls, open(_, _)).WillOnce(Return(1));
    EXPECT_CALL(*mock_linux_syscalls, ioctl(_, _, _)).WillOnce([](auto...) {
        errno = EBUSY;
        return -1;
    });

    MP_EXPECT_THROW_THAT(
        MP_BACKEND.check_if_kvm_is_in_use(), std::runtime_error,
        mpt::match_what(StrEq("Another virtual machine manager is currently running. Please shut it down before "
                              "starting a Multipass instance.")));
}

TEST(LinuxBackendUtils, linux_syscalls_return_expected_values)
{
    const std::string null_path{"/dev/null"};

    auto null_fd = MP_LINUX_SYSCALLS.open(null_path.c_str(), O_RDONLY);

    EXPECT_NE(null_fd, -1);

    // /dev/null does not accept ioctl calls, so it will fail
    EXPECT_EQ(MP_LINUX_SYSCALLS.ioctl(null_fd, FIONREAD, 0), -1);

    EXPECT_EQ(MP_LINUX_SYSCALLS.close(null_fd), 0);
}

TEST(LinuxBackendUtils, get_subnet_bridge_exists_returns_expected_data)
{
    const std::string test_subnet{"10.102.12"};
    const QString bridge_name{"test-bridge"};
    auto [mock_utils, guard] = mpt::MockUtils::inject();

    EXPECT_CALL(*mock_utils, run_cmd_for_output(QString("ip"), QStringList({"-4", "route", "show"}), _))
        .WillOnce(Return(fmt::format("{}.0 dev {} proto kernel scope link", test_subnet, bridge_name)));

    EXPECT_EQ(MP_BACKEND.get_subnet("foo", bridge_name), test_subnet);
}

TEST(LinuxBackendUtils, get_subnet_in_file_returns_expected_data)
{
    const std::string test_subnet{"10.102.12"};
    const QString bridge_name{"test-bridge"};
    auto [mock_utils, utils_guard] = mpt::MockUtils::inject();
    auto [mock_file_ops, file_ops_guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_utils, run_cmd_for_output(QString("ip"), QStringList({"-4", "route", "show"}), _))
        .WillOnce(Return(""));

    EXPECT_CALL(*mock_file_ops, open(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, size(_)).WillOnce(Return(1));
    EXPECT_CALL(*mock_file_ops, read_all(_)).WillOnce(Return(QByteArray::fromStdString(test_subnet)));

    EXPECT_EQ(MP_BACKEND.get_subnet("foo", bridge_name), test_subnet);
}

TEST(LinuxBackendUtils, get_subnet_not_in_file_writes_new_subnet_returns_expected_data)
{
    const QString bridge_name{"test-bridge"};
    std::string generated_subnet;
    auto [mock_utils, utils_guard] = mpt::MockUtils::inject();
    auto [mock_file_ops, file_ops_guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_utils, run_cmd_for_output(QString("ip"), QStringList({"-4", "route", "show"}), _))
        .WillOnce(Return(""))
        .WillOnce(Return("0.0.0.0"));
    EXPECT_CALL(*mock_utils, run_cmd_for_status(QString("ping"), _, _)).WillRepeatedly(Return(false));

    EXPECT_CALL(*mock_file_ops, open(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, size(_)).WillOnce(Return(0));
    EXPECT_CALL(*mock_file_ops, write(A<QFile&>(), _, _)).WillOnce([&generated_subnet](auto&, auto data, auto) {
        generated_subnet = std::string(data);

        return generated_subnet.length();
    });

    EXPECT_EQ(MP_BACKEND.get_subnet("foo", bridge_name), generated_subnet);
}
