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

#ifndef MULTIPASS_FILE_OPS_H
#define MULTIPASS_FILE_OPS_H

#include "singleton.h"

#include <QDir>
#include <QFile>
#include <QString>

#define MP_FILEOPS multipass::FileOps::instance()

namespace multipass
{
class FileOps : public Singleton<FileOps>
{
public:
    FileOps(const Singleton<FileOps>::PrivatePass&);

    // QDir operations
    virtual bool isReadable(QDir& dir);
    virtual bool rmdir(QDir& dir, const QString& dirName);

    // QFile operations
    virtual bool open(QFile& file, QIODevice::OpenMode mode);
    virtual qint64 read(QFile& file, char* data, qint64 maxSize);
    virtual bool remove(QFile& file);
    virtual bool rename(QFile& file, const QString& newName);
    virtual bool resize(QFile& file, qint64 sz);
    virtual bool seek(QFile& file, qint64 pos);
    virtual bool setPermissions(QFile& file, QFileDevice::Permissions permissions);
    virtual qint64 write(QFile& file, const char* data, qint64 maxSize);
};
} // namespace multipass

#endif // MULTIPASS_FILE_OPS_H
