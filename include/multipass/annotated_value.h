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

#include <multipass/reply_concepts.h>
#include <multipass/rpc/multipass.grpc.pb.h>

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
concept CanBeAnnotated =
    std::is_void_v<T> || (std::move_constructible<T> && !std::is_reference_v<T>);

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
    template <typename T>
        requires CanBeAnnotated<T>
    friend class Annotated;
};

template <typename T>
    requires CanBeAnnotated<T>
class [[nodiscard("You must collect() the value or bind it to retrieve the MessageBag")]] Annotated
{
public:
    Annotated(T value, MessageBag messages)
        : _value{std::move(value)}, _messages{std::move(messages)}
    {
    }

    template <CanBeAnnotated U>
    Annotated(T value, Annotated<U>&& qualified_return) : _value{std::move(value)}
    {
        _messages.merge(std::move(qualified_return._messages));
    }

    template <std::convertible_to<std::string>... Mesgs>
    Annotated(T value, Mesgs&&... messages) : _value{std::move(value)}
    {
        ((_messages.add_message(std::forward<Mesgs>(messages))), ...);
    }

    ~Annotated() = default;

    // Disable copy constructors to avoid string copies
    Annotated(const Annotated&) = delete;
    Annotated& operator=(const Annotated&) = delete;

    Annotated(Annotated&&) = default;
    Annotated& operator=(Annotated&&) = default;

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
    template <typename U>
        requires CanBeAnnotated<T>
    friend class Annotated;

    T _value;
    MessageBag _messages;
};

template <>
class [[nodiscard("You must collect() or bind the returned MessageBag")]] Annotated<void>
{
public:
    Annotated() = default;
    Annotated(MessageBag messages) : _messages{std::move(messages)}
    {
    }
    template <CanBeAnnotated U>
    Annotated(Annotated<U>&& qualified_return)
    {
        _messages.merge(std::move(qualified_return._messages));
    }
    template <std::convertible_to<std::string>... Mesgs>
    Annotated(Mesgs&&... messages)
    {
        ((_messages.add_message(std::forward<Mesgs>(messages))), ...);
    }
    ~Annotated() = default;

    Annotated(const Annotated&) = delete;
    Annotated& operator=(const Annotated&) = delete;

    Annotated(Annotated&&) = default;
    Annotated& operator=(Annotated&&) = default;

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
    template <typename U>
        requires CanBeAnnotated<U>
    friend class Annotated;

    MessageBag _messages;
};

template <typename Reply, typename Request>
    requires LogMsgReply<Reply>
void send_messages(grpc::ServerReaderWriterInterface<Reply, Request>* server,
                   MessageBag&& message_bag)
{
    auto reply = Reply{};
    for (const auto& message : message_bag)
    {
        reply.set_reply_message(message);
        server->Write(reply);
    }
}
} // namespace multipass

namespace std
{
// Necessary to allow structured bindings
template <multipass::CanBeAnnotated T>
struct tuple_size<multipass::Annotated<T>> : std::integral_constant<std::size_t, 2>
{
};

template <multipass::CanBeAnnotated T>
struct tuple_element<0, multipass::Annotated<T>>
{
    using type = T;
};

template <multipass::CanBeAnnotated T>
struct tuple_element<1, multipass::Annotated<T>>
{
    using type = multipass::MessageBag;
};

template <>
struct tuple_size<multipass::Annotated<void>> : std::integral_constant<std::size_t, 1>
{
};

template <>
struct tuple_element<0, multipass::Annotated<void>>
{
    using type = multipass::MessageBag;
};
} // namespace std
