/*
 * Copyright (C) 2020 Canonical, Ltd.
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

#include "platform_shared.h"

#include <multipass/constants.h>
#include <multipass/exceptions/settings_exceptions.h>

#include <QKeySequence>

namespace mp = multipass;
namespace mpp = multipass::platform;

QString multipass::platform::default_hotkey()
{
    return QStringLiteral("Ctrl+Alt+U");
}

QString mpp::interpret_general_hotkey(const QString& val)
{
    auto sequence = QKeySequence{val};
    auto ret = sequence.toString();

    if (ret.isEmpty() && !sequence.isEmpty())
        throw InvalidSettingsException(mp::hotkey_key, val, "Invalid key sequence");
    if (sequence.count() > 1)
        throw InvalidSettingsException(mp::hotkey_key, val, "Multiple key sequences are not supported");

    return ret;
}
