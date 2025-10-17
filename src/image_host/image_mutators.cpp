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

#include <multipass/image_host/image_mutators.h>

#include <regex>

namespace multipass::image_mutators
{
constexpr int earliestSupportedSnapcraftMVersion{18};
bool snapcraft_mutator(VMImageInfo& info)
{
    const auto& aliases = info.aliases;
    return aliases.empty() || std::any_of(aliases.begin(), aliases.end(), [](const auto& alias) {
               std::string alias_string{alias.toStdString()};
               std::smatch matches;
               if (!std::regex_match(alias_string,
                                     matches,
                                     std::regex{"core([0-9]+[24680])|([0-9]+[24680])\\.04|devel"}))
                   // Not a supported alias
                   return false;
               // The captured index after 0 (full match) indexes the regex groups in order
               std::string match_string{matches.str(1).empty() ? matches.str(2) : matches.str(1)};
               if (match_string.empty())
                   // Matched devel so all captured groups are empty
                   return true;
               int major_version{std::stoi(match_string)};
               return major_version >= earliestSupportedSnapcraftMVersion;
           });
}

bool core_mutator(VMImageInfo& info)
{
    info.aliases = {"core" + info.release_codename};
    info.os = "Ubuntu";
    info.release = "core-" + info.release_codename;
    info.release_title = "Core " + info.release_codename;
    info.release_codename = info.release_title;

    return info.supported && info.version == "current";
}

bool release_mutator(VMImageInfo& info)
{
    if (info.aliases.contains("lts"))
    {
        info.aliases << "ubuntu";
    }

    return true;
}
} // namespace multipass::image_mutators
