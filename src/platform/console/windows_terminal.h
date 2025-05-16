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

#include <multipass/terminal.h>

#include <windows.h>

namespace multipass
{
class WindowsTerminal : public Terminal
{
public:
    explicit WindowsTerminal();
    virtual ~WindowsTerminal();

    HANDLE cin_handle() const;
    HANDLE cout_handle() const;
    HANDLE cerr_handle() const;

    bool cin_is_live() const override;
    bool cout_is_live() const override;

    std::string read_all_cin() override;
    void set_cin_echo(const bool enable) override;

    ConsolePtr make_console(ssh_channel channel) override;

private:
    const UINT input_code_page;
    const UINT output_code_page;
};
} // namespace multipass

