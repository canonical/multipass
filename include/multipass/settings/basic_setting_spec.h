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

#ifndef MULTIPASS_BASIC_SETTING_SPEC_H
#define MULTIPASS_BASIC_SETTING_SPEC_H

#include "setting_spec.h"

namespace multipass
{
class BasicSettingSpec : public SettingSpec
{
public:
    BasicSettingSpec(QString key, QString default_);
    QString get_key() const override;
    QString get_default() const override;
    QString interpret(QString val) const override;

protected:
    QString key;
    QString default_;
};
} // namespace multipass

#endif // MULTIPASS_BASIC_SETTING_SPEC_H
