/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include <multipass/process_spec.h>

namespace mp = multipass;

// Create Process with fixed list of arguments
QStringList multipass::ProcessSpec::arguments() const
{
    return QStringList();
}

// For cases when multiple instances of this process need different apparmor profiles, use this
// identifier to distinguish them
QString multipass::ProcessSpec::identifier() const
{
    return QString();
}

const QString mp::ProcessSpec::apparmor_profile_name() const
{
    if (!identifier().isNull())
    {
        return QStringLiteral("multipass.") + identifier() + '.' + program();
    }
    else
    {
        return QStringLiteral("multipass.") + program();
    }
}
