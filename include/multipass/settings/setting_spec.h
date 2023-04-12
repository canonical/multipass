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

#ifndef MULTIPASS_SETTING_SPEC_H
#define MULTIPASS_SETTING_SPEC_H

#include <multipass/disabled_copy_move.h>

#include <QString>

#include <memory>
#include <set>

namespace multipass
{
class SettingSpec : private DisabledCopyMove
{
public:
    virtual ~SettingSpec() = default;

    virtual QString get_key() const = 0;
    virtual QString get_default() const = 0;
    virtual QString interpret(QString val) const = 0; // TODO: replace with marshall/unmarshall

public:
    using UPtr = std::unique_ptr<SettingSpec>;

    struct LessByKey
    {
        bool operator()(const SettingSpec& a, const SettingSpec& b) const;
        bool operator()(const SettingSpec::UPtr& a, const SettingSpec::UPtr& b) const;
    };

    using Set = std::set<UPtr, LessByKey>; // would be nice to have gsl::not_null
};
} // namespace multipass

inline bool multipass::SettingSpec::LessByKey::operator()(const SettingSpec& a, const SettingSpec& b) const
{
    return a.get_key() < b.get_key(); // compares lexicographically
}

inline bool multipass::SettingSpec::LessByKey::operator()(const SettingSpec::UPtr& a, const SettingSpec::UPtr& b) const
{
    return a && b ? (*this)(*a, *b) : bool{b}; // nullptr is the lowest, otherwise lexicographical
}

#endif // MULTIPASS_SETTING_SPEC_H
