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

#ifndef MULTIPASS_UPDATE_PROMPT_H
#define MULTIPASS_UPDATE_PROMPT_H

#include <memory>

namespace multipass
{
class UpdateInfo;

class UpdatePrompt
{
public:
    using UPtr = std::unique_ptr<UpdatePrompt>;
    virtual ~UpdatePrompt() = default;

    virtual bool is_time_to_show() = 0;
    virtual void populate(UpdateInfo *update_info) = 0;
    virtual void populate_if_time_to_show(UpdateInfo *update_info) = 0;
};
} // namespace multipass

#endif // MULTIPASS_UPDATE_PROMPT_H
