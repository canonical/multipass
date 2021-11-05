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

#ifndef MULTIPASS_PERSISTENT_SETTING_H
#define MULTIPASS_PERSISTENT_SETTING_H

#include "disabled_copy_move.h"

#include <QString>

#include <memory>

namespace multipass
{
class PersistentSetting : public DisabledCopyMove
{
public:
    using UPtr = std::unique_ptr<PersistentSetting>;
    virtual ~PersistentSetting() = default;

    virtual QString get_key() const = 0;
    virtual QString get_default() const = 0;
    virtual QString interpret(const QString& val) const = 0;
};
} // namespace multipass

#endif // MULTIPASS_PERSISTENT_SETTING_H
