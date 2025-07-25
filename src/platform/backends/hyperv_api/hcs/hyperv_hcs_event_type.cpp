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

#include <hyperv_api/hcs/hyperv_hcs_event_type.h>

// clang-format off
#include <windows.h>
#include <ComputeDefs.h>
// clang-format on

namespace multipass::hyperv::hcs
{

HcsEventType parse_event(const void* hcs_event)
{
    const HCS_EVENT* evt = reinterpret_cast<const HCS_EVENT*>(hcs_event);
    switch (evt->Type)
    {
    case HcsEventSystemExited:
        return HcsEventType::SystemExited;
    default:
        return HcsEventType::Unknown;
    }
}
} // namespace multipass::hyperv::hcs
