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
#include <iostream>
#include <ostream>
#include <istream>
#include <list>
#include <array>
#include <optional>
#include <unordered_map>
#include <functional>
#include <ctime>

/**
 * Computes SHA1 of a file. Only to detect changes, no crypto strength needed
 * \param p file path
 * \return a string with the SHA1 hash in ASCII hex digits
 */
std::string hashFile(const std::filesystem::path& p);

/**
 * Directory tree scanning options
 */
enum class ScanOpt
{
    ComputeHash, ///< When scanning directories, compute file hashes
    OmitHash     ///< When scanning directories, omit file hash computation
};

/**
 * Compare options for directory tree comparisons
 */
class CompareOpt
{
public:
    CompareOpt() {}
    CompareOpt(const std::string& ignoreString);

    bool perm=true;    ///< Compare file permissions (rwxrwxrwx)
    bool owner=true;   ///< Compare file owner/group
    bool mtime=true;   ///< Compare last modified time
    bool size=true;    ///< Compare file size
    bool hash=true;    ///< Compare file hash
    bool symlink=true; ///< Compare symlink targets
};

/**
 * This class is used to load/store information about files and directories
 * in metadata files
 */
class FilesystemElement
{
public:
    /**
     * Empty constructor
     */
    FilesystemElement();

    /**
     * Constructor from a path
     * \param p absolute path
     * \param top top level directory, used to compute relative path
     * \param opt scan options
     */
    FilesystemElement(const std::filesystem::path& p,
                      const std::filesystem::path& top,
                      ScanOpt opt=ScanOpt::ComputeHash);

    /**
     * Constructor from string, used when reading from metadata files
     * \param metadataLine line of the metadata file to construct the object from
     * \param metadataFileName name of metadata file, used for error reporting
     * \param lineNo line number of metadata file, used for error reporting
     * \throws runtime_error in case of errors
     */
    explicit FilesystemElement(const std::string& metadataLine,
                               const std::string& metadataFileName="", int lineNo=-1)
    {
        readFrom(metadataLine,metadataFileName,lineNo);
    }

    /**
     * Construct a FilesystemElement from another FilesystemElement, but
     * changing the path. Used when copying DirectoryNodes
     * \param other FilesysteElement from where all properties except for the
     * relative path will be copied
     * \param relativePath new realtive path
     */
    FilesystemElement(const FilesystemElement& other,
                      const std::filesystem::path& relativePath);

    /**
     * Read from metadata files
     * \param metadataLine line of the metadata file to construct the object from
     * \param metadataFileName name of metadata file, used for error reporting
     * \param lineNo line number of metadata file, used for error reporting
     * \throws runtime_error in case of errors
     */
    void readFrom(const std::string& metadataLine,
                  const std::string& metadataFileName="", int lineNo=-1);

    /**
     * Write the object to an ostream based on the metadata file format
     * \param os ostream where to write
     */
    void writeTo(std::ostream& os) const;

    /**
     * \return the type of the FilesystemElement (reguler file, directory, ...)
     */
    std::filesystem::file_type type() const { return ty; }

    /**
     * \return the file type as string ("file", "directory", ...)
     */
    std::string typeAsString() const;

    /**
     * \return the access permissions
     */
    std::filesystem::perms permissions() const { return per; }

    /**
     * Modify permissions
     */
    void setPermissions(std::filesystem::perms per) { this->per=per; }

    /**
     * \return the owner (user) of the FilesystemElement as a string
     */
    std::string user() const { return us; }

    /**
     * Modify user
     */
    void setUser(const std::string& us) { this->us=us; }

    /**
     * \return the group of the FilesystemElement as a string
     */
    std::string group() const { return gs; }

    /**
     * Modify group
     */
    void setGroup(const std::string& gs) { this->gs=gs; }

    /**
     * \return the last modified time
     */
    time_t mtime() const { return mt; }

    /**
     * Modify mtime
     */
    void setMtime(time_t mt) { this->mt=mt; }

    /**
     * \return the file size.
     * Only valid if the FilesystemElement is a regular file
     */
    off_t size() const { return sz; }

    /**
     * \return the file hash.
     * Only valid if the FilesystemElement is a regular file
     */
    std::string hash() const { return fileHash; }

    /**
     * \return the path of the FilesystemElement, relative to the top directory
     */
    std::filesystem::path relativePath() const { return rp; }

    /**
     * \return the symlink target.
     * Only valid if the FilesystemElement is a symlink
     */
    std::filesystem::path symlinkTarget() const { return symlink; }

    /**
     * \return the hardlink count. Note that this information is not saved
     * in the metadata file, so it is only available if the FilesystemElement
     * has been read from disk.
     */
    uintmax_t hardLinkCount() const { return hardLinkCnt; }

    /**
     * \return true if the FilesystemElement is a directory
     */
    bool isDirectory() const { return ty==std::filesystem::file_type::directory; }

private:
    //Fields that are written to metadata files
    std::filesystem::file_type ty; ///< File type (regular, directory, ...)
    std::filesystem::perms per;    ///< File permissions (rwxrwxrwx)
    std::string us;                ///< File user (owner)
    std::string gs;                ///< File group
    time_t mt=0;                   ///< Modified time
    off_t sz=0;                    ///< Size, only if regular file
    std::string fileHash;          ///< SHA1 hash, only if regular file
#ifndef OPTIMIZE_MEMORY
    std::filesystem::path rp;      ///< File path relative to top level directory
    std::filesystem::path symlink; ///< Symlink target path, only if symlink
#else //OPTIMIZE_MEMORY
    // The path class occupies almost twice the size of a string, so store paths
    // as strings to save memory but transparently convert them to paths on use.
    // This saves memory at the cost of speed.
    std::string rp;
    std::string symlink;
#endif //OPTIMIZE_MEMORY

    //Fields that are not written to metadata files
    uintmax_t hardLinkCnt=1;       ///< Number of hardlinks

    friend bool operator== (const FilesystemElement& a, const FilesystemElement& b);
    friend bool compare(const FilesystemElement& a, const FilesystemElement& b,
                        const CompareOpt& opt);
};

/**
 * Compare operator for sorting.
 * It puts directories first and sorts alphabetically
 */
bool operator< (const FilesystemElement& a, const FilesystemElement& b);

/**
 * Equality/inequality comparison
 */
bool operator== (const FilesystemElement& a, const FilesystemElement& b);
inline bool operator!= (const FilesystemElement& a, const FilesystemElement& b)
{
    return !(a==b);
}

/**
 * Compare two FilesystemElement according to the given options
 */
bool compare(const FilesystemElement& a, const FilesystemElement& b,
             const CompareOpt& opt);

/**
 * Write a FilesystemElement to an ostream based on the metadata file format
 * \param os ostream where to write
 * \param e FilesystemElement to write
 */
inline std::ostream& operator<<(std::ostream& os, const FilesystemElement& e)
{
    e.writeTo(os);
    return os;
}

/**
 * A node of an in-memory representation of the metadata of a directory tree
 */
class DirectoryNode
{
public:
    DirectoryNode() {}

    // Heavy object not ment to be copyable, only move assignable.
    // This declaration implicitly deletes copy constructor and copy assignment
    // in a way that does not upset std::list. Explicit copy is possible with
    // addToDirectoryContent(), used for copying part of a tree into another but
    // it correctly fixes the path when copying so the nodes belong to the tree
    DirectoryNode(DirectoryNode&&)=default;
    DirectoryNode& operator=(DirectoryNode&&)=default;

    /**
     * Constructor
     */
    explicit DirectoryNode(FilesystemElement elem) : elem(elem) {}

    /**
     * This member function is meant to be used when constructing a tree only.
     * If the node is a directory, this member function allows to set its content
     * Note that the directory content is moved, not copied into this object.
     * \param content, a list of other DirectoryNodes to be moved into this object
     * \return a reference to the directory content after being moved
     */
    std::list<DirectoryNode>& setDirectoryContent(std::list<DirectoryNode>&& content);

    /**
     * \return a const reference to the FilesystemElement of this node
     */
    const FilesystemElement& getElement() const { return elem; }

    /**
     * \return a const reference to the nodes contained in the directory, or an
     * empty list otherwise (empty directory or not a directory)
     */
    const std::list<DirectoryNode>& getDirectoryContent() const { return content; }

    /**
     * Remove an element from the directory content. If the element is a
     * directory also the directory content is recursively removed
     * \param toRemove DirectoryNode to remove
     */
    void removeFromDirectoryContent(const DirectoryNode& toRemove);

    /**
     * Make a copy of another directory node into the directory content of this
     * directory. This object must be a directory and must not contain a node
     * with the same name. If also the element to add is a directory,
     * recursively add the entire directory content.
     * \param toAdd DirectoryNode to add
     * \return a reference to the newly added node
     */
    DirectoryNode& addToDirectoryContent(const DirectoryNode& toAdd);

    /**
     * Modify permissions
     */
    void setPermissions(std::filesystem::perms perm) { elem.setPermissions(perm); }

    /**
     * Modify owner (user/group)
     */
    void setOwner(const std::string& user, const std::string& group)
    {
        elem.setUser(user);
        elem.setGroup(group);
    }

    /**
     * Modify mtime
     */
    void setMtime(time_t mtime) { elem.setMtime(mtime); }

private:
    static DirectoryNode& recursiveAdd(DirectoryNode& dst, const DirectoryNode& src);

    FilesystemElement elem;           ///< The filesystem element
    std::list<DirectoryNode> content; ///< If element is a directory, its content
};

/**
 * Compare operator for sorting
 */
inline bool operator< (const DirectoryNode& a, const DirectoryNode& b)
{
    return a.getElement() < b.getElement();
}

/**
 * An in-memory representation of the metadata of a directory tree
 */
class DirectoryTree
{
public:
    DirectoryTree() {}

    /**
     * Construct a directory tree from either a metadata file or a directory
     * \param inputPath if the path is to a directory, use it as the top level
     * directory where to start the directory tree, if it is a file path, assume
     * it is a path to a metadata file
     * \param opt scan options
     */
    DirectoryTree(const std::filesystem::path& inputPath,
                  ScanOpt opt=ScanOpt::ComputeHash)
    {
        fromPath(inputPath,opt);
    }
    
    /**
     * Construct a directory tree by reading from metadata files
     * \param is istream where to read
     * \throws runtime_error in case of errors
     */
    DirectoryTree(std::istream& is, const std::string& metadataFileName="")
    {
        readFrom(is,metadataFileName);
    }

    /**
     * Set the warning callback, that is called when scanning directories or
     * reading metadata files
     */
    void setWarningCallback(std::function<void (const std::string&)> cb)
    {
        warningCallback=cb;
    }

    /**
     * Construct a directory tree from either a metadata file or a directory
     * \param inputPath if the path is to a directory, use it as the top level
     * directory where to start the directory tree, if it is a file path, assume
     * it is a path to a metadata file
     * \param opt scan options
     */
    void fromPath(const std::filesystem::path& inputPath,
                  ScanOpt opt=ScanOpt::ComputeHash)
    {
        if(is_directory(inputPath)) scanDirectory(inputPath,opt);
        else readFrom(inputPath);
    }

    /**
     * Scan directory tree starting from the given top path
     * \param topPath top level directory where to start the directory tree
     * \param opt scan options
     */
    void scanDirectory(const std::filesystem::path& topPath,
                       ScanOpt opt=ScanOpt::ComputeHash);

    /**
     * Read from metadata files
     * \param metadataFile path of the metadata file
     */
    void readFrom(const std::filesystem::path& metadataFile);

    /**
     * Read from metadata files
     * \param is istream where to read
     * \throws runtime_error in case of errors
     */
    void readFrom(std::istream& is, const std::string& metadataFileName="");

    /**
     * Write to metadata files
     * \param metadataFile path of the metadata file
     */
    void writeTo(const std::filesystem::path& metadataFile) const;

    /**
     * Write the object to an ostream based on the metadata file format
     * \param os ostream where to write
     */
    void writeTo(std::ostream& os) const;
    
    /**
     * Deallocate the entire directory tree
     */
    void clear();

    /**
     * \return the root of the directory tree, containing the content of the top
     * directory and all its subfolders, if any
     * NOTE: since all ellements are stored by value this may be a very
     * heavy object, never copy this tree, only access it by reference
     */
    const std::list<DirectoryNode>& getTreeRoot() const { return topContent; }

    /**
     * \return the a flat index of all the files and directories in the
     * directory tree, that is the top directory and all its subdirectories
     * The key is the relative path, while the value is a pointer into the tree
     * data structure
     */
    const std::unordered_map<std::string,DirectoryNode*>& getIndex() const
    {
        return index;
    }

    /**
     * \param p relative path to search
     * \return the corresponding FilesystemElemet if found
     */
    std::optional<FilesystemElement> search(const std::filesystem::path& p) const;

    /**
     * \param p relative path to search
     * \param where an optional string that is added to the exception that is
     * thrown if the path is not found, used to print more informative messages
     * \return the corresponding DirectoryNode
     * \throws runtime_error if the path is not found
     */
    const DirectoryNode& searchNode(const std::filesystem::path& p,
                                    const std::string& where="") const;

    /**
     * Copy part of another directoryTree into this tree.
     * \param srcTree source tree. Nothe that you can pass this to duplicate
     * a part of this tree somewhere else in the same tree
     * \param relativeSrcPath path relative to srcTree pointing to the idem to
     * copy, could be a file, symlink or directory
     * NOTE: relativeSrcPath cannot be empty, so it is not possible to add the
     * entire content of the srcTree with a single call
     * \param relativeDstPath path relative to this tree pointing to an existing
     * directory where the item has to be copied. Path can be empty, in this
     * case add the item to the top directory
     * \throws runtime_error if paths not found or dst path not a directory
     */
    void copyFromTree(const DirectoryTree& srcTree,
                      const std::filesystem::path& relativeSrcPath,
                      const std::filesystem::path& relativeDstPath)
    {
        treeCopy(srcTree,relativeSrcPath,relativeDstPath);
    }

    /**
     * Copy part of another directoryTree into this tree and the filesystem.
     * Only works if the both the source tree and this tree were constructed by
     * scanning a directory, not if they were constructed from a metadata file.
     * WARNING: this actually copies files from your filesystem!
     * \param srcTree source tree. Nothe that you can pass this to duplicate
     * a part of this tree somewhere else in the same tree
     * \param relativeSrcPath path relative to srcTree pointing to the idem to
     * copy, could be a file, symlink or directory
     * NOTE: relativeSrcPath cannot be empty, so it is not possible to add the
     * entire content of the srcTree with a single call
     * \param relativeDstPath path relative to this tree pointing to an existing
     * directory where the item has to be copied. Path can be empty, in this
     * case add the item to the top directory
     * \throws runtime_error if paths not found or dst path not a directory
     */
    void copyFromTreeAndFilesystem(const DirectoryTree& srcTree,
                                   const std::filesystem::path& relativeSrcPath,
                                   const std::filesystem::path& relativeDstPath);

    /**
     * Remove the specified path from this tree.
     * If the path refers to a directory recursively remove all its content.
     * NOTE: relativePath cannot be empty, so it is not possible to remove
     * the entire content of the top directory with a single call
     * \param relativePath path to remove. Must be relative to the topPath
     * this tree was constructed from
     * \throws runtime_error if path not found
     */
    void removeFromTree(const std::filesystem::path& relativePath);

    /**
     * Remove the specified path from this tree and the filesystem.
     * Only works if the tree was constructed by scanning a directory, not if
     * it was constructed from a metadata file.
     * WARNING: this actually deletes files from your filesystem!
     * If the path refers to a directory recursively remove all its content
     * NOTE: relativePath cannot be empty, so it is not possible to remove
     * the entire content of the top directory with a single call
     * \param relativePath path to remove. Must be relative to the topPath
     * this tree was constructed from
     * \return number of files/directories removed from filesystem
     * \throws runtime_error if path not found or if the tree was not
     * constructed by scanning a directory
     */
    int removeFromTreeAndFilesystem(const std::filesystem::path& relativePath);

    /**
     * Add a symlink to this tree.
     * \param symlink FilesystemElement of type symlink. It will be added to the
     * tree to the relative path specified in the FilesystemElement itself.
     * \throws runtime_error if the relative path does refers to a directory
     * not already present in the tree
     */
    void addSymlinkToTree(const FilesystemElement& symlink);

    /**
     * Add a symlink to this tree and the filesystem.
     * WARNING: this actually creates the symlink on your filesystem!
     * \param symlink FilesystemElement of type symlink. It will be added to the
     * tree to the relative path specified in the FilesystemElement itself.
     * \throws runtime_error if the relative path does refers to a directory
     * not already present in the tree or if the tree was not constructed by
     * scanning a directory
     */
    void addSymlinkToTreeAndFilesystem(const FilesystemElement& symlink);

    /**
     * Modify permissions of an entry in this tree.
     * \param relativePath relative path of the entry to modify.
     * \param perms new permissions
     * \throws runtime_error if the relative path does refers to an entry not
     * present in the tree
     */
    void modifyPermissionsInTree(const std::filesystem::path& relativePath,
                                 std::filesystem::perms perm);

    /**
     * Modify permissions of an entry in this tree and the filesystem.
     * WARNING: this actually alters the entry on your filesystem!
     * \param relativePath relative path of the entry to modify.
     * \param perms new permissions
     * \throws runtime_error if the relative path does refers to an entry not
     * present in the tree or if the tree was not constructed by scanning a
     * directory
     */
    void modifyPermissionsInTreeAndFilesystem(const std::filesystem::path& relativePath,
                                              std::filesystem::perms perm);

    /**
     * Modify owner (user/group) of an entry in this tree.
     * \param relativePath relative path of the entry to modify.
     * \param user new user
     * \param group new group
     * \throws runtime_error if the relative path does refers to an entry not
     * present in the tree
     */
    void modifyOwnerInTree(const std::filesystem::path& relativePath,
                           const std::string& user, const std::string& group);

    /**
     * Modify owner (user/group) of an entry in this tree and the filesystem.
     * WARNING: this actually alters the entry on your filesystem!
     * \param relativePath relative path of the entry to modify.
     * \param user new user
     * \param group new group
     * \throws runtime_error if the relative path does refers to an entry not
     * present in the tree or if the tree was not constructed by scanning a
     * directory
     */
    void modifyOwnerInTreeAndFilesystem(const std::filesystem::path& relativePath,
                                        const std::string& user, const std::string& group);

    /**
     * Modify the last modified time (mtime) of an entry in this tree.
     * \param relativePath relative path of the entry to modify.
     * \param mtime new modified time
     * \throws runtime_error if the relative path does refers to an entry not
     * present in the tree
     */
    void modifyMtimeInTree(const std::filesystem::path& relativePath, time_t mtime);

    /**
     * Modify the last modified time (mtime) of an entry in this tree and the
     * filesystem.
     * WARNING: this actually alters the entry on your filesystem!
     * \param relativePath relative path of the entry to modify.
     * \param mtime new modified time
     * \throws runtime_error if the relative path does refers to an entry not
     * present in the tree or if the tree was not constructed by scanning a
     * directory
     */
    void modifyMtimeInTreeAndFilesystem(const std::filesystem::path& relativePath,
                                        time_t mtime);

private:
    // This version of searchNode that returns a non-const pointer is private
    DirectoryNode& searchNode(const std::filesystem::path& p,
                              const std::string& where="");

    void checkTopPath(const std::string& where) const;

    void fixupParentMtime(const std::filesystem::path& parent);

    void recursiveBuildFromPath(const std::filesystem::path& p);

    void recursiveWrite(const std::list<DirectoryNode>& nodes) const;

    struct CopyResult
    {
        CopyResult(const DirectoryNode& src, const DirectoryNode& dst)
            : src(src), dst(dst) {}
        const DirectoryNode& src;
        const DirectoryNode& dst;
    };

    CopyResult treeCopy(const DirectoryTree& srcTree,
                        const std::filesystem::path& relativeSrcPath,
                        const std::filesystem::path& relativeDstPath);

    void recursiveFilesystemCopy(const std::filesystem::path& srcTopPath,
                                 CopyResult nodes);

    /// Add the subtree starting from node including node itself
    void recursiveAddToIndex(const DirectoryNode& node);

    /// Removes the subtree starting from node from index, but not node itself
    void recursiveRemoveFromIndex(const DirectoryNode& node);

    std::list<DirectoryNode> topContent;
    std::unordered_map<std::string,DirectoryNode*> index;
    std::function<void (const std::string&)> warningCallback=
        [](const std::string& s){ std::cerr<<s<<'\n'; };
    std::optional<std::filesystem::path> topPath; // Only if built from directory
    ScanOpt opt;                      // Only used by recursiveBuildFromPath
    mutable std::ostream *os=nullptr; // Only used by recursiveWrite
    mutable bool printBreak;          // Only used by recursiveWrite
};

/**
 * Write a DirectoryTree to an ostream based on the metadata file format
 * \param os ostream where to write
 * \param e FilesystemElement to write
 */
inline std::ostream& operator<<(std::ostream& os, const DirectoryTree& dt)
{
    dt.writeTo(os);
    return os;
}

/**
 * A single difference between N directories
 */
template<unsigned N>
using DirectoryDiffLine=std::array<std::optional<FilesystemElement>,N>;

/**
 * Type returned by diff operations
 */
template<unsigned N>
using DirectoryDiff=std::list<DirectoryDiffLine<N>>;

/**
 * Print a single difference to an ostream based on the diff file format
 * \param os ostream where to write
 * \param diff diff to write
 */
std::ostream& operator<<(std::ostream& os, const DirectoryDiffLine<2>& d);

/**
 * Print a single difference to an ostream based on the diff file format
 * \param os ostream where to write
 * \param diff diff to write
 */
std::ostream& operator<<(std::ostream& os, const DirectoryDiffLine<3>& d);

/**
 * Print a diff to an ostream based on the diff file format
 * \param os ostream where to write
 * \param diff diff to write
 */
std::ostream& operator<<(std::ostream& os, const DirectoryDiff<2>& diff);

/**
 * Print a diff to an ostream based on the diff file format
 * \param os ostream where to write
 * \param diff diff to write
 */
std::ostream& operator<<(std::ostream& os, const DirectoryDiff<3>& diff);

/**
 * Two way diff between two directory trees
 */
DirectoryDiff<2> diff2(const DirectoryTree& a, const DirectoryTree& b,
                       const CompareOpt& opt=CompareOpt());

/**
 * Two way diff between two directory trees
 */
DirectoryDiff<3> diff3(const DirectoryTree& a, const DirectoryTree& b,
                       const DirectoryTree& c, const CompareOpt& opt=CompareOpt());

