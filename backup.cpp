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

#include "backup.h"
#include <thread>

using namespace std;
using namespace std::filesystem;

void scanSourceTargetDir(const path& src, const path& dst, bool threads,
    ScanOpt opt, DirectoryTree& srcTree, DirectoryTree& dstTree)
{
    if(threads)
    {
        thread t([&](){ srcTree.scanDirectory(src,opt); });
        dstTree.scanDirectory(dst,opt);
        t.join();
    } else {
        srcTree.scanDirectory(src,opt);
        dstTree.scanDirectory(dst,opt);
    }
}

int scrub(const path& dst, const path& meta1, const path& meta2)
{
    DirectoryTree dstTree;
    dstTree.scanDirectory(dst);
    return scrubImpl(dst,dstTree,meta1,meta2);
}

int scrub(const path& src, const path& dst, const path& meta1, const path& meta2,
    bool threads)
{
    DirectoryTree srcTree;
    DirectoryTree dstTree;
    scanSourceTargetDir(src,dst,threads,ScanOpt::ComputeHash,srcTree,dstTree);
    return scrubImpl(srcTree,dst,dstTree,meta1,meta2);
}

int scrubImpl(const path& dst, DirectoryTree& dstTree, const path& meta1,
    const path& meta2)
{
    DirectoryTree meta1Tree, meta2Tree;
    meta1Tree.readFrom(meta1);
    meta2Tree.readFrom(meta2);
    //TODO

    return 0;
}

int scrubImpl(DirectoryTree& srcTree, const path& dst, DirectoryTree& dstTree,
    const path& meta1, const path& meta2)
{
    DirectoryTree meta1Tree, meta2Tree;
    meta1Tree.readFrom(meta1);
    meta2Tree.readFrom(meta2);
    //TODO

    return 0;
}
