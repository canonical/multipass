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

#ifndef MULTIPASS_ALIAS_DEFINITION_H
#define MULTIPASS_ALIAS_DEFINITION_H

#include <string>
#include <unordered_map>

namespace multipass
{
struct AliasDefinition
{
    std::string instance;
    std::string command;
    std::string working_directory;
};

inline bool operator==(const AliasDefinition& a, const AliasDefinition& b)
{
    return (a.instance == b.instance && a.command == b.command);
}

typedef typename std::unordered_map<std::string, AliasDefinition> AliasContext;

} // namespace multipass
#endif // MULTIPASS_ALIAS_DEFINITION_H
