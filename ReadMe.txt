This is the ReadMe file of Mujing's Dynamic Storage Allocator written in C. This is a course project for 15-213 (Intro to Computer Systems)


In this project, I implemented my own malloc, free, realloc, and calloc functions. 


Detailed introductions:
/*
 * Dynamic Storage allocator based on implicit free
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