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

#include <multipass/process/simple_process_spec.h>

namespace mp = multipass;

namespace
{
class SimpleProcessSpec : public mp::ProcessSpec
{
public:
    SimpleProcessSpec(const QString& cmd, const QStringList& args) : cmd{cmd}, args{args}
    {
    }

    QString program() const override
    {
        return cmd;
    }
    QStringList arguments() const override
    {
        return args;
    }

    QString apparmor_profile() const override
    {
        return QString();
    }

private:
    const QString cmd;
    const QStringList args;
};
} // namespace

std::unique_ptr<mp::ProcessSpec> mp::simple_process_spec(const QString& cmd, const QStringList& args)
{
    return std::make_unique<::SimpleProcessSpec>(cmd, args);
}
