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

#ifndef MULTIPASS_HYPERV_API_WRAPPER_FWDECL_H
#define MULTIPASS_HYPERV_API_WRAPPER_FWDECL_H

#include <memory>

namespace multipass::hyperv
{

namespace hcs
{
class HCSWrapperInterface;
}

namespace hcn
{
class HCNWrapperInterface;
}

namespace virtdisk
{
class VirtDiskWrapperInterface;
}

using hcs_sptr_t = std::shared_ptr<multipass::hyperv::hcs::HCSWrapperInterface>;
using hcn_sptr_t = std::shared_ptr<multipass::hyperv::hcn::HCNWrapperInterface>;
using virtdisk_sptr_t = std::shared_ptr<multipass::hyperv::virtdisk::VirtDiskWrapperInterface>;
} // namespace multipass::hyperv

#endif
