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

#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/xz_image_decoder.h>

#include <fmt/format.h>

#include <fstream>
#include <vector>

namespace mp = multipass;

namespace
{
bool verify_decode(const xz_ret& ret)
{
    switch (ret)
    {
    case XZ_OK:
        break;
    case XZ_STREAM_END:
        return false;
    case XZ_MEM_ERROR:
        throw std::runtime_error("xz decoder memory allocation failed");
    case XZ_MEMLIMIT_ERROR:
        throw std::runtime_error("xz decoder memory usage limit reached");
    case XZ_FORMAT_ERROR:
        throw std::runtime_error("not a xz file");
    case XZ_OPTIONS_ERROR:
        throw std::runtime_error("unsupported options in the xz headers");
    case XZ_DATA_ERROR:
    case XZ_BUF_ERROR:
        throw std::runtime_error("xz file is corrupt");
    default:
        throw std::runtime_error("xz decoder unhandled error condition");
    }

    return true;
}
} // namespace

mp::XzImageDecoder::XzImageDecoder() : xz_decoder{xz_dec_init(XZ_DYNALLOC, 1u << 26), xz_dec_end}
{
    xz_crc32_init();
    xz_crc64_init();
}

void mp::XzImageDecoder::decode_to(const std::filesystem::path& xz_file_path,
                                   const std::filesystem::path& decoded_image_path,
                                   const ProgressMonitor& monitor) const
{
    std::ifstream xz_file{xz_file_path, std::ios::binary | std::ios::in};
    if (!xz_file.is_open())
        throw std::runtime_error{
            fmt::format("failed to open {} for reading", xz_file_path.string())};

    std::ofstream decoded_file{decoded_image_path, std::ios::binary | std::ios::out};
    if (!decoded_file.is_open())
        throw std::runtime_error{
            fmt::format("failed to open {} for writing", decoded_image_path.string())};

    struct xz_buf decode_buf
    {
    };
    const auto max_size = 65536u;

    std::vector<char> read_data, write_data;
    read_data.reserve(max_size);
    write_data.reserve(max_size);

    decode_buf.in = reinterpret_cast<unsigned char*>(read_data.data());
    decode_buf.in_pos = 0;
    decode_buf.in_size = 0;
    decode_buf.out = reinterpret_cast<unsigned char*>(write_data.data());
    decode_buf.out_pos = 0;
    decode_buf.out_size = max_size;

    const auto file_size = std::filesystem::file_size(xz_file_path);
    std::int64_t total_bytes_extracted{0};

    auto last_progress = -1;
    while (true)
    {
        if (decode_buf.in_pos == decode_buf.in_size)
        {
            xz_file.read(read_data.data(), max_size);
            decode_buf.in_size = xz_file.gcount();
            decode_buf.in_pos = 0;
            total_bytes_extracted += decode_buf.in_size;
            auto progress = (total_bytes_extracted / (float)file_size) * 100;
            if (last_progress != progress)
                monitor(LaunchProgress::EXTRACT, progress);
            last_progress = progress;
        }

        if (!verify_decode(xz_dec_run(xz_decoder.get(), &decode_buf)))
        {
            decoded_file.write(write_data.data(), decode_buf.out_pos);
            return;
        }

        if (decode_buf.out_pos == max_size)
        {
            decoded_file.write(write_data.data(), decode_buf.out_pos);
            decode_buf.out_pos = 0;
        }
    }
}
