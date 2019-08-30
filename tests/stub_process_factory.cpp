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

#include "stub_process_factory.h"

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace ::testing;

namespace
{

class StubProcess : public mp::Process
{
public:
    StubProcess(std::unique_ptr<mp::ProcessSpec>&& spec,
                std::vector<mpt::StubProcessFactory::ProcessInfo>& process_list)
    {
        mpt::StubProcessFactory::ProcessInfo p{spec->program(), spec->arguments()};
        process_list.emplace_back(p);
    }

    QString program() const override
    {
        return "";
    }
    QStringList arguments() const override
    {
        return {};
    }
    QString working_directory() const override
    {
        return "";
    }
    QProcessEnvironment process_environment() const override
    {
        return QProcessEnvironment();
    }

    void start() override
    {
        emit started();
    }
    void kill() override
    {
        mp::ProcessState exit_state;
        exit_state.exit_code = 0;
        emit finished(exit_state);
    }

    bool wait_for_started(int msecs = 30000) override
    {
        return true;
    }
    bool wait_for_finished(int msecs = 30000) override
    {
        return true;
    }

    bool running() const override
    {
        return true;
    }

    mp::ProcessState process_state() const override
    {
        return mp::ProcessState();
    }

    QByteArray read_all_standard_output() override
    {
        return "";
    }
    QByteArray read_all_standard_error() override
    {
        return "";
    }

    qint64 write(const QByteArray& data) override
    {
        return 0;
    }

    void close_write_channel() override
    {
    }

    mp::ProcessState execute(const int /*timeout*/ = 3000) override
    {
        mp::ProcessState process_state;
        process_state.exit_code = 0;
        return process_state;
    }

    void setup_child_process() override
    {
    }
};
} // namespace

std::unique_ptr<mp::Process> mpt::StubProcessFactory::create_process(std::unique_ptr<ProcessSpec>&& spec) const
{
    return std::make_unique<StubProcess>(std::move(spec),
                                         const_cast<std::vector<mpt::StubProcessFactory::ProcessInfo>&>(process_list));
}

std::unique_ptr<mp::test::StubProcessFactory::Scope> mp::test::StubProcessFactory::Inject()
{
    ProcessFactory::reset(); // cannot mock unless singleton reset
    ProcessFactory::mock<StubProcessFactory>();
    return std::make_unique<mp::test::StubProcessFactory::Scope>();
}

mpt::StubProcessFactory::Scope::~Scope()
{
    ProcessFactory::reset();
}

std::vector<mpt::StubProcessFactory::ProcessInfo> mpt::StubProcessFactory::Scope::process_list()
{
    return stub_instance().process_list;
}

mpt::StubProcessFactory& mpt::StubProcessFactory::stub_instance()
{
    return dynamic_cast<mpt::StubProcessFactory&>(instance());
}
