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

#include "mock_process_factory.h"

#include <QProcess>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace ::testing;

std::unique_ptr<mp::Process> mpt::MockProcessFactory::create_process(std::unique_ptr<mp::ProcessSpec>&& spec) const
{
    auto process = std::make_unique<NiceMock<mpt::MockProcess>>(std::move(spec),
                                                                const_cast<std::vector<ProcessInfo>&>(process_list));
    if (callback)
        callback.value()(process.get());
    return process;
}

mpt::MockProcess::MockProcess(std::unique_ptr<mp::ProcessSpec>&& spec,
                              std::vector<mpt::MockProcessFactory::ProcessInfo>& process_list)
    : spec{std::move(spec)}
{
    success_exit_state.exit_code = 0;

    ON_CALL(*this, start()).WillByDefault(Invoke([this] { emit started(); }));
    ON_CALL(*this, kill()).WillByDefault(Invoke([this] {
        mp::ProcessState exit_state;
        exit_state.exit_code = 0;
        emit finished(exit_state);
    }));
    ON_CALL(*this, running()).WillByDefault(Return(true));
    ON_CALL(*this, process_state()).WillByDefault(Return(success_exit_state));
    ON_CALL(*this, execute(_)).WillByDefault(Return(success_exit_state));

    mpt::MockProcessFactory::ProcessInfo p{program(), arguments()};
    process_list.emplace_back(p);
}

void mpt::MockProcessFactory::register_callback(const mpt::MockProcessFactory::Callback& cb)
{
    callback = cb;
}

std::unique_ptr<mp::test::MockProcessFactory::Scope> mp::test::MockProcessFactory::Inject()
{
    ProcessFactory::mock<MockProcessFactory>();
    return std::make_unique<mp::test::MockProcessFactory::Scope>();
}

mpt::MockProcessFactory::Scope::~Scope()
{
    ProcessFactory::reset();
}

std::vector<mpt::MockProcessFactory::ProcessInfo> mpt::MockProcessFactory::Scope::process_list()
{
    return mock_instance().process_list;
}

mpt::MockProcessFactory& mpt::MockProcessFactory::mock_instance()
{
    try
    {
        return dynamic_cast<mpt::MockProcessFactory&>(instance());
    }
    catch (std::bad_cast&)
    {
        throw std::runtime_error("ProcessFactory::instance() called before MockProcessFactory::Inject()");
    }
}

void mpt::MockProcessFactory::Scope::register_callback(const mpt::MockProcessFactory::Callback& cb)
{
    mock_instance().register_callback(cb);
}

// MockProcess implementation

QString mpt::MockProcess::program() const
{
    return spec->program();
}
QStringList mpt::MockProcess::arguments() const
{
    return spec->arguments();
}
QString mpt::MockProcess::working_directory() const
{
    return spec->working_directory();
}
QProcessEnvironment mpt::MockProcess::process_environment() const
{
    return spec->environment();
}

bool mpt::MockProcess::wait_for_started(int)
{
    return true;
}
bool mpt::MockProcess::wait_for_finished(int)
{
    return true;
}

qint64 mpt::MockProcess::write(const QByteArray&)
{
    return 0;
}

void mpt::MockProcess::close_write_channel()
{
}

void mpt::MockProcess::setup_child_process()
{
}
