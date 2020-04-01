/*
 * Copyright (C) 2020 Canonical, Ltd.
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

#ifndef MULTIPASS_STANDARD_PATHS_H
#define MULTIPASS_STANDARD_PATHS_H

#include "singleton.h"

namespace multipass
{
class StandardPaths : public Singleton<StandardPaths>
{
public:
    StandardPaths(const Singleton<StandardPaths>::PrivatePass&);
};
} // namespace multipass
#endif // MULTIPASS_STANDARD_PATHS_H
