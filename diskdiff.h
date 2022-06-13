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

#pragma once

#include <filesystem>
#include <ostream>
#include <ctime>

/**
 * Computes SHA1 of a file. Only to detect changes, no crypto strength needed
 * \param p file path
 * \return a string with the SHA1 hash in ASCII hex digits
 */
std::string hashFile(const std::filesystem::path& p);

/**
 * This class is used to load/store information about files and directories
 * in diff files
 */
class FilesystemElement
{
public:
    /**
     * Emppty constructor
     */
    FilesystemElement();

    /**
     * Constructor from a path
     * \param p absolute path
     * \param top top level directory, used to compute relative path
     */
    FilesystemElement(const std::filesystem::path& p,
                      const std::filesystem::path& top);

    /**
     * Constructor from string, used when reading from diff files
     * \param diffLine line of the diff file to construct the object from
     * \param diffFileName name of diff file, used for error reporting
     * \param lineNo line number of diff file, used for error reporting
     * \throws runtime_error in case of errors
     */
    FilesystemElement(const std::string& diffLine,
                      const std::string& diffFileName="", int lineNo=-1)
    {
        readFrom(diffLine,diffFileName,lineNo);
    }

    /**
     * Read from diff files
     * \param diffLine line of the diff file to construct the object from
     * \param diffFileName name of diff file, used for error reporting
     * \param lineNo line number of diff file, used for error reporting
     * \throws runtime_error in case of errors
     */
    void readFrom(const std::string& diffLine,
                  const std::string& diffFileName="", int lineNo=-1);

    /**
     * Write the object to an ostream based on the diff file format
     * \param os ostream where to write
     */
    void writeTo(std::ostream& os);

    std::filesystem::file_type type;    ///< File type (regular, directory, ...)
    std::filesystem::perms permissions; ///< File permissions (rwxrwxrwx)
    std::string user;                   ///< File user (owner)
    std::string group;                  ///< File group
    time_t mtime=0;                     ///< Modified time
    off_t size=0;                       ///< Size, only if regular file
    std::string hash;                   ///< SHA1 hash, only if regular file
    std::filesystem::path relativePath; ///< File path relative to top level directory
    std::filesystem::path symlinkTarget;///< Symlink target path, only if symlink
};


/**
 * Recursively list all files in a directory and its subdirectories listing
 * their properties
 *
 * On an SSD it processed 87.5GB in 137s ~650MB/s with SHA1 hashing
 * On an HDD it processed 87.5GB in 1134s ~79MB/s with SHA1 hashing
 */
class FileLister
{
public:
    /**
     * Constructor
     * \param os ink ostream where listed data will be printed
     */
    explicit FileLister(std::ostream& os) : os(os) {}

    /**
     * Perform the actual work, for each entry found print
     * - type
     * - permissions
     * - owner and group
     * - for regular files, hash, size and last modified date
     * - for symlinks, target
     * \param top top level directory where to start listing
     */
    void listFiles(const std::filesystem::path& top);

    /**
     * \return true if the last listFiles call encountered unsupported file
     * types
     */
    bool unsupportedFilesFound() const { return unsupported; }

private:
    void recursiveListFiles(const std::filesystem::path& p);
    std::ostream& os;
    std::filesystem::path top;
    bool printBreak;
    bool unsupported=false;
};
