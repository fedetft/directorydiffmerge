# DDM: The DirectoryDiffMerge tool

Currently in alpha release. TODO: write better documentation.

## Use as backup tool

ddm uses two metadata files stored in the backup path to have triple redundancy
for metadata, and single redundancy for data. Of course, since this is a backup,
a second copy of the data is the source directory itself, so if the single backup
copy becomes corrupted, ddm can fix the backup by taking the corrupted files from
the source directory.

ddm implements bit rot detction for both the source and backup directory. TODO: document me

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
