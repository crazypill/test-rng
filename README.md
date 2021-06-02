# true-rng-playlist

This utlity creates a *truly random playlist* using a TRNG device.  It will also work if pointed at */dev/random* or */dev/urandom* but won't be as random as they are mostly pseudo-random...

I was annoyed that most music apps use *SQL* to randomly pull a playlist when in shuffle mode.  iTunes and SqueezeCenter both seem to clump up and play some tracks way more often than real-random would produce (I have no science to back this, all empirical).  Also they tend to never play certain tracks.  I've been paying attention for ten years to this.  It took me that long to get off my ass and fix it.

Point this utlity at a folder of files and it will take the full recursive file list and truly randomize it-  This doesn't use *SQL* or **anything weird**.  Just a very random list of indices that map to the file list.  Point it at your music folder and it will generate a playlist if you give the file a .m3u extension.

As an example (on Mac):

`trueRNG --dir ~/iTunes/Music --output my_really_random_playlist.m3u`

The above example will most likely fail because there is no randon number generator specified.  On my system the TrueRNGv3 comes up as: /dev/cu.usbmodem14401 (which is the default).  You will get a different modem device depending on several factors.  The code doesn't incorporate any platform specific code to find a particular TRNG device.

`trueRNG --device /dev/cu.usbmodem14401 --dir ~/iTunes/Music --output my_really_random_playlist.m3u`

A text file is output which simply lists the paths.  This is a stripped down version of the .m3u file format (which is a music playlist that most apps can import or use directly).

The theory of operation is simple:  we fill an array with random numbers that are unique.  They span from zero to the number of files.  The list is generated using the same algorithm as arc4random_uniform (of which I used the actual source code but swapped out the random number input).  This allows us to specify an upper bound which is the number of files.  Once the array is filled, the code then uses that array as an index into the file list.  From there a text file is output with this random order.
