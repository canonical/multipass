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
 */

#include "animated_spinner.h"

#include <cassert>
#include <iostream>

namespace mp = multipass;

namespace
{
void clear_line(std::ostream& out)
{
    out << "\x1B[2K\x1B[0A";        // Delete current line
    out << "\x1B[0E" << std::flush; // Move cursor to leftmost position of current line
}
} // namespace

mp::AnimatedSpinner::AnimatedSpinner(std::ostream& cout, bool is_live)
    : spinner{'|', '/', '-', '\\'}, cout{cout}, running{false}, is_live(is_live)
{
}

mp::AnimatedSpinner::~AnimatedSpinner()
{
    stop();
}

void mp::AnimatedSpinner::start(const std::string& start_message)
{
    std::unique_lock<decltype(mutex)> lock{mutex};
    if (!running)
    {
        current_message = start_message;
        running = true;

        if (is_live)
        {
            clear_line(cout);
            cout << start_message << "  " << std::flush;
            t = std::thread(&AnimatedSpinner::draw, this);
        }
    }
}

void mp::AnimatedSpinner::start()
{
    if (is_live && !current_message.empty())
        start(current_message);
}

void mp::AnimatedSpinner::stop()
{
    std::unique_lock<decltype(mutex)> lock{mutex};
    if (running)
    {
        running = false;
        cv.notify_one();
        lock.unlock();
        if (t.joinable())
            t.join();
    }

    if (is_live)
        clear_line(cout);
}

void mp::AnimatedSpinner::print(std::ostream& stream, const std::string& message)
{
    stop();

    stream << message;

    start();
}

void mp::AnimatedSpinner::draw()
{
    std::unique_lock<decltype(mutex)> lock{mutex};

    assert(is_live && "should only draw when running live");

    auto it = spinner.begin();
    while (running)
    {
        if (++it == spinner.end())
            it = spinner.begin();
        cout << "\b" << *it << std::flush;
        cv.wait_for(lock, std::chrono::milliseconds(100));
    }
    cout << "\b" << " " << std::flush;
}
