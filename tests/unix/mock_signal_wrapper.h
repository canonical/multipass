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

#ifndef MULTIPASS_MOCK_SIGNAL_WRAPPER_H
#define MULTIPASS_MOCK_SIGNAL_WRAPPER_H

#include "../common.h"
#include "../mock_singleton_helpers.h"

#include <multipass/platform_unix.h>

namespace multipass::test
{

class MockSignalWrapper : public platform::SignalWrapper
{
public:
    using SignalWrapper::SignalWrapper;

    MOCK_METHOD(int, mask_signals, (int, const sigset_t*, sigset_t*), (const, override));
    MOCK_METHOD(int, send, (pthread_t, int), (const, override));
    MOCK_METHOD(int, wait, (const sigset_t&, int&), (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockSignalWrapper, platform::SignalWrapper);
};

} // namespace multipass::test

#endif // MULTIPASS_MOCK_SIGNAL_WRAPPER_H
