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
 * This helper class handles loading the directory trees in memory needed for
 * doing backups, and handles the logic behind saving metadata files.
 */
class TreeManager
{
public:
    /**
     * Constructor with source tree. Scan source and backup directories, and
     * load metadata files.
     * \param src source directory path (directory to be backed up)
     * \param dst destination (backup) directory path
     * \param meta1 first copy of the metadata for the destination directory
     * \param meta2 second copy of the metadata for the destination directory
     * \param opt scan options
     * \param threads if true, scan in parallel
     * \param warningCallback warning callback
     * \throws exception if scanning or loading fails
     */
    TreeManager(const path& src, const path& dst, const path& meta1,
                const path& meta2, ScanOpt opt, bool threads,
                function<void (const string&)> warningCallback)
        : meta1(meta1), meta2(meta2), srcTreePresent(true)
    {
        loadMetadataFiles(meta1Tree,meta2Tree,meta1,meta2,warningCallback);
        scanSourceTargetDir(src,dst,threads,opt,srcTree,dstTree,warningCallback);
    }

    /**
     * Constructor without source tree. Scan only backup directories, and
     * load metadata files.
     * \param dst destination (backup) directory path
     * \param meta1 first copy of the metadata for the destination directory
     * \param meta2 second copy of the metadata for the destination directory
     * \param opt scan options
     * \param warningCallback warning callback
     * \throws exception if scanning or loading fails
     */
    TreeManager(const path& dst, const path& meta1, const path& meta2,
                ScanOpt opt, function<void (const string&)> warningCallback)
        : meta1(meta1), meta2(meta2), srcTreePresent(false)
    {
        loadMetadataFiles(meta1Tree,meta2Tree,meta1,meta2,warningCallback);
        cout<<"Scanning backup directory... "; cout.flush();
        if(warningCallback) dstTree.setWarningCallback(warningCallback);
        dstTree.scanDirectory(dst,opt);
        cout<<"Done.\n";
    }

    /**
     * \return true if the TreeManager was constructed with the source directory
     */
    bool hasSourceTree() const { return srcTreePresent; }

    /**
     * \return the source directory tree, can only be called if hasSourceTree()==true
     */
    const DirectoryTree& getSrcTree() const
    {
        assert(srcTreePresent);
        return srcTree;
    }

    /**
     * \return the backup directory tree
     */
    DirectoryTree& getDstTree() { return dstTree; }

    /**
     * \return the first metadata directory tree
     */
    DirectoryTree& getMeta1Tree() { return meta1Tree; }

    /**
     * \return the second metadata directory tree.
     * After a call to discardMeta2Tree() can no longer be called.
     */
    DirectoryTree& getMeta2Tree()
    {
        assert(meta2TreePresent);
        return meta2Tree;
    }

    /**
     * Discard the second metadata tree after scrubbing to save memory.
     * When saving metadata files, the first tree is saved to both files.
     */
    void discardMeta2Tree()
    {
        meta2Tree.clear();
        meta2TreePresent=false;
    }

    /**
     * If called, when this object is destructed, the metadata files will
     * be saved to disk
     */
    void saveMetadataOnExit() { save=true; }

    /**
     * If called and saveMetadataOnExit() is called too, when this object is
     * destructed a backup copy of the first metadata file will be kept
     */
    void saveMeta1PreviousVersion() { meta1NeedsBackup=true; }

    /**
     * If called and saveMetadataOnExit() is called too, when this object is
     * destructed a backup copy of the second metadata file will be kept
     */
    void saveMeta2PreviousVersion() { meta2NeedsBackup=true; }

    /**
     * Destructor, saves metadata files to disk if saveMetadataOnExit() has
     * been called
     */
    ~TreeManager();

private:
    /**
     * \param meta1Tree first metadata file will be loaded here
     * \param meta2Tree first metadata file will be loaded here
     * \param meta1 path to first copy of the metadata path
     * \param meta2 path to second copy of the metadata path
     * \param warningCallback warning callback
     */
    void loadMetadataFiles(DirectoryTree& meta1Tree, DirectoryTree& meta2Tree,
                       const path& meta1, const path& meta2,
                       function<void (const string&)> warningCallback);

    DirectoryTree srcTree, dstTree, meta1Tree, meta2Tree;
    const path meta1, meta2;
    const bool srcTreePresent;
    bool meta2TreePresent=true;
    bool save=false, meta1NeedsBackup=false, meta2NeedsBackup=false;
};

void TreeManager::loadMetadataFiles(DirectoryTree& meta1Tree,
    DirectoryTree& meta2Tree, const path& meta1, const path& meta2,
    function<void (const string&)> warningCallback) try
{
    cout<<"Loading metatata files... "; cout.flush();
    if(warningCallback) meta1Tree.setWarningCallback(warningCallback);
    if(warningCallback) meta2Tree.setWarningCallback(warningCallback);
    meta1Tree.readFrom(meta1);
    meta2Tree.readFrom(meta2);
    cout<<"Done.\n";
} catch(exception& e) {
    cout<<e.what()<<"\nIt looks like at least one of the metadata files is "
        <<"corrupted to the point that it cannot be read. The cause may be "
        <<"an unclean unmount of the filesystem (did you run an fsck?), "
        <<"you tried to edit a metadata file with a text editor or "
        <<"bit rot occurred in a metadata file.\n"
        <<redb<<"Unrecoverable inconsistencies found."<<reset<<" You will "
        <<"need to manually fix the backup directory, possibly by "
        <<"recreating metadata files and replacing the corrupted one(s).\n"
        <<"the 'ddm diff' command may help to troubleshoot bad metadata.\n";
    throw;
}

TreeManager::~TreeManager()
{
    if(save==false) return;
    cout<<"Updating metadata file 1\n";
    if(meta1NeedsBackup)
    {
        auto meta1bak=meta1;
        meta1bak+=".bak";
        rename(meta1,meta1bak);
    }
    meta1Tree.writeTo(meta1);
    cout<<"Updating metadata file 2\n";
    if(meta2NeedsBackup)
    {
        auto meta2bak=meta2;
        meta2bak+=".bak";
        rename(meta2,meta2bak);
    }
    if(meta2TreePresent) meta2Tree.writeTo(meta2);
    else meta1Tree.writeTo(meta2); //Not a mistake, write meta1Tree to both files
}

/**
 * Get a yes/no answer from the user
 * \return true if the user entered 'y'
 */
static bool askYesNo()
{
    char c;
    do {
        c=tolower(cin.get());
    } while(c!='y' && c!='n');
    return c=='y';
}

/**
 * Return value of tryToFixBackupEntry
 */
enum class FixupResult
{
    Failed,
    Success,
    SuccessDiffInvalidated,
    SuccessMetadataInvalidated,
    SuccessDiffMetadataInvalidated
};

/**
 * Try to fix an inconsistent backup state found during a scrub.
 * This function only sees a single difference, and is called to handle the
 * more difficult case when the metadata files agree but the content of the
 * backup directory differs from them.
 * \param srcTree freshly scanned metadata for src
 * NOTE: may be nullptr if source dir not given by the user
 * \param dstTree freshly scanned metadata for dst
 * \param meta1Tree first metadata file loaded as tree
 * \param meta2Tree first metadata file loaded as tree
 * \param diff diff line representing the inconsistent state to try fixing
 * \return fixup result
 */
static FixupResult tryToFixBackupEntry(const DirectoryTree *srcTree,
                                       DirectoryTree& dstTree,
                                       DirectoryTree& meta1Tree,
                                       DirectoryTree& meta2Tree,
                                       const DirectoryDiffLine<3>& d)
{
    assert(d[1]==d[2]);
    if(!d[0])
    {
        path relPath=d[1].value().relativePath();
        string type=d[1].value().typeAsString();
        file_type ty=d[1].value().type();
        cout<<"The "<<type<<" "<<relPath<<" is missing in the backup directory "
            <<"but the metadata files agree it should be there.\n";
        //Symlinks are special, as the metadata file contains enough information
        //(the link target) to recreate them
        if(ty==file_type::symlink)
        {
            cout<<"Creating the missing symbolic link.\n";
            dstTree.addSymlinkToTreeAndFilesystem(d[1].value());
            return FixupResult::Success;
        }
        //Handling regular files and directories
        if(srcTree==nullptr)
        {
            cout<<"If you re-run the scrub giving me also the source directory "
                <<"(-s option) I may be able to help by looking for the "<<type
                <<" there, but until then, there's nothing I can do.\n";
            return FixupResult::Failed;
        } else {
            cout<<"Trying to see if I can find the missing "<<type<<" in the "
                <<"source directory.\n";
            auto item=srcTree->search(relPath);
            if(item.has_value()==false)
            {
                cout<<"The "<<type<<" was not found. There's nothing I can do, "
                    <<"but I recommend to double check the source directory "
                    <<"path. If it's wrong, please re-run the command with the "
                    <<"correct path. If it's correct, please check the source "
                    <<"directory manually, if the "<<type<<" really isn't there "
                    <<"maybe it was deleted manually both there and in the "
                    <<"backup directory. If this is the only error you could "
                    <<"delete and recreate the metadata files.\n";
                return FixupResult::Failed;
            }
            if(item.value()==d[1].value())
            {
                cout<<"The "<<type<<" was found in the source directory and "
                    <<"matches with the backup metadata.\n"
                    <<"Copying it back into the backup directory.\n";
                dstTree.copyFromTreeAndFilesystem(*srcTree,relPath,relPath.parent_path());
                return ty==file_type::directory ? FixupResult::SuccessDiffInvalidated
                                                : FixupResult::Success;
            } else {
                cout<<"An entry was found in the source directory however, its "
                    <<"properties\n"<<item.value()<<"\ndo not match the missing "
                    <<type<<".\n";
                CompareOpt opt;
                opt.perm=false;
                opt.owner=false;
                opt.mtime=false;
                if(compare(item.value(),d[1].value(),opt))
                {
                    cout<<"However, the content is the same, updating backup.\n";
                    dstTree.copyFromTreeAndFilesystem(*srcTree,relPath,
                                                      relPath.parent_path());
                    if(item.value().permissions()!=d[1].value().permissions())
                    {
                        auto perm=item.value().permissions();
                        meta1Tree.modifyPermissionsInTree(relPath,perm);
                        meta2Tree.modifyPermissionsInTree(relPath,perm);
                    }
                    if(item.value().user()!=d[1].value().user() ||
                       item.value().group()!=d[1].value().group())
                    {
                        auto u=item.value().user();
                        auto g=item.value().group();
                        meta1Tree.modifyOwnerInTree(relPath,u,g);
                        meta2Tree.modifyOwnerInTree(relPath,u,g);
                    }
                    if(item.value().mtime()!=d[1].value().mtime())
                    {
                        auto mtime=item.value().mtime();
                        meta1Tree.modifyMtimeInTree(relPath,mtime);
                        meta2Tree.modifyMtimeInTree(relPath,mtime);
                    }
                    if(ty==file_type::directory)
                        return FixupResult::SuccessDiffMetadataInvalidated;
                    return FixupResult::SuccessMetadataInvalidated;
                } else {
                    cout<<"And the difference includes the entry content. "
                        <<"However, as the entry in the backup is gone, and "
                        <<"the source directory has changed, the best I can "
                        <<"do is copy the new entry to the backup.\n";
                    dstTree.copyFromTreeAndFilesystem(*srcTree,relPath,
                                                      relPath.parent_path());
                    meta1Tree.removeFromTree(relPath);
                    meta1Tree.copyFromTree(*srcTree,relPath,relPath.parent_path());
                    meta2Tree.removeFromTree(relPath);
                    meta2Tree.copyFromTree(*srcTree,relPath,relPath.parent_path());
                    if(item.value().isDirectory() || d[1].value().isDirectory())
                        return FixupResult::SuccessDiffMetadataInvalidated;
                    return FixupResult::SuccessMetadataInvalidated;
                }
            }
        }
    } else if(!d[1]) {
        path relPath=d[0].value().relativePath();
        string type=d[0].value().typeAsString();
        file_type ty=d[0].value().type();
        cout<<"The "<<type<<" "<<relPath<<" is present in the backup "
            <<"directory but the metadata files agree it should not be there.\n"
            <<"Do you want to DELETE it? [y/n]\n";
        if(askYesNo()==false) return FixupResult::Failed;
        cout<<"Removing the "<<type<<".\n";
        int cnt=dstTree.removeFromTreeAndFilesystem(relPath);
        cout<<"Removed "<<cnt<<" files or directories.\n";
        return ty==file_type::directory ? FixupResult::SuccessDiffInvalidated
                                        : FixupResult::Success;
    } else {
        path relPath=d[1].value().relativePath();
        string type=d[1].value().typeAsString();
        file_type ty=d[1].value().type();
        cout<<"The metadata files agree on the properties of the "<<type<<" "
            <<relPath<<" but the entry in the backup directory differs.\n";
        CompareOpt opt;
        opt.perm=false;
        opt.owner=false;
        opt.mtime=false;
        if(compare(d[0].value(),d[1].value(),opt))
        {
            cout<<"However, the content is the same, updating backup directory.\n";
            if(d[0].value().permissions()!=d[1].value().permissions())
            {
                auto perm=d[1].value().permissions();
                dstTree.modifyPermissionsInTreeAndFilesystem(relPath,perm);
            }
            if(d[0].value().user()!=d[1].value().user() ||
               d[0].value().group()!=d[1].value().group())
            {
                auto u=d[1].value().user();
                auto g=d[1].value().group();
                dstTree.modifyOwnerInTreeAndFilesystem(relPath,u,g);
            }
            if(d[0].value().mtime()!=d[1].value().mtime())
            {
                auto mtime=d[1].value().mtime();
                dstTree.modifyMtimeInTreeAndFilesystem(relPath,mtime);
            }
            return FixupResult::Success;
        } else {
            cout<<"And the difference includes the entry content.\n";
            if(ty!=d[0].value().type())
                cout<<yellowb<<"Also, the types differ!"<<reset<<"\n";

            bool bitrot=false;
            CompareOpt opt;
            opt.size=false;
            opt.hash=false;
            opt.symlink=false;
            if(compare(d[0].value(),d[1].value(),opt))
            {
                bitrot=true;
                cout<<redb<<"Bit rot in the backup directory detected."
                    <<reset<<" The content of a file changed but the modified "
                    <<"time did not. I suggest running a SMART check as your "
                    <<"backup disk may be unreliable.\n";
            }

            //Symlinks are special, as the metadata file contains enough
            //information (the link target) to recreate them
            if(ty==file_type::symlink && d[0].value().type()==file_type::symlink)
            {
                if(bitrot==false)
                {
                    cout<<"Do you want to UPDATE the symbolic link? [y/n]\n";
                    if(askYesNo()==false) return FixupResult::Failed;
                }
                cout<<"First removing the old symbolic link.\n";
                int cnt=dstTree.removeFromTreeAndFilesystem(relPath);
                cout<<"Removed "<<cnt<<" entry. Creating updated symbolic link.\n";
                dstTree.addSymlinkToTreeAndFilesystem(d[1].value());
                return FixupResult::Success;
            }
            //Handling regular files and directories
            if(srcTree==nullptr)
            {
                cout<<"If you re-run the scrub giving me also the source directory "
                    <<"(-s option) I may be able to help by looking for the "<<type
                    <<" there, but until then, there's nothing I can do.\n";
                return FixupResult::Failed;
            } else {
                cout<<"Trying to see if I can find the missing "<<type<<" in the "
                    <<"source directory.\n";
                auto item=srcTree->search(relPath);
                if(item.has_value()==false)
                {
                    cout<<"The "<<type<<" was not found. There's nothing I can do, "
                        <<"but I recommend to double check the source directory "
                        <<"path. If it's wrong, please re-run the command with the "
                        <<"correct path. If it's correct, please check the source "
                        <<"directory manually, if the "<<type<<" really isn't there "
                        <<"maybe it was deleted manually both there and in the "
                        <<"backup directory. If this is the only error you could "
                        <<"delete and recreate the metadata files.\n";
                    return FixupResult::Failed;
                }
                if(item.value()==d[1].value())
                {
                    cout<<"The "<<type<<" was found in the source directory and "
                        <<"matches with the backup metadata.\n";
                    if(bitrot==false)
                    {
                        cout<<"Do you want to DELETE the "<<d[0].value().typeAsString()
                            <<" in the backup directory and REPLACE it with the "
                            <<type<<" in the source directory? [y/n]\n";
                        if(askYesNo()==false) return FixupResult::Failed;
                    }
                    int cnt=dstTree.removeFromTreeAndFilesystem(relPath);
                    cout<<"Removed "<<cnt<<" files or directories.\nReplacing "
                        <<"the content of the backup directory with the one of "
                        <<"the source directory.\n";
                    dstTree.copyFromTreeAndFilesystem(*srcTree,relPath,relPath.parent_path());
                    if(ty==file_type::directory || d[0].value().isDirectory())
                        return FixupResult::SuccessDiffInvalidated;
                    return FixupResult::Success;
                } else {
                    cout<<"An entry was found in the source directory however, its "
                        <<"properties\n"<<item.value()<<"\ndo not match the missing "
                        <<type<<".\n";
                    if(item.value()==d[0].value())
                    {
                        cout<<"But the source directory matches with the backup "
                            <<"directory.\nDid you do a backup without updating "
                            <<"the backup metadata? Assuming the metadata is not "
                            <<"up to date.\n";
                        meta1Tree.removeFromTree(relPath);
                        meta1Tree.copyFromTree(*srcTree,relPath,relPath.parent_path());
                        meta2Tree.removeFromTree(relPath);
                        meta2Tree.copyFromTree(*srcTree,relPath,relPath.parent_path());
                        cout<<"Metadata updated to reflect the source and backup.\n";
                        if(bitrot)
                            cout<<yellowb<<"About the bit rot."<<reset
                                <<"Either you restored a backup and that explains "
                                <<"why the source and backup directory are the same "
                                <<"and in this case you overwrote the good file, "
                                <<"or something strange happened to the mtime.\n";
                        if(item.value().isDirectory() || ty==file_type::directory)
                            return FixupResult::SuccessDiffMetadataInvalidated;
                        return FixupResult::SuccessMetadataInvalidated;
                    } else {
                        if(item.value().type()!=d[1].value().type())
                            cout<<yellowb<<"Also, the types differ!"<<reset<<"\n";
                    }
                    CompareOpt opt;
                    opt.perm=false;
                    opt.owner=false;
                    opt.mtime=false;
                    if(compare(item.value(),d[0].value(),opt))
                    {
                        cout<<"However, the content is the same, updating backup.\n";
                        if(item.value().permissions()!=d[1].value().permissions())
                        {
                            auto perm=item.value().permissions();
                            dstTree.modifyPermissionsInTreeAndFilesystem(relPath,perm);
                        }
                        if(item.value().user()!=d[1].value().user() ||
                        item.value().group()!=d[1].value().group())
                        {
                            auto u=item.value().user();
                            auto g=item.value().group();
                            dstTree.modifyOwnerInTreeAndFilesystem(relPath,u,g);
                        }
                        if(item.value().mtime()!=d[1].value().mtime())
                        {
                            auto mtime=item.value().mtime();
                            dstTree.modifyMtimeInTreeAndFilesystem(relPath,mtime);
                        }
                        //in this case src and backup directories differ only in
                        // metadata, but metadata files differ in content!
                        cout<<"Updating metadata files too.\n";
                        meta1Tree.removeFromTree(relPath);
                        meta1Tree.copyFromTree(*srcTree,relPath,relPath.parent_path());
                        meta2Tree.removeFromTree(relPath);
                        meta2Tree.copyFromTree(*srcTree,relPath,relPath.parent_path());
                        if(bitrot)
                            cout<<yellowb<<"About the bit rot."<<reset
                                <<"Either you restored a backup and that explains "
                                <<"why the source and backup directory are the same "
                                <<"and in this case you overwrote the good file, "
                                <<"or something strange happened to the mtime.\n";
                        if(ty==file_type::directory || d[0].value().isDirectory())
                            return FixupResult::SuccessDiffMetadataInvalidated;
                        return FixupResult::SuccessMetadataInvalidated;
                    } else {
                        cout<<"And the difference includes the entry content.\n"
                            <<"Do you want to DELETE the "<<d[0].value().typeAsString()
                            <<" in the backup directory and REPLACE it with the "
                            <<item.value().typeAsString()<<" in the source "
                            <<"directory? [y/n]\n";
                        if(askYesNo()==false) return FixupResult::Failed;
                        int cnt=dstTree.removeFromTreeAndFilesystem(relPath);
                        cout<<"Removed "<<cnt<<" files or directories.\nReplacing "
                            <<"the content of the backup directory with the one of "
                            <<"the source directory.\n";
                        dstTree.copyFromTreeAndFilesystem(*srcTree,relPath,
                                                        relPath.parent_path());
                        meta1Tree.removeFromTree(relPath);
                        meta1Tree.copyFromTree(*srcTree,relPath,relPath.parent_path());
                        meta2Tree.removeFromTree(relPath);
                        meta2Tree.copyFromTree(*srcTree,relPath,relPath.parent_path());
                        if(ty==file_type::directory || item.value().isDirectory()
                           || d[0].value().isDirectory())
                            return FixupResult::SuccessDiffMetadataInvalidated;
                        return FixupResult::SuccessMetadataInvalidated;
                    }
                }
            }
        }
    }
    return FixupResult::Failed;
}

/**
 * Fix an inconsistent metadata state found during a scrub.
 * This function only sees a single difference.
 * \param goodTree good tree, used for copying the good entry if present
 * \param badTree metadata tree to fix
 * \param goodEntry diff entry representing the good filesystem state
 * \param badEntry diff entry representing the bad filesystem state
 * \return fixup result
 */
static FixupResult fixMetadataEntry(DirectoryTree& goodTree,
                                    DirectoryTree& badTree,
                                    const optional<FilesystemElement>& goodEntry,
                                    const optional<FilesystemElement>& badEntry)
{
    //NOTE: this could be optimized to look into the actual difference, as for
    //example if a directory differs only in the modifies time this code would
    //remove the entire directory and recreate it. However, this code only
    //works with an in-memory data structure and not with actual files and
    //directories, so this inefficient implementation is not that slow, and
    //optimizing it will make it as complicated as tryToFixBackupEntry
    if(badEntry.has_value())
        badTree.removeFromTree(badEntry.value().relativePath());

    if(goodEntry.has_value())
    {
        path relPath=goodEntry.value().relativePath();
        badTree.copyFromTree(goodTree,relPath,relPath.parent_path());
    }

    if((goodEntry.has_value() && goodEntry.value().isDirectory()) ||
       (badEntry.has_value()  && badEntry.value().isDirectory()))
        return FixupResult::SuccessDiffMetadataInvalidated;
    return FixupResult::SuccessMetadataInvalidated;
}

/**
 * Implementation code that scrubs the backup directory
 * \param tm the tree manager containing:
 * - freshly scanned metadata tree for src (optional)
 * - freshly scanned metadata tree for dst
 * - first copy of the metadata tree
 * - second copy of the metadata tree
 * \param fixup if true, attempt to fix inconsistencies in the backup directory
 * \return 0 if no action was needed,
 *         1 if recoverable errors found and fixed
 *         2 if unrecoverable errors found
 */
static int scrubImpl(TreeManager& tm, bool fixup)
{
    cout<<"Comparing backup directory with metadata... "; cout.flush();
    auto diff=diff3(tm.getDstTree(),tm.getMeta1Tree(),tm.getMeta2Tree());
    cout<<"Done.\n";

    if(diff.empty())
    {
        cout<<greenb<<"Scrub complete."<<reset<<" No differences found.\n";
        return 0;
    }
    cout<<yellowb<<"Inconsitencies found."<<reset
        <<" Processing them one by one.\nNote: in the following diff a is "
        <<"the backup directory, b is metadata file 1 while c is metadata file 2\n";
    bool unrecoverable=false, maybeRecoverable=false, redo=false,
         updateMeta1=false, updateMeta2=false;
    do {
        if(redo)
        {
            redo=false;
            cout<<"\nThe fixup operation modified the backup directory content "
                <<"in a way that invalidated the list of inconsistencies. Rechecking.\n"
                <<"Comparing backup directory with metadata... "; cout.flush();
            diff=diff3(tm.getDstTree(),tm.getMeta1Tree(),tm.getMeta2Tree());
            cout<<"Done.\n";
        }
        for(auto& d : diff)
        {
            // NOTE: we're intentionally comparing the optional<FilesystemElement>
            // and not the FilesystemElement as this covers also the cases where
            // items are missing
            if(d[0]==d[1] && d[0]!=d[2])
            {
                cout<<d<<"Assuming metadata file 2 inconsistent in this case.\n";
                auto result=fixMetadataEntry(tm.getDstTree(),tm.getMeta2Tree(),d[0],d[2]);
                updateMeta2=true;
                if(result==FixupResult::SuccessDiffMetadataInvalidated)
                {
                    redo=true;
                    break;
                }
            } else if(d[0]==d[2] && d[0]!=d[1]) {
                cout<<d<<"Assuming metadata file 1 inconsistent in this case.\n";
                auto result=fixMetadataEntry(tm.getDstTree(),tm.getMeta1Tree(),d[0],d[1]);
                updateMeta1=true;
                if(result==FixupResult::SuccessDiffMetadataInvalidated)
                {
                    redo=true;
                    break;
                }
            } else if(d[1]==d[2] && d[0]!=d[1]) {
                cout<<d<<"Metadata files are consistent between themselves "
                    <<"but differ from backup directory content.\n";
                if(fixup)
                {
                    cout<<"Trying to fix this.\n";
                    const DirectoryTree *src=nullptr;
                    if(tm.hasSourceTree()) src=&tm.getSrcTree();
                    auto result=tryToFixBackupEntry(src,tm.getDstTree(),
                                                    tm.getMeta1Tree(),
                                                    tm.getMeta2Tree(),d);
                    switch(result)
                    {
                        case FixupResult::Success:
                            break;
                        case FixupResult::Failed:
                            unrecoverable=true;
                            break;
                        case FixupResult::SuccessDiffInvalidated:
                            redo=true;
                            break;
                        case FixupResult::SuccessMetadataInvalidated:
                            updateMeta1=updateMeta2=true;
                            break;
                        case FixupResult::SuccessDiffMetadataInvalidated:
                            updateMeta1=updateMeta2=redo=true;
                            break;
                    }
                    if(redo) break;
                } else {
                    cout<<"Not attempting to fix this because --fixup option "
                        <<"not given.\n";
                    maybeRecoverable=true;
                }
            } else if(d[0]!=d[1] && d[1]!=d[2]) {
                cout<<d<<"Metadata files are inconsistent both among themselves "
                    <<"and with backup directory content. Nothing can be done.\n";
                unrecoverable=true;
            } else assert(false); //Invalid diff
            cout<<'\n';
        }
    } while(redo==true);
    cout<<"Inconsistencies processed.\n";

    if(unrecoverable==false && maybeRecoverable==false)
    {
        tm.saveMetadataOnExit();
        if(updateMeta1) tm.saveMeta1PreviousVersion();
        if(updateMeta2) tm.saveMeta2PreviousVersion();
        cout<<yellowb<<"Inconsitencies found"<<reset<<" but it was possible to "
            <<"automatically reconcile them.\nBackup directory is now good.\n";
        return 1;
    } else if(unrecoverable) {
        cout<<redb<<"Unrecoverable inconsistencies found."<<reset<<" You will "
            <<"need to manually fix the backup directory.\n";
        if(maybeRecoverable)
        {
            cout<<"Some inconsistencies may be automatically recoverable by "
                <<"running again this command with the --fixup option.\n";
            if(tm.hasSourceTree()==false)
                cout<<"You may want to give me access to the source diectory "
                    <<"as well (-s option)\n";
        }
        return 2;
    } else {
        cout<<redb<<"Unrecovered inconsistencies found."<<reset<<" However it "
            <<"looks like it is possible to attempt recovering all "
            <<"inconsistencies automatically by running this command again "
            <<"and adding the --fixup option.\n";
        if(tm.hasSourceTree()==false)
            cout<<"You may want to give me access to the source diectory "
                <<"as well (-s option)\n";
        return 2;
    }
}

void scanSourceTargetDir(const path& src, const path& dst, bool threads,
    ScanOpt opt, DirectoryTree& srcTree, DirectoryTree& dstTree,
    function<void (const string&)> warningCallback)
{
    cout<<"Scanning source and backup directory... "; cout.flush();
    if(warningCallback) srcTree.setWarningCallback(warningCallback);
    if(warningCallback) dstTree.setWarningCallback(warningCallback);
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
    cout<<"Done.\n";
}

int scrub(const path& dst, const path& meta1, const path& meta2, bool fixup,
          function<void (const string&)> warningCallback)
{
    cout<<"Scrubbing backup directory "<<dst<<"\n"
        <<"by comparing it with metadata files:\n- "<<meta1<<"\n- "<<meta2<<"\n";
    TreeManager tm(dst,meta1,meta2,ScanOpt::ComputeHash,warningCallback);
    return scrubImpl(tm,fixup);
}

int scrub(const path& src, const path& dst, const path& meta1, const path& meta2,
          bool fixup, bool threads, function<void (const string&)> warningCallback)
{
    cout<<"Scrubbing backup directory "<<dst<<"\n"
        <<"by comparing it with metadata files:\n- "<<meta1<<"\n- "<<meta2<<"\n"
        <<"and with source directory "<<src<<"\n";
    TreeManager tm(src,dst,meta1,meta2,ScanOpt::ComputeHash,threads,warningCallback);
    return scrubImpl(tm,fixup);
}

/**
 * Perform a backup by comparing the source and target directories, and applying
 * differences so the target directory becomes equal to the source
 * \param scrTree source directory
 * \param dstTree backup directory
 * \param metaTree optional metadata tree
 * \return 0 on success,
 *         1 if recoverable errors found and fixed
 *         2 if unrecoverable errors found
 */
static int backupImpl(const DirectoryTree& srcTree, DirectoryTree& dstTree,
                      DirectoryTree *metaTree=nullptr)
{
    cout<<"Performing backup.\n"
        <<"Comparing source directory with backup directory... "; cout.flush();
    auto diff=diff2(srcTree,dstTree);
    cout<<"Done.\n";

    bool bitrot=false;
    if(diff.empty()) cout<<"No differences found.\n";
    for(auto& d : diff)
    {
        // NOTE: we're intentionally comparing the optional<FilesystemElement>
        // and not the FilesystemElement as this covers also the cases where
        // items are missing
        if(!d[0])
        {
            path relPath=d[1].value().relativePath();
            cout<<"- Removing "<<d[1].value().typeAsString()<<" "<<relPath
                <<" from backup directory.\n";
            dstTree.removeFromTreeAndFilesystem(relPath);
            if(metaTree) metaTree->removeFromTree(relPath);
        } else if(!d[1]) {
            path relPath=d[0].value().relativePath();
            cout<<"- Copying "<<d[0].value().typeAsString()<<" "<<relPath
                <<" to backup directory.\n";
            dstTree.copyFromTreeAndFilesystem(srcTree,relPath,relPath.parent_path());
            if(metaTree) metaTree->copyFromTree(srcTree,relPath,relPath.parent_path());
        } else {
            path relPath=d[0].value().relativePath();
            CompareOpt opt;
            opt.perm=false;
            opt.owner=false;
            if(d[0].value().type()!=file_type::regular ||
               d[1].value().type()!=file_type::regular)
            {
                opt.mtime=false; //The issue below applies only to regular files
            } else {
                //NOTE: when doing a backup with full hash computation if the
                //files differ only by the mtime, we can detect the hashes are
                //the same and only modify the mtime. When hash computation is
                //omitted, however, we must treat files with different mtime as
                //modified and do a full file copy otherwise if a file changed
                //but its size remained the same it will not be backed up!
                if(d[0].value().hash().empty()==false &&
                   d[1].value().hash().empty()==false) opt.mtime=false;
            }
            if(compare(d[0].value(),d[1].value(),opt))
            {
                cout<<"- Updating the metadata of the "
                    <<d[0].value().typeAsString()<<" "<<relPath
                    <<" in the backup directory.\n";
                if(d[0].value().permissions()!=d[1].value().permissions())
                {
                    auto perm=d[0].value().permissions();
                    dstTree.modifyPermissionsInTreeAndFilesystem(relPath,perm);
                    if(metaTree) metaTree->modifyPermissionsInTree(relPath,perm);
                }
                if(d[0].value().user()!=d[1].value().user() ||
                    d[0].value().group()!=d[1].value().group())
                {
                    auto u=d[0].value().user();
                    auto g=d[0].value().group();
                    dstTree.modifyOwnerInTreeAndFilesystem(relPath,u,g);
                    if(metaTree) metaTree->modifyOwnerInTree(relPath,u,g);
                }
                if(d[0].value().mtime()!=d[1].value().mtime())
                {
                    auto mtime=d[0].value().mtime();
                    dstTree.modifyMtimeInTreeAndFilesystem(relPath,mtime);
                    if(metaTree) metaTree->modifyMtimeInTree(relPath,mtime);
                }
            } else {
                CompareOpt opt;
                opt.size=false;
                opt.hash=false;
                opt.symlink=false;
                if(compare(d[0].value(),d[1].value(),opt))
                {
                    bitrot=true;
                    cout<<redb<<"Bit rot in the source directory detected."
                        <<reset<<" The content of the "<<d[0].value().typeAsString()
                        <<" "<<relPath<<" changed but the modified time did not.\n"
                        <<"NOT backing up this "<<d[0].value().typeAsString()
                        <<" as the backup copy may be good one.\n";
                } else {
                    bool replace=true;
                    if(d[0].value().mtime()<d[1].value().mtime())
                    {
                        cout<<d<<"The "<<d[1].value().typeAsString()<<" "
                            <<relPath<<" in the backup directory is newer than "
                            <<"the "<<d[0].value().typeAsString()<<" in the "
                            <<"source directory, (did you write something "
                            <<"directly in the backup directory?)\nDo you want "
                            <<"me to DELETE the backup entry and REPLACE it "
                            <<"with the entry in the source directory? [y/n]\n";
                        if(askYesNo()==false)
                        {
                            replace=false;
                            cout<<yellowb<<"Leaving backup inconsistent."<<reset
                                <<" Note that you have to solve this manually, "
                                <<"and consider that the "
                                <<d[0].value().typeAsString()<<" in the source "
                                <<"directory is currently without a backup.\n";
                        }
                    }
                    if(replace)
                    {
                        cout<<"- Replacing the "<<d[1].value().typeAsString()
                            <<" "<<relPath<<" in the backup directory with the "
                            <<d[0].value().typeAsString()<<" in the source directory.\n";
                        dstTree.removeFromTreeAndFilesystem(relPath);
                        dstTree.copyFromTreeAndFilesystem(srcTree,relPath,
                                                          relPath.parent_path());
                        if(metaTree)
                        {
                            metaTree->removeFromTree(relPath);
                            metaTree->copyFromTree(srcTree,relPath,
                                                  relPath.parent_path());
                        }
                    }
                }
            }
        }
    }
    if(bitrot)
        cout<<redb<<"Bit rot was detected in the source directory."<<reset
            <<" As this tool by design never writes into the source directory "
            <<"during a backup, you will have to fix this manually. Review the "
            <<"listed files, and if bit rot is confirmed, then manually replace "
            <<"the rotten files in the source directory with the good copy in the "
            <<"backup directory.\nI suggest also running a SMART check as your "
            <<"source disk may be unreliable.\n";
    else cout<<greenb<<"Backup complete."<<reset<<"\n";
    return bitrot ? 2 : 0;
}

int backup(const path& src, const path& dst, const path& meta1, const path& meta2,
           bool fixup, bool hashAllFiles, bool threads,
           function<void (const string&)> warningCallback)
{
    cout<<"Backing up directory "<<src<<"\nto directory "<<dst<<"\n"
        <<"and metadata files:\n- "<<meta1<<"\n- "<<meta2<<"\n";
    ScanOpt opt=hashAllFiles ? ScanOpt::ComputeHash : ScanOpt::OmitHash;
    TreeManager tm(src,dst,meta1,meta2,opt,threads,warningCallback);
    cout<<"Scrubbing backup directory.\n";
    int result=scrubImpl(tm,fixup);
    switch(result)
    {
        case 1:
            cout<<"Do you want to continue with the backup? [y/n]\n";
            if(askYesNo()==false) return result;
            break;
        case 2:
            cout<<redb<<"Refusing to perform backup to an inconsistent directory."
                <<reset<<"\n";
            return result;
    }
    //NOTE: after the scrub the two metadata trees are consistent, so only keep
    //one to save RAM. Note that even though both metadata trees should be also
    //consistent with the dstTree (which would suggest we can clear both), this
    //is not the case as when hashAllFiles is false, the dstTree has no hashes
    //for all unmodified files, while the metadata trees have them, so we need
    //one metadata tree in order to not lose hashes when writing the updated
    //metadata files
    tm.discardMeta2Tree();
    tm.saveMetadataOnExit();
    int result2=backupImpl(tm.getSrcTree(),tm.getDstTree(),&tm.getMeta1Tree());
    if(result2!=0) result=result2;
    if(hashAllFiles==false)
    {
        cout<<"Computing missing hashes in metadata files... "; cout.flush();
        tm.getMeta1Tree().bindToTopPath(dst);
        try {
            tm.getMeta1Tree().computeMissingHashes();
        } catch(exception& e) {
            cout<<redb<<"Warning:"<<reset<<" an exception was thrown while"
                <<"computing missing hashes. The metadata files may be corrupt"
                <<"in a silent way. Open them and look for an * instead of an"
                <<"hash for some files. Bit rot protection will not work"
                <<"for those files.\n";
            throw;
        }
        cout<<"Done.\n";
    }
    return result;
}

int backup(const path& src, const path& dst, bool threads,
           function<void (const string&)> warningCallback)
{
    cout<<"Backing up directory "<<src<<"\nto directory "<<dst<<"\n";
    DirectoryTree srcTree, dstTree;
    scanSourceTargetDir(src,dst,threads,ScanOpt::OmitHash,srcTree,dstTree,
                        warningCallback);
    return backupImpl(srcTree,dstTree);
}
