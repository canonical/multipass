/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include "unconfined_system.h"

#include <multipass/process.h>

namespace mp = multipass;

mp::UnconfinedSystem::UnconfinedSystem()
{
}

std::unique_ptr<mp::Process> mp::UnconfinedSystem::create_process(std::unique_ptr<mp::ProcessSpec>&& process_spec) const
{
    return std::make_unique<mp::Process>(std::move(process_spec));
}
