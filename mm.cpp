/*
 * mm.cpp - Bare-Metal Implicit Free List Memory Allocator
 * * Architecture Overview:
 * - List Type: Implicit Free List (Allocated and free blocks are interleaved).
 * - Search Algorithm: First-Fit (O(N) time complexity).
 * - Block Alignment: 8 bytes (Double Word).
 * - Minimum Block Size: 16 bytes (4-byte Header + 4-byte Footer + 8-byte Payload minimum).
 * * Metadata Structure (Boundary Tags):
 * Every block contains a 4-byte Header and a 4-byte Footer.
 * Because sizes are always multiples of 8, the lowest 3 bits of the size are always 0.
 * We hijack the 0th bit (LSB) to store the Allocation Status (1 = Allocated, 0 = Free).
 */

#include <cstddef>
#include <unistd.h> 

// MACROS (Silicon-Level Pointer Arithmetic)

#define WSIZE 4             // Word and header/footer size (bytes)
#define DSIZE 8             // Double word size (bytes)
#define CHUNKSIZE (1<<12)   // Default heap extension size (4096 bytes / 4KB)

// Pack a size and allocated bit into a single 32-bit word for the boundary tags
#define PACK(size, alloc) ((size) | (alloc))

// Read and write a 4-byte word at address p
#define GET(p) (*(unsigned int*)(p))
#define PUT(p,val) (*(unsigned int*)(p) = (val))

// Extract the size from a boundary tag. 
// ~0x7 is a bitmask (111...1000) that zeroes out the bottom 3 status bits.
#define GET_SIZE(p) (GET(p) & ~0x7)

// Extract the allocation status bit from a boundary tag.
// 0x1 is a bitmask (000...0001) that isolates the 0th bit.
#define GET_ALLOC(p) (GET(p) & 0x1)

// Compute the address of the Header and Footer given a payload pointer (bp)
#define HDRP(bp) ((char*)(bp) - WSIZE)
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// Compute the address of the Next and Previous payload pointers (bp)
// NEXT_BLKP: Read current header size, jump forward by that size.
// PREV_BLKP: Read previous footer size (located DSIZE behind current bp), jump backward.
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE((char*)(bp) - DSIZE))


// --- GLOBAL VARIABLES ---
// Anchor pointer that permanently points to the start of the heap (Prologue block payload)
static char *heap_listp = 0;


// --- FORWARD DECLARATIONS ---
static void *coalesce(void *bp);
static void *extend_heap(size_t words);


// --- INTERNAL ALLOCATOR ENGINE ---

/*
 * find_fit - Performs a First-Fit linear search across the implicit list.
 * Time Complexity: O(N) where N is total blocks (allocated + free).
 * Terminates safely when it reads the Epilogue block (Size: 0).
 */
static void *find_fit(size_t asize) {
    void *bp;
    for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        // Condition: Block must be free AND large enough for the requested aligned size
        if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    return NULL; // Heap full, requires sbrk
}

/*
 * place - Carves out the requested memory (asize) from a larger free block (csize).
 * Splitting Rule: Only split if the leftover space is >= 16 bytes (minimum block size).
 * Otherwise, suffer internal fragmentation and give the user the entire block.
 */
static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));

    if((csize - asize) >= (2 * DSIZE)) {
        // SPLIT: Allocate the requested size
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        
        // Jump the pointer to the leftover space and create a new free block
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
    } else {
        // NO SPLIT: Mark the entire block as allocated
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/*
 * coalesce - Merges adjacent free blocks using boundary tag coalescing.
 * Prevents the heap from shattering into unusable microscopic fragments.
 * Relies on the physical ordering of PUT operations to safely overwrite old boundaries.
 */
static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if(prev_alloc && next_alloc) {
        // Case 1: Both adjacent blocks allocated. Nothing to merge.
        return bp;
    } else if(prev_alloc && !next_alloc) {
        // Case 2: Next block is free.
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0)); // Uses updated header to overwrite Next's footer
    } else if(!prev_alloc && next_alloc) {
        // Case 3: Previous block is free. Pointer must shift left.
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } else {
        // Case 4: Both adjacent blocks are free. Massive merge.
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))) + GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

/*
 * extend_heap - Requests raw physical RAM from the OS via sbrk().
 * Enforces 8-byte alignment, formats the raw bytes into a free block,
 * and rebuilds the Epilogue boundary at the new edge of the heap.
 */
static void *extend_heap(size_t words) {
    void *bp;
    size_t size;
    
    // Ensure the requested memory maintains 8-byte alignment
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    
    bp = sbrk(size);
    if (bp == (void *)-1) return NULL; // OS is out of memory

    // Format the newly acquired raw silicon
    PUT(HDRP(bp), PACK(size, 0));         // Overwrites old Epilogue header
    PUT(FTRP(bp), PACK(size, 0));         // New free block footer
    
    // Re-establish the Epilogue block (Size: 0, Alloc: 1) at the new physical boundary
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); 

    return coalesce(bp); // Merge with previous free block if possible
}


// --- API WRAPPER ---

/*
 * mm_init - The ignition sequence. Runs once at startup.
 * Allocates 16 initial bytes to establish alignment padding, the Prologue block,
 * and the Epilogue block, preventing out-of-bounds traversal.
 */
int mm_init(void) {
    if ((heap_listp = (char *)sbrk(4 * WSIZE)) == (void *)-1) {
        return -1;
    }

    // Word 0: Alignment padding (Forces 8-byte alignment for all future payloads)
    PUT(heap_listp, 0);                            
    // Word 1 & 2: Prologue Block (Size: 8, Alloc: 1)
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); 
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); 
    // Word 3: Epilogue Block (Size: 0, Alloc: 1). Acts as the loop termination barrier.
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     
    
    // Anchor the global pointer strictly to the Prologue payload
    heap_listp += (2 * WSIZE);

    // Expand the empty heap with a usable block of memory
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
        return -1;
    }
    return 0;
}