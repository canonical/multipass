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

#ifndef MULTIPASS_DEFAULT_UPDATE_PROMPT_H
#define MULTIPASS_DEFAULT_UPDATE_PROMPT_H

#include <multipass/update_prompt.h>
#include <chrono>
#include <memory>

namespace multipass
{
class NewReleaseMonitor;

/*
 * Update Prompt that checks Default
 */
class DefaultUpdatePrompt : public UpdatePrompt
{
public:
    DefaultUpdatePrompt();
    ~DefaultUpdatePrompt();

    bool is_time_to_show() override;
    void populate(UpdateInfo *update_info) override;
    void populate_if_time_to_show(UpdateInfo *update_info) override;

private:
    std::unique_ptr<NewReleaseMonitor> monitor;
    std::chrono::system_clock::time_point last_shown;
};
} // namespace multipass

#endif // MULTIPASS_DEFAULT_UPDATE_PROMPT_H
