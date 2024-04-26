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

#ifndef MULTIPASS_RUNTIME_INSTANCE_INFO_HELPER_H
#define MULTIPASS_RUNTIME_INSTANCE_INFO_HELPER_H

#include <string>

namespace multipass
{
class VirtualMachine;
class DetailedInfoItem;
class InstanceDetails;

// Note: we could extract other code to info/list populating code here, but that is left as a future improvement
struct RuntimeInstanceInfoHelper
{
    static void populate_runtime_info(VirtualMachine& vm,
                                      DetailedInfoItem* info,
                                      InstanceDetails* instance_info,
                                      const std::string& original_release,
                                      bool parallelize);
};

} // namespace multipass

#endif // MULTIPASS_RUNTIME_INSTANCE_INFO_HELPER_H
