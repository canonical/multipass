/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <concepts>
#include <iterator>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace multipass
{

// The idea is to idiomatically have a MessageBag in the stack,
// call a function f() such that:
//  auto&& [retval1, message_bag] = g();
//  auto retval2 = f().collect(message_bag);
// And in the end return {val,message_bag};
// Later on the message bag can be iterated for messages.

template <typename T>
// The payload should never be a reference, and no move constructor will fail compilation
concept Qualifiable = std::is_void_v<T> || (std::move_constructible<T> && !std::is_reference_v<T>);

class MessageBag
{
public:
    using iterator = std::vector<std::string>::iterator;
    using const_iterator = std::vector<std::string>::const_iterator;

    iterator begin()
    {
        return _messages.begin();
    }
    iterator end()
    {
        return _messages.end();
    }

    const_iterator begin() const
    {
        return _messages.begin();
    }
    const_iterator end() const
    {
        return _messages.end();
    }

    void clear()
    {
        _messages.clear();
    }

    template <std::convertible_to<std::string> String>
    void add_message(String&& message)
    {
        _messages.emplace_back(std::forward<String>(message));
    }

    MessageBag() = default;

    // Disable copying to avoid string copies
    MessageBag(const MessageBag&) = delete;
    MessageBag& operator=(const MessageBag&) = delete;

    MessageBag(MessageBag&&) = default;
    MessageBag& operator=(MessageBag&&) = default;

private:
    // The message vector is wrapped to minimize the API surface
    std::vector<std::string> _messages;

    void merge(MessageBag&& message_bag)
    {
        // Meant to consume the message bag
        _messages.insert(_messages.end(),
                         std::make_move_iterator(message_bag._messages.begin()),
                         std::make_move_iterator(message_bag._messages.end()));
        message_bag.clear();
    }

    // To allow usage of merge()
    template <Qualifiable T>
    friend class Qualified;
};

template <Qualifiable T>
class [[nodiscard("You must collect() the value or bind it to retrieve the MessageBag")]] Qualified
{
public:
    Qualified(T value, MessageBag messages)
        : _value{std::move(value)}, _messages{std::move(messages)}
    {
    }

    template <Qualifiable U>
    Qualified(T value, Qualified<U>&& qualified_return) : _value{std::move(value)}
    {
        _messages.merge(std::move(qualified_return._messages));
    }

    template <std::convertible_to<std::string>... Mesgs>
    Qualified(T value, Mesgs&&... messages) : _value{std::move(value)}
    {
        ((_messages.add_message(std::forward<Mesgs>(messages))), ...);
    }

    ~Qualified() = default;

    // Disable copy constructors to avoid string copies
    Qualified(const Qualified&) = delete;
    Qualified& operator=(const Qualified&) = delete;

    Qualified(Qualified&&) = default;
    Qualified& operator=(Qualified&&) = default;

    const auto& get_messages() const
    {
        return _messages;
    }

    T collect(MessageBag& messages)
    {
        messages.merge(std::move(_messages));
        return std::move(_value);
    }

    template <std::convertible_to<std::string> String>
    void add_message(String&& message)
    {
        _messages.add_message(std::forward<String>(message));
    }

    // Necessary to write auto&& [val, bag] = f();
    template <std::size_t I>
    decltype(auto) get() &&
    {
        if constexpr (I == 0)
            return std::move(_value);
        else if constexpr (I == 1)
            return std::move(_messages);
    }

    // Disable auto and auto& [val, bag] = f();
    template <std::size_t I>
    decltype(auto) get() & = delete;

    template <std::size_t I>
    decltype(auto) get() const& = delete;

private:
    // To allow message merge in the cross-type constructor
    template <Qualifiable U>
    friend class Qualified;

    T _value;
    MessageBag _messages;
};

template <>
class [[nodiscard("You must collect() or bind the returned MessageBag")]] Qualified<void>
{
public:
    Qualified(MessageBag messages) : _messages{std::move(messages)}
    {
    }

    template <Qualifiable U>
    Qualified(Qualified<U>&& qualified_return)
    {
        _messages.merge(std::move(qualified_return._messages));
    }

    template <std::convertible_to<std::string>... Mesgs>
    Qualified(Mesgs&&... messages)
    {
        ((_messages.add_message(std::forward<Mesgs>(messages))), ...);
    }

    ~Qualified() = default;

    Qualified(const Qualified&) = delete;
    Qualified& operator=(const Qualified&) = delete;

    Qualified(Qualified&&) = default;
    Qualified& operator=(Qualified&&) = default;

    const auto& get_messages() const
    {
        return _messages;
    }

    void collect(MessageBag& messages)
    {
        messages.merge(std::move(_messages));
    }

    template <std::convertible_to<std::string> String>
    void add_message(String&& message)
    {
        _messages.add_message(std::forward<String>(message));
    }

    template <std::size_t I>
    decltype(auto) get() &&
    {
        if constexpr (I == 0)
            return std::move(_messages);
    }

    template <std::size_t I>
    decltype(auto) get() & = delete;

    template <std::size_t I>
    decltype(auto) get() const& = delete;

private:
    template <Qualifiable U>
    friend class Qualified;

    MessageBag _messages;
};
} // namespace multipass

namespace std
{
// Necessary to allow structured bindings
template <multipass::Qualifiable T>
struct tuple_size<multipass::Qualified<T>> : std::integral_constant<std::size_t, 2>
{
};

template <multipass::Qualifiable T>
struct tuple_element<0, multipass::Qualified<T>>
{
    using type = T;
};

template <multipass::Qualifiable T>
struct tuple_element<1, multipass::Qualified<T>>
{
    using type = multipass::MessageBag;
};

template <>
struct tuple_size<multipass::Qualified<void>> : std::integral_constant<std::size_t, 1>
{
};

template <>
struct tuple_element<0, multipass::Qualified<void>>
{
    using type = multipass::MessageBag;
};
} // namespace std
