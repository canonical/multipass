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

#include <string>
#include <winerror.h>

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

    [[nodiscard]] operator bool() const noexcept
    {
        return !FAILED(result);
    }

    [[nodiscard]] explicit operator HRESULT() const noexcept
    {
        return result;
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

    operator bool() const noexcept
    {
        return static_cast<bool>(code);
    }
};
} // namespace multipass::hyperv

#endif
