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

#ifndef MULTIPASS_BOOL_SETTING_SPEC_H
#define MULTIPASS_BOOL_SETTING_SPEC_H

#include "basic_setting_spec.h"

namespace multipass
{
class BoolSettingSpec : public BasicSettingSpec
{
public:
    using BasicSettingSpec::BasicSettingSpec;
    QString interpret(const QString& val) const override;
};
} // namespace multipass

#endif // MULTIPASS_BOOL_SETTING_SPEC_H
