/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#include <multipass/snap_utils.h>

namespace mu = multipass::utils;

bool mu::is_snap()
{
    // Decide if Snap based on if $SNAP env var is set.
    // TODO: can this be better?
    return !snap_dir().isEmpty();
}

QByteArray mu::snap_dir()
{
    return qgetenv("SNAP"); // Inside snap, this can be trusted.
}

QByteArray mu::snap_common_dir()
{
    return qgetenv("SNAP_COMMON"); // Inside snap, this can be trusted
}
