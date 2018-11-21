#include "mm.h"
#include <stdio.h>

#define ALIGNMENT 16
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

#define OVERHEAD (sizeof(block_header))
#define HDRP(bp) ((char *)(bp) - sizeof(block_header))
#define HDRP2(bp) ((char *)(bp) - sizeof(size_t))

#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_SIZE(p) (GET(p) & ~0xF)

#define GET_CURR(p) ((block_header *)(p))->curr_size_alloc
#define GET_PREV(p) ((block_header *)(p))->prev_size_alloc

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))

#define IS_ALIGNED(p) (((unsigned long)p & 15) == 0)


// ADDED
#define CHUNK_SIZE (1 << 14)
#define CHUNK_ALIGN(size) (((size)+(CHUNK_SIZE-1)) & ~(CHUNK_SIZE-1))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE( GET_PREV(HDRP(bp)) ))
#define FTRP(bp) ((char *)(bp)+GET_SIZE(HDRP(bp))-OVERHEAD)

#define GET(p) (*(size_t *)(p))
#define GET2(p) (*(size_t *)((void*)p + sizeof(size_t)))
#define PUT(p, val) (*(size_t *)(p) = (val))
#define PACK(size, alloc) ((size) | (alloc))

void *coalesce(void *bp);
int max(int a, int b);
void p_debug(char* str);
void set_allocated1(void *bp, size_t size);
void pack(void* bp, size_t size, size_t alloc);

void set_curr(void* bp, size_t size, size_t alloc);
void set_prev(void* bp, size_t size, size_t alloc);
size_t get_size_curr(void* bp);
size_t get_size_prev(void* bp);
size_t get_alloc_curr(void* bp);
size_t get_alloc_prev(void* bp);

void* prev_blkp(void* bp);
void* next_blkp(void* bp);

int dbg = 0;
int case_print = 0;
int coal_info = 0;

typedef struct {
  size_t curr_size_alloc;
  size_t prev_size_alloc;
} block_header;

typedef struct list_node {
 struct list_node *prev;
 struct list_node *next;
} list_node;

void* first_bp;
list_node* free_list_head;
list_node* free_list_tail;
void* heap_gbl;
void* end_heap_gbl;

void mm_init(void *heap, size_t heap_size)
{
  printf("mm_init 1\n");
  heap_gbl = heap;
  printf("mm_init 2\n");
  void *bp;
  bp = (void*)heap + sizeof(block_header);
  printf("mm_init 3\n");  
  set_curr((void*)heap + heap_size, 0, 1);                // SET SIZE OF TERMINATOR TO ZERO
  printf("mm_init 4\n");  
  set_prev((void*)heap + heap_size, heap_size - (sizeof(block_header)), 1);
  printf("mm_init 5\n");  
                                    /* for terminator */                     
  set_curr(bp, heap_size - (sizeof(block_header)), 0);    // set size of first unallocated block
  printf("mm_init 6\n");  
  set_prev(bp, 0, 1);                                // set prev block size info as 0 to indicate beginning of heap boundary
  printf("mm_init 7\n");  

  /*
  free_list_head->prev = NULL;
  free_list_head->next = (list_node*)bp;
  free_list_head->next = NULL;  
  free_list_tail->prev = (list_node*)bp;
  */

  first_bp = bp;
}

void set_allocated1(void *bp, size_t size) {

  size_t extra_size = get_size_curr(bp) - size;

  if (extra_size > ALIGN(1 + OVERHEAD)) {                 // SPLIT BLOCK
    set_curr(bp, size, 1);                              // size first block
    set_prev(next_blkp(bp), size, 1);                   // give second block size information about first block for prev_blkp

    set_curr(next_blkp(bp), extra_size, 0);             // size second block
  }
  set_curr(bp, get_size_curr(bp), 1);                    // set first block as allocated
}

static void set_allocated(void *bp)
{}

void *mm_malloc(size_t size)
{
  int need_size = max(size, sizeof(list_node));
  int new_size = ALIGN(need_size + OVERHEAD);

  void *bp = first_bp;
  
  while (get_size_curr(bp) != 0) {
    if(!get_alloc_curr(bp) && (get_size_curr(bp) >= new_size )) {
      set_allocated1(bp, new_size);
      set_allocated(bp);
      return bp;
    }

    bp = next_blkp(bp);
  }

  if(dbg) printf("FAIL!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
  return NULL;
}

void mm_free(void *bp)
{

  printf("mm_free \n");
  //GET_ALLOC(HDRP(bp)) = 0;  
  set_curr(bp, get_size_curr(bp), 0);
}

void *coalesce(void *bp) {
  printf("coal 1\n");
  size_t prev_alloc = get_alloc_prev(bp);
  size_t next_alloc = get_alloc_curr(next_blkp(bp));             // Get next block alloc info
  size_t size = get_size_curr(bp);

    if (prev_alloc && next_alloc) {                         /* Case 1 */ 
    }
    else if (prev_alloc && !next_alloc) {                   /* Case 2 */
      size += get_size_curr(next_blkp(bp));                 // update new size of coalesced block  
      set_curr(bp, size, 0);                              // set size of coalesced block  
      set_prev(next_blkp(bp), size, 0);                   // give next block size information about coalesced block size
    }
    else if (!prev_alloc && next_alloc) {                   /* Case 3 */
      size += get_size_prev(bp);
      set_prev(next_blkp(bp), size, 0);                   // give next block size information about coalesced block size
      set_curr(prev_blkp(bp), size, 0);                   // give next block size information about coalesced block size
      bp = prev_blkp(bp);
    }
    else {                                                  /* Case 4 */
      size += get_size_prev(bp) + get_size_curr(next_blkp(bp));  
      set_curr(prev_blkp(bp), size, 0);                   // set beginning of coalesced block size
      set_prev(next_blkp(bp), size, 0);                   // give info to next block about coalesced block size
      bp = prev_blkp(bp);
    }

  return bp;
}

void p_debug(char* str)
{
  //if(dbg) printf("init: %i\tset_alloc: %i\tmm_alloc: %i\tfree: %i\tcoal: %i\n", init_count,set_allocated_count, mm_malloc_count, free_count, coal_count);
  //if(dbg || 1) printf(str);
  //if(dbg || 1) printf("\n");
}

int max(int a, int b)
{
  if(a >= b) return a;
  return b;
}

void set_curr(void* bp, size_t size, size_t alloc)
{ PUT(HDRP(bp), PACK(size, alloc));    }

void set_prev(void* bp, size_t size, size_t alloc)
{ PUT(HDRP2(bp), PACK(size, alloc));   }

size_t get_size_curr(void* bp)
{ return GET_SIZE(HDRP(bp));  }

size_t get_size_prev(void* bp)
{ return GET_SIZE(HDRP2(bp));  }

size_t get_alloc_curr(void* bp)
{ return GET_ALLOC(HDRP(bp)); }

size_t get_alloc_prev(void* bp)
{ return GET_ALLOC(HDRP2(bp)); }

void* prev_blkp(void* bp)
{ return PREV_BLKP(bp);       }

void* next_blkp(void* bp)
{ return NEXT_BLKP(bp);       }





