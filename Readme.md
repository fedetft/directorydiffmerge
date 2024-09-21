# DDM: The DirectoryDiffMerge tool

Version 1.00.

ddm is a generic tool designed to compare directory trees as opposed to lines
inside files. Although tools primarily targeted at files such as diff can
compare entire directories, ddm improves upon the use of file-based diff tools
in key areas:

- ddm also compares file metadata. When comparing directories, ddm can tell
you if files differ in their owner, group, permissions, last modified time and
more. Of course, you can freely mask any metadata you don't want to compare
for fine grain control of your comparisons.
- ddm is aware of the directory structure. Indeed, ddm internally compares
directory trees instead of lists of files. This leads to more meaningful, less
cluttered output. Assume for example you are comparing two directory trees and
only one of those incudes a directory with thousands of files. Standard diffing
tool such as diff will report every single file as different, while ddm just
reports you that a directory is missing.

ddm is based on an internal representation of a directory tree and its metadata.
This representation has an in-memory form, used internally by the tool, and a
printable form that the ddm tool cam produce as an output, as well as accept as
input. You can produce a metadata file from a directory with the `ddm ls` option.
Indeed, when comparing directories, ddm interchangebly accepts either a path to
a directory in your filesystem or a ddm metadata file.
This option opens up many possibilities such as comparing a directory with a
previous version without the need to save a copy of the entire directory and
all the contained files, or to compare two directories on separate machines by
only transfering the metadata file.

ddm can compare directories with the `ddm diff` option.
It can perform two and three way comparisons between directories.

ddm is not limited to comparing directories.
After the comparison, it can also change the content of a target directory to
make it equal to a source directory.
This makes ddm useful as a backup tool, for this reason this option is
called `ddm backup`.


## Use as backup tool

ddm can use two metadata files stored in the backup path to have triple redundancy
for metadata, and single redundancy for data.
This is the recommended way to use ddm for backups. Of course, since this is a backup,
a second copy of the data is the source directory itself, so if the single backup
copy becomes corrupted, ddm can fix the backup by taking the corrupted files from
the source directory.

When performing backup with triple metadata redundancy, ddm implements bit rot
detction for both the source and backup directory.

### Initializing the backup

The first time you want to create a backup with ddm you need to create the backup directory. Assuming you have the directory to backup named `directory` in path `srcdir_path` and want to create the backup directory in `backup_path`, use the following commands.

```
cp -P -r --preserve=all srcdir_path/directory backup_path  # Copy the directory to backup
ddm ls backup_path/directory -o backup_path/m1.ddm         # Create 1st metadata file
cp backup_path/m1.ddm backup_path/m2.ddm                   # Create 2nd metadata file (copy)
```

### Updating the backup

Once the backup has been created, you can back up your source directory any time you want with the following command.

```
ddm backup --fixup -s srcdir_path/directory -t backup_path/directory backup_path/m1.ddm backup_path/m2.ddm
```

It is best to write the command in a shell script file to use it every time you want to update the backup.
DO NOT swap the source and backup directories! Remeber, `-s` stands for source directory, where ddm will READ, and `-t` stands for target directory, where ddm will WRITE.

Also note that you SHOULD NOT MODIFY the content of the source directory while the backup is in progress.

The `--fixup` option is optional, if passed, ddm will try to fix the backup directory if problems arise. Note that the process may be interactive, thus requiring user input.

### Updating the backup (fast version)

The default backup command computes the hashes of all files in both the source and backup directory, in order to check all files for bit rot. This is of course slow, so it is also possible to do a fast backup by omitting this bulk hash computation, by means of the `--nohash` option.

```
ddm backup --nohash --fixup -s srcdir_path/directory -t backup_path/directory backup_path/m1.ddm backup_path/m2.ddm
```

Note that ddm will still compute the hashes of all the files that have been modified, so as to keep the metadata files up to date with the latest hashes. In this way, it is possible to freely alternate between fast backups with the `--nohash` option and backups with bit rot checks.

It is of course recommended to perform a backup with bit rot check from time to time to prevent bit rot accumulation.


### Scrubbing the backup

If you want to check if the backup is consistent you can do a scrub. The `--fixup` option allows to fix problems if present.

```
ddm scrub --fixup -s srcdir_path/directory -t backup_path/directory backup_path/m1.ddm backup_path/m2.ddm
```

Or if you don't have currently access to the source directory you can use this version of the command (note that if backup files are found to be corrupted, you need to re-run the command with the source directory to fix the backup).

```
ddm scrub --fixup backup_path/directory backup_path/m1.ddm backup_path/m2.ddm
```

## Design limitations

The current version of ddm has the following design limitations:

- ddm does not handle hardlinks. ddm will print a warning if files with multiple hardlinks are found, and will treat multiple hardlinks to the same file as separate files. If preserving the hardlink struture is unimportant to you, you can ignore the warning.
- ddm will not handle special file types such as named pipes or sockets. These kind of files usually don't appear in user directories, which is the primary application for ddm. When comparing directories that include those special file types a warning will be printed that they can't be properly compared. Backup of directories including special file types is not supported.
- ddm will print warnings for files and directories it can't read for permission reasons. This isn't really a limitation, if the directory you're comparing includes files the user ddm is launched as can't read, then you should run ddm as root or as an appropriate user.


