/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#include <multipass/file_ops.h>

namespace mp = multipass;

mp::FileOps::FileOps(const Singleton<FileOps>::PrivatePass& pass) : Singleton<FileOps>::Singleton{pass}
{
}

bool mp::FileOps::isReadable(QDir& dir)
{
    return dir.isReadable();
}

bool mp::FileOps::rmdir(QDir& dir, const QString& dirName)
{
    return dir.rmdir(dirName);
}

bool mp::FileOps::open(QFile& file, QIODevice::OpenMode mode)
{
    return file.open(mode);
}

qint64 mp::FileOps::read(QFile& file, char* data, qint64 maxSize)
{
    return file.read(data, maxSize);
}

bool mp::FileOps::remove(QFile& file)
{
    return file.remove();
}

bool mp::FileOps::rename(QFile& file, const QString& newName)
{
    return file.rename(newName);
}

bool mp::FileOps::resize(QFile& file, qint64 sz)
{
    return file.resize(sz);
}

bool mp::FileOps::seek(QFile& file, qint64 pos)
{
    return file.seek(pos);
}

bool mp::FileOps::setPermissions(QFile& file, QFileDevice::Permissions permissions)
{
    return file.setPermissions(permissions);
}

qint64 mp::FileOps::write(QFile& file, const char* data, qint64 maxSize)
{
    return file.write(data, maxSize);
}
