/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#include <src/platform/backends/shared_linux/process.h>
#include <src/platform/backends/shared_linux/process_factory.h>

namespace multipass
{
namespace test
{

class StubProcessSpec : public ProcessSpec
{
public:
    QString program() const override
    {
        return QString();
    }
};

class StubProcess : public Process
{
public:
    StubProcess() : Process{std::make_unique<StubProcessSpec>()}
    {
    }
};

class StubProcessFactory : public ProcessFactory
{
    std::unique_ptr<Process> create_process(std::unique_ptr<ProcessSpec>&&) const override
    {
        return std::make_unique<StubProcess>();
    }
};
} // namespace test
} // namespace multipass

#endif // MULTIPASS_STUB_LOGGER_H
