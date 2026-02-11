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

#include "applevz_utils.h"

#include <applevz/applevz_wrapper.h>
#include <qemu/qemu_img_utils.h>

namespace
{
mp::Path convert_to_asif(const mp::Path& source_path)
{
    return source_path + ".asif";
}
} // namespace

namespace multipass::applevz
{
Path convert_to_supported_format(const Path& image_path)
{
    if (MP_APPLEVZ.macos_at_least(26, 0) && !is_asif_image(image_path))
    {
        return convert_to_asif(image_path);
    }
    else
    {
        return backend::convert_to_raw(image_path);
    }
}

void resize_image(const MemorySize& disk_space, const Path& image_path)
{
}
} // namespace multipass::applevz
