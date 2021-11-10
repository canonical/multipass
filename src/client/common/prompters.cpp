/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#include <multipass/cli/prompters.h>
#include <multipass/exceptions/cmd_exceptions.h>

#include <iostream>

namespace multipass
{

std::string PlainPrompter::prompt(const std::string& text)
{
    term->cout() << text << ": ";

    std::string value;
    std::getline(term->cin(), value);

    if (term->cin().eof())
        throw ValueException("Unexpected end-of-file");

    return value;
}

} // namespace multipass
