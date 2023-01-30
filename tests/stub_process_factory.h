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

#ifndef MULTIPASS_STUB_PROCESS_FACTORY_H
#define MULTIPASS_STUB_PROCESS_FACTORY_H

#include "common.h"
#include "process_factory.h" // rely on build system to include the right implementation

#include <multipass/process/process.h>

namespace multipass
{
namespace test
{

class StubProcessFactory : public ProcessFactory
{
public:
    struct ProcessInfo
    {
        QString command;
        QStringList arguments;
    };

    // StubProcessFactory installed with Inject() call, and uninstalled when the Scope object deleted
    struct Scope
    {
        ~Scope();

        // Get info about the Processes launched with this list
        std::vector<ProcessInfo> process_list();
    };

    static std::unique_ptr<Scope> Inject();

    // Implementation
    using ProcessFactory::ProcessFactory;

    std::unique_ptr<Process> create_process(std::unique_ptr<ProcessSpec>&& spec) const override;

private:
    static StubProcessFactory& stub_instance();
    std::vector<ProcessInfo> process_list;
};

} // namespace test
} // namespace multipass

#endif // MULTIPASS_STUB_PROCESS_FACTORY_H
