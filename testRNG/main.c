//
//  main.c
//  testRNG
//
//  Created by Alex Lelievre on 5/31/21.
//

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <dirent.h>


#define NORMAL_COLOR  "\x1B[0m"
#define GREEN  "\x1B[32m"
#define BLUE  "\x1B[34m"



typedef struct tagPathLink
{
    const char*         path;
    struct tagPathLink* next;
} PathLink;


#pragma mark -


uint8_t* create_bitmap_array( uint32_t item_count )
{
    uint32_t num_bytes = item_count / sizeof( uint8_t );
    
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
    int fd = open( "/dev/cu.usbmodem14401", O_RDWR );
    if( fd < 0 )
        perror( "open failed" );

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
        perror( "read failed" );
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
                {
                    (*tail)->next = link;
                    *tail = link;
                }
                else
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
//              printf( "%s%s\n", GREEN, d_path );
              get_directory_content( d_path, head, tail );
          }
        }
    }
    closedir( d );
}


void dispose_directory_content( PathLink* head )
{
    while( head )
    {
        if( head->path )
            free( head->path );
        
        PathLink* dead = head;
        head = head->next;
        free( dead );
    }
}



#pragma mark -


int main( int argc, const char * argv[] )
{
    int       fd   = -1;
    PathLink* head = NULL;
    PathLink* tail = NULL;
    uint32_t  item_count = 100;
    
    printf( "%s\n", NORMAL_COLOR );
    
    uint8_t* bitmap = create_bitmap_array( item_count );
    if( !bitmap )
        goto exit_gracefully;
    
    fd = start_random_generator();
    if( fd < 0 )
    {
        perror( "start_random_generator failed" );
        goto exit_gracefully;
    }
    
//    show_directory_content( argv[1] );
    get_directory_content( argv[1], &head, &tail );
    
    while( head )
    {
        printf( "%s\n", head->path );
        head = head->next;
    }
    
    
    uint32_t* array = fill_random_index_array( fd, item_count, bitmap );
    if( array )
    {
        for( int i = 0; i < item_count; i++ )
        {
            printf( "value[%d]: %d\n", i, array[i] );
        }
        
        free( array );
    }

exit_gracefully:
    printf( "%s\n", NORMAL_COLOR );
    dispose_directory_content( head );
    dispose_bitmap_array( bitmap );
    stop_random_generator( fd );
    return 0;
}
