
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
 * TODO list
 * - symlink handling is wrong, the weakly_canonical function resolve multiple
 *   levels of symlinks which is what we don't want. A symlink to another symlink
 *   shall not be flattened into pointing directly to the target file.
 *   Also, for broken symlinks the broken target is not preserved
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
