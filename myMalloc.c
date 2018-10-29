/* Simple Malloc implementation. */

#include <stdio.h>
#include <unistd.h> // for brk and sbrk
#include <stdlib.h> // for atexit

/*
 * Represent the explicit free-list.
 * prev: points to previous free block
 * next: points to next free block
 * size: size of the usable memory in block
 */
typedef struct block_metadata {
   struct block_metadata *prev;
   struct block_metadata *next;
   size_t size;
} block;

block *head = NULL;

/* allocate a fixed number of bytes that is a multiple
 * of the memory page size
 */
#ifndef ALLOC_UNIT
#define ALLOC_UNIT 3*sysconf( _SC_PAGESIZE )
#endif

#ifndef MIN_DEALLOC
#define MIN_DEALLOC 1 * sysconf(_SC_PAGESIZE)
#endif

#define BLOCK_MEM( ptr ) ( void * )( (unsigned long)ptr + sizeof( block ) )
#define BLOCK_HEADER( ptr ) ( void * )( (unsigned long)ptr - sizeof( block ) )

/*
 * Print the current state of blocks on free-list.
 */
void stats( char * stage ) {
   printf( "Program break at %s : %ld \n", stage, sbrk( 0 ) );

   /* print the current free-list */
   block *ptr = head;
   while( ptr ) {
      printf( "block addr: %ld, size: %ld \n", ptr, ptr->size );
      ptr = ptr->next;
   }
}

/*
 * Split the block 'b' after 'sizeof( block ) + size' bytes and
 * return the new block.
 */
block * splitBlock( block *b, size_t size ) {
   block *newBlock;
   newBlock = (block *) ( (unsigned long)b + sizeof( block ) + size );
   newBlock->size = b->size - ( sizeof( block ) + size );
   b->size = size;
   return newBlock;
}

/*
 * Adds a block to the free list keeping the list sorted by block
 * begin address, this helps when scanning continous blocks
 */
void addToFreeList( block *freeBlock ) {
   printf( "Adding %ld with size %ld to free list \n", freeBlock,
           freeBlock->size );
   freeBlock->next = NULL;
   freeBlock->prev = NULL;
   if( !head || (unsigned long) head >= (unsigned long) freeBlock ) {
      if( head ) {
         head->prev = freeBlock;
      }
      freeBlock->next = head;
      head = freeBlock;
      return;
   }

   block *ptr = head;
   while( ptr->next && (unsigned long) ptr->next < (unsigned long) freeBlock ) {
      ptr = ptr->next;
   }
   freeBlock->next = ptr->next;
   freeBlock->prev = ptr;
   ( ptr->next )->prev = freeBlock;
   ptr->next = freeBlock;
   return;
}

/* Removes a block from the free list */
void removeFromFreeList( block * b ) {
   if( !b->prev ) {
      if( b->next ) {
         head = b->next;
      } else {
         head = NULL;
      }
   } else {
      b->prev->next = b->next;
   }
   if( b->next ) {  
      b->next->prev = b->prev;
   }
}

void * _malloc( size_t size ) {
   block *ptr = head;
   while( ptr ) {
      /* traverse free list and find suitable block */
      if( ptr->size >= size ) {
         removeFromFreeList( ptr );
         if( ptr->size == size ) {
            return BLOCK_MEM( ptr );
         }
         // block is bigger than requested, split and add
         // the spare to free-list
         block *newBlock = splitBlock( ptr, size );
         addToFreeList( newBlock );
         return BLOCK_MEM( ptr );
      }
      ptr = ptr->next;
   }

   /* Unable to find free block on free list
    * So, ask from OS for more memory using sbrk.
    *
    * We alloc more alloc_size and then split the newly
    * allocated block to keep the spare space in our free list.
    */
   size_t alloc_size;
   if( size >= ALLOC_UNIT ) {
      alloc_size = size + sizeof( block );
   } else {
      alloc_size = ALLOC_UNIT;
   }

   ptr = sbrk( alloc_size );
   if( !ptr ) {
      printf( "Failed to alloc %d \n", alloc_size );
   }
   ptr->prev = NULL;
   ptr->next = NULL;
   /* only store the usable size */
   ptr->size = alloc_size - sizeof( block );

   if( alloc_size > size + sizeof( block ) ){
      // split the newly allocated block and add to the free block
      // chunk to free list and return other chunk.
      block *newBlock = splitBlock( ptr, size );
      addToFreeList( newBlock );
   }
   return BLOCK_MEM( ptr );
}

/* scans the free list to find continuous free blocks that
 * can be coaleases and also checks if our last free block
 * ends where the program break is. If it does, and the free
 * block is larger then MIN_DEALLOC then the block is released
 * to the OS, by calling brk to set the program break to the begin of
 * the block */
void scanAndCoalesce() {
   block *curr = head;
   unsigned long curr_addr, next_addr;
   while( curr->next ) {
      curr_addr = ( unsigned long ) curr;
      next_addr = ( unsigned long ) curr->next;
      if( curr_addr + sizeof( block ) + curr->size == next_addr ) {
         // found two continous block, merge to form a new large block
         curr->size += curr->next->size + sizeof( block );
         curr->next = curr->next->next;
         if( curr->next->next ) {
            curr->next->next->prev = curr;
         } else {
            break;
         }
      }
      curr = curr->next;
   }
   stats( "after merge" );
   /* check if last free block ends on the program break and is
    * big enough to be releases to the OS
    */
   unsigned long program_break = (unsigned long) sbrk(0);
   if( program_break == 0 ) {
      printf( "failed to retrive program break\n" );
      return;
   }
   curr_addr = ( unsigned long ) curr;
   if( curr_addr + curr->size + sizeof( block ) == program_break
       && curr->size >= MIN_DEALLOC ) {
      removeFromFreeList( curr );
      if( brk( curr ) != 0 ) {
         printf( "error freeing memory \n" );
      }
   }
}

/*
 * Puts the block on free-list and merge the contigous blocks
 */
void _free( void * addr ) {
   block *block_addr = BLOCK_HEADER( addr );
   addToFreeList( block_addr );
   stats( "before coalesing " );
   scanAndCoalesce();
}

void cleanup() {
   printf( "Cleaning up memory ...\n" );
   if( head != NULL ) {
      if( brk( head ) != 0 ) {
         printf( "Failed to cleanup memory" );
      }
   }
   head = NULL;
}

int main() {
   atexit( cleanup );

   printf( "Malloc implementation\n" );

   stats( "beginning" );
   int *p1 =_malloc( 64 );
   stats( "after allocating 64 bytes" );
   int *p2 = _malloc( 1 );
   _free( p1 );
   return 0;
}
