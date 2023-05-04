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

#ifndef MULTIPASS_DBUS_WRAPPERS_H
#define MULTIPASS_DBUS_WRAPPERS_H

#include "multipass/singleton.h"

#include <multipass/disabled_copy_move.h>

#include <QString>
#include <QVariant>
#include <QtDBus/QtDBus>

#include <cassert>
#include <memory>
#include <optional>

namespace multipass::backend::dbus
{

class DBusInterface : private DisabledCopyMove
{
public:
    virtual ~DBusInterface() = default;

    virtual bool is_valid() const
    {
        assert(iface);
        return iface->isValid();
    }

    virtual QDBusError last_error() const
    {
        assert(iface);
        return iface->lastError();
    }

    virtual QString interface() const
    {
        assert(iface);
        return iface->interface();
    }

    virtual QString path() const
    {
        assert(iface);
        return iface->path();
    }

    virtual QString service() const
    {
        assert(iface);
        return iface->service();
    }

    QDBusMessage call(QDBus::CallMode mode, const QString& method, const QVariant& arg1 = {}, const QVariant& arg2 = {},
                      const QVariant& arg3 = {}) // three args should be enough for the DBus methods we need
    {
        return call_impl(mode, method, arg1, arg2, arg3); // indirection avoids default args in virtual method
    }

protected:
    DBusInterface() = default; // for mocks

    virtual QDBusMessage call_impl(QDBus::CallMode mode, const QString& method, const QVariant& arg1,
                                   const QVariant& arg2, const QVariant& arg3)
    {
        assert(iface);
        if (!arg1.isValid())
            return iface->call(mode, method);
        if (!arg2.isValid())
            return iface->call(mode, method, arg1);
        if (!arg3.isValid())
            return iface->call(mode, method, arg1, arg2);

        return iface->call(mode, method, arg1, arg2, arg3);
    }

private:
    friend class DBusConnection;
    explicit DBusInterface(
        std::unique_ptr<QDBusInterface>&& iface) noexcept // client creates QDBusInterface, so we can noexcept
        : iface{std::move(iface)}
    {
    }

    std::unique_ptr<QDBusInterface> iface;
};

class DBusConnection : private DisabledCopyMove
{
public:
    virtual ~DBusConnection() = default;

    virtual bool is_connected() const
    {
        return connection.value().isConnected();
    }

    virtual QDBusError last_error() const
    {
        return connection.value().lastError();
    }

    virtual std::unique_ptr<DBusInterface> get_interface(const QString& service, const QString& path,
                                                         const QString& interface) const
    {
        auto con = connection.value();
        assert(con.isConnected());
        auto qiface = std::make_unique<QDBusInterface>(service, path, interface, con);

        return std::unique_ptr<DBusInterface>(new DBusInterface(std::move(qiface))); /* std::make_unique can't call
                                      private ctors, so we call it ourselves; but the ctor is noexcept, so no leaks */
    }

protected:
    friend class DBusProvider;
    explicit DBusConnection(bool create_bus) // allow mocks to skip bus fetching
        : connection{create_bus ? std::optional{QDBusConnection::systemBus()} : std::nullopt}
    {
    }

private:
    std::optional<QDBusConnection> connection;
};

class DBusProvider : public Singleton<DBusProvider>
{
public:
    explicit DBusProvider(const Singleton::PrivatePass& pass) : Singleton{pass}, system_bus{/* create_bus = */ true}
    {
    }

    virtual const DBusConnection& get_system_bus() const // return ref for polymorphism (and no chopping in mocks)
    {
        return system_bus;
    }

private:
    DBusConnection system_bus;
};
} // namespace multipass::backend::dbus

#endif // MULTIPASS_DBUS_WRAPPERS_H
