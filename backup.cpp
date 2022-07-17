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
#include "color.h"
#include <iostream>
#include <thread>
#include <cassert>

using namespace std;
using namespace std::filesystem;

/**
 * Try to fix an inconsistent backup state found during a scrub.
 * This function only sees a single difference, and is called to handle the
 * more difficult case when the metadata files agree but the content of the
 * backup directory differs from them.
 * \param src source directory path. NOTE: may be nullptr if source dir not
 * given by the user
 * \param srcTree freshly scanned metadata for src, assumes no modifications
 * occurred. NOTE: may be nullptr if source dir not given by the user
 * \param dst backup directory path, used to perform modifications
 * \param diff diff line representing the inconsistent state to try fixing
 * \return true if the state was fixed, false otherwise
 */
static bool tryToFixBackupFile(const path *src, DirectoryTree *srcTree,
                               const path& dst, const DirectoryDiffLine<3>& d)
{
    assert(d[1]==d[2]);
    if(!d[0])
    {
        string type=d[1].value().typeAsString();
        cout<<"The "<<type<<" is missing in the backup directory "
            <<"but the metadata files agree it should be there.\n";
        if(src==nullptr)
        {
            cout<<"If you re-run the scrub giving me also the source directory "
                <<"I may be able to help by looking for the "<<type<<" there, "
                <<"but until then, there's nothing I can do.\n";
            return false;
        } else {
            cout<<"Trying to see if I can find the missing "<<type<<" in the "
                <<"source directory.\n";
            path srcElementPath=*src / d[1].value().relativePath();
            path dstElementPath= dst / d[1].value().relativePath();
            FilesystemElement srcElement;
            try {
                srcElement=FilesystemElement(srcElementPath,*src);
            } catch(exception& e) {
                cout<<"That failed with error: "<<e.what()<<". There's nothing "
                    <<"I can do, but I recommend to double check the source "
                    <<"directory path. If it's wrong, please re-run the command "
                    <<"with the correct path. If it's correct, please check the "
                    <<"source directory manually, if the "<<type<<" really isn't "
                    <<"there maybe it was deleted manually both there and in the "
                    <<"backup directory.\n";
                return false;
            }
            if(srcElement==d[1].value())
            {
                //FIXME: if this is a directory, then it does not make sense
                //to process subsequent inconsistencies as we invalidated the diff
                cout<<"The "<<type<<" was found in the source directory and "
                    <<"matches with the backup metadata.\n"
                    <<"Copying it back into the backup directory.\n";
                //BUG: mtime differs, and this also causes the parent directory's
                //mtime to differ!
                copy(srcElementPath,dstElementPath);
                return true;
            } else {
                //TODO: look into exactly what differs, if the difference
                //is in the metadata, such as mtime or perms can be fixed
                cout<<"Something was found in the source directory with path "
                    <<srcElementPath<<" however, its properties\n"<<srcElement
                    <<" d not match the missing "<<type<<". At this point "
                    <<"there's nothing I can do.\n";
                return false;
            }
        }
    } else if(!d[1]) {
        string type=d[0].value().typeAsString();
        path dstElementPath=dst / d[0].value().relativePath();
        //FIXME: if this is a directory, then it does not make sense
        //to process subsequent inconsistencies as we invalidated the diff
        cout<<"The "<<type<<" "<<dstElementPath<<" is present in the backup "
            <<"directory but the metadata files agree it should not be there.\n"
            <<"Removing the "<<type<<".\n";
        //BUG: this also causes the parent directory's mtime to differ!
        auto n=remove_all(dst/d[0].value().relativePath());
        cout<<"Removed "<<n<<" files or directories\n";
        return true;
    } else {
        //TODO: look into exactly what differs, if the difference
        //is in the metadata, such as mtime or perms can be fixed
        //TODO: could try to see if source dir is consistent with
        //metadata files and copy file from source dir to backup dir
        //TODO: if it's a directory?
    }
    return false;
}

/**
 * Implementation code that scrubs only the backup directory
 * \param src source directory path. NOTE: may be nullptr if source dir not
 * given by the user
 * \param srcTree freshly scanned metadata for src, assumes no modifications
 * occurred. NOTE: may be nullptr if source dir not given by the user
 * \param dst backup directory path, used to perform modifications
 * \param dstTree freshly scanned metadata for dst, assumes no modifications occurred
 * \param meta1 first copy of the metadata path
 * \param meta2 second copy of the metadata path
 * \return 0 if no action was needed,
 *         1 if recoverable errors found and fixed
 *         2 if unrecoverable errors found
 */
static int scrubImpl(const path *src, DirectoryTree *srcTree,
                     const path& dst, DirectoryTree& dstTree,
                     const path& meta1, const path& meta2)
{
    cout<<"Loading metatata files... "; cout.flush();
    DirectoryTree meta1Tree, meta2Tree;
    try {
        meta1Tree.readFrom(meta1);
        meta2Tree.readFrom(meta2);
        cout<<"Done.\n";
    } catch(exception& e) {
        cout<<e.what()<<"\nIt looks like at least one of the metadata files is "
            <<"corrupted to the point that it cannot be read.\n"
            <<redb<<"Unrecoverable inconsistencies found."<<reset<<" You will "
            <<"need to manually fix the backup directory.\n";
        return 2;
    }

    cout<<"Comparing backup directory with metadata... "; cout.flush();
    auto diff=diff3(dstTree,meta1Tree,meta2Tree);
    cout<<"Done.\n";

    if(diff.empty())
    {
        cout<<greenb<<"All good."<<reset<<" No differences found.\n";
        return 0;
    }
    cout<<yellowb<<"Inconsitencies found."<<reset
        <<" Processing them one by one.\nNote: in the following diff a is "
        <<"the backup directory, b is metadata file 1 while c is metadata file 2\n";
    bool recovered=true, updateMeta1=false, updateMeta2=false;
    for(auto& d : diff)
    {
        // NOTE: we're intentionally comparing the optional<FilesystemElement>
        // and not the FilesystemElement as this covers also the cases where
        // items are missing
        if(d[0]==d[1] && d[0]!=d[2])
        {
            cout<<d<<"Assuming metadata file 2 inconsistent in this case.\n";
            updateMeta2=true;
        } else if(d[0]==d[2] && d[0]!=d[1]) {
            cout<<d<<"Assuming metadata file 1 inconsistent in this case.\n";
            updateMeta1=true;
        } else if(d[1]==d[2] && d[0]!=d[1]) {
            cout<<d<<"Metadata files are consistent between themselves "
                <<"but differ from backup directory content. Trying to fix this.\n";
            if(tryToFixBackupFile(src,srcTree,dst,d)==false) recovered=false;
        } else if(d[0]!=d[1] && d[1]!=d[2]) {
            cout<<d<<"Metadata files are inconsistent both among themselves "
                <<"and with backup directory content. Nothing can be done.\n";
            recovered=false;
        } else assert(false); //Invalid diff
        cout<<'\n';
    }
    cout<<"Inconsistencies processed.\n";

    if(recovered)
    {
        if(updateMeta1)
        {
            cout<<"Replacing metadata file 1 with current backup directory "
                <<"metadata (previous version saved with .bak extension)\n";
            auto meta1bak=meta1;
            meta1bak+=".bak";
            rename(meta1,meta1bak);
            dstTree.writeTo(meta1);
        }
        if(updateMeta2)
        {
            cout<<"Replacing metadata file 2 with current backup directory "
                <<"metadata (previous version saved with .bak extension)\n";
            auto meta2bak=meta2;
            meta2bak+=".bak";
            rename(meta2,meta2bak);
            dstTree.writeTo(meta2);
        }
        cout<<yellowb<<"Inconsitencies found"<<reset<<" but it was possible to "
            <<"automatically reconcile them.\nBackup directory is now good.\n";
        return 1;
    } else {
        cout<<redb<<"Unrecoverable inconsistencies found."<<reset<<" You will "
            <<"need to manually fix the backup directory.\n";
        return 2;
    }
}

void scanSourceTargetDir(const path& src, const path& dst, bool threads,
    ScanOpt opt, DirectoryTree& srcTree, DirectoryTree& dstTree)
{
    if(threads)
    {
        string foregroundException;
        string backgroundException;
        thread t([&](){
            try {
                srcTree.scanDirectory(src,opt);
            } catch(exception& e) {
                backgroundException=e.what();
            }
        });
        try {
            dstTree.scanDirectory(dst,opt);
        } catch(exception& e) {
            foregroundException=e.what();
        }
        t.join();
        if(backgroundException.empty()==false)
        {
            if(foregroundException.empty()==false) foregroundException+=' ';
            foregroundException+=backgroundException;
        }
        if(foregroundException.empty()==false)
            throw runtime_error(foregroundException);
    } else {
        srcTree.scanDirectory(src,opt);
        dstTree.scanDirectory(dst,opt);
    }
}

int scrub(const path& dst, const path& meta1, const path& meta2)
{
    cout<<"Scrubbing backup directory "<<dst<<"\n"
        <<"by comparing it with metadata files:\n- "<<meta1<<"\n- "<<meta2<<"\n"
        <<"Scanning backup directory... "; cout.flush();
    DirectoryTree dstTree;
    dstTree.scanDirectory(dst);
    cout<<"Done.\n";
    return scrubImpl(nullptr,nullptr,dst,dstTree,meta1,meta2);
}

int scrub(const path& src, const path& dst, const path& meta1, const path& meta2,
    bool threads)
{
    cout<<"Scrubbing backup directory "<<dst<<"\n"
        <<"by comparing it with metadata files:\n- "<<meta1<<"\n- "<<meta2<<"\n"
        <<"and with source directory "<<src<<"\n"
        <<"Scanning source and backup directory... "; cout.flush();
    DirectoryTree srcTree;
    DirectoryTree dstTree;
    scanSourceTargetDir(src,dst,threads,ScanOpt::ComputeHash,srcTree,dstTree);
    cout<<"Done.\n";
    return scrubImpl(&src,&srcTree,dst,dstTree,meta1,meta2);
}
