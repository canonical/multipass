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
 */

#pragma once

#include "../common.h"
#include "../mock_singleton_helpers.h"

#include <multipass/platform_unix.h>

namespace multipass::test
{

class MockPosixSignal : public platform::PosixSignal
{
public:
    using PosixSignal::PosixSignal;

    MOCK_METHOD(int, pthread_sigmask, (int, const sigset_t*, sigset_t*), (const, override));
    MOCK_METHOD(int, pthread_kill, (pthread_t, int), (const, override));
    MOCK_METHOD(int, sigwait, (const sigset_t&, int&), (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockPosixSignal, platform::PosixSignal);
};

} // namespace multipass::test
