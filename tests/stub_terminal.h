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

#ifndef MULTIPASS_STUB_TERMINAL_H
#define MULTIPASS_STUB_TERMINAL_H

#include <multipass/terminal.h>

namespace multipass
{
namespace test
{

class StubTerminal : public multipass::Terminal
{
public:
    StubTerminal()
        : cout_stream{null_stream}
    {}
    StubTerminal(std::ostream& cout)
        : cout_stream{cout}
    {}
    ~StubTerminal() override = default;

    std::istream& cin() override
    {
        return null_stream;
    }
    std::ostream& cout() override
    {
        return cout_stream;
    }
    std::ostream& cerr() override
    {
        return cout_stream;
    }

    bool cin_is_tty() const override
    {
        return true;
    }

    bool cout_is_tty() const override
    {
        return true;
    }
private:
    std::stringstream null_stream;
    std::ostream &cout_stream;
};

} // namespace test
} // namespace multipass

#endif // MULTIPASS_STUB_TERMINAL_H
