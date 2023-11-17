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

#include <multipass/disabled_copy_move.h>
#include <multipass/terminal.h>

#include <string>
#include <vector>

#ifndef MULTIPASS_CLI_PROMPTERS_H
#define MULTIPASS_CLI_PROMPTERS_H

namespace multipass
{
class Prompter : private DisabledCopyMove
{
public:
    explicit Prompter(Terminal*);

    virtual ~Prompter() = default;

    virtual std::string prompt(const std::string&) const = 0;

protected:
    Prompter() = default;
};

class BasePrompter : public Prompter
{
public:
    explicit BasePrompter(Terminal* term) : term(term){};

protected:
    Terminal* term;
};

class PlainPrompter : public BasePrompter
{
public:
    using BasePrompter::BasePrompter;

    std::string prompt(const std::string&) const override;
};

class PassphrasePrompter : public PlainPrompter
{
public:
    using PlainPrompter::PlainPrompter;

    std::string prompt(const std::string& text = "Please enter passphrase") const override;

private:
    class ScopedEcholessInput
    {
    public:
        explicit ScopedEcholessInput(Terminal* term) : term(term)
        {
            term->set_cin_echo(false);
        };

        virtual ~ScopedEcholessInput()
        {
            term->set_cin_echo(true);
        }

    private:
        Terminal* term;
    };
};

class NewPassphrasePrompter : public PassphrasePrompter
{
public:
    using PassphrasePrompter::PassphrasePrompter;

    std::string prompt(const std::string& text = "Please re-enter passphrase") const override;
};

class BridgePrompter : private DisabledCopyMove
{
public:
    explicit BridgePrompter(Terminal* term) : term(term){};

    ~BridgePrompter() = default;

    bool bridge_prompt(const std::vector<std::string>& nets_need_bridging) const;

private:
    BridgePrompter() = default;

    Terminal* term;
};
} // namespace multipass

#endif // MULTIPASS_CLI_PROMPTERS_H
