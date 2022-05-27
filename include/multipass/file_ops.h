/*
 * Copyright (C) 2021-2022 Canonical, Ltd.
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

#include <QByteArray>
#include <QDir>
#include <QFileDevice>
#include <QSaveFile>
#include <QString>
#include <QTextStream>

#include <fstream>

#define MP_FILEOPS multipass::FileOps::instance()

namespace multipass
{
class FileOps : public Singleton<FileOps>
{
public:
    FileOps(const Singleton<FileOps>::PrivatePass&) noexcept;

    // QDir operations
    virtual QDir current() const;
    virtual bool isReadable(const QDir& dir) const;
    virtual bool mkpath(const QDir& dir, const QString& dirName) const;
    virtual bool rmdir(QDir& dir, const QString& dirName) const;

    // QFile operations
    virtual bool exists(const QFile& file) const;
    virtual bool is_open(const QFile& file) const;
    virtual bool open(QFileDevice& file, QIODevice::OpenMode mode) const;
    virtual QFileDevice::Permissions permissions(const QFile& file) const;
    virtual qint64 read(QFile& file, char* data, qint64 maxSize) const;
    virtual QByteArray read_all(QFile& file) const;
    virtual QString read_line(QTextStream& text_stream) const;
    virtual bool remove(QFile& file) const;
    virtual bool rename(QFile& file, const QString& newName) const;
    virtual bool resize(QFile& file, qint64 sz) const;
    virtual bool seek(QFile& file, qint64 pos) const;
    virtual bool setPermissions(QFile& file, QFileDevice::Permissions permissions) const;
    virtual qint64 size(QFile& file) const;
    virtual qint64 write(QFile& file, const char* data, qint64 maxSize) const;
    virtual qint64 write(QFileDevice& file, const QByteArray& data) const;

    // QSaveFile operations
    virtual bool commit(QSaveFile& file) const;

    // std operations
    virtual void open(std::fstream& stream, const char* filename, std::ios_base::openmode mode) const;
};
} // namespace multipass

#endif // MULTIPASS_FILE_OPS_H
