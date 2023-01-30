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

#ifndef MULTIPASS_ANIMATED_SPINNER_H
#define MULTIPASS_ANIMATED_SPINNER_H

#include <future>
#include <string>
#include <vector>

namespace multipass
{
class AnimatedSpinner
{
public:
    explicit AnimatedSpinner(std::ostream& cout);
    ~AnimatedSpinner();

    void start(const std::string& message);
    void start();
    void stop();
    void print(std::ostream& stream, const std::string& message);

private:
    void draw();
    const std::vector<char> spinner;
    std::ostream& cout;
    bool running;
    std::string current_message;
    std::mutex mutex;
    std::condition_variable cv;
    std::thread t;
};
}

#endif // MULTIPASS_ANIMATED_SPINNER_H
