# DDM: The DirectoryDiffMerge tool

Currently in alpha release. TODO: write better documentation.

## Use as backup tool

ddm uses two metadata files stored in the backup path to have triple redundancy
for metadata, and single redundancy for data. Of course, since this is a backup,
a second copy of the data is the source directory itself, so if the single backup
copy becomes corrupted, ddm can fix the backup by taking the corrupted files from
the source directory.

ddm implements bit rot detction forboth the source and backup directory. TODO: document me

### Initializing the backup

The first time you want to create a backup with ddm you need to create the backup directory. Assuming you have a backup directory `directory` in path `srcdir_path` and want to create the backup in `backup_path`, use the following commands.

```
cp -P -r --preserve=all srcdir_path/directory backup_path  # Copy the directory to backup
ddm ls srcdir_path/directory -o backup_path/metadata1.ddm  # Create 1st metadata file
cp backup_path/metadata1.ddm backup_path/metadata2.ddm     # Create 2nd metadata file
```

### Updating the backup

Once the backup has been created, you can back up your source directory any time you want with the following command. The `--fixup` option is optional, if passed, ddm will try to fix the backup directory if problems arise. Note that the process may be interactiove, thus requiring user input.

```
ddm backup --fixup -s srcdir_path/directory -t backup_path/directory backup_path/metadata1.ddm backup_path/metadata2.ddm
```

### Scrubbing the backup

If you want to check if the backup is consistent you can do a scrub. The `--fixup` option allows to fix problems if present.

```
ddm scrub --fixup -s srcdir_path/directory -t backup_path/directory backup_path/metadata1.ddm backup_path/metadata2.ddm
```

Or if you don't have currently access to the source directory you can use this version of the command (note that if backup files are found to be corrupted, you need to re-run the command with the source directory to fix the backup).

```
ddm scrub --fixup backup_path/directory backup_path/metadata1.ddm backup_path/metadata2.ddm
```
