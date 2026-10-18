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

// This is not an empty file -- initguid.h must precede any header that uses
// DEFINE_GUID to emit GUID storage in exactly one translation unit. Without
// this, GUID symbols referenced by Virtdisk.h remain extern declarations
// and the linker will report unresolved externals

#include <windows.h>
#include <initguid.h>
#include <virtdisk.h>
