/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#ifndef MULTIPASS_MOCK_ENVIRONMENT_HELPER_H
#define MULTIPASS_MOCK_ENVIRONMENT_HELPER_H

#include <QByteArray>

namespace multipass
{
namespace test
{

class SetEnvScope
{
public:
    explicit SetEnvScope(const QByteArray& name, const QByteArray& new_value) : name(name)
    {
        old_value = qgetenv(name.constData());

        qputenv(name.constData(), new_value.constData());
    }
    ~SetEnvScope()
    {
        if (old_value.isNull())
            qunsetenv(name.constData());
        else
            qputenv(name.constData(), old_value.constData());
    }

private:
    QByteArray name, old_value;
};

class UnsetEnvScope
{
public:
    explicit UnsetEnvScope(const QByteArray& name) : name(name)
    {
        old_value = qgetenv(name.constData());
        if (!old_value.isNull())
            qunsetenv(name.constData());
    }
    ~UnsetEnvScope()
    {
        if (!old_value.isNull())
            qputenv(name.constData(), old_value.constData());
    }

private:
    QByteArray name, old_value;
};

} // namespace test
} // namespace multipass

#endif // MULTIPASS_MOCK_ENVIRONMENT_HELPER_H
