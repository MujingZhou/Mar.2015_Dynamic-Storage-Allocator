/*
 * Simple, 32-bit and 64-bit clean allocator based on implicit free
 * lists, first fit placement, and boundary tag coalescing, as described
 * in the CS:APP2e text. Blocks must be aligned to doubleword (8 byte)
 * boundaries. Minimum block size is 16 bytes.
 *
 * Name : Mujing Zhou
 * Andrew ID: mujingz
 * Version 3.0
 * 
 * Overview:
 * 1. Segregated free list of size LIST_NUM = 24.
 * 2. First-fit strategy for searching the free blocks.
 * 3. LIFO strategy for insertion into free list
 * 4. Store allocated status in the second bit of header in next block
 *    to save the size of footer for allocated block.
 * 5. Use offset in free list for finding next/prev free block
 *
 *
 * Structure of heap:
 * Alignment padding [4 bytes]
 * Entry of free list[4 bytes * LIST_NUM]
 * Prologue          [4 bytes + 4 bytes]
 * Heap for allocation/free
 * Epilogue          [4 bytes]
 *
 * "heap_listp" always points to prologue block(alignemnt to 8)
 * "free_listp" always points to the first free list (LIST1)
 *
 *
 * Structure of blocks:
 * 1. Free blocks(16 bytes)
 *
 *    header   [4 bytes]
 *    next     [4 bytes] (offset)
 *    prev     [4 bytes] (offset)
 *    footer   [4 bytes] (offset)
 *
 * 2. Allocated blocks
 *    header   [4 bytes]
 *    payload
 *
 *
 * Structure of header:
 * Bit 0: 0 for free state of current block
 *        1 for allocated state of current block
 *
 * Bit 1: 0 for free state of prev block
 *        1 for allocated state of prev block
 *
 * Bit 2: 0
 *
 * Bit 3-31: size of current block
 *
 *
 *
 *
 *
 *
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mm.h"
#include "memlib.h"

/*
 * If NEXT_FIT defined use next fit search, else use first fit search
 */
#define NEXT_FITx

/* $begin mallocmacros */
/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */
#define DSIZE       8       /* Doubleword size (bytes) */
#define CHUNKSIZE  (1<<8)-(1<<5)  /* Extend heap by this amount (bytes) */

/* Define different size of different free list */
#define LIST1       (1<<4)
#define LIST2        24
#define LIST3        48
#define LIST4       (1<<7)
#define LIST5       (1<<8)
#define LIST6       (1<<9)
#define LIST7       (1<<10)
#define LIST8       (1<<11)
#define LIST9       (1<<12)
#define LIST10       9200
#define LIST11       12000
#define LIST12       16000
#define LIST13       20000
#define LIST14       24000
#define LIST15       28000
#define LIST16       32000
#define LIST17       40000
#define LIST18      (1<<15)
#define LIST19      (1<<16)
#define LIST20      (1<<17)
#define LIST21      (1<<18)
#define LIST22      (1<<19)
#define LIST23      (1<<20)

#define LIST_NUM 24        /* Number of free list */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)          (*(unsigned int *)(p))
#define PUT_HD(p, val)  (GET(p) = (GET(p) & 0x2) | (val))  /* Keep former second bit for a/f of prev block */
#define PUT(p, val)    (GET(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)                    

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) 

/* Convert between address and offset relative to first block */
#define PTR_OFF(p,ptr) PUT(p,(long)(ptr)-(long)(heap_listp))
#define OFF_PTR(p)     (GET(p)+heap_listp)

/* Set the second bit of current for status(a/f) of prev block */
#define PREV_ALLOC(bp)    (GET(HDRP(NEXT_BLKP(bp))) |= (0x2))
#define PREV_UNALLOC(bp)  (GET(HDRP(NEXT_BLKP(bp))) &= (~0x2))

/* Given block ptr bp, computer status(a/f) of prev block */
#define GET_PREV_ALLOC(bp)  (GET(HDRP(bp)) & (0x2))

/* $end mallocmacros */

/* Global variables */
static char *heap_listp = 0;   /* Pointer to first block */
static char *free_listp = 0;   /* Pointer to beginning of free list */


#ifdef NEXT_FIT
static char *rover;           /* Next fit rover */
#endif

/* Function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void printblock(void *bp);
static void checkblock(void *bp);
static void *find_block(void *list,size_t asize);
/*
 * mm_init - Initialize the memory manager
 */
/* $begin mminit */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE+(LIST_NUM)*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                                     /* Alignment padding */
    PUT(heap_listp + ((LIST_NUM+1)*WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + ((LIST_NUM+2)*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + ((LIST_NUM+3)*WSIZE), PACK(0, 1));     /* Epilogue header */
    free_listp = heap_listp + (1*WSIZE);                    /* Point to beginning of free list */
    heap_listp += ((LIST_NUM+2)*WSIZE);                     /* Heap_listp point to prologue block */
    PREV_ALLOC(heap_listp);
    
    for(int i = 0; i < (LIST_NUM); i++){                    /* Initialize free_list */
        PUT(free_listp+i*WSIZE,0);
    }
    
    /* $end mminit */
    
#ifdef NEXT_FIT
    rover = heap_listp;
#endif
    /* $begin mminit */
    
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    
    return 0;
}
/* $end mminit */

/*
 * mm_malloc - Allocate a block with at least size bytes of payload
 */
/* $begin mmmalloc */
void *mm_malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;
    
    /* $end mmmalloc */
    if (heap_listp == 0){
        mm_init();
    }
    /* $begin mmmalloc */
    /* Ignore spurious requests */
    if (size == 0)
        return NULL;
    
    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (WSIZE) + (DSIZE-1)) / DSIZE);
    
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }
    
    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    
    return bp;
}
/* $end mmmalloc */

/*
 * mm_free - Free a block
 * Set s
 *
 */
/* $begin mm_free */
void mm_free(void *bp)
{
    
    if(bp == 0)
        return;
    
    size_t size = GET_SIZE(HDRP(bp));

    if (heap_listp == 0){                   /* Not yet initialized */
        mm_init();
    }
    
    
    PUT_HD(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PREV_UNALLOC(bp);
    coalesce(bp);
}
/* $end mmfree */


/*
 * list_entry - serach for list that fits
 */
/* $begin list_entry */
static inline int list_entry(size_t size){
    if (size<=LIST1){
        return 0;
    }
    else if (size<=LIST2){
        return 1;
    }
    else if (size<=LIST3){
        return 2;
    }
    else if (size<=LIST4){
        return 3;
    }
    else if (size<=LIST5){
        return 4;
    }
    else if (size<=LIST6){
        return 5;
    }
    else if (size<=LIST7){
        return 6;
    }
    else if (size<=LIST8){
        return 7;
    }
    else if (size<=LIST9){
        return 8;
    }
    else if (size<=LIST10){
        return 9;
    }
    else if (size<=LIST11){
        return 10;
    }
    else if (size<=LIST12){
        return 11;
    }
    else if (size<=LIST13){
        return 12;
    }
    else if (size<=LIST14){
        return 13;
    }
    else if (size<=LIST15){
        return 14;
    }
    else if (size<=LIST16){
        return 15;
    }
    else if (size<=LIST17){
        return 16;
    }
    else if (size<=LIST18){
        return 17;
    }
    else if (size<=LIST19){
        return 18;
    }
    else return LIST_NUM-1;
}
/* $end list_entry */


/* freelist_insert - insert freed or splitted block
 * to the head of freelist
 */
/* $begin freelist_insert */
static inline void freelist_insert(void *bp){
    size_t size=GET_SIZE(HDRP(bp));
    unsigned int * list=(unsigned int *)free_listp+list_entry(size);
    
    if (*(unsigned int *)list == 0) {          /* Freelist is empty */
        PTR_OFF(list, bp);
        PUT(bp, 0);
        PUT((unsigned int *)bp + 1, 0);
    }
    
    else {                                      /* Freelist not empty */
        PUT(bp, *(unsigned int*)list);
        PUT((unsigned int *)bp + 1, 0);
        PTR_OFF((unsigned int *)OFF_PTR(list)+ 1, bp);
        PTR_OFF(list, bp);
    }
}
/* $end freelist_insert */


/* freelist_delete - delete freed or splitted block */
/* $begin freelist_delete */
static inline void freelist_delete(void *bp){
    size_t size=GET_SIZE(HDRP(bp));
    unsigned int * list=(unsigned int *)free_listp+list_entry(size);
    
    if (GET(bp)==0&&GET(bp+WSIZE)==0){         /* Freelist is empty */
        PUT(list, 0);
    }
    
    if (GET(bp)==0&&GET(bp+WSIZE)!=0){         /* Last free block of freelist */
        PUT(OFF_PTR(bp+WSIZE),0);
    }
    
    if (GET(bp)!=0&&GET(bp+WSIZE)==0){         /* First free block of freelist */
        PUT((unsigned int *)OFF_PTR(bp)+1,0);
        PUT(list, GET(bp));
    }
    
    if (GET(bp)!=0&&GET(bp+WSIZE)!=0){         /* In the middle of freelist */
        PUT((unsigned int *)OFF_PTR(bp)+1,GET(bp+WSIZE));
        PUT(OFF_PTR(bp+WSIZE),GET(bp));
    }
}
/* $end freelist_delete */


/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 */
/* $begin mmfree */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_PREV_ALLOC(bp);
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    
    if (prev_alloc && next_alloc) {            /* Case 1 */
        freelist_insert(bp);
    }
    
    else if (prev_alloc && !next_alloc) {      /* Case 2 */
        
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT_HD(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));
        freelist_insert(bp);
    }
    
    else if (!prev_alloc && next_alloc) {      /* Case 3 */
        freelist_delete(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT_HD(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        freelist_insert(bp);
    }
    
    else {                                     /* Case 4 */
        freelist_delete(PREV_BLKP(bp));
        freelist_delete(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
        GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT_HD(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        freelist_insert(bp);
    }
    /* $end mmfree */
#ifdef NEXT_FIT
    /* Make sure the rover isn't pointing into the free block */
    /* that we just coalesced */
    
    if ((rover > (char *)bp) && (rover < NEXT_BLKP(bp)))
        rover = bp;
#endif
    /* $begin mmfree */
    return bp;
}
/* $end mmfree */

/*
 * mm_realloc - Naive implementation of realloc
 */
void *mm_realloc(void *ptr, size_t size)
{
    {
        size_t oldsize;
        void *newptr;
        
        /* If size == 0 then this is just free, and we return NULL. */
        if(size == 0) {
            mm_free(ptr);
            return 0;
        }
        
        /* If oldptr is NULL, then this is just malloc. */
        if(ptr == NULL) {
            return mm_malloc(size);
        }
        
        newptr = mm_malloc(size);
        
        /* If realloc() fails the original block is left untouched  */
        if(!newptr) {
            return 0;
        }
        
        /* Copy the old data. */
        oldsize = GET_SIZE(HDRP(ptr));
        if(size < oldsize) oldsize = size;
        memcpy(newptr, ptr, oldsize);
        
        /* Free the old block. */
        mm_free(ptr);
        
        return newptr;
    }
}

/*
 * The remaining routines are internal helper routines
 */

/*
 * extend_heap - Extend heap with free block and return its block pointer
 */
/* $begin mmextendheap */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    
    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE; //line:vm:mm:beginextend
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;                                        //line:vm:mm:endextend
    
    /* Initialize free block header/footer and the epilogue header */
    PUT_HD(HDRP(bp), PACK(size, 0));         /* Free block header */   //line:vm:mm:freeblockhdr
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */   //line:vm:mm:freeblockftr
    PUT_HD(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */ //line:vm:mm:newepihdr
    
    /* Coalesce if the previous block was free */
    return coalesce(bp);                                          //line:vm:mm:returnblock
}
/* $end mmextendheap */

/*
 * place - Place block of asize bytes at start of free block bp
 *         and split if remainder would be at least minimum block size
 */
/* $begin mmplace */
/* $begin mmplace-proto */
static void place(void *bp, size_t asize)
/* $end mmplace-proto */
{
    size_t csize = GET_SIZE(HDRP(bp));
    freelist_delete(bp);
    if ((csize - asize) >= (2*DSIZE)) {
        PUT_HD(HDRP(bp), PACK(asize, 1));
//        PUT(FTRP(bp), PACK(asize, 1));
        PREV_ALLOC(bp);
        bp = NEXT_BLKP(bp);
        PUT_HD(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        PREV_UNALLOC(bp);
        freelist_insert(bp);
    }
    else {
        PUT_HD(HDRP(bp), PACK(csize, 1));
//        PUT(FTRP(bp), PACK(csize, 1));
        PREV_ALLOC(bp);
    }
}
/* $end mmplace */


/* 
 * find_block - search list for a block
 */
/* $begin find_block */
static void *find_block(void *list,size_t asize){
    char* bp=OFF_PTR(list);
    if (GET(list)==0){
        return NULL;
    }
    while(bp)
    {
        if(GET_SIZE(HDRP(bp))>=asize)
            return (void*)bp;
        if (GET(bp)==0){
            return NULL;
        }
        bp=OFF_PTR((unsigned int *)bp);
    }
    return NULL;
}
/* $end find_block */

/*
 * find_fit - Find a fit for a block in list with asize bytes
 */
/* $begin mmfirstfit */
/* $begin mmfirstfit-proto */
static void *find_fit(size_t asize)
/* $end mmfirstfit- proto */
{
    /* $end mmfirstfit */
    
#ifdef NEXT_FIT
    /* Next fit search */
    char *oldrover = rover;
    
    /* Search from the rover to the end of list */
    for ( ; GET_SIZE(HDRP(rover)) > 0; rover = NEXT_BLKP(rover))
        if (!GET_ALLOC(HDRP(rover)) && (asize <= GET_SIZE(HDRP(rover))))
            return rover;
    
    /* search from start of list to old rover */
    for (rover = heap_listp; rover < oldrover; rover = NEXT_BLKP(rover))
        if (!GET_ALLOC(HDRP(rover)) && (asize <= GET_SIZE(HDRP(rover))))
            return rover;
    
    return NULL;  /* no fit found */
#else
    /* $begin mmfirstfit */
    /* First fit search */
    
    int entry=list_entry(asize);
    char *bp;
    while(entry<LIST_NUM){
        if (GET(free_listp+WSIZE*entry)==0){        /* fail to find in current list, find next */
            entry++;
            continue;
        }
        else{
            bp=(char*)find_block(free_listp+entry*WSIZE,asize);
            entry++;
        }
        if(bp) {
            return bp;
        }
    }
    return NULL; /* No fit */
    /* $end mmfirstfit */
#endif
}

static void printblock(void *bp)
{
    size_t hsize, halloc, fsize, falloc;
    
    mm_checkheap(0);
    hsize = GET_SIZE(HDRP(bp));
    halloc = GET_ALLOC(HDRP(bp));
    fsize = GET_SIZE(FTRP(bp));
    falloc = GET_ALLOC(FTRP(bp));
    
    if (hsize == 0) {
        printf("%p: EOL\n", bp);
        return;
    }
    
    printf("%p: header: [%ld:%c] footer: [%ld:%c]\n", bp,
           (long)hsize, (char)(halloc ? 'a' : 'f'),
           (long)fsize, (char)(falloc ? 'a' : 'f'));
}

static void checkblock(void *bp)
{
    if ((size_t)bp % 8)
        printf("Error: %p is not doubleword aligned\n", bp);                    // Check block alignment
    if (GET(HDRP(bp)) != GET(FTRP(bp)))
        printf("Error: header does not match footer\n");                        // Check header/footer match
    if (GET_ALLOC(HDRP(bp))&&GET_ALLOC(HDRP(NEXT_BLKP(bp))))
        printf("Error: %p and %p is not coaleasced correctly\n",bp,NEXT_BLKP(bp));// Check no consecutive free blocks
    if (GET_SIZE(bp)<DSIZE*2)
        printf("Error: %p does not meet the minumum block size\n",bp);            // Check minimum size
}

/*
 * checkheap - Minimal check of the heap for consistency
 */
void mm_checkheap(int verbose)
{
    char *bp = heap_listp;
    
    if (verbose)
        printf("Heap (%p):\n", heap_listp);
    
    if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp))) // Check prologue blocks
        printf("Bad prologue header\n");
    if ((GET_SIZE(FTRP(heap_listp)) != DSIZE) || !GET_ALLOC(FTRP(heap_listp)))
        printf("Bad prologue footer\n");
    checkblock(heap_listp);                                                    // Check alignment of epilogue blocks
    
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (verbose)
            printblock(bp);
        checkblock(bp);                                                        // Check alignemnt of other blocks
    }
    
    if (verbose)
        printblock(bp);
    if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))                   // Check epilogue block
        printf("Bad epilogue header\n");
}

void *calloc (size_t nmemb, size_t size)
{
    size_t bytes = nmemb * size;
    void *newptr;
    
    newptr = malloc(bytes);
    memset(newptr, 0, bytes);
    
    return newptr;
}
