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

#include <multipass/cli/client_common.h>
#include <multipass/cli/prompters.h>
#include <multipass/exceptions/cli_exceptions.h>
#include <multipass/format.h>

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

// TODO when we have remote client-daemon communication, we need to get the daemon's platform
constexpr bool on_windows()
{
    return
#ifdef MULTIPASS_PLATFORM_WINDOWS
        true;
#else
        false;
#endif
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

bool mp::BridgePrompter::bridge_prompt(const std::vector<std::string>& nets_need_bridging) const
{
    assert(nets_need_bridging.size()); // precondition

    static constexpr auto plural =
        "Multipass needs to create {} to connect to {}.\nThis will temporarily disrupt "
        "connectivity on those interfaces.\n\nDo you want to continue (yes/no)? ";
    static constexpr auto singular =
        "Multipass needs to create a {} to connect to {}.\nThis will temporarily disrupt "
        "connectivity on that interface.\n\nDo you want to continue (yes/no)? ";
    static constexpr auto nodes = on_windows() ? "switches" : "bridges";
    static constexpr auto node = on_windows() ? "switch" : "bridge";

    if (term->is_live())
    {
        if (nets_need_bridging.size() != 1)
            fmt::print(term->cout(), plural, nodes, fmt::join(nets_need_bridging, ", "));
        else
            fmt::print(term->cout(), singular, node, nets_need_bridging[0]);

        while (true)
        {
            std::string answer;
            std::getline(term->cin(), answer);
            if (std::regex_match(answer, mp::client::yes_answer))
                return true;
            else if (std::regex_match(answer, mp::client::no_answer))
                return false;
            else
                term->cout() << "Please answer yes/no: ";
        }
    }

    return false;
}

bool mp::DeviceBindingPrompter::device_binding_prompt(const std::vector<std::string>& devices_need_binding) const
{
    assert(devices_need_binding.size()); // precondition

    static constexpr auto plural =
        "Multipass needs to bind the following PCI devices to vfio-pci on the host: {}.\n"
        "This may disrupt host drivers using those devices.\n\n"
        "Do you want to continue (yes/no)? ";
    static constexpr auto singular =
        "Multipass needs to bind the following PCI device to vfio-pci on the host: {}.\n"
        "This may disrupt the host driver using that device.\n\n"
        "Do you want to continue (yes/no)? ";

    if (term->is_live())
    {
        if (devices_need_binding.size() != 1)
            fmt::print(term->cout(), plural, fmt::join(devices_need_binding, ", "));
        else
            fmt::print(term->cout(), singular, devices_need_binding[0]);

        while (true)
        {
            std::string answer;
            std::getline(term->cin(), answer);
            if (std::regex_match(answer, mp::client::yes_answer))
                return true;
            else if (std::regex_match(answer, mp::client::no_answer))
                return false;
            else
                term->cout() << "Please answer yes/no: ";
        }
    }

    return false;
}
