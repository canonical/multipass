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

#ifndef MULTIPASS_JOURNALD_WRAPPER_H
#define MULTIPASS_JOURNALD_WRAPPER_H

#include <multipass/singleton.h>
#include <string_view>

namespace multipass
{
namespace logging
{
class JournaldWrapper : public Singleton<JournaldWrapper>
{
public:
    JournaldWrapper(const Singleton<JournaldWrapper>::PrivatePass&) noexcept;

    /**
     * Write an entry to the journal
     *
     * @param [in] message_fmtstr Message format string
     * @param [in] message Message
     * @param [in] priority_fmtstr Priority format string
     * @param [in] priority Priority
     * @param [in] category_fmtstr Category format string
     * @param [in] category Category
     *
     * NOTE: All format strings MUST be NUL-terminated.
     */
    virtual void write_journal(std::string_view message_fmtstr,
                               std::string_view message,
                               std::string_view priority_fmtstr,
                               int priority,
                               std::string_view category_fmtstr,
                               std::string_view category) const;
};
} // namespace logging
} // namespace multipass
#endif // MULTIPASS_JOURNALD_WRAPPER_H
