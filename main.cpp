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

#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <functional>
#include <boost/program_options.hpp>
#include "core.h"

using namespace std;
using namespace std::filesystem;
using namespace boost::program_options;

/**
 * Print help and terminate the program
 */
static void help()
{
    cerr<<R"(ddm: DirectoryDiffMerge tool

Legend:
<dir> : directory
<met> : metadata file
<d|m> : either directory or metadata file
<dif> : diff file
<ign> : list of metadata to ignore
        one or more of {perm,owner,mtime,size,hash,symlink,all}
        (when using 'all' only the presence of a file and its type matters)

Usage:
ddm ls <dir>                        # List directory, write metadata to stdout
ddm ls <dir> -n                     # List directory, omit hash computation
ddm ls <dir> -o <met>               # List directory, write metadata to file
ddm diff <d|m> <d|m>                # Diff directories or metadata, write stdout
ddm diff <d|m> <d|m> -n             # Diff directories (omit hash) or metadata
ddm diff <d|m> <d|m> -o <dif>       # Diff directories or metadata, write file
ddm diff <d|m> <d|m> -i <ign>       # Diff ignoring certain metadata
ddm diff <d|m> <d|m> <d|m>          # Three way diff, write stdout

ddm scrub <dir> <met> <met>         # Check for bit rot, correct if possible
ddm backup -s <dir> -t <dir>        # Backup source dir (readonly) to target dir
ddm backup -s <dir> -t <dir> <met> <met>  # Backup and update bit rot copies
                                          # assumes meatadata is consistent,
                                          # better do a scrub before
ddm sync -s <d|m> -t <d|m> -o <dir> # ??? TODO
)";
    exit(100);
}

/**
 * Print a warning
 */
static void printWarning(const string& message)
{
    cerr<<message<<'\n';
}

/**
 * ddm ls command
 */
static int ls(variables_map& vm, ostream& out)
{
    vector<string> inputs;
    if(vm.count("input")) inputs=vm["input"].as<vector<string>>();

    if(vm.count("help")   || vm.count("source") || vm.count("target")
    || vm.count("ignore") || inputs.size()>1)
    {
        cerr<<R"(ddm ls
Usage:
ddm ls <dir>                        # List directory, write metadata to stdout
ddm ls <dir> -n                     # List directory, omit hash computation
ddm ls <dir> -o <met>               # List directory, write metadata to file
)";
        return 100;
    }

    ScanOpt opt=vm.count("nohash") ? ScanOpt::OmitHash : ScanOpt::ComputeHash;
    DirectoryTree dt;
    dt.setWarningCallback(printWarning);
    dt.scanDirectory(inputs.empty() ? "." : inputs.at(0), opt);
    out<<dt;
    return 0;
}

/**
 * ddm diff command
 */
static int diff(variables_map& vm, ostream& out)
{
    vector<string> inputs;
    if(vm.count("input")) inputs=vm["input"].as<vector<string>>();

    if(vm.count("help") || vm.count("source") || vm.count("target")
    || inputs.size()<2 || inputs.size()>3)
    {
        cerr<<R"(ddm diff
Usage:
ddm diff <d|m> <d|m>                # Diff directories or metadata, write stdout
ddm diff <d|m> <d|m> -n             # Diff directories (omit hash) or metadata
ddm diff <d|m> <d|m> -o <dif>       # Diff directories or metadata, write file
ddm diff <d|m> <d|m> -i <ign>       # Diff ignoring certain metadata
ddm diff <d|m> <d|m> <d|m>          # Three way diff, write stdout
)";
        return 100;
    }

    ScanOpt sopt=vm.count("nohash") ? ScanOpt::OmitHash : ScanOpt::ComputeHash;
    CompareOpt copt;
    if(vm.count("ignore")) copt=CompareOpt(vm["ignore"].as<string>());

    if(inputs.size()==2)
    {
        DirectoryTree a,b;
        a.setWarningCallback(printWarning);
        b.setWarningCallback(printWarning);
        a.fromPath(inputs.at(0),sopt);
        b.fromPath(inputs.at(1),sopt);
        auto diff=diff2(a,b,copt);
        out<<diff;
        return diff.size()==0 ? 0 : 1; //Allow to check if differences found
    } else {
        DirectoryTree a,b,c;
        a.setWarningCallback(printWarning);
        b.setWarningCallback(printWarning);
        c.setWarningCallback(printWarning);
        a.fromPath(inputs.at(0),sopt);
        b.fromPath(inputs.at(1),sopt);
        c.fromPath(inputs.at(2),sopt);
        auto diff=diff3(a,b,c,copt);
        out<<diff;
        return diff.size()==0 ? 0 : 1; //Allow to check if differences found
    }
}

int main(int argc, char *argv[])
{
    //Basic sanity check
    if(argc<2) help();

    //Force program_options to treat the first option separately as program name
    argc--;
    argv++;

    //Parse command line
    options_description desc("options");
    desc.add_options()
        ("help,h",   "prints this")
        ("source,s", value<string>(), "source")
        ("target,t", value<string>(), "target")
        ("ignore,i", value<string>(), "ignore")
        ("output,o", value<string>(), "output")
        ("nohash,n", "omit hash computation")
        ("input",    value<vector<string>>(), "input") //Positional catch-all
    ;
    positional_options_description p;
    p.add("input", -1);
    variables_map vm;
    store(command_line_parser(argc,argv).options(desc).positional(p).run(),vm);
    notify(vm);

    //Handle redirecting output to file
    ostream *out=&cout;
    ofstream outfile;
    if(vm.count("output"))
    {
        string outFileName=vm["output"].as<string>();
        if(exists(outFileName))
        {
            cerr<<"Output file "<<outFileName<<" already exists. Aborting.\n";
            return 10;
        }
        outfile.open(outFileName);
        if(!outfile)
        {
            cerr<<"Error opening "<<outFileName<<". Aborting.\n";
            return 10;
        }
        out=&outfile;
    }

    //Decide what to do
    const map<string,function<int (variables_map&, ostream&)>> operations=
    {
        {"ls",   ls},
        {"diff", diff},
    };
    auto it=operations.find(argv[0]);
    if(it==operations.end()) help();
    return it->second(vm,*out);
}
