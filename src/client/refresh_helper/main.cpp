/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include <multipass/exceptions/snap_environment_exception.h>
#include <multipass/rpc/multipass.grpc.pb.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStringList>

#include <grpcpp/grpcpp.h>

#include <boost/json.hpp>

#include <fmt/ranges.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace mp = multipass;

namespace
{
// Tag of the internal qcow2 snapshot that holds a guest's memory while the
// instance is suspended. Keep in sync with
// multipass::backend::suspend_snapshot_tag (qemu_img_utils.h), which cannot be
// included here without dragging in the whole daemon-side platform.
constexpr auto suspend_snapshot_tag = "suspend";

// Exit codes consumed by the pre-refresh snap hook.
constexpr auto exit_ok = 0;        // prepared / no blockers
constexpr auto exit_error = 1;     // unexpected failure
constexpr auto exit_blocked = 2;   // suspended guest state blocks the refresh
constexpr auto exit_no_daemon = 3; // daemon not reachable (offline scan may apply)

constexpr auto instances_db_name = "multipassd-vm-instances.json";
constexpr auto image_records_db_name = "multipassd-instance-image-records.json";
constexpr auto default_prepare_timeout = std::chrono::seconds{480}; // < snapd's 10m hook budget

const auto blocked_instances_help =
    "Resume each instance ('multipass start <name>') and shut it down cleanly ('multipass stop "
    "<name>'), then retry the refresh. If an instance cannot be resumed, start it anyway and "
    "discard its stale suspended state explicitly ('multipass start <name>' followed by "
    "'multipass stop --force <name>'; note that --force only works on a started instance).";

std::string snap_common_dir()
{
    const auto dir = qgetenv("SNAP_COMMON");
    return dir.isEmpty() ? std::string{} : std::string{dir.constData(), std::size_t(dir.size())};
}

std::string default_data_dir()
{
    const auto base = snap_common_dir();
    if (base.empty())
        return {};

    return (std::filesystem::path{base} / "data" / "multipassd").generic_string();
}

std::string refresh_server_address()
{
    const auto address = qgetenv("MULTIPASS_REFRESH_SERVER_ADDRESS");
    if (!address.isEmpty())
        return std::string{address.constData(), std::size_t(address.size())};

    const auto base = snap_common_dir();
    return fmt::format("unix:{}/multipass_refresh_socket",
                       base.empty() ? std::string{"/run"} : base);
}

std::string qemu_img_path()
{
    return (std::filesystem::path{QCoreApplication::applicationDirPath().toStdString()} /
            "qemu-img")
        .generic_string();
}

enum class SnapshotCheck
{
    NoSnapshot,
    HasSnapshot,
    Failed
};

SnapshotCheck image_has_suspend_snapshot(const std::string& qemu_img, const std::string& image_path)
{
    QProcess process;
    process.setProgram(QString::fromStdString(qemu_img));
    process.setArguments(
        {QStringLiteral("snapshot"), QStringLiteral("-l"), QString::fromStdString(image_path)});

    process.start();
    if (!process.waitForFinished(30000) || process.exitStatus() != QProcess::NormalExit ||
        process.exitCode() != 0)
    {
        std::cerr << fmt::format("could not inspect the disk image {}: {}\n",
                                 image_path,
                                 QString::fromUtf8(process.readAllStandardError()).toStdString());
        return SnapshotCheck::Failed;
    }

    // qemu-img snapshot -l output: the snapshot tag is the second column of
    // each list entry.
    const auto lines = QString::fromUtf8(process.readAllStandardOutput())
                           .split(QChar{'\n'}, Qt::SkipEmptyParts);
    for (const auto& line : lines)
    {
        const auto columns = line.simplified().split(QChar{' '});
        if (columns.size() >= 2 && columns[1].toStdString() == suspend_snapshot_tag)
            return SnapshotCheck::HasSnapshot;
    }

    return SnapshotCheck::NoSnapshot;
}

// Returns nullopt when the file exists but cannot be read or parsed (the
// caller must then fail closed); an absent file yields an empty map.
std::optional<std::unordered_set<std::string>> instance_names_from_db(const QString& db_path)
{
    QFile file{db_path};
    if (!file.exists())
        return std::unordered_set<std::string>{};

    if (!file.open(QIODevice::ReadOnly))
    {
        std::cerr << fmt::format("could not read the instance database {}: {}\n",
                                 db_path.toStdString(),
                                 file.errorString().toStdString());
        return std::nullopt;
    }

    std::unordered_set<std::string> names;
    try
    {
        const auto bytes = file.readAll();
        const auto json = boost::json::parse(
            std::string_view{bytes.constData(), std::size_t(bytes.size())});

        for (const auto& [name, value] : json.as_object())
            if (!value.at("deleted").as_bool())
                names.insert(name);
    }
    catch (const std::exception& e)
    {
        std::cerr << fmt::format("could not parse the instance database {}: {}\n",
                                 db_path.toStdString(),
                                 e.what());
        return std::nullopt;
    }

    return names;
}

// See instance_names_from_db for the error convention.
std::optional<std::unordered_map<std::string, std::string>> image_paths_from_db(
    const QString& db_path)
{
    QFile file{db_path};
    if (!file.exists())
        return std::unordered_map<std::string, std::string>{};

    if (!file.open(QIODevice::ReadOnly))
    {
        std::cerr << fmt::format("could not read the image record database {}: {}\n",
                                 db_path.toStdString(),
                                 file.errorString().toStdString());
        return std::nullopt;
    }

    std::unordered_map<std::string, std::string> image_paths;
    try
    {
        const auto bytes = file.readAll();
        const auto json = boost::json::parse(
            std::string_view{bytes.constData(), std::size_t(bytes.size())});

        for (const auto& [name, value] : json.as_object())
            image_paths.emplace(name, value.at("image").at("path").as_string().c_str());
    }
    catch (const std::exception& e)
    {
        std::cerr << fmt::format("could not parse the image record database {}: {}\n",
                                 db_path.toStdString(),
                                 e.what());
        return std::nullopt;
    }

    return image_paths;
}

// Finds instances that hold a "suspend" qcow2 snapshot: their next start would
// resume guest memory that may not be compatible with a new QEMU build.
// Returns false if the scan could not be completed (fail safe: the caller
// must then refuse the refresh).
bool offline_suspended_instances(const std::string& data_dir, std::vector<std::string>& suspended)
{
    QDir dir{QString::fromStdString(data_dir)};
    const auto names = instance_names_from_db(dir.filePath(instances_db_name));
    if (!names)
        return false; // cannot verify the instances: fail closed

    // The vault nests its record DB under the "vault" subdirectory (see
    // DefaultVMImageVault, whose data_dir is data_dir_path / "vault").
    const auto image_paths = image_paths_from_db(
        QDir(dir.filePath("vault")).filePath(image_records_db_name));
    if (!image_paths)
        return false; // cannot verify the images: fail closed

    const auto qemu_img = qemu_img_path();
    if (names->empty())
        return true;

    if (!QFile::exists(QString::fromStdString(qemu_img)))
    {
        std::cerr << fmt::format("cannot verify instance images: {} is missing\n", qemu_img);
        return false;
    }

    for (const auto& name : *names)
    {
        const auto image_it = image_paths->find(name);
        if (image_it == image_paths->cend())
        {
            // An instance without an image record cannot be verified; refuse
            // the refresh rather than assume it is safe.
            std::cerr << fmt::format("cannot verify the disk image of \"{}\": no image record\n",
                                     name);
            return false;
        }

        const auto check = image_has_suspend_snapshot(qemu_img, image_it->second);
        if (check == SnapshotCheck::Failed)
            return false; // inspection failed: refuse the refresh (fail safe)

        if (check == SnapshotCheck::HasSnapshot)
            suspended.push_back(name);
    }

    return true;
}

std::string join_names(const google::protobuf::RepeatedPtrField<std::string>& names)
{
    std::vector<std::string> as_vector;
    as_vector.reserve(names.size());
    for (const auto& name : names)
        as_vector.push_back(name);

    return fmt::format("{}", fmt::join(as_vector, ", "));
}

int prepare(const std::chrono::seconds timeout)
{
    mp::PingRequest probe_request;
    mp::PingReply probe_reply;

    const auto channel = grpc::CreateChannel(refresh_server_address(),
                                             grpc::InsecureChannelCredentials());

    // The daemon may be mid-startup or mid-shutdown; give it a moment before
    // concluding it is not there. A served channel answers the unsupported
    // ping with UNIMPLEMENTED; UNAVAILABLE means nobody is listening.
    auto daemon_up = false;
    for (int attempt = 0; attempt < 3 && !daemon_up; ++attempt)
    {
        grpc::ClientContext probe_context;
        probe_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(2));
        const auto probe_status = mp::Rpc::NewStub(channel)->ping(&probe_context,
                                                                  probe_request,
                                                                  &probe_reply);
        if (probe_status.error_code() != grpc::StatusCode::UNAVAILABLE)
            daemon_up = true;
        else if (attempt < 2)
            std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (!daemon_up)
    {
        std::cerr << "multipass daemon is not reachable\n";
        return exit_no_daemon;
    }

    grpc::ClientContext prepare_context;
    prepare_context.set_deadline(std::chrono::system_clock::now() + timeout);

    mp::PrepareRefreshRequest request;
    mp::PrepareRefreshReply reply;
    auto stream = mp::RefreshRpc::NewStub(channel)->prepare_refresh(&prepare_context);

    if (!stream->Write(request) || !stream->WritesDone())
    {
        std::cerr << fmt::format("could not send the refresh preparation request: {}\n",
                                 prepare_context.debug_error_string());
        return exit_error;
    }

    // Replies carrying a log line are streamed daemon logs; the data reply
    // carries an empty log line.
    auto got_data_reply = false;
    while (stream->Read(&reply))
    {
        if (!reply.log_line().empty())
        {
            std::cerr << reply.log_line();
            continue;
        }

        got_data_reply = true;
        break;
    }

    if (!got_data_reply)
    {
        const auto status = stream->Finish();
        std::cerr << fmt::format("refresh preparation failed: {}\n",
                                 status.ok() ? "no reply" : status.error_message());
        return exit_error;
    }

    if (reply.suspended_instances_size() > 0)
    {
        std::cerr << fmt::format(
            "Cannot refresh multipass: suspended guest memory state found for instance(s): {}\n"
            "{}\n",
            join_names(reply.suspended_instances()),
            blocked_instances_help);
        return exit_blocked;
    }

    if (!reply.error_message().empty())
    {
        std::cerr << fmt::format("Cannot refresh multipass: {}\n", reply.error_message());
        return exit_blocked;
    }

    if (reply.stopped_instances_size() > 0)
        std::cout << fmt::format("Prepared the snap refresh; instance(s) {} were stopped and "
                                 "will be started again by the refreshed daemon\n",
                                 join_names(reply.stopped_instances()));
    else
        std::cout << "Prepared the snap refresh; no instances had to be stopped\n";

    return exit_ok;
}

int abort_refresh()
{
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + default_prepare_timeout);

    const auto channel = grpc::CreateChannel(refresh_server_address(),
                                             grpc::InsecureChannelCredentials());
    mp::AbortRefreshRequest request;
    mp::AbortRefreshReply reply;
    auto stream = mp::RefreshRpc::NewStub(channel)->abort_refresh(&context);

    if (!stream->Write(request) || !stream->WritesDone())
    {
        std::cerr << fmt::format("could not send the refresh abort request: {}\n",
                                 context.debug_error_string());
        return exit_error;
    }

    // Drain the stream, surfacing streamed daemon logs.
    while (stream->Read(&reply))
    {
        if (!reply.log_line().empty())
            std::cerr << reply.log_line();
    }

    const auto status = stream->Finish();
    if (!status.ok())
    {
        std::cerr << fmt::format("refresh abort failed: {}\n", status.error_message());
        return exit_error;
    }

    return exit_ok;
}

int scan_offline(const std::optional<std::string>& data_dir_override)
{
    const auto data_dir = data_dir_override ? *data_dir_override : default_data_dir();
    if (data_dir.empty())
    {
        std::cerr << "no multipass data directory available\n";
        return exit_error;
    }

    std::vector<std::string> suspended;
    if (!offline_suspended_instances(data_dir, suspended))
        return exit_error; // could not verify: fail safe

    if (suspended.empty())
        return exit_ok;

    std::cerr << fmt::format(
        "Cannot refresh multipass: suspended guest memory state found for instance(s): {}\n{}\n",
        fmt::join(suspended, ", "),
        blocked_instances_help);
    return exit_blocked;
}

int print_usage()
{
    std::cerr << "usage: multipass-refresh-helper prepare [--timeout <seconds>]\n"
                 "       multipass-refresh-helper abort\n"
                 "       multipass-refresh-helper scan-offline [--data-dir <dir>]\n";
    return exit_error;
}
} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv); // for applicationDirPath-based qemu-img lookup

    const std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty())
        return print_usage();

    if (args.front() == "prepare")
    {
        auto timeout = default_prepare_timeout;
        for (auto it = std::next(args.cbegin()); it != args.cend(); ++it)
            if (*it == "--timeout" && std::next(it) != args.cend())
                timeout = std::chrono::seconds(std::stoi(*++it));

        return prepare(timeout);
    }

    if (args.front() == "abort")
        return abort_refresh();

    if (args.front() == "scan-offline")
    {
        std::optional<std::string> data_dir;
        for (auto it = std::next(args.cbegin()); it != args.cend(); ++it)
            if (*it == "--data-dir" && std::next(it) != args.cend())
                data_dir = *++it;

        return scan_offline(data_dir);
    }

    return print_usage();
}
