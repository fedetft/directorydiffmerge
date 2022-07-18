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

#include "extfs.h"
#include <fcntl.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

using namespace std;
using namespace std::filesystem;

//
// class ext_file_status
//

string ext_file_status::lookupUser(uid_t uid)
{
    unique_lock<mutex> l(m);
    auto it=userCache.find(uid);
    if(it!=userCache.end()) return it->second;

    if(structBuffer.empty()) allocateStructBuffer();

    struct passwd pw;
    struct passwd *result;
    if(getpwuid_r(uid, &pw, structBuffer.data(), structBuffer.size(), &result))
        throw runtime_error("lookupUser");
    if(result) it=userCache.insert(it,{uid,result->pw_name});
    else it=userCache.insert(it,{uid,to_string(uid)}); //Not found
    return it->second;
}

string ext_file_status::lookupGroup(gid_t gid)
{
    unique_lock<mutex> l(m);
    auto it=groupCache.find(gid);
    if(it!=groupCache.end()) return it->second;

    if(structBuffer.empty()) allocateStructBuffer();

    struct group gr;
    struct group *result;
    if(getgrgid_r(gid, &gr, structBuffer.data(), structBuffer.size(), &result))
        throw runtime_error("lookupGroup");
    if(result) it=groupCache.insert(it,{gid,result->gr_name});
    else it=userCache.insert(it,{gid,to_string(gid)}); //Not found
    return it->second;
}

void ext_file_status::allocateStructBuffer()
{
    const int defaultValue=2048;
    int u=sysconf(_SC_GETPW_R_SIZE_MAX);
    if(u<0) u=defaultValue;
    int g=sysconf(_SC_GETGR_R_SIZE_MAX);
    if(g<0) g=defaultValue;
    structBuffer.resize(max(u,g));
}

mutex ext_file_status::m;
string ext_file_status::structBuffer;
map<uid_t, string> ext_file_status::userCache;
map<gid_t, string> ext_file_status::groupCache;

const file_type ext_file_status::typeLut[16]=
{
    file_type::unknown,   //000
    file_type::fifo,      //001
    file_type::character, //002
    file_type::unknown,   //003
    file_type::directory, //004
    file_type::unknown,   //005
    file_type::block,     //006
    file_type::unknown,   //007
    file_type::regular,   //010
    file_type::unknown,   //011
    file_type::symlink,   //012
    file_type::unknown,   //013
    file_type::socket,    //014
    file_type::unknown,   //015
    file_type::unknown,   //016
    file_type::unknown    //017
};

//
//
//

void ext_last_write_time(const path& p, time_t mtime)
{
    string s=p.string();
    timespec t[2];
    t[0].tv_sec=0;
    t[0].tv_nsec=UTIME_OMIT;
    t[1].tv_sec=mtime;
    t[1].tv_nsec=0;
    if(utimensat(AT_FDCWD,s.c_str(),t,AT_SYMLINK_NOFOLLOW)!=0)
        throw runtime_error(string("ext_last_write_time failed with path ")+s);
}
