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

#include "file_ops.h"
#include "xz_image_decoder.h"

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

    using Decoder = std::function<void(const QString&, const QString&)>;
    using DefaultDecoderT = XzImageDecoder;

    virtual QString copy_to_dir(const QString& file, const QDir& output_dir) const;
    [[nodiscard]] virtual QString compute_hash(
        QIODevice& device,
        const QCryptographicHash::Algorithm algo = QCryptographicHash::Sha256) const;
    [[nodiscard]] virtual QString compute_file_hash(
        const QString& file,
        const QCryptographicHash::Algorithm algo = QCryptographicHash::Sha256) const;

    virtual void verify_file_hash(const QString& file, const QString& hash) const;

    virtual QString extract_file(const QString& file,
                                 const Decoder& decoder,
                                 bool delete_original = false) const;

    template <class DecoderT = DefaultDecoderT>
    QString extract_file(const QString& file,
                         const ProgressMonitor& monitor,
                         bool delete_original = false,
                         const DecoderT& = DecoderT{}) const;

    using HostMap = std::unordered_map<std::string, VMImageHost*>;
    using Hosts = std::vector<VMImageHost*>;

    [[nodiscard]] virtual HostMap configure_image_host_map(const Hosts& image_hosts) const;
};

template <class DecoderT>
QString ImageVaultUtils::extract_file(const QString& file,
                                      const ProgressMonitor& monitor,
                                      bool delete_original,
                                      const DecoderT& decoder) const
{
    auto decoder_fn = [&monitor, &decoder](const QString& encoded_file,
                                           const QString& destination) {
        return decoder.decode_to(encoded_file, destination, monitor);
    };

    return extract_file(file, decoder_fn, delete_original);
}

} // namespace multipass
