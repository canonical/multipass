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

#ifndef MULTIPASS_CREATE_ALIAS_H
#define MULTIPASS_CREATE_ALIAS_H

#include <multipass/cli/alias_dict.h>
#include <multipass/cli/return_codes.h>

#include <optional>

namespace multipass
{
namespace cmd
{
ReturnCode create_alias(AliasDict& aliases, const std::string& alias_name, const AliasDefinition& alias_definition,
                        std::ostream& cout, std::ostream& cerr,
                        const std::optional<std::string>& context = std::nullopt);
} // namespace cmd
} // namespace multipass

#endif // MULTIPASS_CREATE_ALIAS_H
