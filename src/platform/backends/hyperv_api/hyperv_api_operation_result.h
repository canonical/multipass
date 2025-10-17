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

#include <winerror.h>

#include <string>

#include <fmt/format.h>
#include <fmt/std.h>

namespace multipass::hyperv
{

/**
 * A simple HRESULT wrapper which is boolable for
 * convenience.
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

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return result == ERROR_SUCCESS;
    }

    [[nodiscard]] explicit operator HRESULT() const noexcept
    {
        return result;
    }

    [[nodiscard]] explicit operator unsigned_hresult_t() const noexcept
    {
        return static_cast<unsigned_hresult_t>(result);
    }

    [[nodiscard]] operator std::error_code() const noexcept
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
     * Status code of the operation. Evaluates to
     * true and greater or equal to 0 on success.
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
        return static_cast<bool>(code);
    }

    [[nodiscard]] operator std::error_code() const noexcept
    {
        return code;
    }
};
} // namespace multipass::hyperv

/**
 * Formatter type specialization for ResultCode
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::ResultCode, Char>
{
    constexpr auto parse(basic_format_parse_context<Char>& ctx)
    {
        return ctx.begin();
    }

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
{
    constexpr auto parse(basic_format_parse_context<Char>& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const multipass::hyperv::OperationResult& opr, FormatContext& ctx) const
    {
        return format_to(ctx.out(), "{}", opr.code);
    }
};
