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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#ifndef MULTIPASS_NAME_GENERATOR_H
#define MULTIPASS_NAME_GENERATOR_H

#include "disabled_copy_move.h"

#include <memory>
#include <string>

namespace multipass
{
class NameGenerator : private DisabledCopyMove
{
public:
    using UPtr = std::unique_ptr<NameGenerator>;
    virtual ~NameGenerator() = default;
    virtual std::string make_name() = 0;

protected:
    NameGenerator() = default;
};

NameGenerator::UPtr make_default_name_generator();
}
#endif //MULTIPASS_NAME_GENERATOR_H
