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

#pragma once

#include <multipass/mount_handler.h>

namespace multipass::test
{
class MockMountHandler : public MountHandler
{
public:
    MOCK_METHOD(void, activate_impl, (ServerVariant, std::chrono::milliseconds), (override));
    MOCK_METHOD(void, deactivate_impl, (bool), (override));
    MOCK_METHOD(bool, is_active, (), (override));
};
} // namespace multipass::test
