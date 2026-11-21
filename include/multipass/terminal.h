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

#include <multipass/handles.h>

#include <istream>
#include <memory>
#include <ostream>
#include <string>

namespace multipass
{
/**
 * NativeHandles holds the platform-specific handles for a terminal's streams
 */
struct NativeHandles
{
    Handle cin;
    Handle cout;
    Handle cerr;
};

class Terminal
{
public:
    virtual ~Terminal() = default;

    virtual std::istream& cin();
    virtual std::ostream& cout();
    virtual std::ostream& cerr();

    virtual bool cin_is_live() const = 0;
    virtual bool cout_is_live() const = 0;

    /**
     * Get the platform-specific terminal stream handles
     @returns The three stream handles: cin, cout, cerr.
     */
    virtual NativeHandles native_handles() const = 0;

    bool is_live() const;

    virtual std::string read_all_cin();
    virtual void set_cin_echo(const bool enable) = 0;

    using UPtr = std::unique_ptr<Terminal>;
    static UPtr make_terminal();
};
} // namespace multipass
