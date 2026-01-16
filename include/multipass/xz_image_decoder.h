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

#include <multipass/progress_monitor.h>

#include <memory>

#include <xz.h>

namespace multipass
{
class XzImageDecoder
{
public:
    XzImageDecoder();

    void decode_to(const std::filesystem::path& xz_file_path,
                   const std::filesystem::path& decoded_file_path,
                   const ProgressMonitor& monitor) const;

    using XzDecoderUPtr = std::unique_ptr<xz_dec, decltype(xz_dec_end)*>;

private:
    XzDecoderUPtr xz_decoder;
};
} // namespace multipass
