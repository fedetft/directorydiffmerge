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
 */
void scanSourceTargetDir(const std::filesystem::path& src,
                         const std::filesystem::path& dst,
                         bool threads, ScanOpt opt,
                         DirectoryTree& srcTree,
                         DirectoryTree& dstTree);

/**
 * Scrub only the backup directory
 * \param dst destination (backup) directory path
 * \param meta1 first copy of the metadata for the destination directory
 * \param meta2 second copy of the metadata for the destination directory
 */
int scrub(const std::filesystem::path& dst,
          const std::filesystem::path& meta1,
          const std::filesystem::path& meta2);

/**
 * Scrub both the directory to be backed up and the backup directory
 * \param src source directory path (directory to be backed up)
 * \param dst destination (backup) directory path
 * \param meta1 first copy of the metadata for the destination directory
 * \param meta2 second copy of the metadata for the destination directory
 * \param threads if true, scan in parallel
 */
int scrub(const std::filesystem::path& src,
          const std::filesystem::path& dst,
          const std::filesystem::path& meta1,
          const std::filesystem::path& meta2,
          bool threads);
