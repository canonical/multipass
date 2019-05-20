/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#include "private_pass_provider.h"

#include <memory>
#include <mutex>
#include <type_traits>

namespace multipass
{

// A mockable singleton. To use, inherit (publicly) using the CRTP idiom
template <typename T>
class Singleton : public PrivatePassProvider<Singleton<T>>
{
public:
    using Base = PrivatePassProvider<Singleton<T>>;
    using PrivatePass = typename Base::PrivatePass;

    constexpr Singleton(const PrivatePass&);
    virtual ~Singleton() = default;
    static T& instance();

    // non-copyable
    Singleton(const Singleton&) = delete;
    Singleton(Singleton&&) = delete;
    void operator=(const Singleton&) = delete;
    void operator=(Singleton&&) = delete;

protected:
    template <typename U, typename = std::enable_if_t<std::is_base_of<T, U>::value>>
    static void mock();  // only works if instance not called yet, or after reset
    static void reset(); // not thread-safe, make sure no other threads using this singleton anymore!

private:
    template <typename U>
    static void init();

    static std::unique_ptr<std::once_flag> once;
    static std::unique_ptr<T> single;
};

} // namespace multipass

template <typename T>
inline constexpr multipass::Singleton<T>::Singleton(const multipass::Singleton<T>::PrivatePass&)
{
}

template <typename T>
std::unique_ptr<std::once_flag> multipass::Singleton<T>::once = std::make_unique<std::once_flag>();

template <typename T>
std::unique_ptr<T> multipass::Singleton<T>::single = nullptr;

template <typename T>
template <typename U, typename>
inline void multipass::Singleton<T>::mock()
{
    init<U>();
}

template <typename T>
inline T& multipass::Singleton<T>::instance()
{
    init<T>();
    return *single;
}

template <typename T>
inline void multipass::Singleton<T>::reset()
{
    once = std::make_unique<std::once_flag>(); // flag itself not assignable, so we use a ptr
    single.reset(nullptr);
}

template <typename T>
template <typename U>
inline void multipass::Singleton<T>::init()
{
    std::call_once(*once, [] { single = std::make_unique<U>(Base::pass); });
}

#endif // MULTIPASS_SINGLETON_H
