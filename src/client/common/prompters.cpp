/*
 * Copyright (C) 2021-2022 Canonical, Ltd.
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
#include <multipass/exceptions/cli_exceptions.h>

#include <iostream>

namespace mp = multipass;

std::string mp::PlainPrompter::prompt(const std::string& text) const
{
    term->cout() << text << ": ";

    std::string value;
    std::getline(term->cin(), value);

    if (!term->cin().good())
        throw PromptException("Failed to read value");

    return value;
}

mp::PassphrasePrompter::PassphrasePrompter(Terminal* term) : BasePrompter(term)
{
    term->set_cin_echo(false);
}

mp::PassphrasePrompter::~PassphrasePrompter()
{
    term->set_cin_echo(true);
}

std::string mp::PassphrasePrompter::prompt(const std::string& text) const
{
    term->cout() << text;

    std::string passphrase;
    std::getline(term->cin(), passphrase);

    if (!term->cin().good())
        throw PromptException("Failed to read passphrase");

    term->cout() << "\n";

    return passphrase;
}
