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

#ifndef MULTIPASS_QEMU_IMG_UTILS_H
#define MULTIPASS_QEMU_IMG_UTILS_H

#include <multipass/path.h>

#include <chrono>

namespace multipass
{
class MemorySize;

namespace backend
{
using namespace std::chrono_literals;

constexpr auto image_resize_timeout = std::chrono::duration_cast<std::chrono::milliseconds>(5min).count();

void resize_instance_image(const MemorySize& disk_space, const multipass::Path& image_path);
Path convert_to_qcow_if_necessary(const Path& image_path);
} // namespace backend
} // namespace multipass
#endif // MULTIPASS_QEMU_IMG_UTILS_H
