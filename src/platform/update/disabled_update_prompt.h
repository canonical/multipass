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

#ifndef MULTIPASS_DISABLED_UPDATE_PROMPT_H
#define MULTIPASS_DISABLED_UPDATE_PROMPT_H

#include <multipass/update_prompt.h>

namespace multipass
{

class DisabledUpdatePrompt : public UpdatePrompt
{
public:
    bool is_time_to_show() override { return false; }
    void populate(UpdateInfo *) override {}
    void populate_if_time_to_show(UpdateInfo *) override {}
};
} // namespace multipass

#endif // MULTIPASS_DISABLED_UPDATE_PROMPT_H
