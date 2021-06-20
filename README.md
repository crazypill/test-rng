# true-rng-playlist

This utlity creates a *truly random playlist* using a TRNG device.  It will also work if pointed at */dev/random* or */dev/urandom* but won't be as random as they are mostly pseudo-random...

I was annoyed that most music apps use *SQL* to randomly pull a playlist when in shuffle mode.  iTunes and SqueezeCenter both seem to clump up and play some tracks way more often than real-random would produce (I have no science to back this, all empirical).  Also they tend to never play certain tracks.  I've been paying attention to this for ten years.  It took me that long to get off my ass and fix it.
Plus I wanted to be able to play all my music from start to finish completely randomly and hear every track only once. 

Point this utlity at a folder of files and it will take the full recursive file list and truly randomize it-  This doesn't use *SQL* or **anything weird**.  Just a very random list of indices that map to the file list.  Point it at your music folder and it will generate a playlist if you give the file a .m3u extension.

As an example (on Mac):

`trueRNG --dir ~/iTunes/Music --output my_really_random_playlist.m3u`

The above example will use the pseudo random number generator by default.  On my system the TrueRNGv3 mostly comes up as: /dev/cu.usbmodem14401.  You will get a different modem device depending on several factors, so check /dev directory for yours.  
The code doesn't incorporate any platform specific code to find a particular TRNG device.

`trueRNG --device /dev/cu.usbmodemXXXXX --dir ~/iTunes/Music --output my_really_random_playlist.m3u`

A text file is output which simply lists the paths.  This is a stripped down version of the .m3u file format (which is a music playlist that most apps can import or use directly).

The theory of operation is simple:  we fill an array with random numbers that are unique.  They span from zero to the number of files.  
The list is generated using the same algorithm as arc4random_uniform (of which I used the actual source code but swapped out the random number input).  
This allows us to specify an upper bound which is the number of files.  To fill the array we need to know if the number is already in the array and this could take a while if we have to search the array each time.  
To avoid this, the code uses a bitmap field where each bit represents each number that could be in the array.  Instead of searching the array, we check the bitmap.  Once the array is filled, the code then uses that array as an index into the file list.  

From there a text file is output with this random order.

If you are porting this code and run into the mach/time issue, just remove the include and make the timeGetTimeMs() routine return 0.  It's only used for timing and not critical to the functionality.  
The rest should be completely portable to any POSIX or Linux system.  

Most likely this will compile with:
`cc main.c -o trueRNG`
