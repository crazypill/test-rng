//
//  main.c
//  testRNG
//
//  Created by Alex Lelievre on 5/31/21.
//

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <getopt.h>
#if __APPLE__
#include <mach/mach_time.h>
#endif

#define NORMAL_COLOR  "\x1B[0m"
#define GREEN  "\x1B[32m"
#define BLUE  "\x1B[34m"


#define TRNG_DEVICE   "/dev/urandom"
#define PROGRAM_NAME  "true-rng-playlist"
#define VERSION       "100"

// define this to pull a few numbers from the RNG before startup
//#define PRIME_RNG


#ifdef WIN32
typedef DWORD    TimeInterval;
#else
typedef uint64_t TimeInterval;
#endif


typedef struct tagPathLink
{
    const char*         path;
    struct tagPathLink* next;
} PathLink;


#pragma mark -



static const char* s_trng_device       = TRNG_DEVICE;
static const char* s_musicRepoPath     = NULL;
static const char* s_musicPlayListPath = NULL;


#pragma mark -

#if __APPLE__ || defined( WIN32 )
TimeInterval timeGetTimeMS()
{
#ifdef WIN32
    return timeGetTime();
#else
    static mach_timebase_info_data_t sTimebaseInfo;
    uint64_t now = mach_absolute_time();
    
    // Convert to milliseconds.
    
    // If this is the first time we've run, get the timebase.
    // We can use denom == 0 to indicate that sTimebaseInfo is
    // uninitialised because it makes no sense to have a zero
    // denominator is a fraction.
    
    if( sTimebaseInfo.denom == 0 )
        mach_timebase_info( &sTimebaseInfo );
    
    // Do the maths. We hope that the multiplication doesn't
    // overflow; the price you pay for working in fixed point.
    return (now / 1000000) * sTimebaseInfo.numer / sTimebaseInfo.denom;
#endif
}
#else
TimeInterval timeGetTimeMS()
{
    return 0; // not used for anything other than debug timing, so please implement as you like!
}
#endif


#pragma mark -


uint8_t* create_bitmap_array( uint32_t item_count )
{
    uint32_t num_bytes = item_count / 8;    // eight bits in a byte righto?
    
    // allocate a bit array to hold 0..n bits to prevent searching the list to see if a number is already in there
    uint8_t* bitmap = (uint8_t*)malloc( num_bytes );
    if( !bitmap )
    {
        perror( "failed to allocate bitmap" );
        return NULL;
    }
    
    memset( bitmap, 0, num_bytes );
    return bitmap;
}


void dispose_bitmap_array( uint8_t* bitmap )
{
    if( bitmap )
        free( bitmap );
}


void setbit( uint32_t value, uint8_t* bitmap, uint32_t bitmap_count )
{
    if( !bitmap )
        return;
    
    // map value into the bitmap, first find the array element, then bit
    uint32_t index = value / 8;
    uint8_t  bit   = value % 8;
//    printf( "index: %d, bit number: %d\n", index, bit );
    
    bitmap[index] |= (1 << bit);
}


bool getbit( uint32_t value, uint8_t* bitmap, uint32_t bitmap_count )
{
    if( !bitmap )
        return false;
    
    // map value into the bitmap, first find the array element, then bit
    uint32_t index = value / 8;
    uint8_t  bit   = value % 8;
    
    return bitmap[index] & (1 << bit);
}


#pragma mark -


int start_random_generator()
{
    int fd = open( s_trng_device, O_RDWR );
    if( fd < 0 )
        perror( "open failed" );

#ifdef PRIME_RNG
    // start the generator by reading a tad from it
    for( int i = 0; i < 10; i++ )
    {
        uint32_t rando = 0;
        ssize_t bytesRead = read( fd, &rando, sizeof( uint32_t ) );
#ifdef DEBUG
        if( bytesRead != sizeof( uint32_t ) )
            printf( "rng priming failed...\n" );
#endif
    }
#endif
    return fd;
}


void stop_random_generator( int fd )
{
    close( fd );
}


uint32_t get_random( int fd )
{
    uint32_t rando = 0;
    ssize_t bytesRead = read( fd, &rando, sizeof( uint32_t ) );
    if( bytesRead != sizeof( uint32_t ) )
        printf( "read failed...\n" );
    return rando;
}


/*
 * Calculate a uniformly distributed random number less than upper_bound
 * avoiding "modulo bias".
 *
 * Uniformity is achieved by generating new random numbers until the one
 * returned is outside the range [0, 2**32 % upper_bound).  This
 * guarantees the selected random number will be inside
 * [2**32 % upper_bound, 2**32) which maps back to [0, upper_bound)
 * after reduction modulo upper_bound.
 */
uint32_t get_random_uniform( int fd, uint32_t upper_bound )
{
    uint32_t r, min;

    if( upper_bound < 2 )
        return 0;

    /* 2**32 % x == (2**32 - x) % x */
    min = -upper_bound % upper_bound;

    /*
     * This could theoretically loop forever but each retry has
     * p > 0.5 (worst case, usually far better) of selecting a
     * number inside the range we need, so it should rarely need
     * to re-roll.
     */
    for( ;; )
    {
        r = get_random( fd );
        if( r >= min )
            break;
    }

    return r % upper_bound;
}


#pragma mark -

uint32_t* fill_random_index_array( int fd, uint32_t item_count, uint8_t* bitmap )
{
    uint32_t fill_count = 0;
    
    uint32_t* array = (uint32_t*)malloc( item_count * sizeof( uint32_t ) );
    if( !array )
        return NULL;
    
    while( fill_count < item_count )
    {
        uint32_t v = get_random_uniform( fd, item_count );
        
        // see if we already have this number in the array...
        if( !getbit( v, bitmap, item_count ) )
        {
            // number isn't there so add it...
            setbit( v, bitmap, item_count );
            array[fill_count++] = v;
        }
    }

    return array;
}

#pragma mark -


void show_directory_content( const char* path )
{
    if( !path )
    return;

    DIR* d = opendir( path ); // open the path

    if( !d )
    return;

    struct dirent* dir;
    while( (dir = readdir( d )) != NULL ) // if we were able to read somehting from the directory
    {
        if( dir-> d_type != DT_DIR ) // if the type is not directory just print it with blue
            printf( "%s%s/%s\n", BLUE, path, dir->d_name );
        else
        {
          // if it is a directory, recurse (which is bad but won't be unbound at least)
          if( dir -> d_type == DT_DIR && strcmp( dir->d_name, "." ) != 0 && strcmp( dir->d_name, ".." ) != 0 )
          {
              char d_path[1024]; // here I am using sprintf which is safer than strcat
              sprintf( d_path, "%s/%s", path, dir->d_name );
              printf( "%s%s\n", GREEN, d_path );
              show_directory_content( d_path );
          }
        }
    }
    closedir( d );
}


void get_directory_content( const char* path, PathLink** head, PathLink** tail )
{
    if( !path )
        return;

    DIR* d = opendir( path ); // open the path

    if( !d )
        return;

    struct dirent* dir;

    // if we were able to read somehting from the directory
    while( (dir = readdir( d )) != NULL )
    {
        if( dir-> d_type != DT_DIR )
        {
            // add file to array
            size_t path_length = strlen( path ) + strlen( dir->d_name ) + 2; // 2 bytes extra space, one for the slash and one for the null byte
            char* full_path = malloc( path_length );
            sprintf( full_path, "%s/%s", path, dir->d_name );
            
            PathLink* link = malloc( sizeof( PathLink ) );
            
            if( link )
            {
                link->path = full_path;
                link->next = NULL;
                
                if( !*head )
                    *head = link;
                
                if( *tail )
                    (*tail)->next = link;
                *tail = link;
            }
        }
        else
        {
          // if it is a directory, recurse (which is bad but won't be unbound at least)
          if( dir -> d_type == DT_DIR && strcmp( dir->d_name, "." ) != 0 && strcmp( dir->d_name, ".." ) != 0 )
          {
              char d_path[4096];
              sprintf( d_path, "%s/%s", path, dir->d_name );
              get_directory_content( d_path, head, tail );
          }
        }
    }
    closedir( d );
}


uint32_t count_directory_contents( PathLink* head )
{
    uint32_t count = 0;
    while( head )
    {
        ++count;
        head = head->next;
    }
    return count;
}



void dispose_directory_content( PathLink* head )
{
    while( head )
    {
        if( head->path )
            free( (void*)head->path );
        
        PathLink* dead = head;
        head = head->next;
        free( (void*)dead );
    }
}


bool file_ext_is_audio( const char* filename )
{
    // strrchr to '.' then compare extension if there is one...  files without an extension are ignored.
    return true;
}

#pragma mark -




void version( int argc, const char* argv[] )
{
    printf( "%s, version %s", PROGRAM_NAME, "1.0.0" );
#ifdef DEBUG
    fputs(", compiled with debugging output", stdout);
#endif
    puts(".\n\
        Copyright (c) 2021 Far Out Labs, LLC.\n\
        This program comes with ABSOLUTELY NO WARRANTY. This is free software, and you\n\
        are welcome to redistribute it under certain conditions.  See the GNU General\n\
        Public License (version 3.0) for more details.");
    return;
}


void usage( int argc, const char* argv[] )
{
    printf( "Typical usage: %s --dir \"/my_music_folder\" --output \"/my_truly_random_playlist.m3u\"\n", PROGRAM_NAME );
    return;
}


void help( int argc, const char* argv[] )
{
    version( argc, argv );
    puts("");
    usage( argc, argv );
    puts( "\n\
        Special parameters:\n\
            -H, --help                 Show this help and exit.\n\
            -v, --version              Show version and licensing information, and exit.\n\
        Override parameters:\n\
            -e, --device               Set the random number generator device to use.\n\
         Required parameters:\n\
            -d, --dir                  Set the directory to use.\n\
            -o, --output               Set the filename to use as output.\n\
        " );
}


void handle_command( int argc, const char * argv[] )
{
    int c = '\0';          /* for getopt_long() */
    int option_index = 0;  /* for getopt_long() */

    const static struct option long_options[] = {
        {"help",                    no_argument,       0, 'H'},
        {"version",                 no_argument,       0, 'v'},
        {"dir ",                    required_argument, 0, 'd'},
        {"device",                  required_argument, 0, 'e'},
        {"output",                  required_argument, 0, 'o'},

        {0, 0, 0, 0}
        };

    while( (c = getopt_long( argc, (char* const*)argv, "Hvd:e:o:", long_options, &option_index)) != -1 )
    {
        switch( c )
        {
            /* Complete help (-H | --help) */
            case 'H':
                help( argc, argv );
                break;

            /* Version information (-v | --version) */
            case 'v':
                version( argc, argv );
                break;

            case 'd':
                s_musicRepoPath = optarg;
                break;

            case 'e':
                s_trng_device = optarg;
                break;

            case 'o':
                s_musicPlayListPath = optarg;
                break;
        }
    }
}


#pragma mark -

int main( int argc, const char * argv[] )
{
    int       fd   = -1;
    PathLink* head = NULL;
    PathLink* tail = NULL;
    uint8_t*  bitmap = NULL;
    char** shadow_index = NULL;
    
    handle_command( argc, argv );

    TimeInterval elapsed = 0;
    TimeInterval startTime = timeGetTimeMS();
    TimeInterval currentTime = timeGetTimeMS();
    printf( "Starting random number generator...\n" );
    
    fd = start_random_generator();
    if( fd < 0 )
    {
        perror( "start_random_generator failed" );
        goto exit_gracefully;
    }
    
    if( !s_musicRepoPath )
    {
        printf( "music repo path missing!" );
        goto exit_gracefully;
    }

    if( !s_musicPlayListPath )
    {
        printf( "output playlist path missing!" );
        goto exit_gracefully;
    }

    elapsed = timeGetTimeMS() - currentTime;
    currentTime = timeGetTimeMS();
    
    printf( "Done in %llu ms.\n\nGetting directory contents...\n", elapsed );
    get_directory_content( s_musicRepoPath, &head, &tail );
    
    // now create a shadown index of linked list so we don't have to iterate the list to retrieve the elements
    uint32_t dir_count = count_directory_contents( head );
    
    shadow_index = (char**)malloc( dir_count * sizeof( char* ) );
    if( !shadow_index )
    {
        perror( "failed to create shadow_index!" );
        goto exit_gracefully;
    }
    uint32_t index = 0;
    PathLink* item = head;
    while( item )
    {
        shadow_index[index++] = (char*)item->path;
        item = item->next;
    }
    
    bitmap = create_bitmap_array( dir_count );
    if( !bitmap )
        goto exit_gracefully;


    elapsed = timeGetTimeMS() - currentTime;
    currentTime = timeGetTimeMS();

    printf( "Done in %llu ms.\n\nRandomizing contents for %d items...\n", elapsed, dir_count );
    uint32_t* array = fill_random_index_array( fd, dir_count, bitmap );
    if( array )
    {
        FILE* mp = fopen( s_musicPlayListPath, "w" );
        if( mp )
        {
            elapsed = timeGetTimeMS() - currentTime;
            currentTime = timeGetTimeMS();

            printf( "Done in %llu ms.\n\nWriting playlist...\n", elapsed );
            for( int i = 0; i < dir_count; i++ )
            {
                if( file_ext_is_audio( shadow_index[array[i]] ) )
                {
                    printf( "[%d:%d] %s\n", i, array[i], shadow_index[array[i]] );
                    fprintf( mp, "%s\n", shadow_index[array[i]] );
                }
            }
            
            fclose( mp );
        }
        
        free( array );
    }
    
    
exit_gracefully:
    elapsed = timeGetTimeMS() - currentTime;
    printf( "Done in %llu ms.\n\nElapsed time: %llu ms.\n", elapsed, timeGetTimeMS() - startTime );
    dispose_directory_content( head );
    dispose_bitmap_array( bitmap );
    stop_random_generator( fd );
    if( shadow_index )
        free( shadow_index );
    return 0;
}
