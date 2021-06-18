/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#ifndef MULTIPASS_JSON_UTILS_H
#define MULTIPASS_JSON_UTILS_H

#include "temp_dir.h"

#include <multipass/network_interface.h>
#include <multipass/optional.h>

#include <string>
#include <vector>

#include <QString>

namespace mp = multipass;
namespace mpt = multipass::test;

std::string make_instance_json(const mp::optional<std::string>& default_mac = mp::nullopt,
                               const std::vector<mp::NetworkInterface>& extra_ifaces = {},
                               const std::vector<std::string>& extra_instances = {});

std::unique_ptr<mpt::TempDir> plant_instance_json(const std::string& contents); // unique_ptr bypasses missing move ctor

void check_interfaces_in_json(const QString& file, const std::string& mac,
                              const std::vector<mp::NetworkInterface>& extra_interfaces);

#endif // MULTIPASS_JSON_UTILS_H
