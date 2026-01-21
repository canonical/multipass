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

#include "reset_process_factory.h"

namespace mp = multipass;
namespace mpt = multipass::test;

namespace
{
class ResettableProcessFactory : public mp::ProcessFactory
{
public:
    static void Reset()
    {
        mp::ProcessFactory::reset();
    }

    using mp::ProcessFactory::ProcessFactory;
};
} // namespace

mpt::ResetProcessFactory::ResetProcessFactory()
{
    ResettableProcessFactory::Reset();
}

mpt::ResetProcessFactory::~ResetProcessFactory()
{
    ResettableProcessFactory::Reset();
}
