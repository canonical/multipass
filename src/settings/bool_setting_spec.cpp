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

#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/settings/bool_setting_spec.h>

namespace mp = multipass;

namespace
{
QString interpret_bool(QString val)
{ // constrain accepted values to avoid QVariant::toBool interpreting non-empty strings (such as "nope") as true
    static constexpr auto convert_to_true = {"on", "yes", "1"};
    static constexpr auto convert_to_false = {"off", "no", "0"};
    auto lower_val = val.toLower();

    if (std::find(cbegin(convert_to_true), cend(convert_to_true), lower_val) != cend(convert_to_true))
        val = QStringLiteral("true");
    else if (std::find(cbegin(convert_to_false), cend(convert_to_false), lower_val) != cend(convert_to_false))
        val = QStringLiteral("false");

    return val;
}

QString interpret_impl(const QString& key, QString val)
{
    auto ret = interpret_bool(std::move(val));
    if (ret != "true" && ret != "false")
        throw mp::InvalidSettingException(key, ret, "Invalid value, try \"true\" or \"false\"");

    return ret;
}

std::pair<QString, QString> munch_params(QString key, QString default_) // work around use after move
{
    auto interpreted_default = interpret_impl(key, std::move(default_));
    return {std::move(key), std::move(interpreted_default)};
}
} // namespace

mp::BoolSettingSpec::BoolSettingSpec(QString key, QString default_)
    : BoolSettingSpec{munch_params(std::move(key), std::move(default_))}
{
}

mp::BoolSettingSpec::BoolSettingSpec(std::pair<QString, QString> params)
    : BasicSettingSpec(std::move(params.first), std::move(params.second))
{
}

QString mp::BoolSettingSpec::interpret(QString val) const
{
    return interpret_impl(key, std::move(val));
}
