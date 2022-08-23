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
#include <boost/program_options.hpp>
#include "core.h"
#include "backup.h"
#include "color.h"

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

ddm scrub <dir> <met> <met>             # Check for bit rot, correct if possible
ddm scrub -s <dir> -t <dir> <met> <met> # Check for bit rot, correct if possible
                                        # using source dir to copy files from

ddm backup -s <dir> -t <dir>                # Backup source dir to target dir
ddm backup -s <dir> -t <dir> <met> <met>    # Backup and update bit rot copies
                                            # also performs scrub of backup
)";
// ddm sync -s <d|m> -t <d|m> -o <dir> # ??? TODO
// )";
    exit(100);
}

/**
 * Print a warning
 */
static void printWarning(const string& message)
{
    cerr<<yellowb<<message<<reset<<'\n';
}

/**
 * ddm ls command
 */
static int lsCmd(variables_map& vm, ostream& out)
{
    vector<path> inputs;
    if(vm.count("input")) inputs=vm["input"].as<vector<path>>();

    if(vm.count("help")   || vm.count("source") || vm.count("target")
    || vm.count("ignore") || vm.count("fixup")  || inputs.size()>1)
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
static int diffCmd(variables_map& vm, ostream& out)
{
    vector<path> inputs;
    if(vm.count("input")) inputs=vm["input"].as<vector<path>>();

    if(vm.count("help")  || vm.count("source") || vm.count("target")
    || vm.count("fixup") || inputs.size()<2 || inputs.size()>3)
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

/**
 * ddm scrub command
 */
static int scrubCmd(variables_map& vm, ostream& out)
{
    vector<path> inputs;
    if(vm.count("input")) inputs=vm["input"].as<vector<path>>();

    bool err=true;
    if(!vm.count("help") && !vm.count("ignore") && !vm.count("nohash"))
    {
        if(vm.count("source") && vm.count("target") && inputs.size()==2)
            err=false;
        if(!vm.count("source") && !vm.count("target") && inputs.size()==3)
            err=false;
    }
    if(err)
    {
        cerr<<R"(ddm diff
Usage:
ddm scrub <dir> <met> <met>             # Check for bit rot, correct if possible
ddm scrub -s <dir> -t <dir> <met> <met> # Check for bit rot, correct if possible
                                        # also checks source dir
)";
        return 100;
    }

    if(vm.count("source") && vm.count("target"))
        return scrub(vm["source"].as<path>(),vm["target"].as<path>(),
                     inputs.at(0),inputs.at(1),vm.count("fixup"),
                     !vm.count("singlethread"),printWarning);
    else
        return scrub(inputs.at(0),inputs.at(1),inputs.at(2),vm.count("fixup"),
                     printWarning);
}

/**
 * ddm backup command
 */
static int backupCmd(variables_map& vm, ostream& out)
{
    vector<path> inputs;
    if(vm.count("input")) inputs=vm["input"].as<vector<path>>();

    if(vm.count("help") || vm.count("ignore") || vm.count("nohash") ||
       !vm.count("source") || !vm.count("target") ||
       (inputs.size()!=0 && inputs.size()!=2))
    {
        cerr<<R"(ddm backup
Usage:
ddm backup -s <dir> -t <dir>                # Backup source dir to target dir
ddm backup -s <dir> -t <dir> <met> <met>    # Backup and update bit rot copies
                                            # also performs scrub of backup
)";
        return 100;
    }

    if(inputs.size()==2)
        return backup(vm["source"].as<path>(),vm["target"].as<path>(),
                     inputs.at(0),inputs.at(1),vm.count("fixup"),
                     !vm.count("singlethread"),printWarning);
    else
        return backup(vm["source"].as<path>(),vm["target"].as<path>(),
                      !vm.count("singlethread"),printWarning);
}

int main(int argc, char *argv[]) try
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
        ("source,s", value<path>(), "source")
        ("target,t", value<path>(), "target")
        ("ignore,i", value<string>(), "ignore")
        ("output,o", value<path>(), "output")
        ("nohash,n", "omit hash computation")
        ("fixup",    "attempt to fixup backup directory if scrub finds issues")
        ("singlethread", "don't scan source and target dir in separate threads")
        ("input",    value<vector<path>>(), "input") //Positional catch-all
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
        auto outFileName=vm["output"].as<path>();
        if(exists(outFileName))
        {
            cerr<<redb<<"Output file "<<outFileName
                <<" already exists. Aborting."<<reset<<'\n';
            return 10;
        }
        outfile.open(outFileName);
        if(!outfile)
        {
            cerr<<redb<<"Error opening "<<outFileName<<". Aborting."<<reset<<'\n';
            return 10;
        }
        out=&outfile;
    }

    //Decide what to do
    const map<string,function<int (variables_map&, ostream&)>> operations=
    {
        {"ls",     lsCmd},
        {"diff",   diffCmd},
        {"scrub",  scrubCmd},
        {"backup", backupCmd},
    };
    auto it=operations.find(argv[0]);
    if(it==operations.end()) help();
    return it->second(vm,*out);
} catch(exception& e) {
    cerr<<"\n"<<redb<<"Error: "<<e.what()<<reset<<"\n";
    return 10;
}
