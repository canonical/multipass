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

#include <multipass/memory_size.h>
#include <multipass/path.h>
#include <multipass/singleton.h>

#define MP_APPLEVZ_UTILS multipass::applevz::AppleVZImageUtils::instance()

namespace multipass::applevz
{
class AppleVZImageUtils : public Singleton<AppleVZImageUtils>
{
public:
    using Singleton<AppleVZImageUtils>::Singleton;

    virtual Path convert_to_supported_format(const Path& image_path) const;
    virtual void resize_image(const MemorySize& disk_space, const Path& image_path) const;
    virtual bool macos_at_least(int major, int minor, int patch = 0) const;
};
} // namespace multipass::applevz
