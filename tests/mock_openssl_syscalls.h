/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#ifndef MULTIPASS_MOCK_OPENSSL_SYSCALLS_H
#define MULTIPASS_MOCK_OPENSSL_SYSCALLS_H

#include <premock.hpp>

#include <openssl/evp.h>

DECL_MOCK(EVP_PBE_scrypt);

#endif // MULTIPASS_MOCK_OPENSSL_SYSCALLS_H
