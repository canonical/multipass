/*
 * Copyright (C) 2017 Canonical, Ltd.
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

#ifndef MULTIPASS_CONSOLE_H
#define MULTIPASS_CONSOLE_H

#include <array>
#include <functional>
#include <memory>

namespace multipass
{
class Console
{
public:
    using UPtr = std::unique_ptr<Console>;

    struct WindowGeometry
    {
        int rows;
        int columns;
    };

    virtual ~Console() = default;

    virtual void setup_console() = 0;
    virtual int read_console(std::array<char, 512>& buffer) = 0;
    virtual void write_console(const char* buffer, int bytes) = 0;
    virtual void signal_console() = 0;
    virtual bool is_window_size_changed() = 0;
    virtual bool is_interactive() = 0;
    virtual WindowGeometry get_window_geometry() = 0;

    static UPtr make_console();

protected:
    explicit Console() = default;
    Console(const Console&) = delete;
    Console& operator=(const Console&) = delete;

private:
    virtual void restore_console() = 0;
};
}
#endif // MULTIPASS_CONSOLE_H
