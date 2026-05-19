#include <cstdlib>
#include <cstdint>

// --- KERNEL SIMULATION SANDBOX ---
#define MAX_HEAP (20 * (1 << 20)) // 20 MB
static char *mem_heap;
static char *mem_brk;
static char *mem_max_addr;

void mem_init_sandbox() {
    mem_heap = (char *)std::malloc(MAX_HEAP);
    mem_brk = (char *)mem_heap;
    mem_max_addr = (char *)(mem_heap + MAX_HEAP);
}

void *mem_sbrk(int incr) {
    char *old_brk = mem_brk;
    if ((incr < 0) || ((mem_brk + incr) > mem_max_addr)) {
        return (void *)-1;
    }
    mem_brk += incr;
    return (void *)old_brk;
}

// --- MACROS & ALIGNMENT ---
#define WSIZE       4
#define DSIZE       8
#define CHUNKSIZE  (1<<12) 

#define MAX(x, y) ((x) > (y)? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc))
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define HDRP(bp)  ((char *)(bp) - WSIZE)
#define FTRP(bp)  ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))
#define NEXT_FREE_BLKP(bp) (*(char **)(bp))
#define PREV_FREE_BLKP(bp) (*(char **)(bp+DSIZE))
static char *heap_listp = 0;
static char *free_list_head = NULL;

// --- ENGINE MECHANICS ---
static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
        return bp;
    }
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = (char *)mem_sbrk(size)) == -1) return NULL;

    PUT(HDRP(bp), PACK(size, 0));         // Free block header
    PUT(FTRP(bp), PACK(size, 0));         // Free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // New epilogue header

    return coalesce(bp);
}

static void *find_fit(size_t asize) {
    void *bp;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    return NULL;
}

static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    } else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

// --- PUBLIC API ---
int mm_init(void) {
    if ((heap_listp = (char *)mem_sbrk(4 * WSIZE)) == (void *)-1) return -1;
    
    PUT(heap_listp, 0);                            // Alignment padding
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); // Prologue header
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); // Prologue footer
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     // Epilogue header
    heap_listp += (2 * WSIZE);

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) return -1;
    return 0;
}

void *mm_malloc(size_t size) {
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0) return NULL;

    if (size <= DSIZE) {
        asize = 2 * DSIZE;
    } 
    else {
        asize = (size + DSIZE + (DSIZE - 1)) & ~0x7;
    }

    if ((bp = (char *)find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = (char *)extend_heap(extendsize / WSIZE)) == NULL) return NULL;
    place(bp, asize);
    return bp;
}

void mm_free(void *bp) {
    if (bp == NULL) return;
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

//Explicit Free List Functions

void remove_block(void *bp){
    char *prev_blk = PREV_FREE_BLKP(bp);
    char *next_blk = NEXT_FREE_BLKP(bp);
    if(prev_blk != NULL && next_blk != NULL){
        NEXT_FREE_BLKP(prev_blk) = next_blk;
        PREV_FREE_BLKP(next_blk) = prev_blk;
    }
    else if(prev_blk != NULL && next_blk == NULL){
        NEXT_FREE_BLKP(prev_blk) = NULL;
    }
    else if(prev_blk == NULL && next_blk != NULL){
        free_list_head = next_blk;
        PREV_FREE_BLKP(next_blk) = NULL;
    }
    else{
        free_list_head = NULL;
    }
}

void insert_block(void *bp){
    if(free_list_head == NULL){
        NEXT_FREE_BLKP(bp) = NULL;
        PREV_FREE_BLKP(bp) = NULL;
        free_list_head = (char*)(bp);
    }
    else{
        NEXT_FREE_BLKP(bp) = free_list_head;
        PREV_FREE_BLKP(bp) = NULL;
        PREV_FREE_BLKP(free_list_head) = (char*)(bp);
        free_list_head = (char*)(bp);
    }
}