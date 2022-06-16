/***************************************************************************
 *   Copyright (C) 2022 by Terraneo Federico                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

/*
 * An extended version of the C++17 <filesystem> status() and symlink_status()
 * calls exposing also the file's user and group strings, as well as modified
 * time.
 */

#pragma once

#include <filesystem>
#include <string>
#include <map>
#include <mutex>
#include <stdexcept>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

class ext_file_status;
ext_file_status ext_status(const std::filesystem::path& p);
ext_file_status ext_symlink_status(const std::filesystem::path& p);

/**
 * Extended version of std::filesystem::file_status
 */
class ext_file_status
{
public:
    /**
     * Default constructor, results in an empty object
     */
    ext_file_status()
    {
        memset(&st,0,sizeof(st));
    }

    /**
     * \return the file type (as std::file_status)
     */
    std::filesystem::file_type type() const noexcept
    {
        return typeLut[(st.st_mode>>12) & 0xf];
    }

    /**
     * \return the file permissions (as std::file_status)
     */
    std::filesystem::perms permissions() const noexcept
    {
        return static_cast<std::filesystem::perms>(st.st_mode & 07777);
    }

    /**
     * \return the file size (there are other more standard ways to do that)
     */
    off_t file_size() const noexcept { return st.st_size; }

    /**
     * \return the file last modified time
     */
    time_t mtime() const { return st.st_mtime; }

    /**
     * \return the number of hard links
     */
    uintmax_t hard_link_count() const { return st.st_nlink; }

    /**
     * \return the file user string
     */
    std::string user() const { return lookupUser(st.st_uid); }

    /**
     * \return the file group string
     */
    std::string group() const { return lookupGroup(st.st_gid); }

private:

    /**
     * \param uid numerical user id
     * \return user as string
     */
    static std::string lookupUser(uid_t uid);

    /**
     * \param numerical group id
     * \return group as string
     */
    static std::string lookupGroup(gid_t gid);

    /**
     * Allocate structBuffer to be used by getpwuid_r/getgrgid_r
     * Meant to be called only if structBuffer is empty
     * Must lock the mutex m before calling this function
     */
    static void allocateStructBuffer();

    static std::mutex m;
    static std::string structBuffer;
    static std::map<uid_t, std::string> userCache;
    static std::map<gid_t, std::string> groupCache;

    static const std::filesystem::file_type typeLut[16];

    struct stat st;

    friend ext_file_status ext_status(const std::filesystem::path& p);
    friend ext_file_status ext_symlink_status(const std::filesystem::path& p);
};

/**
 * Extended version of std::filesystem::ext_status
 * \param p path to stat
 * \return ext_file_status
 */
inline ext_file_status ext_status(const std::filesystem::path& p)
{
    ext_file_status result;
    if(stat(p.string().c_str(),&result.st)!=0)
        throw std::runtime_error("ext_status");
    return result;
}

/**
 * Extended version of std::filesystem::ext_symlink_status
 * \param p path to lstat
 * \return ext_file_status
 */
inline ext_file_status ext_symlink_status(const std::filesystem::path& p)
{
    ext_file_status result;
    if(lstat(p.string().c_str(),&result.st)!=0)
        throw std::runtime_error("ext_symlink_status");
    return result;
}
