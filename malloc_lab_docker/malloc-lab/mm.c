/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"


#define WSIZE 4 // words size
#define DSIZE 8 // Double Words size
#define CHUNKSIZE (1<<12)

#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN_BLOCK_SIZE 24

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p)) // 4바이트 값 읽기
#define PUT(p, val) (*(unsigned int *) (p) = (val)) // 4바이트 값 쓰기

#define GET_SIZE(p) (GET(p) & ~0x7) // Header-Footer에서 size만 추출
#define GET_ALLOC(p) (GET(p) & 0x1) // alloc 비트만 추출

#define HDRP(bp) ((char *)(bp) - WSIZE) //payload->Header 주소
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) //payload -> Footer 주소
#define PREV_FREE(bp) (*(void **)(bp))
#define NEXT_FREE(bp) (*(void **)((char *)(bp) + DSIZE))

#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(((char*)(bp) - WSIZE))) //현재 payload → 다음 payload
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE))) //현재 payload → 이전 payload

static char *heap_listp = 0;
static void *free_listp = 0;

//static void *rover = 0;

static void insert_free(void *bp);
static void remove_free(void *bp);
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);



/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    free_listp = NULL; 
    
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp,0);
    PUT(heap_listp + (1*WSIZE) , PACK(DSIZE, 1));
    PUT(heap_listp + (2*WSIZE) , PACK(DSIZE, 1));
    PUT(heap_listp + (3*WSIZE) , PACK(0, 1));
    heap_listp += (2*WSIZE);

    if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;

    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
   asize = ALIGN(size + DSIZE);

    if (asize < MIN_BLOCK_SIZE)
        asize = MIN_BLOCK_SIZE;
    
        /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;

    place(bp, asize);
    return bp;
}

// first fit

// static void *find_fit(size_t asize){
//     void *bp;
//     for (bp = heap_listp; GET_SIZE(HDRP(bp))>0; bp = NEXT_BLKP(bp)){
//         if (!GET_ALLOC(HDRP(bp)) && asize <= GET_SIZE(HDRP(bp))){
//             return bp;
//         }
//     }
//     return NULL;
// }

static void *find_fit(size_t asize)
{
    void *bp;

    for (bp = free_listp; bp != NULL; bp = NEXT_FREE(bp)) {

        if (asize <= GET_SIZE(HDRP(bp))) {
            return bp;
        }
    }

    return NULL;
}


// best fit
// static void *find_fit(size_t asize)
// {
//     void *bp;
//     void *best_bp = NULL;
//     size_t min_diff = (size_t)-1;

//     for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {

//         if (!GET_ALLOC(HDRP(bp)) &&
//             asize <= GET_SIZE(HDRP(bp))) {

//             size_t diff = GET_SIZE(HDRP(bp)) - asize;

//             if (diff < min_diff) {
//                 min_diff = diff;
//                 best_bp = bp;
//             }

//             // 딱 맞는 블록이면 더 좋은 후보가 존재할 수 없음
//             if (diff == 0)
//                 break;
//         }
//     }

//     return best_bp;
// }

static void place(void *bp, size_t asize){
    size_t csize = GET_SIZE(HDRP(bp));

    remove_free(bp);
    
    if ((csize - asize) >= MIN_BLOCK_SIZE){
        PUT(HDRP(bp),PACK(asize,1));
        PUT(FTRP(bp),PACK(asize,1));

        void *next_bp = NEXT_BLKP(bp);

        PUT(HDRP(next_bp), PACK(csize - asize, 0));
        PUT(FTRP(next_bp), PACK(csize - asize, 0));
        
        insert_free(next_bp);
    }
    else{
        PUT(HDRP(bp),PACK(csize,1));
        PUT(FTRP(bp),PACK(csize,1));
    }
    
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}
/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
        return mm_malloc(size);

    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    size_t old_size = GET_SIZE(HDRP(ptr));
    size_t old_payload = old_size - DSIZE;

    size_t new_asize = ALIGN(size + DSIZE);
    if (new_asize < MIN_BLOCK_SIZE)
        new_asize = MIN_BLOCK_SIZE;

    /* 1. 현재 블록이 이미 충분하면 그대로 사용 */
    if (new_asize <= old_size)
        return ptr;

    /* 2. 다음 블록이 free이고 합치면 충분하면 확장 */
    void *next_bp = NEXT_BLKP(ptr);

    if (!GET_ALLOC(HDRP(next_bp))) {

        size_t next_size = GET_SIZE(HDRP(next_bp));

        if (old_size + next_size >= new_asize) {

            remove_free(next_bp);

            size_t total = old_size + next_size;

            PUT(HDRP(ptr), PACK(total, 1));
            PUT(FTRP(ptr), PACK(total, 1));

            return ptr;
        }
    }

    /* 3. 안 되면 새로 할당 */
    void *newptr = mm_malloc(size);

    if (newptr == NULL)
        return NULL;

    size_t copySize = old_payload;

    if (size < copySize)
        copySize = size;

    memcpy(newptr, ptr, copySize);

    mm_free(ptr);

    return newptr;
}

static void *extend_heap(size_t words){
    char *bp;
    size_t size;

    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));

    return coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
        /* 병합 없음 */
    }

    else if (prev_alloc && !next_alloc) {
        void *next_bp = NEXT_BLKP(bp);

        remove_free(next_bp);

        size += GET_SIZE(HDRP(next_bp));

        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc) {
        void *prev_bp = PREV_BLKP(bp);

        remove_free(prev_bp);

        size += GET_SIZE(HDRP(prev_bp));

        PUT(HDRP(prev_bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));

        bp = prev_bp;
    }

    else {
        void *prev_bp = PREV_BLKP(bp);
        void *next_bp = NEXT_BLKP(bp);

        remove_free(prev_bp);
        remove_free(next_bp);

        size += GET_SIZE(HDRP(prev_bp))
              + GET_SIZE(HDRP(next_bp));

        PUT(HDRP(prev_bp), PACK(size, 0));
        PUT(FTRP(next_bp), PACK(size, 0));

        bp = prev_bp;
    }

    insert_free(bp);

    return bp;
}
static void insert_free(void *bp)
{
    PREV_FREE(bp) = NULL;
    NEXT_FREE(bp) = free_listp;

    if (free_listp != NULL)
        PREV_FREE(free_listp) = bp;

    free_listp = bp;
}

static void remove_free(void *bp)
{
    void *prev = PREV_FREE(bp);
    void *next = NEXT_FREE(bp);

    if (prev != NULL)
        NEXT_FREE(prev) = next;
    else
        free_listp = next;

    if (next != NULL)
        PREV_FREE(next) = prev;
}