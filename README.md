# true-rng-playlist

This utlity creates a truly random playlist using a TRNG device.  It will also work if pointed at /dev/random or urandom.

Point the utlity at a folder of files and it will take the full recursive file list and truly randomize it-  This doesn't use SQL or anything weird.  Just a very random list of indices that map to the file list.

A text file is output which simply lists the paths.  This is a stripped down version of the .m3u file format (which is a music playlist).
