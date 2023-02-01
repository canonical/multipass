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
#ifndef MULTIPASS_BASE_CLOUD_INIT_CONFIG_H
#define MULTIPASS_BASE_CLOUD_INIT_CONFIG_H

namespace multipass
{
constexpr auto base_cloud_init_config = "growpart:\n"
                                        "    mode: auto\n"
                                        "    devices: [\"/\"]\n"
                                        "    ignore_growroot_disabled: false\n"
                                        "users:\n"
                                        "    - default\n"
                                        "manage_etc_hosts: true\n";
}

#endif // MULTIPASS_BASE_CLOUD_INIT_CONFIG_H
