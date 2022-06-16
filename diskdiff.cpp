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

#include <list>
#include <sstream>
#include <cassert>
#include <iomanip>
#include <crypto++/sha.h>
#include <crypto++/hex.h>
#include <crypto++/files.h>
#include <crypto++/filters.h>
#include "extfs.h"
#include "diskdiff.h"

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
    : type(file_type::unknown), permissions(perms::unknown) {}

FilesystemElement::FilesystemElement(const path& p, const path& top)
    : relativePath(p.lexically_relative(top))
{
    auto s=ext_symlink_status(p);
    permissions=s.permissions();
    user=s.user();
    group=s.group();
    mtime=s.mtime();
    type=s.type();
    hard_link_count=s.hard_link_count();
    switch(s.type())
    {
        case file_type::regular:
            size=s.file_size();
            hash=hashFile(p);
            break;
        case file_type::directory:
            dir=true;
            break;
        case file_type::symlink:
            symlinkTarget=read_symlink(p);
            break;
        default:
            type=file_type::unknown; //We don't handle other types
    }
}

void FilesystemElement::readFrom(const string& diffLine,
                                 const string& diffFileName, int lineNo)
{
    auto fail=[&](const string& m)
    {
        string s=diffFileName;
        if(diffFileName.empty()==false) s+=": ";
        s+=m;
        if(lineNo>0) s+=" at line"+to_string(lineNo);
        s+=", wrong line is '"+diffLine+"'";
        throw runtime_error(s);
    };

    istringstream in(diffLine);
    string permStr;
    in>>permStr;
    if(!in || permStr.size()!=10) fail("Error reading permission string");
    switch(permStr.at(0))
    {
        case '-': type=file_type::regular;   dir=false; break;
        case 'd': type=file_type::directory; dir=true;  break;
        case 'l': type=file_type::symlink;   dir=false; break;
        case '?': type=file_type::unknown;   dir=false; break;
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
    permissions=static_cast<perms>(pe);
    in>>user>>group;
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
    mtime=timegm(&t);
    if(!in || mtime==-1) fail("Error reading mtime");
    string tz;
    tz.resize(6);
    in.read(tz.data(),6);
    if(!in || tz!=" +0000") fail("Error reading mtime");
    switch(type)
    {
        case file_type::regular:
            in>>size;
            if(!in) fail("Error reading size");
            in>>hash;
            if(!in || hash.size()!=40) fail("Error reading hash");
            break;
        case file_type::symlink:
            in>>symlinkTarget;
            if(!in) fail("Error reading symlink target");
            break;
    }
    in>>relativePath;
    if(!in) fail("Error reading path");
    if(in.get()!=EOF) fail("Extra characters at end of line");
    //Initialize non-written fields to defaults
    hard_link_count=1;
}

void FilesystemElement::writeTo(ostream& os)
{
    switch(type)
    {
        case file_type::regular:   os<<'-'; break;
        case file_type::directory: os<<'d'; break;
        case file_type::symlink:   os<<'l'; break;
        default:                   os<<'?'; break;
    }
    int pe=static_cast<int>(permissions);
    os<<(pe & 0400 ? 'r' : '-')
      <<(pe & 0200 ? 'w' : '-')
      <<(pe & 0100 ? 'x' : '-')
      <<(pe & 0040 ? 'r' : '-')
      <<(pe & 0020 ? 'w' : '-')
      <<(pe & 0010 ? 'x' : '-')
      <<(pe & 0004 ? 'r' : '-')
      <<(pe & 0002 ? 'w' : '-')
      <<(pe & 0001 ? 'x' : '-');
    os<<' '<<user<<' '<<group<<' ';
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
    assert(gmtime_r(&mtime,&t)==&t);
    os<<put_time(&t,"%F %T +0000")<<' ';
    switch(type)
    {
        case file_type::regular:
            os<<size<<' '<<hash<<' ';
            break;
        case file_type::symlink:
            os<<symlinkTarget<<' ';
            break;
    }
    os<<relativePath<<'\n';
}

bool operator< (const FilesystemElement& a, const FilesystemElement& b)
{
    // Sort alphabetically (case sensitive) but put directories first
    if(a.dir==b.dir) return a.relativePath < b.relativePath;
    return a.dir > b.dir;
}

//
// class FileLister
//

void FileLister::listFiles(const path& top)
{
    this->top=absolute(top);
    if(!is_directory(this->top))
        throw logic_error(top.string()+" is not a directory");
    printBreak=false;
    unsupported=false;
    recursiveListFiles(this->top);
}

void FileLister::recursiveListFiles(const path& p)
{
    if(printBreak) os<<'\n';

    list<FilesystemElement> fe;
    for(auto& it : directory_iterator(p))
        fe.push_back(FilesystemElement(it.path(),top));
    fe.sort();
    for(auto& e : fe)
    {
        e.writeTo(os);
        if(e.type==file_type::unknown)
        {
            cerr<<"Warning: "<<e.relativePath<<" has unsupported file type\n";
            unsupported=true;
        }
        if(e.type!=file_type::directory && e.hard_link_count!=1)
        {
            cerr<<"Warning: "<<e.relativePath<<" has multiple hardlinks ("<<e.hard_link_count<<")\n";
            unsupported=true;
        }
    }
    printBreak=fe.empty()==false;

    for(auto& e : fe)
    {
        //NOTE: we list directories, not symlinks to directories. This also
        //saves us from worrying about filesystem loops through directory symlinks.
        if(e.dir) recursiveListFiles(top / e.relativePath);
    }
}
