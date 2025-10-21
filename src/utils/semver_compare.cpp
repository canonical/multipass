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

#include <multipass/utils/semver_compare.h>
#include <stdexcept>

#include <semver.hpp>

namespace multipass
{

std::strong_ordering operator<=>(const opaque_semver& lhs, const opaque_semver& rhs)
{
    auto parse_version = [](const opaque_semver& svv) {
        semver::version v;
        if (!semver::parse(svv.value, v))
        {
            std::printf("error: %s\n", svv.value.c_str());
            throw std::invalid_argument{svv.value};
        }
        return v;
    };
    const auto lhs_v = parse_version(lhs);
    const auto rhs_v = parse_version(rhs);

    return lhs_v <=> rhs_v;
}

} // namespace multipass
