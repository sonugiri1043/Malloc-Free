Memory layout of a Process

The memory allocated to each process is composed of multiple segments.
We are particularly interested on the heap (also known as the data segment),
an area from which memory can be dynamically allocated at run time. The top
end of the heap is called the program break.

Adjusting the program break
We can move the program break on our C program by using sbrk() and brk().

#include <unistd.h>
int brk(void *end_data_segment);
// Returns 0 on success, or –1 on error

void *sbrk(intptr_t increment);
// Returns previous program break on success, or (void *) –1 on error

The first, moves the program break to the address pointed by addr,
Since virtual memory is allocated in units of pages, end_data_segment is
effectively rounded up to the next page boundary. while the latter increments
the program break by increment bytes. On success, sbrk()  returns the previous
address of the program break. In other words, if we have increased the program
break, then the return value is a pointer to the start of the newly allocated
block of memory. The call sbrk(0)  returns the current setting of the program
break without changing it. This can be useful if we want to track the size of
the heap, perhaps in

In order to monitor the behavior of a memory allocation package.
those are the basic building blocks that our malloc implementation will rely on.

https://github.com/sonugiri1043/ImplementYourOwn

Our implementation keeps a doubly linked listed of free memory blocks and every time
_malloc gets called, we traverse the linked list looking for a block with at least the
size requested by the user. If a block with the exact requested size exists, we remove
it from the list and return its address to the user; if the block is larger, we split it
into two blocks, return the one with the requested size to the user and adds the newly
created block to the list.

If we are unable to find a block on the list, we must “ask” the OS for more memory,
by using the sbrk function. To reduce the number of calls to sbrk, we alloc a fixed
number of bytes that is a multiple of the memory page size, defined as:

#define ALLOC_UNIT 3 * sysconf(_SC_PAGESIZE)

After the call to sbrk (where our program break changes value) we create a new block with
the allocated size. The metadata on this block contains the size, next and previous blocks
and is allocated on the first 24 bytes of the block (this is our overhead). Since we may
have allocated much more memory then the user requested, we split this new block and return
the one with the exact same size as requested

The BLOCK_MEM macro, defined as:
#define BLOCK_MEM(ptr) ((void *)((unsigned long)ptr + sizeof(block_t)))
returns skips the metadata at given ptr and returns the address of the memory area that is
available for the user.

The _free function is quite straightforward, given a pointer that was previously “malloced”
to the user, we must find its metadata (by using the BLOCK_HEADER macro) and add it to our
free linked list. After that, the function scanAndCoalease() is called to do some cleaning:

scanAndCoalease() first traverses the linked list looking for continuous blocks
(two different memory blocks that are free and correspond to continuous addresses). We keep
the blocks sorted by address to make this step easier. For every two continuous blocks found,
we merge both blocks to reduce our total overhead (less metadata to keep).

After finding the last block on our free list, we check if this blocks ends on the program break.
If that is true, and the block is big enough (where “big” is defined as MIN_DEALLOC, also a
multiple of the page size), we remove the block from our list and move the program break to the
beginning of the block, by calling brk.

NOTE: when asking for a large size, malloc actually used mmap instead of brk to allocate the memory.

#include <unistd.h>
long sysconf(int name);
// sysconf - get configuration information at run time
// PAGESIZE - _SC_PAGESIZE
//            Size of a page in bytes