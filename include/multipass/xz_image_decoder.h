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

#ifndef MULTIPASS_XZ_IMAGE_DECODER_H
#define MULTIPASS_XZ_IMAGE_DECODER_H

#include <multipass/path.h>
#include <multipass/progress_monitor.h>

#include <memory>

#include <QFile>

#include <xz.h>

namespace multipass
{
class XzImageDecoder
{
public:
    XzImageDecoder(const Path& xz_file_path);

    void decode_to(const Path& decoded_file_path, const ProgressMonitor& monitor);

    using XzDecoderUPtr = std::unique_ptr<xz_dec, decltype(xz_dec_end)*>;

private:
    QFile xz_file;
    XzDecoderUPtr xz_decoder;
};
} // namespace multipass
#endif // MULTIPASS_XZ_IMAGE_DECODER_H
