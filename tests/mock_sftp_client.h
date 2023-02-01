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

#ifndef MOCK_SFTP_CLIENT_H
#define MOCK_SFTP_CLIENT_H

#include "common.h"
#include <multipass/ssh/sftp_client.h>

namespace multipass::test
{
struct MockSFTPClient : public SFTPClient
{
    MOCK_METHOD(bool, is_remote_dir, (const fs::path& path), (override));
    MOCK_METHOD(bool, push, (const fs::path& source_path, const fs::path& target_path, Flags flags), (override));
    MOCK_METHOD(bool, pull, (const fs::path& source_path, const fs::path& target_path, Flags flags), (override));
    MOCK_METHOD(void, from_cin, (std::istream & cin, const fs::path& target_path, bool make_parent), (override));
    MOCK_METHOD(void, to_cout, (const fs::path& source_path, std::ostream& cout), (override));
};
} // namespace multipass::test

#endif // MOCK_SFTP_CLIENT_H
