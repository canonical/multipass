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

#pragma once

#include "disabled_copy_move.h"

#include <memory>
#include <string>

namespace multipass
{
namespace petname
{
enum NumWords
{
    One,
    Two,
    Three,
};
}

class PetnameInterface : private DisabledCopyMove
{
public:
    using UPtr = std::unique_ptr<PetnameInterface>;
    virtual ~PetnameInterface() = default;
    virtual std::string make_name() = 0;

protected:
    PetnameInterface() = default;
};

PetnameInterface::UPtr make_petname_provider(petname::NumWords words = petname::NumWords::Two,
                                             char separator = '-');
} // namespace multipass
