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

#ifndef MULTIPASS_MOCK_UTILS_H
#define MULTIPASS_MOCK_UTILS_H

#include "common.h"
#include "mock_singleton_helpers.h"

#include <multipass/ssh/ssh_key_provider.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine.h>

namespace multipass::test
{
class MockUtils : public Utils
{
public:
    using Utils::Utils;
    MOCK_CONST_METHOD1(filesystem_bytes_available, qint64(const QString&));
    MOCK_METHOD1(exit, void(int));
    MOCK_CONST_METHOD3(run_cmd_for_output, std::string(const QString&, const QStringList&, const int));
    MOCK_CONST_METHOD3(run_cmd_for_status, bool(const QString&, const QStringList&, const int));
    MOCK_METHOD2(make_file_with_content, void(const std::string&, const std::string&));
    MOCK_METHOD3(make_file_with_content, void(const std::string&, const std::string&, const bool&));
    MOCK_CONST_METHOD3(wait_for_cloud_init, void(VirtualMachine*, std::chrono::milliseconds, const SSHKeyProvider&));
    MOCK_CONST_METHOD0(get_kernel_version, std::string());
    MOCK_CONST_METHOD1(generate_scrypt_hash_for, QString(const QString&));

    MP_MOCK_SINGLETON_BOILERPLATE(MockUtils, Utils);
};
} // namespace multipass::test
#endif // MULTIPASS_MOCK_UTILS_H
