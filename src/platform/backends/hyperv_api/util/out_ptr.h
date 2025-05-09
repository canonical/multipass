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

#ifndef MULTIPASS_HYPERV_API_UTIL_OUT_PTR_H
#define MULTIPASS_HYPERV_API_UTIL_OUT_PTR_H

#include <tuple>
#include <type_traits>

namespace multipass::hyperv::util
{

namespace detail
{
template <class, class, class...>
struct is_resettable : std::false_type
{
};

template <class S, class P, class... A>
struct is_resettable<S,
                     P,
                     std::void_t<decltype(std::declval<S&>().reset(std::declval<P>(), std::declval<A>()...))>,
                     A...> : std::true_type
{
};

template <class S, class P, class... A>
inline constexpr bool is_resettable_v = is_resettable<S, P, A...>::value;
} // namespace detail

// A "just-enough" implementation of out_ptr
// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1132r5.html
template <class Smart,
          class Pointer = typename Smart::pointer, // defaults to Smart::pointer
          class... Args>
class out_ptr_t
{
    Smart* sp_ = nullptr;       // the smart pointer we’re updating
    Pointer raw_ = nullptr;     // where the C API will write the new handle
    std::tuple<Args...> extra_; // optional args forwarded to reset()

public:
    // Construction just remembers where to put things
    out_ptr_t(Smart& s, Args&&... a) : sp_(std::addressof(s)), extra_(std::forward<Args>(a)...)
    {
    }

    // Non‑copyable, non‑movable – you get one shot per call site
    out_ptr_t(const out_ptr_t&) = delete;
    out_ptr_t& operator=(const out_ptr_t&) = delete;
    out_ptr_t(out_ptr_t&&) = delete;
    out_ptr_t& operator=(out_ptr_t&&) = delete;

    // The whole point: give the C API a place to stuff the pointer
    operator Pointer*() const noexcept
    {
        return std::addressof(const_cast<Pointer&>(raw_));
    }
    operator void**() const noexcept
    {
        return reinterpret_cast<void**>(std::addressof(raw_));
    }

    // Cleanup: push the freshly‑filled pointer back into the smart pointer
    ~out_ptr_t()
    {
        std::apply(
            [&](auto&&... xs) {
                if (sp_ && raw_)
                {
                    if constexpr (detail::is_resettable_v<Smart, Pointer, Args...>)
                    {
                        sp_->reset(raw_, std::forward<decltype(xs)>(xs)...);
                    }
                    else
                    {
                        *sp_ = Smart(raw_, std::forward<decltype(xs)>(xs)...);
                    }
                }
            },
            extra_);
    }
};

// Helper that lets you write std::out_ptr(up) with CTAD‑like deduction
template <class Smart, class... Args>
[[nodiscard]] auto out_ptr(Smart& s, Args&&... a)
{
    using Ptr = typename Smart::pointer;
    return out_ptr_t<Smart, Ptr, Args...>(s, std::forward<Args>(a)...);
}

} // namespace multipass::hyperv::util

#endif
