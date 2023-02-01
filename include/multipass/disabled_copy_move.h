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

#ifndef MULTIPASS_DISABLED_COPY_MOVE_H
#define MULTIPASS_DISABLED_COPY_MOVE_H

namespace multipass
{

/**
 * A simple base class to disable copy and move construction and assignment.
 *
 * To use, inherit privately, Like so:
 * @code
 *  class Foo : private DisabledCopyMove {...};`
 * @endcode
 */
class DisabledCopyMove
{
public:
    DisabledCopyMove(const DisabledCopyMove&) = delete;
    DisabledCopyMove& operator=(const DisabledCopyMove&) = delete;

protected:
    DisabledCopyMove() = default;
    ~DisabledCopyMove() = default; // non-virtual, but needs protected - see Core Guidelines C.35
};
} // namespace multipass

#endif // MULTIPASS_DISABLED_COPY_MOVE_H
