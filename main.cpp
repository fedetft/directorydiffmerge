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
#include <functional>
#include <map>
#include <boost/program_options.hpp>
#include "diskdiff.h"

using namespace std;
using namespace std::filesystem;
using namespace boost::program_options;

/*
 * - We shall fail and/or report an error/warning if when listing a directory
 *   or loading a diff file we encounter an unhandled file type (not regular,
 *   directory or symlink).
 *
 * NON-issues: we don't want to handle hardlinks.
 * For us, they're two separate files.
 * MAYBE-issues: shall we handle filesystem loops through symlinks to directories?
 */

bool ls(variables_map& vm)
{
    string outFileName;
    ofstream outfile;
    auto getOutputFile=[&]() -> ostream&
    {
        if(outFileName.empty()) return cout;
        outfile.open(outFileName);
        return outfile;
    };

    if(vm.count("out"))
    {
        outFileName=vm["out"].as<string>();
        if(exists(outFileName))
        {
            cerr<<"Output file "<<outFileName<<" already exists. Aborting.\n";
            return true;
        }
    }

    if(vm.count("source"))
    {
        FileLister fl(getOutputFile());
        fl.listFiles(vm["source"].as<string>());
        return true;
    }

    return false;
}

bool test(variables_map&)
{
    string line;
    while(getline(cin,line))
    {
        FilesystemElement fe(line);
        fe.writeTo(cout);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    options_description desc("diskdiff options");
    desc.add_options()
        ("help",     "prints this")
        ("source,s", value<string>(), "source directory")
        ("out,o",    value<string>(), "save data to arg instead of stdout")
    ;

    if(argc<2)
    {
        cout<<desc<<'\n';
        return 1;
    }

    string op=argv[1];
    variables_map vm;
    store(parse_command_line(argc,argv,desc),vm);
    notify(vm);

    const map<string,function<bool (variables_map&)>> operations=
    {
        {"ls", ls},
        {"test", test}
    };

    auto it=operations.find(op);
    if(it!=operations.end() && it->second(vm)) return 0;

    //No valid option passed, print help
    cout<<desc<<'\n';
    return 1;
}
