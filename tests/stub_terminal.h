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

#ifndef MULTIPASS_STUB_TERMINAL_H
#define MULTIPASS_STUB_TERMINAL_H

#include <multipass/terminal.h>

#include "stub_console.h"

namespace multipass
{
namespace test
{

class StubTerminal : public multipass::Terminal
{
public:
    StubTerminal(std::ostream& cout, std::ostream& cerr, std::istream& cin)
        : cout_stream{cout}, cerr_stream{cerr}, cin_stream{cin}
    {}

    ~StubTerminal() override = default;

    std::istream& cin() override
    {
        return cin_stream;
    }
    std::ostream& cout() override
    {
        return cout_stream;
    }
    std::ostream& cerr() override
    {
        return cerr_stream;
    }

    bool cin_is_live() const override
    {
        return false;
    }

    bool cout_is_live() const override
    {
        return false;
    }

    void set_cin_echo(const bool enable) override
    {
    }

    ConsolePtr make_console(ssh_channel channel) override
    {
        return std::make_unique<StubConsole>();
    }

private:
    std::ostream &cout_stream;
    std::ostream& cerr_stream;
    std::istream& cin_stream;
};

} // namespace test
} // namespace multipass

#endif // MULTIPASS_STUB_TERMINAL_H
