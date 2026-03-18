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

#pragma once

#include "xz_image_decoder.h"

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <QCryptographicHash>

#define MP_IMAGE_VAULT_UTILS multipass::ImageVaultUtils::instance()

namespace multipass
{
class VMImageHost;

class ImageVaultUtils : public Singleton<ImageVaultUtils>
{
public:
    ImageVaultUtils(const PrivatePass&) noexcept;

    using Decoder = std::function<void(const std::filesystem::path&, const std::filesystem::path&)>;
    using DefaultDecoderT = XzImageDecoder;

    virtual std::filesystem::path copy_to_dir(const std::filesystem::path& file,
                                              const QDir& output_dir) const;
    [[nodiscard]] virtual std::string compute_hash(
        std::istream& stream,
        const QCryptographicHash::Algorithm algo = QCryptographicHash::Sha256) const;
    [[nodiscard]] virtual std::string compute_file_hash(
        const std::filesystem::path& file,
        const QCryptographicHash::Algorithm algo = QCryptographicHash::Sha256) const;

    virtual void verify_file_hash(const std::filesystem::path& file, const std::string& hash) const;

    virtual std::filesystem::path extract_file(const std::filesystem::path& file,
                                               const Decoder& decoder,
                                               bool delete_original = false) const;

    template <class DecoderT = DefaultDecoderT>
    std::filesystem::path extract_file(const std::filesystem::path& file,
                                       const ProgressMonitor& monitor,
                                       bool delete_original = false,
                                       const DecoderT& = DecoderT{}) const;

    using HostMap = std::unordered_map<std::string, VMImageHost*>;
    using Hosts = std::vector<VMImageHost*>;

    [[nodiscard]] virtual HostMap configure_image_host_map(const Hosts& image_hosts) const;
};

template <class DecoderT>
std::filesystem::path ImageVaultUtils::extract_file(const std::filesystem::path& file,
                                                    const ProgressMonitor& monitor,
                                                    bool delete_original,
                                                    const DecoderT& decoder) const
{
    auto decoder_fn = [&monitor, &decoder](const std::filesystem::path& encoded_file,
                                           const std::filesystem::path& destination) {
        return decoder.decode_to(encoded_file, destination, monitor);
    };

    return extract_file(file, decoder_fn, delete_original);
}

} // namespace multipass
