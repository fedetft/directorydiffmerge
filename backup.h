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
#include "core.h"

/**
 * Scan source and target directories, possibly in parallel
 * \param src source directory path
 * \param dst destination directory path
 * \param threads if true, scan in parallel
 * \param opt scan options
 * \param scrTree scanned source directory will be placed here
 * \param dstTree scanned destination directory will be placed here
 * \param warningCallback warning callback
 * \throws exception if loading the trees fail
 */
void scanSourceTargetDir(const std::filesystem::path& src,
                         const std::filesystem::path& dst,
                         bool threads, ScanOpt opt,
                         DirectoryTree& srcTree,
                         DirectoryTree& dstTree,
                         std::function<void (const std::string&)> warningCallback={});

/**
 * Scrub the backup directory
 * \param dst destination (backup) directory path
 * \param meta1 first copy of the metadata for the destination directory
 * \param meta2 second copy of the metadata for the destination directory
 * \param fixup if true, attempt to fix inconsistencies in the backup directory
 * \param warningCallback warning callback
 * \return 0 if no action was needed,
 *         1 if recoverable errors found and fixed
 *         2 if unrecoverable errors found
 */
int scrub(const std::filesystem::path& dst,
          const std::filesystem::path& meta1,
          const std::filesystem::path& meta2,
          bool fixup,
          std::function<void (const std::string&)> warningCallback={});

/**
 * Scrub the backup directory using the source directory to copy
 * missing/corrupted files from.
 * \param src source directory path (directory to be backed up)
 * \param dst destination (backup) directory path
 * \param meta1 first copy of the metadata for the destination directory
 * \param meta2 second copy of the metadata for the destination directory
 * \param fixup if true, attempt to fix inconsistencies in the backup directory
 * \param threads if true, scan in parallel
 * \param warningCallback warning callback
 * \return 0 if no action was needed,
 *         1 if recoverable errors found and fixed
 *         2 if unrecoverable errors found
 */
int scrub(const std::filesystem::path& src,
          const std::filesystem::path& dst,
          const std::filesystem::path& meta1,
          const std::filesystem::path& meta2,
          bool fixup, bool threads,
          std::function<void (const std::string&)> warningCallback={});

/**
 * Backup with bit rot detection support (requires two copies of metadata in
 * the backup directory). Scrubs the backup directory first, and then proceeds
 * with the backup.
 * \param src source directory path (directory to be backed up)
 * \param dst destination (backup) directory path
 * \param meta1 first copy of the metadata for the destination directory
 * \param meta2 second copy of the metadata for the destination directory
 * \param fixup if true, attempt to fix inconsistencies in the backup directory
 * \param hashAllFiles if false, don't compute the hash of files when scanning
 * directories. This greatly speeds up the backup but at the price of not
 * checking the entire directories for bit rot. Note that when saving metadata
 * files after a backup, the hashes of only the files that changed are computed
 * anyway, so as to keep the metadata files with all the necessary information
 * for future use, so even with this option some hash computation may happen.
 * It is thus recommended to periodically do a backup with hashAllFiles=true
 * \param threads if true, scan in parallel
 * \param warningCallback warning callback
 * \return 0 if no action was needed,
 *         1 if recoverable errors found and fixed
 *         2 if unrecoverable errors found
 */
int backup(const std::filesystem::path& src,
           const std::filesystem::path& dst,
           const std::filesystem::path& meta1,
           const std::filesystem::path& meta2,
           bool fixup, bool hashAllFiles, bool threads,
           std::function<void (const std::string&)> warningCallback={});

/**
 * Simple backup with no bit rot detection support.
 * \param src source directory path (directory to be backed up)
 * \param dst destination (backup) directory path
 * \param threads if true, scan in parallel
 * \param warningCallback warning callback
 * \return 0 if no action was needed,
 *         1 if recoverable errors found and fixed
 *         2 if unrecoverable errors found
 */
int backup(const std::filesystem::path& src,
           const std::filesystem::path& dst,
           bool threads,
           std::function<void (const std::string&)> warningCallback={});
