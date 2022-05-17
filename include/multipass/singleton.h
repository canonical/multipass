/*
 * Copyright (C) 2019-2022 Canonical, Ltd.
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

#ifndef MULTIPASS_SINGLETON_H
#define MULTIPASS_SINGLETON_H

#include "disabled_copy_move.h"
#include "private_pass_provider.h"

#include <memory>
#include <mutex>
#include <type_traits>

namespace multipass
{

// A mockable singleton. To use, inherit (publicly) using the CRTP idiom
template <typename T>
class Singleton : public PrivatePassProvider<Singleton<T>>, private DisabledCopyMove
{
public:
    using Base = PrivatePassProvider<Singleton<T>>;
    using PrivatePass = typename Base::PrivatePass;

    constexpr Singleton(const PrivatePass&) noexcept;
    virtual ~Singleton() noexcept = default;
    template <typename... Args>
    static T& instance(Args&&... args) noexcept(noexcept(T(Base::pass, args...)));

protected:
    template <typename U, typename... Args, typename = std::enable_if_t<std::is_base_of<T, U>::value>>
    static void mock(Args&&... args) noexcept(
        noexcept(U(Base::pass, args...))); // only works if instance not called yet, or after reset
    static void reset() noexcept; // not thread-safe, make sure no other threads using this singleton anymore!

private:
    template <typename U, typename... Args>
    static void init(Args&&... args) noexcept(noexcept(U(Base::pass, args...)));

    static std::unique_ptr<std::once_flag> once;
    static std::unique_ptr<T> single;
};

} // namespace multipass

template <typename T>
inline constexpr multipass::Singleton<T>::Singleton(const multipass::Singleton<T>::PrivatePass&) noexcept
{
}

template <typename T>
std::unique_ptr<std::once_flag> multipass::Singleton<T>::once = std::make_unique<std::once_flag>();

template <typename T>
std::unique_ptr<T> multipass::Singleton<T>::single = nullptr;

template <typename T>
template <typename U, typename... Args, typename>
inline void multipass::Singleton<T>::mock(Args&&... args) noexcept(noexcept(U(Base::pass, args...)))
{
    init<U>(std::forward<Args>(args)...);
}

template <typename T>
template <typename... Args>
inline T& multipass::Singleton<T>::instance(Args&&... args) noexcept(noexcept(T(Base::pass, args...)))
{
    init<T>(std::forward<Args>(args)...);
    return *single;
}

template <typename T>
inline void multipass::Singleton<T>::reset() noexcept
{
    once = std::make_unique<std::once_flag>(); // flag itself not assignable, so we use a ptr
    single.reset(nullptr);
}

template <typename T>
template <typename U, typename... Args>
inline void multipass::Singleton<T>::init(Args&&... args) noexcept(noexcept(U(Base::pass, args...)))
{
    std::call_once(*once, [&args...] { single = std::make_unique<U>(Base::pass, std::forward<Args>(args)...); });
}

#endif // MULTIPASS_SINGLETON_H
