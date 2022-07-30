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
 * An extended version of the C++17 <filesystem> API providing some missing
 * features.
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

//Forward declarartions
class ext_file_status;
ext_file_status ext_status(const std::filesystem::path& p);
ext_file_status ext_symlink_status(const std::filesystem::path& p);
std::string ext_lookup_user(uid_t uid);
std::string ext_lookup_group(gid_t gid);

/**
 * Extended version of std::filesystem::file_status. The standard does not
 * provide a way to get the user and group of a file, so this one was added.
 * Moreover, although the standard has a way to get the last write time, it
 * is a separate call requiring a second stat syscall per file, while this
 * extended version allows to access the modified time with a single stat.
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
    std::string user() const { return ext_lookup_user(st.st_uid); }

    /**
     * \return the file group string
     */
    std::string group() const { return ext_lookup_group(st.st_gid); }

private:
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

/**
 * \param uid numerical user id
 * \return user as string
 */
std::string ext_lookup_user(uid_t uid);

/**
 * \param user as string
 * \return uid numerical user id
 */
uid_t ext_lookup_user(const std::string& user);

/**
 * \param numerical group id
 * \return group as string
 */
std::string ext_lookup_group(gid_t gid);

/**
 * \param group as string
 * \return numerical group id
 */
gid_t ext_lookup_group(const std::string& group);

/**
 * Extended version of std::filesystem::last_write_time. The standard does not
 * provide a way to alter the last write time of symlinks, so this one was
 * added. TODO: the time format is not compatible with <filesystem>, it's just
 * a plain time_t
 * \param p path (does not follow symlink)
 * \param mtime last modified file time
 */
void ext_symlink_last_write_time(const std::filesystem::path& p, time_t mtime);

/**
 * The standard does not provide a way to set the owner/group of a file,
 * so this function provides this functionality
 * \param p path (does not follow symlink)
 * \param user new user
 * \param group new grup
 */
void ext_symlink_change_ownership(const std::filesystem::path& p,
                                  const std::string& user, const std::string& group);
