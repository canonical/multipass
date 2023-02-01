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

    return get_input(term->cin());
}

std::string mp::PassphrasePrompter::prompt(const std::string& text) const
{
    ScopedEcholessInput scoped_echoless_input(term);

    auto passphrase = PlainPrompter::prompt(text);

    term->cout() << "\n";

    return passphrase;
}

std::string mp::NewPassphrasePrompter::prompt(const std::string& text) const
{
    auto passphrase = PassphrasePrompter::prompt();

    // Confirm the passphrase is the same by re-entering it
    if (passphrase != PassphrasePrompter::prompt(text))
    {
        throw PromptException("Passphrases do not match");
    }

    return passphrase;
}
