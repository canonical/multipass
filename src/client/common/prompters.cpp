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

namespace
{
auto get_input(std::istream& cin)
{
    std::string value;
    std::getline(cin, value);

    if (!cin.good())
        throw mp::PromptException("Failed to read value");

    return value;
}
} // namespace

std::string mp::PlainPrompter::prompt(const std::string& text) const
{
    term->cout() << text << ": ";

    auto value = get_input(term->cin());

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

    auto passphrase = get_input(term->cin());

    term->cout() << "\n";

    return passphrase;
}

std::string mp::NewPassphrasePrompter::prompt(const std::string& prompt1, const std::string& prompt2) const
{
    std::string passphrase;

    while (true)
    {
        term->cout() << prompt1;

        passphrase = get_input(term->cin());

        term->cout() << "\n" << prompt2;

        // Confirm the passphrase is the same by re-entering it
        if (passphrase == get_input(term->cin()))
        {
            break;
        }
        else
        {
            term->cout() << "\nPassphrases do not match. Please try again.\n";
        }
    }

    term->cout() << "\n";

    return passphrase;
}
