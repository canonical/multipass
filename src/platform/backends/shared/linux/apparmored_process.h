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

#include <multipass/process/basic_process.h>

#include <QByteArray>
#include <QString>

#include <memory>
#include <string>

namespace multipass
{
class AppArmor;
class ProcessSpec;

class AppArmoredProcess final : public BasicProcess
{
public:
    AppArmoredProcess(const AppArmor& apparmor,
                      std::shared_ptr<ProcessSpec> process_spec,
                      QString profile_name,
                      QByteArray policy);
    ~AppArmoredProcess() override;

    const QString& apparmor_profile_name() const noexcept;
    const QByteArray& apparmor_profile() const noexcept;

protected:
    void setup_child_process() final;

private:
    const AppArmor& apparmor;
    const QString profile_name;
    const QByteArray policy;

    // This has to be pre-populated since we can't allocate memory in fork() state.
    const std::string aa_exec_str;
};

} // namespace multipass
