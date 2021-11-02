/*
 * Copyright (C) 2019-2021 Canonical, Ltd.
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

#include <multipass/cli/client_common.h>
#include <multipass/constants.h>
#include <multipass/exceptions/autostart_setup_exception.h>
#include <multipass/logging/log.h>
#include <multipass/logging/standard_logger.h>
#include <multipass/persistent_settings_handler.h>
#include <multipass/platform.h>
#include <multipass/settings.h>
#include <multipass/standard_paths.h>
#include <multipass/utils.h>

#include <fmt/ostream.h>

#include <QKeySequence>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
const auto file_extension = QStringLiteral("conf");
const auto daemon_root = QStringLiteral("local");
const auto client_root = QStringLiteral("client");
const auto petenv_name = QStringLiteral("primary");
const auto autostart_default = QStringLiteral("true");

QString persistent_settings_filename()
{
    static const auto file_pattern = QStringLiteral("%2.%1").arg(file_extension); // note the order
    static const auto user_config_path = QDir{MP_STDPATHS.writableLocation(mp::StandardPaths::GenericConfigLocation)};
    static const auto dir_path = QDir{user_config_path.absoluteFilePath(mp::client_name)};
    static const auto path = dir_path.absoluteFilePath(file_pattern.arg(mp::client_name));

    return path;
}

QString daemon_settings_filename()
{              // TODO@ricab remove - tmp code, to keep feature parity until we introduce routing handlers
    return ""; // TODO@ricab implement this
}

QString default_hotkey()
{
    return QKeySequence{mp::hotkey_default}.toString(QKeySequence::NativeText); // outcome depends on platform
}

mp::ReturnCode return_code_for(const grpc::StatusCode& code)
{
    return code == grpc::StatusCode::UNAVAILABLE ? mp::ReturnCode::DaemonFail : mp::ReturnCode::CommandFail;
}

std::string message_box(const std::string& message)
{
    std::string::size_type divider_length = 50;
    {
        std::istringstream m(message);
        std::string s;
        while (getline(m, s, '\n'))
        {
            divider_length = std::max(divider_length, s.length());
        }
    }

    const auto divider = std::string(divider_length, '#');

    return '\n' + divider + '\n' + message + '\n' + divider + '\n';
}

} // namespace

mp::ReturnCode mp::cmd::standard_failure_handler_for(const std::string& command, std::ostream& cerr,
                                                     const grpc::Status& status, const std::string& error_details)
{
    fmt::print(cerr, "{} failed: {}\n{}", command, status.error_message(),
               !error_details.empty()
                   ? fmt::format("{}\n", error_details)
                   : !status.error_details().empty() ? fmt::format("{}\n", status.error_details()) : "");

    return return_code_for(status.error_code());
}

bool mp::cmd::update_available(const mp::UpdateInfo& update_info)
{
    return update_info.version() != "";
}

std::string mp::cmd::update_notice(const mp::UpdateInfo& update_info)
{
    return ::message_box(fmt::format("{}\n{}\n\nGo here for more information: {}", update_info.title(),
                                     update_info.description(), update_info.url()));
}

void mp::client::register_settings_handlers()
{
    auto setting_defaults = std::map<QString, QString>{
        {mp::petenv_key, petenv_name}, {mp::autostart_key, autostart_default}, {mp::hotkey_key, default_hotkey()}};

    for (const auto& [k, v] : platform::extra_settings_defaults())
        if (k.startsWith(client_root))
            setting_defaults.insert_or_assign(k, v);

    MP_SETTINGS.register_handler(
        std::make_unique<PersistentSettingsHandler>(persistent_settings_filename(), std::move(setting_defaults)));

    { // TODO@ricab remove from client - temporary code, to keep feature parity until we introduce routing handlers
        auto daemon_defaults = std::map<QString, QString>{{mp::driver_key, mp::platform::default_driver()},
                                                          {mp::bridged_interface_key, ""},
                                                          {mp::mounts_key, mp::platform::default_privileged_mounts()}};

        for (const auto& [k, v] : platform::extra_settings_defaults())
            if (k.startsWith(daemon_root))
                daemon_defaults.insert_or_assign(k, v);

        MP_SETTINGS.register_handler(
            std::make_unique<PersistentSettingsHandler>(daemon_settings_filename(), std::move(daemon_defaults)));
    }
}

std::shared_ptr<grpc::Channel> mp::client::make_channel(const std::string& server_address,
                                                        mp::RpcConnectionType conn_type,
                                                        mp::CertProvider& cert_provider)
{
    std::shared_ptr<grpc::ChannelCredentials> creds;
    if (conn_type == mp::RpcConnectionType::ssl)
    {
        auto opts = grpc::SslCredentialsOptions();
        opts.server_certificate_request = GRPC_SSL_REQUEST_SERVER_CERTIFICATE_BUT_DONT_VERIFY;
        opts.pem_cert_chain = cert_provider.PEM_certificate();
        opts.pem_private_key = cert_provider.PEM_signing_key();
        creds = grpc::SslCredentials(opts);
    }
    else if (conn_type == mp::RpcConnectionType::insecure)
    {
        creds = grpc::InsecureChannelCredentials();
    }
    else
    {
        throw std::runtime_error("Unknown connection type");
    }
    return grpc::CreateChannel(server_address, creds);
}

std::string mp::client::get_server_address()
{
    const auto address = qgetenv("MULTIPASS_SERVER_ADDRESS").toStdString();
    if (!address.empty())
    {
        mp::utils::validate_server_address(address);
        return address;
    }

    return mp::platform::default_server_address();
}

std::unique_ptr<mp::SSLCertProvider> mp::client::get_cert_provider()
{
    auto data_dir = MP_STDPATHS.writableLocation(StandardPaths::AppDataLocation);
    auto client_cert_dir = mp::utils::make_dir(data_dir, "client-certificate");
    return std::make_unique<mp::SSLCertProvider>(client_cert_dir);
}

void mp::client::set_logger()
{
    set_logger(mpl::Level::info);
}

void mp::client::set_logger(mpl::Level verbosity)
{
    mpl::set_logger(std::make_shared<mpl::StandardLogger>(verbosity));
}

void mp::client::pre_setup()
{
    try
    {
        platform::setup_gui_autostart_prerequisites();
    }
    catch (AutostartSetupException& e)
    {
        mpl::log(mpl::Level::error, "client", fmt::format("Failed to set up autostart prerequisites: {}", e.what()));
        mpl::log(mpl::Level::debug, "client", e.get_detail());
    }
}

void mp::client::post_setup()
{
    platform::sync_winterm_profiles();
}
