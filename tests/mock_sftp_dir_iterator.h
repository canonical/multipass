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

#ifndef MOCK_SFTP_DIR_ITERATOR_H
#define MOCK_SFTP_DIR_ITERATOR_H

#include "common.h"
#include <multipass/ssh/sftp_dir_iterator.h>

namespace multipass::test
{
struct MockSFTPDirIterator : public SFTPDirIterator
{
    MOCK_METHOD(bool, hasNext, (), (override, const));
    MOCK_METHOD(SFTPAttributesUPtr, next, (), (override));
};
} // namespace multipass::test
#endif // MOCK_SFTP_DIR_ITERATOR_H
