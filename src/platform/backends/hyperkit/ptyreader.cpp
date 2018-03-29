/*
 * Copyright (C) 2017 Canonical, Ltd.
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
 * Authored by: Gerry Boland <gerry.boland@canonical.com>
 */

#include "ptyreader.h"

#include <multipass/logging/log.h>

extern "C" {
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
}

namespace mp = multipass;
namespace mpl = multipass::logging;

class mp::PtyReader::PtyFd
{
public:
    PtyFd(const PtyFd&) = delete;
    PtyFd& operator=(const PtyFd&) = delete;

    explicit PtyFd(const QString& pty_name)
    {
        fd = ::open(pty_name.toLatin1().constData(), O_RDONLY | O_NOCTTY);
        if (fd < 0)
        {
            ::close(fd);
            fd = -1;
            throw std::runtime_error(qPrintable("Unable to open Pty: " + pty_name));
        }

        ::fcntl(fd, F_SETFD, FD_CLOEXEC);
        ::fcntl(fd, F_SETFL, O_NONBLOCK);

        int res = ::tcgetattr(fd, &termios_orig);
        if (res)
        {
            ::close(fd);
            throw std::runtime_error(
                qPrintable("Error: reading Pty attributes of " + pty_name + " failed with code " + errno));
        }

        struct termios termios = termios_orig;
        termios.c_cflag &= (CLOCAL);         // ignore modem control lines
        termios.c_oflag &= ~(ONLCR);         // so do not have to deal with \r
        termios.c_ospeed &= ~(ONOEOT);       // discard EOT (^D) on output
        termios.c_lflag &= ~(ECHO | ECHONL); // disable echoing, including newlines

        res = ::tcsetattr(fd, TCSANOW, &termios); // apply the new config immediately
        if (res)
        {
            ::close(fd);
            throw std::runtime_error(
                qPrintable("Error: setting Pty attributes of " + pty_name + " failed with code " + errno));
        }
    }

    ~PtyFd()
    {
        if (-1 == fd)
        {
            return;
        }

        // restore pty state on close
        ::tcsetattr(fd, TCSANOW, &termios_orig);
        ::fcntl(fd, F_SETFD, 0);
        ::close(fd);
    }

    int get() const noexcept
    {
        return fd;
    }

private:
    struct termios termios_orig;
    int fd = -1;
};

mp::PtyReader::PtyReader(const QString& pty_name, QObject* parent)
    : QObject(parent), fd{std::make_unique<PtyFd>(pty_name)}, read_notifier(fd->get(), QSocketNotifier::Read)
{
    QObject::connect(&read_notifier, &QSocketNotifier::activated, this, &PtyReader::doRead);
    read_notifier.setEnabled(true);
}

mp::PtyReader::~PtyReader() = default;

static inline qint64 safe_read(int fd, void* data, size_t maxlen)
{
    ssize_t ret = 0;
    do
    {
        ret = ::read(fd, data, maxlen);
    } while (ret == -1 && errno == EINTR);
    return ret;
}

bool mp::PtyReader::doRead()
{
    QByteArray line;
    size_t bytes = 0;

    if (ioctl(fd->get(), FIONREAD, &bytes) < 0)
    {
        mpl::log(mpl::Level::error, "pty-reader", "could not get read buffer size, using fallback");
        bytes = 1024;
    }
    line.resize(bytes); // pre-allocate buffer size
    const auto bytes_read = safe_read(fd->get(), line.data(), bytes);
    if (bytes_read > 0)
    {
        // shrink buffer as del/backspace characters are interpreted only during read()
        // Drop the end new-line while we're at it
        line.resize(bytes_read - 1);

        // special case: filter lone new-lines
        if (line.size() == 0)
        {
            return false;
        }
    }
    else
    {
        return false;
    }

    emit lineRead(line);
    return true;
}
