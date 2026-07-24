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

#include <hyperv_api/hyperv_api_string_conversion.h>

#include <winerror.h>

#include <string>

#include <fmt/std.h>
#include <fmt/xchar.h>

namespace multipass::hyperv
{

/**
 * A simple HRESULT wrapper for convenience.
 */
struct ResultCode
{
    using unsigned_hresult_t = std::make_unsigned_t<HRESULT>;

    ResultCode(HRESULT r) noexcept : result(r)
    {
    }

    ResultCode& operator=(HRESULT r) noexcept
    {
        result = r;
        return *this;
    }

    bool success() const noexcept
    {
        return static_cast<HRESULT>(*this) == S_OK;
    }

    [[nodiscard]] explicit operator HRESULT() const noexcept
    {
        return result;
    }

    [[nodiscard]] explicit operator unsigned_hresult_t() const noexcept
    {
        return static_cast<unsigned_hresult_t>(result);
    }

    [[nodiscard]] explicit operator std::error_code() const noexcept
    {
        return std::error_code{result, std::system_category()};
    }

private:
    HRESULT result{};
};

/**
 * An object that describes the result of an HCN operation
 * performed through HCNWrapper.
 */
struct OperationResult
{

    /**
     * Status code of the operation. Equal to S_OK on success.
     */
    ResultCode code;

    /**
     * A message that describes the result of the operation.
     * It might contain an error message describing the error
     * when the operation fails, or details regarding the status
     * of a successful operation.
     */
    std::wstring status_msg;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return static_cast<HRESULT>(code) == S_OK;
    }

    [[nodiscard]] operator std::error_code() const noexcept
    {
        return static_cast<std::error_code>(code);
    }

    /**
     * Make a OperationResult from WIN32 system errpr code
     *
     * @param win32_system_errc System error code
     * @param status_msg Message
     */
    static OperationResult from_win32(uint32_t win32_result_code, std::wstring status_msg)
    {
        const auto win32_as_hresult = HRESULT_FROM_WIN32(win32_result_code);
        const std::error_code ec{static_cast<HRESULT>(win32_as_hresult), std::system_category()};
        const std::wstring msg_as_wstring = to_wstring(ec.message());
        // https://learn.microsoft.com/en-us/windows/win32/api/winerror/nf-winerror-hresult_from_win32
        return OperationResult(
            win32_as_hresult,
            fmt::format(L"{} : {} ({:#x})",
                        status_msg,
                        msg_as_wstring,
                        static_cast<std::make_unsigned_t<HRESULT>>(win32_as_hresult)));
    }

    static OperationResult success()
    {
        return OperationResult{.code = NOERROR, .status_msg = L""};
    }

    static OperationResult failure(std::wstring reason)
    {
        return OperationResult{.code = E_FAIL, .status_msg = reason};
    }
};
} // namespace multipass::hyperv

/**
 * Formatter type specialization for ResultCode
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::ResultCode, Char>
    : formatter<basic_string_view<Char>, Char>
{

    std::string hint(const multipass::hyperv::ResultCode& rc) const
    {
        switch (static_cast<std::make_unsigned_t<HRESULT>>(rc))
        {
        // HCN: There are no more endpoints available from the endpoint mapper.
        case 0x800706d9:
        // HCS: The operation could not be started because a required feature is not installed
        case 0x80370114:
            return {"(Hint: Did you enable the `Virtual Machine Platform` feature?)"};
        }
        return "";
    }

    template <typename FormatContext>
    auto format(const multipass::hyperv::ResultCode& rc, FormatContext& ctx) const
    {
        const std::error_code ec{static_cast<HRESULT>(rc), std::system_category()};
        const auto hint_r = hint(rc);
        return format_to(ctx.out(),
                         "{:#x}: {}{}",
                         static_cast<std::make_unsigned_t<HRESULT>>(rc),
                         ec.message(),
                         hint_r);
    }
};

/**
 * Formatter type specialization for ResultCode
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::OperationResult, Char>
    : formatter<basic_string_view<Char>, Char>
{

    template <typename FormatContext>
    auto format(const multipass::hyperv::OperationResult& opr, FormatContext& ctx) const
    {
        return format_to(ctx.out(), "{}", opr.code);
    }
};
