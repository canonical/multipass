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

#include <multipass/file_ops.h>
#include <multipass/format.h>
#include <multipass/image_host/vm_image_host.h>
#include <multipass/utils.h>
#include <multipass/vm_image_vault.h>
#include <multipass/vm_image_vault_utils.h>
#include <multipass/xz_image_decoder.h>

#include <QByteArray>
#include <QFileInfo>

#include <fmt/std.h>

#include <stdexcept>

namespace mp = multipass;

mp::ImageVaultUtils::ImageVaultUtils(const PrivatePass& pass) noexcept : Singleton{pass}
{
}

std::filesystem::path mp::ImageVaultUtils::copy_to_dir(const std::filesystem::path& file,
                                                       const QDir& output_dir) const
{
    if (file.empty())
        return "";

    if (!MP_FILEOPS.exists(file))
        throw std::runtime_error(fmt::format("File {} not found", file));

    auto new_location = output_dir.filesystemPath() / file.filename();

    std::error_code ec{};
    std::filesystem::copy_options options{};
    MP_FILEOPS.copy(file, new_location, options, ec);
    if (ec)
        throw std::runtime_error(fmt::format("Failed to copy {} to {}", file, new_location));

    return new_location;
}

std::string mp::ImageVaultUtils::compute_hash(std::istream& stream,
                                              const QCryptographicHash::Algorithm algo) const
{
    QCryptographicHash hash{algo};

    constexpr std::size_t buf_size = 8192;
    char buf[buf_size] = {0};
    do
    {
        stream.read(buf, buf_size);
        if (stream.bad())
            throw std::runtime_error("Failed to read data from device to hash");
        if (auto count = stream.gcount(); count > 0)
            hash.addData(QByteArrayView{buf, static_cast<qsizetype>(count)});
    } while (stream);

    return hash.result().toHex().toStdString();
}

std::string mp::ImageVaultUtils::compute_file_hash(const std::filesystem::path& path,
                                                   const QCryptographicHash::Algorithm algo) const
{
    std::fstream file;
    MP_FILEOPS.open(file, path.c_str(), std::ios::in | std::ios::binary);
    if (!file)
        throw std::runtime_error(fmt::format("Failed to open {}", path));

    return compute_hash(file, algo);
}

void mp::ImageVaultUtils::verify_file_hash(const std::filesystem::path& file,
                                           const std::string& hash) const
{
    const std::string sha512_prefix = "sha512:";
    std::string hash_to_check = hash;
    QCryptographicHash::Algorithm algo = QCryptographicHash::Sha256;

    if (utils::istarts_with(hash, sha512_prefix))
    {
        hash_to_check = hash.substr(sha512_prefix.length());
        algo = QCryptographicHash::Sha512;
    }

    const auto file_hash = compute_file_hash(file, algo);

    if (!utils::iequals(file_hash, hash_to_check))
    {
        throw std::runtime_error(fmt::format("Hash of {} does not match (expected {} but got {})",
                                             file,
                                             hash_to_check,
                                             file_hash));
    }
}

std::filesystem::path mp::ImageVaultUtils::extract_file(const std::filesystem::path& file,
                                                        const Decoder& decoder,
                                                        bool delete_original) const
{
    const auto new_path = MP_FILEOPS.remove_extension(file);
    decoder(file, new_path);

    if (delete_original)
    {
        std::error_code ec{};
        MP_FILEOPS.remove(file, ec);
    }

    return new_path;
}

mp::ImageVaultUtils::HostMap mp::ImageVaultUtils::configure_image_host_map(
    const Hosts& image_hosts) const
{
    HostMap remote_image_host_map{};
    for (const auto& image_host : image_hosts)
    {
        for (const auto& remote : image_host->supported_remotes())
        {
            remote_image_host_map[remote] = image_host;
        }
    }

    return remote_image_host_map;
}
