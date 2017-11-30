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

#ifndef PTYREADER_H
#define PTYREADER_H

#include <QObject>
#include <QSocketNotifier>

namespace multipass
{

class PtyReader : public QObject
{
    Q_OBJECT
public:
    PtyReader(const QString &pty_name, QObject *parent = 0);
    ~PtyReader();

signals:
    void lineRead(const QByteArray line);
    void eofRead();

private slots:
    bool doRead();

private:
    class PtyFd;
    const std::unique_ptr<PtyFd> fd;
    QSocketNotifier read_notifier;
};

} // namespace multipass

#endif // PTYREADER_H
