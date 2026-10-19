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

#pragma once

#include <fmt/format.h>

#include <string>

namespace multipass::hyperv::hcn
{

/**
 * Compute the deterministic endpoint `Name` tag for a given instance.
 *
 * All endpoints created for a given instance (the primary/management endpoint
 * as well as any extra interfaces) are tagged with the same name. This allows
 * the endpoints belonging to an instance to be discovered and removed purely
 * by name, without needing to know (or resolve) the instance's compute
 * system RuntimeId -- which requires the compute system to still exist/be
 * reopenable.
 *
 * @param vm_name Name of the instance.
 * @return The deterministic endpoint name for the instance.
 */
inline std::string endpoint_name_for(const std::string& vm_name)
{
    return fmt::format("multipass-{}", vm_name);
}

} // namespace multipass::hyperv::hcn
