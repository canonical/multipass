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

#include <shared/windows/wsa_init_wrapper.h>

#include <multipass/logging/log.h>

#include <windows.h>
#include <WS2tcpip.h>
#include <WinSock2.h>

namespace mp = multipass;
namespace mpl = mp::logging;
namespace multipass
{
wsa_init_wrapper::wsa_init_wrapper()
    : wsa_data(new ::WSAData()), wsa_init_result(::WSAStartup(MAKEWORD(2, 2), wsa_data))
{
    constexpr auto category = "wsa-init-wrapper";
    mpl::debug(category, " initialized WSA, status `{}`", wsa_init_result);

    if (!operator bool())
    {

        mpl::error(category,
                   " WSAStartup failed with `{}`: {}",
                   wsa_init_result,
                   std::system_category().message(wsa_init_result));
    }
}

wsa_init_wrapper::~wsa_init_wrapper()
{
    /**
     * https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-wsacleanup
     * There must be a call to WSACleanup for each successful call to WSAStartup.
     * Only the final WSACleanup function call performs the actual cleanup.
     * The preceding calls simply decrement an internal reference count in the WS2_32.DLL.
     */
    if (operator bool())
    {
        WSACleanup();
    }
    delete wsa_data;
}
} // namespace multipass
