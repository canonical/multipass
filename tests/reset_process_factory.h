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

#ifndef MULTIPASS_RESETTABLE_PROCESS_FACTORY_H
#define MULTIPASS_RESETTABLE_PROCESS_FACTORY_H

#include "process_factory.h" // rely on build system to include the right implementation

namespace multipass
{
namespace test
{

// This resets the ProcessFactory on creation & destruction
struct ResetProcessFactory
{
    ResetProcessFactory();
    ~ResetProcessFactory();
};

} // namespace test
} // namespace multipass

#endif // MULTIPASS_RESETTABLE_PROCESS_FACTORY_H
