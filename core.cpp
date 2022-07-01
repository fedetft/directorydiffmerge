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

#include <sstream>
#include <cassert>
#include <iomanip>
#include <unordered_set>
#include <crypto++/sha.h>
#include <crypto++/hex.h>
#include <crypto++/files.h>
#include <crypto++/filters.h>
#include "extfs.h"
#include "core.h"

using namespace std;
using namespace std::filesystem;

string hashFile(const path& p)
{
    using namespace CryptoPP;
    SHA1 hash;
    string result;
    FileSource fs(p.string().c_str(),true,
                  new HashFilter(hash,new HexEncoder(new StringSink(result))));
    return result;
}

//
// class FilesystemElement
//

FilesystemElement::FilesystemElement()
    : ty(file_type::unknown), per(perms::unknown) {}

FilesystemElement::FilesystemElement(const path& p, const path& top, ScanOpt opt)
    : rp(p.lexically_relative(top))
{
    auto s=ext_symlink_status(p);
    per=s.permissions();
    us=s.user();
    gs=s.group();
    mt=s.mtime();
    ty=s.type();
    hardLinkCnt=s.hard_link_count();
    switch(s.type())
    {
        case file_type::regular:
            sz=s.file_size();
            if(opt==ScanOpt::ComputeHash) fileHash=hashFile(p);
            break;
        case file_type::directory:
            break;
        case file_type::symlink:
            symlink=read_symlink(p);
            break;
        default:
            ty=file_type::unknown; //We don't handle other types
    }
}

void FilesystemElement::readFrom(const string& metadataLine,
                                 const string& metadataFileName, int lineNo)
{
    auto fail=[&](const string& m)
    {
        string s=metadataFileName;
        if(metadataFileName.empty()==false) s+=": ";
        s+=m;
        if(lineNo>0) s+=" at line "+to_string(lineNo);
        s+=", wrong line is '"+metadataLine+"'";
        throw runtime_error(s);
    };

    istringstream in(metadataLine);
    string permStr;
    in>>permStr;
    if(!in || permStr.size()!=10) fail("Error reading permission string");
    switch(permStr.at(0))
    {
        case '-': ty=file_type::regular;   break;
        case 'd': ty=file_type::directory; break;
        case 'l': ty=file_type::symlink;   break;
        case '?': ty=file_type::unknown;   break;
        default: fail("Unrecognized file type");
    }
    int pe=0;
    for(int i=0;i<3;i++)
    {
        string permTriple=permStr.substr(3*i+1,3);
        assert(permTriple.size()==3);
        pe<<=3;
        if(permTriple[0]=='r') pe |= 0004;
        else if(permTriple[0]!='-') fail("Permissions not correct");
        if(permTriple[1]=='w') pe |= 0002;
        else if(permTriple[1]!='-') fail("Permissions not correct");
        if(permTriple[2]=='x') pe |= 0001;
        else if(permTriple[2]!='-') fail("Permissions not correct");
    }
    per=static_cast<perms>(pe);
    in>>us>>gs;
    if(!in) fail("Error reading user/group");
    // Time is complicated. The format string "%F %T" always causes the stream
    // fail bit to be set. But expanding %F as %Y-%m-%d works, go figure.
    // Additionally, trying to add %z to parse time zone always fails. After all
    // there's no field for the time zone in struct tm, so where exactly is
    // get_time expected to put that data? Since time zone correction would have
    // to be done with custom code and there may be corner cases I'm not aware
    // of, I decided to only support UTC and check the +0000 string manually
    struct tm t;
    in>>get_time(&t,"%Y-%m-%d %T");
    mt=timegm(&t);
    if(!in || mt==-1) fail("Error reading mtime");
    string tz;
    tz.resize(6);
    in.read(tz.data(),6);
    if(!in || tz!=" +0000") fail("Error reading mtime");
    switch(ty)
    {
        case file_type::regular:
            in>>sz;
            if(!in) fail("Error reading size");
            in>>fileHash;
            if(!in) fail("Error reading hash");
            if(fileHash=="*") fileHash.clear(); // * means omitted hash
            else if(fileHash.size()!=40) fail("Error reading hash");
            break;
        case file_type::symlink:
            in>>symlink;
            if(!in) fail("Error reading symlink target");
            break;
    }
    in>>rp;
    if(!in) fail("Error reading path");
    if(in.get()!=EOF) fail("Extra characters at end of line");
    //Initialize non-written fields to defaults
    hardLinkCnt=1;
}

void FilesystemElement::writeTo(ostream& os) const
{
    switch(ty)
    {
        case file_type::regular:   os<<'-'; break;
        case file_type::directory: os<<'d'; break;
        case file_type::symlink:   os<<'l'; break;
        default:                   os<<'?'; break;
    }
    int pe=static_cast<int>(per);
    os<<(pe & 0400 ? 'r' : '-')
      <<(pe & 0200 ? 'w' : '-')
      <<(pe & 0100 ? 'x' : '-')
      <<(pe & 0040 ? 'r' : '-')
      <<(pe & 0020 ? 'w' : '-')
      <<(pe & 0010 ? 'x' : '-')
      <<(pe & 0004 ? 'r' : '-')
      <<(pe & 0002 ? 'w' : '-')
      <<(pe & 0001 ? 'x' : '-');
    os<<' '<<us<<' '<<gs<<' ';
    // Time is complicated. The gmtime_r functions, given its name, should fill
    // a struct tm with GMT time, but the documentation says UTC. And it's
    // unclear how it handles leap seconds, that should be the difference
    // between GMT and UTC. Nobody on the Internet appears to know exactly, but
    // it appears to be OS dependent.
    // Additionally put_time has a format string %z to print the time zone, but
    // a struct tm has no fields to encode the time zone, so where does put_time
    // take the time zone information that it prints? Not sure.
    // So I decided to print +0000 manually as a string to be extra sure
    struct tm t;
    assert(gmtime_r(&mt,&t)==&t);
    os<<put_time(&t,"%F %T +0000")<<' ';
    switch(ty)
    {
        case file_type::regular:
            //Print * instead of hash when omitted
            if(fileHash.empty()) os<<sz<<" * ";
            else os<<sz<<' '<<fileHash<<' ';
            break;
        case file_type::symlink:
            os<<symlink<<' ';
            break;
    }
    os<<rp;
}

bool operator< (const FilesystemElement& a, const FilesystemElement& b)
{
    // Sort alphabetically (case sensitive) but put directories first
    if(a.isDirectory()==b.isDirectory()) return a.relativePath() < b.relativePath();
    return a.isDirectory() > b.isDirectory();
}

bool operator== (const FilesystemElement& a, const FilesystemElement& b)
{
    // NOTE: either a or b may have been constructed with file has computation
    // omitted. So if either has an empty hash, this does not cause them to
    // not be equal, but if both have a hash, they must be the same
    return a.ty==b.ty && a.per==b.per && a.us==b.us && a.gs==b.gs
        && a.mt==b.mt && a.sz==b.sz   && a.rp==b.rp && a.symlink==b.symlink
        && (a.fileHash.empty() || b.fileHash.empty() || a.fileHash == b.fileHash);
}

//
// class DirectoryNode
//

list<DirectoryNode>& DirectoryNode::setDirectoryContent(list<DirectoryNode>&& content)
{
    assert(elem.isDirectory());
    this->content=std::move(content);
    return this->content;
}

//
// class DirectoryTree
//

void DirectoryTree::scanDirectory(const path& topPath, ScanOpt opt)
{
    clear();
    this->opt=opt;
    this->topPath=absolute(topPath);
    if(!is_directory(this->topPath))
        throw logic_error(topPath.string()+" is not a directory");
    recursiveBuildFromPath(this->topPath);
    this->topPath.clear();
}

void DirectoryTree::readFrom(const path& metadataFile)
{
    clear();
    ifstream in(metadataFile);
    if(!in) throw runtime_error(string("file not found: ")+metadataFile.string());
    readFrom(in,metadataFile.string());
}

void DirectoryTree::readFrom(istream& is, const string& metadataFileName)
{
    clear();
    int lineNo=0;
    string line;
    list<DirectoryNode> nodes;
    
    auto fail=[&metadataFileName,&lineNo](const string& m)
    {
        string s=metadataFileName;
        if(metadataFileName.empty()==false) s+=": ";
        s+=m+" before line "+to_string(lineNo);
        throw runtime_error(s);
    };
    
    auto add=[&nodes,&fail,this](){
        if(nodes.empty()) return;
        string p=nodes.front().getElement().relativePath().parent_path();
        for(auto& n : nodes)
        {
            auto& e=n.getElement();
            if(p!=e.relativePath().parent_path()) fail("different paths grouped");
            auto inserted=index.insert({e.relativePath().string(),&n});
            if(inserted.second==false) fail("index insert failed (duplicate?)");
            if(e.type()==file_type::unknown && warningCallback)
                warningCallback(string("Warning: ")+e.relativePath().string()+" unsupported file type");
        }
        if(topContent.empty())
        {
            if(p.empty()==false) fail("file does not start with top level directory");
            topContent=std::move(nodes);
            nodes=list<DirectoryNode>();
        } else {
            auto it=index.find(p);
            if(it==index.end()) fail("directory content not preceded by index insert");
            if(it->second->getDirectoryContent().empty()==false)
                fail("duplicate noncontiguous directory content");
            it->second->setDirectoryContent(std::move(nodes));
            nodes=list<DirectoryNode>();
        }
    };

    while(getline(is,line))
    {
        lineNo++;
        if(line.empty()) add();
        else {
            DirectoryNode n(FilesystemElement(line,metadataFileName,lineNo));
            nodes.push_back(std::move(n));
        }
    }
    add();
}

void DirectoryTree::writeTo(std::ostream& os) const
{
    this->os=&os;
    printBreak=false;
    recursiveWrite(topContent);
    this->os=nullptr;
}

void DirectoryTree::clear()
{
    topPath.clear();
    topContent.clear();
    index.clear();
}

void DirectoryTree::recursiveBuildFromPath(const std::filesystem::path& p)
{
    list<DirectoryNode> nodes, *nodesPtr;
    for(auto& it : directory_iterator(topPath / p))
        nodes.push_back(DirectoryNode(FilesystemElement(it.path(),topPath,opt)));
    nodes.sort();
    if(topContent.empty())
    {
        topContent=std::move(nodes);
        nodesPtr=&topContent;
    } else {
        auto it=index.find(p.string());
        assert(it!=index.end());
        nodesPtr=&it->second->setDirectoryContent(std::move(nodes));
    }

    for(auto& n : *nodesPtr)
    {
        auto& e=n.getElement();
        auto inserted=index.insert({e.relativePath().string(),&n});
        assert(inserted.second==true);
        if(e.type()==file_type::unknown && warningCallback)
            warningCallback(string("Warning: ")+e.relativePath().string()+" unsupported file type");
        if(e.type()!=file_type::directory && e.hardLinkCount()!=1 && warningCallback)
            warningCallback(string("Warning: ")+e.relativePath().string()+" has multiple hardlinks");
    }

    for(auto& n : *nodesPtr)
    {
        //NOTE: we list directories, not symlinks to directories. This also
        //saves us from worrying about filesystem loops through directory symlinks.
        auto& e=n.getElement();
        if(e.isDirectory()) recursiveBuildFromPath(e.relativePath());
    }
}

void DirectoryTree::recursiveWrite(const std::list<DirectoryNode>& nodes) const
{
    if(printBreak) *os<<'\n';
    for(auto&n : nodes) *os<<n.getElement()<<'\n';
    printBreak=nodes.empty()==false;
    for(auto&n : nodes)
    {
        if(n.getElement().isDirectory()==false) break;
        recursiveWrite(n.getDirectoryContent());
    }
}

//
// class DirectoryDiff
//

std::ostream& operator<<(std::ostream& os, const DirectoryDiff<2>& diff)
{
    for(auto& d : diff)
    {
        if(d[0]) os<<"- "<<d[0].value()<<'\n';
        if(d[1]) os<<"+ "<<d[1].value()<<'\n';
        os<<'\n';
    }
    return os;
}

/**
 * Helper class to implement compare2 recursively
 */
class Compare2Helper
{
public:
    Compare2Helper(const DirectoryTree& a, const DirectoryTree& b)
        : aIndex(a.getIndex()), bIndex(b.getIndex()) {}

    void recursiveCompare(const list<DirectoryNode>& a,
                          const list<DirectoryNode>& b);

    const unordered_map<string,DirectoryNode*>& aIndex;
    const unordered_map<string,DirectoryNode*>& bIndex;
    DirectoryDiff<2> result;
};

void Compare2Helper::recursiveCompare(const list<DirectoryNode>& a,
        const list<DirectoryNode>& b)
{
    unordered_set<string> itemNames;
    for(auto& n : a) itemNames.insert(n.getElement().relativePath().string());
    for(auto& n : b) itemNames.insert(n.getElement().relativePath().string());
    list<array<DirectoryNode*,2>> commonDrectories;
    for(auto& itn : itemNames)
    {
        auto ita=aIndex.find(itn);
        auto itb=bIndex.find(itn);
        if(ita!=aIndex.end() && itb!=bIndex.end())
        {
            auto& ae=ita->second->getElement();
            auto& be=itb->second->getElement();
            if(ae!=be) result.push_back({ae,be});
            // Pruning comparison, only go down common directories
            // that's only possible in 2 way compare
            if(ae.isDirectory() && be.isDirectory())
                commonDrectories.push_back({ita->second,itb->second});
        } else if(ita==aIndex.end() && itb!=bIndex.end()) {
            result.push_back({nullopt,itb->second->getElement()});
        } else if(ita!=aIndex.end() && itb==bIndex.end()) {
            result.push_back({ita->second->getElement(),nullopt});
        } else assert(false);
    }
    itemNames.clear(); //Save memory while doing recursion
    for(auto& dirs : commonDrectories)
        recursiveCompare(dirs[0]->getDirectoryContent(),
                         dirs[1]->getDirectoryContent());
}

DirectoryDiff<2> compare2(const DirectoryTree& a, const DirectoryTree& b)
{
    Compare2Helper cmp(a,b);
    cmp.recursiveCompare(a.getTreeRoot(),b.getTreeRoot());
    return std::move(cmp.result);
}
