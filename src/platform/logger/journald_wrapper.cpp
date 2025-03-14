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

#include "journald_wrapper.h"

#define SD_JOURNAL_SUPPRESS_LOCATION
#include <systemd/sd-journal.h>

namespace multipass::logging
{

JournaldWrapper::JournaldWrapper(const Singleton<JournaldWrapper>::PrivatePass& pass) noexcept
    : Singleton<JournaldWrapper>::Singleton{pass}
{
}

void JournaldWrapper::write_journal(std::string_view message_fmtstr,
                                    std::string_view message,
                                    std::string_view priority_fmtstr,
                                    int priority,
                                    std::string_view category_fmtstr,
                                    std::string_view category) const
{
    sd_journal_send(message_fmtstr.data(),
                    static_cast<int>(message.size()),
                    message.data(),
                    priority_fmtstr.data(),
                    priority,
                    category_fmtstr.data(),
                    static_cast<int>(category.size()),
                    category.data(),
                    nullptr);
}

} // namespace multipass::logging
