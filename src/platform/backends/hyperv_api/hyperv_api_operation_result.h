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

#ifndef MULTIPASS_HYPERV_API_OPERATION_RESULT_H
#define MULTIPASS_HYPERV_API_OPERATION_RESULT_H

#include <winerror.h>

#include <string>

#include <fmt/format.h>

namespace multipass::hyperv
{

/**
 * A simple HRESULT wrapper which is boolable for
 * convenience.
 */
struct ResultCode
{

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
        return !FAILED(result);
    }

    [[nodiscard]] explicit operator HRESULT() const noexcept
    {
        return result;
    }

    [[nodiscard]] explicit operator std::make_unsigned_t<HRESULT>() const noexcept
    {
        return static_cast<HRESULT>(result);
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
    const ResultCode code;

    /**
     * A message that describes the result of the operation.
     * It might contain an error message describing the error
     * when the operation fails, or details regarding the status
     * of a successful operation.
     */
    const std::wstring status_msg;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return static_cast<bool>(code);
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

    template <typename FormatContext>
    auto format(const multipass::hyperv::ResultCode& rc, FormatContext& ctx) const
    {
        return format_to(ctx.out(), "{0:#x}", static_cast<std::make_unsigned_t<HRESULT>>(rc));
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
        return format_to(ctx.out(), "{0:#x}", opr.code);
    }
};

#endif // MULTIPASS_HYPERV_API_OPERATION_RESULT_H
