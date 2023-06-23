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

#ifndef MULTIPASS_VIRTUAL_MACHINE_IMAGE_H
#define MULTIPASS_VIRTUAL_MACHINE_IMAGE_H

#include <multipass/path.h>

#include <vector>

namespace multipass
{
class VMImage
{
public:
    Path image_path;
    std::string id;
    std::string original_release;
    std::string current_release;
    std::string release_date;
    std::vector<std::string> aliases;
};
}
#endif // MULTIPASS_VIRTUAL_MACHINE_IMAGE_H
