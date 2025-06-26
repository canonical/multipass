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

#include <QStringList>

namespace multipass
{

class AppArmor
{
public:
    AppArmor();

    void load_policy(const QByteArray& aa_policy) const;
    void remove_policy(const QByteArray& aa_policy) const;

    void next_exec_under_policy(const QByteArray& aa_policy_name) const;

private:
    const QStringList apparmor_args;
};

class AppArmorException : public std::runtime_error
{
public:
    AppArmorException(const std::string& msg) : runtime_error{msg}
    {
    }
};

} // namespace multipass
