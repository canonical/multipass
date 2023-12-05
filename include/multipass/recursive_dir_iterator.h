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

#ifndef RECURSIVE_DIR_ITERATOR_H
#define RECURSIVE_DIR_ITERATOR_H

#include <filesystem>
#include <optional>

namespace multipass
{
namespace fs = std::filesystem;

// this is just a thin wrapper around std::filesystem::directory_entry, used for mocking purposes
class DirectoryEntry
{
public:
    DirectoryEntry() = default;
    DirectoryEntry(const DirectoryEntry&) = default;
    DirectoryEntry(DirectoryEntry&&) = default;
    explicit DirectoryEntry(const fs::directory_entry& _entry) : entry{_entry}
    {
    }

    DirectoryEntry& operator=(const DirectoryEntry&) = default;
    DirectoryEntry& operator=(DirectoryEntry&&) noexcept = default;

    virtual ~DirectoryEntry() = default;

    virtual void assign(const fs::path& path)
    {
        entry.assign(path);
    }

    virtual void assign(const fs::path& path, std::error_code& err)
    {
        entry.assign(path, err);
    }

    virtual void replace_filename(const fs::path& path)
    {
        entry.replace_filename(path);
    }

    virtual void replace_filename(const fs::path& path, std::error_code& err)
    {
        entry.replace_filename(path, err);
    }

    virtual void refresh()
    {
        entry.refresh();
    }

    virtual void refresh(std::error_code& err) noexcept
    {
        entry.refresh(err);
    }

    virtual const fs::path& path() const noexcept
    {
        return entry.path();
    }

    virtual bool exists() const
    {
        return entry.exists();
    }

    virtual bool exists(std::error_code& err) const noexcept
    {
        return entry.exists(err);
    }

    virtual bool is_block_file() const
    {
        return entry.is_block_file();
    }

    virtual bool is_block_file(std::error_code& err) const noexcept
    {
        return entry.is_block_file(err);
    }

    virtual bool is_character_file() const
    {
        return entry.is_character_file();
    }

    virtual bool is_character_file(std::error_code& err) const noexcept
    {
        return entry.is_character_file(err);
    }

    virtual bool is_directory() const
    {
        return entry.is_directory();
    }

    virtual bool is_directory(std::error_code& err) const noexcept
    {
        return entry.is_directory(err);
    }

    virtual bool is_fifo() const
    {
        return entry.is_fifo();
    }

    virtual bool is_fifo(std::error_code& err) const noexcept
    {
        return entry.is_fifo(err);
    }

    virtual bool is_other() const
    {
        return entry.is_other();
    }

    virtual bool is_other(std::error_code& err) const noexcept
    {
        return entry.is_other(err);
    }

    virtual bool is_regular_file() const
    {
        return entry.is_regular_file();
    }

    virtual bool is_regular_file(std::error_code& err) const noexcept
    {
        return entry.is_regular_file(err);
    }

    virtual bool is_socket() const
    {
        return entry.is_socket();
    }

    virtual bool is_socket(std::error_code& err) const noexcept
    {
        return entry.is_socket(err);
    }

    virtual bool is_symlink() const
    {
        return entry.is_symlink();
    }

    virtual bool is_symlink(std::error_code& err) const noexcept
    {
        return entry.is_symlink(err);
    }

    virtual uintmax_t file_size() const
    {
        return entry.file_size();
    }

    virtual uintmax_t file_size(std::error_code& err) const noexcept
    {
        return entry.file_size(err);
    }

    virtual uintmax_t hard_link_count() const
    {
        return entry.hard_link_count();
    }

    virtual uintmax_t hard_link_count(std::error_code& err) const noexcept
    {
        return entry.hard_link_count(err);
    }

    virtual fs::file_time_type last_write_time() const
    {
        return entry.last_write_time();
    }

    virtual fs::file_time_type last_write_time(std::error_code& err) const noexcept
    {
        return entry.last_write_time(err);
    }

    virtual fs::file_status status() const
    {
        return entry.status();
    }

    virtual fs::file_status status(std::error_code& err) const noexcept
    {
        return entry.status(err);
    }

    virtual fs::file_status symlink_status() const
    {
        return entry.symlink_status();
    }

    virtual fs::file_status symlink_status(std::error_code& err) const noexcept
    {
        return entry.symlink_status(err);
    }

    virtual bool operator==(const DirectoryEntry& rhs) const noexcept
    {
        return entry == rhs.entry;
    }

private:
    fs::directory_entry entry;
};

// wrapper class around std::filesystem::recursive_directory_iterator used for mocking purposes
class RecursiveDirIterator
{
public:
    RecursiveDirIterator() = default;
    RecursiveDirIterator(const fs::path& path, std::error_code& err) : iter{path, err}
    {
    }

    virtual bool hasNext()
    {
        return iter != fs::end(iter);
    }

    virtual const DirectoryEntry& next()
    {
        return current = DirectoryEntry{*iter++};
    }

    virtual ~RecursiveDirIterator() = default;

private:
    fs::recursive_directory_iterator iter;
    DirectoryEntry current;
};

// wrapper class around std::filesystem::directory_iterator used for mocking purposes
class DirIterator
{
public:
    DirIterator() = default;
    DirIterator(const fs::path& path, std::error_code& err) : self{path / "."}, parent{path / ".."}, iter{path, err}
    {
    }

    virtual bool hasNext()
    {
        return iter != fs::end(iter);
    }

    virtual const DirectoryEntry& next()
    {
        if (self)
        {
            const fs::directory_entry entry{*self};
            self.reset();
            return current = DirectoryEntry{entry};
        }

        if (parent)
        {
            const fs::directory_entry entry{*parent};
            parent.reset();
            return current = DirectoryEntry{entry};
        }

        return current = DirectoryEntry{*iter++};
    }

    virtual ~DirIterator() = default;

private:
    std::optional<fs::path> self;
    std::optional<fs::path> parent;
    fs::directory_iterator iter;
    DirectoryEntry current;
};
} // namespace multipass

#endif // RECURSIVE_DIR_ITERATOR_H
