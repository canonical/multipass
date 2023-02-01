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

#include <multipass/disabled_copy_move.h>

#include <type_traits>

namespace mp = multipass;

namespace
{
class Foo : private multipass::DisabledCopyMove
{
public:
    virtual ~Foo()
    {
    }
};

class Bar : public Foo
{
};

class Baz : public Bar
{
public:
    Baz() = default;
};

template <class T>
class Buz : public Baz
{
};

template <typename T>
void check_traits()
{
    static_assert(!std::is_copy_constructible_v<T>);
    static_assert(!std::is_copy_assignable_v<T>);
    static_assert(!std::is_move_constructible_v<T>);
    static_assert(!std::is_move_assignable_v<T>);

    static_assert(std::is_nothrow_default_constructible_v<T>);
    static_assert(std::is_nothrow_destructible_v<T>);
}

[[maybe_unused]] void check_all_traits()
{
    check_traits<Foo>();
    check_traits<Bar>();
    check_traits<Baz>();
    check_traits<Buz<int>>();
}
} // namespace
