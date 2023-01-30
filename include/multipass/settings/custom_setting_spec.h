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

#ifndef MULTIPASS_CUSTOM_SETTING_SPEC_H
#define MULTIPASS_CUSTOM_SETTING_SPEC_H

#include "basic_setting_spec.h"

#include <functional>

namespace multipass
{
class CustomSettingSpec : public multipass::BasicSettingSpec
{
public:
    CustomSettingSpec(QString key, QString default_, std::function<QString(QString)> interpreter);
    QString interpret(QString val) const override;

protected:
    std::function<QString(const QString&)> interpreter;
};
} // namespace multipass

#endif // MULTIPASS_CUSTOM_SETTING_SPEC_H
