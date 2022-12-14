/*
 * Copyright (C) 2019-2022 Canonical, Ltd.
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

#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/process/basic_process.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
// Want to call qRegisterMetaType just once
static auto reg = []() -> bool {
    qRegisterMetaType<multipass::ProcessState>();
    qRegisterMetaType<QProcess::ProcessError>();
    return true;
}();
} // namespace

mp::BasicProcess::CustomQProcess::CustomQProcess(BasicProcess* p) : QProcess{p}, p{p}
{
}

void mp::BasicProcess::CustomQProcess::setupChildProcess()
{
    p->setup_child_process();
}

mp::BasicProcess::BasicProcess(std::shared_ptr<mp::ProcessSpec> spec) : process_spec{spec}, process{this}
{
    connect(&process, &QProcess::started, this, &mp::BasicProcess::handle_started);
    connect(&process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            [this](int exit_code, QProcess::ExitStatus exit_status) {
                mp::ProcessState process_state;

                if (exit_status == QProcess::NormalExit)
                {
                    process_state.exit_code = exit_code;
                }
                else // crash
                {
                    process_state.error = mp::ProcessState::Error{process.error(), error_string()};
                }
                emit mp::Process::finished(process_state);
            });
    connect(&process, &QProcess::errorOccurred,
            [this](QProcess::ProcessError error) { emit mp::Process::error_occurred(error, error_string()); });
    connect(&process, &QProcess::readyReadStandardOutput, this, &mp::Process::ready_read_standard_output);
    connect(&process, &QProcess::readyReadStandardError, this, &mp::Process::ready_read_standard_error);
    connect(&process, &QProcess::stateChanged, this, &mp::Process::state_changed);

    process.setProgram(process_spec->program());
    process.setArguments(process_spec->arguments());
    process.setProcessEnvironment(process_spec->environment());
    if (!process_spec->working_directory().isNull())
        process.setWorkingDirectory(process_spec->working_directory());

    // TODO: multiline output produces poor formatting in logs, needs improving
    QObject::connect(&process, &QProcess::readyReadStandardError, [this]() {
        // Using readAllStandardError() removes it from buffer for Process consumers, so peek() instead.
        // This copies the implementation of QProcess::readAllStandardError() replacing the read with peek.
        auto original = process.readChannel();
        process.setReadChannel(QProcess::StandardError);
        QByteArray data = process.peek(process.bytesAvailable());
        process.setReadChannel(original);
        mpl::log(process_spec->error_log_level(), qUtf8Printable(process_spec->program()), qUtf8Printable(data));
    });
}

mp::BasicProcess::~BasicProcess() = default;

QString mp::BasicProcess::program() const
{
    return process.program();
}

QStringList mp::BasicProcess::arguments() const
{
    return process.arguments();
}

QString mp::BasicProcess::working_directory() const
{
    return process.workingDirectory();
}

QProcessEnvironment mp::BasicProcess::process_environment() const
{
    return process.processEnvironment();
}

qint64 mp::BasicProcess::process_id() const
{
    return pid;
}

void mp::BasicProcess::start()
{
    process.start();
}

void mp::BasicProcess::terminate()
{
    process.terminate();
}

void mp::BasicProcess::kill()
{
    process.kill();
}

bool mp::BasicProcess::wait_for_started(int msecs)
{
    return process.waitForStarted(msecs);
}

bool mp::BasicProcess::wait_for_finished(int msecs)
{
    return process.waitForFinished(msecs);
}

bool mp::BasicProcess::wait_for_ready_read(int msecs)
{
    return process.waitForReadyRead(msecs);
}

mp::ProcessState mp::BasicProcess::process_state() const
{
    mp::ProcessState state;

    if (process.error() != QProcess::ProcessError::UnknownError)
    {
        state.error = mp::ProcessState::Error{process.error(), error_string()};
    }
    else if (process.state() != QProcess::Running && process.exitStatus() == QProcess::NormalExit)
    {
        state.exit_code = process.exitCode();
    }

    return state;
}

QString mp::BasicProcess::error_string() const
{
    return QString{"program: %1; error: %2"}.arg(process_spec->program(), process.errorString());
}

bool mp::BasicProcess::running() const
{
    return process.state() == QProcess::Running;
}

QByteArray mp::BasicProcess::read_all_standard_output()
{
    return process.readAllStandardOutput();
}

QByteArray mp::BasicProcess::read_all_standard_error()
{
    return process.readAllStandardError();
}

qint64 mp::BasicProcess::write(const QByteArray& data)
{
    return process.write(data);
}

void mp::BasicProcess::close_write_channel()
{
    process.closeWriteChannel();
}

void mp::BasicProcess::set_process_channel_mode(QProcess::ProcessChannelMode mode)
{
    process.setProcessChannelMode(mode);
}

mp::ProcessState mp::BasicProcess::execute(const int timeout)
{
    mp::ProcessState exit_state;
    start();

    if (!process.waitForStarted(timeout) || !process.waitForFinished(timeout) ||
        process.exitStatus() != QProcess::NormalExit)
    {
        mpl::log(mpl::Level::error, qUtf8Printable(process_spec->program()), qUtf8Printable(process.errorString()));
        exit_state.error = mp::ProcessState::Error{process.error(), error_string()};
        return exit_state;
    }

    exit_state.exit_code = process.exitCode();
    return exit_state;
}

void mp::BasicProcess::setup_child_process()
{
}

void mp::BasicProcess::handle_started()
{
    pid = process.processId(); // save this, so we know it even after finished
    const auto& program = process_spec->program().toStdString();
    mpl::log(mpl::Level::debug, program,
             fmt::format("[{}] started: {} {}", pid, program, process_spec->arguments().join(' ')));

    emit mp::Process::started();
}
