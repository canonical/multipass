/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/settings/bool_setting_spec.h>

namespace mp = multipass;

namespace
{
QString interpret_bool(QString val)
{ // constrain accepted values to avoid QVariant::toBool interpreting non-empty strings (such as "nope") as true
    static constexpr auto convert_to_true = {"on", "yes", "1"};
    static constexpr auto convert_to_false = {"off", "no", "0"};
    val = val.toLower();

    if (std::find(cbegin(convert_to_true), cend(convert_to_true), val) != cend(convert_to_true))
        return QStringLiteral("true");
    else if (std::find(cbegin(convert_to_false), cend(convert_to_false), val) != cend(convert_to_false))
        return QStringLiteral("false");
    else
        return val;
}
} // namespace

QString mp::BoolSettingSpec::interpret(const QString& val) const
{
    auto ret = interpret_bool(val);
    if (ret != "true" && ret != "false")
        throw InvalidSettingException(key, val, "Invalid flag, try \"true\" or \"false\"");

    return ret;
}
