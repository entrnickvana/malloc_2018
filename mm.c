#include "mm.h"
#include <stdio.h>

#define ALIGNMENT 16
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

#define OVERHEAD (sizeof(block_header))
#define HDRP(bp) ((char *)(bp) - sizeof(block_header))

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
#define PUT(p, val) (*(size_t *)(p) = (val))
#define PACK(size, alloc) ((size) | (alloc))

void *coalesce(void *bp);
int max(int a, int b);
void p_debug(char* str);
void set_allocated1(void *bp, size_t size);
void p_addr(void* begin, void* end, void* current);
void info(void* bp, int loop);
void pack(void* bp, size_t size, size_t alloc);
size_t get_size(void* bp);
void set_size(void* bp, size_t sz);
size_t get_alloc(void* bp);
void set_alloc(void* bp, size_t alloc);
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
  heap_gbl = heap;

  void *bp;
  bp = (void*)heap + sizeof(block_header);

  set_size_curr((void*)heap + heapsize, 0);               // SET SIZE OF TERMINATOR TO ZERO
  set_alloc_curr((void*)heap + heapsize, 1);              // SET TERMINATOR AS ALLOCATED
  set_size_prev((void*)heap + heapsize, 0);
  set_alloc_curr((void*)heap + heapsize, heap_size - (sizeof(block_header))); // SET TERMINATOR AS ALLOCATED  

                                    /* for terminator */                     
  set_size_curr(bp, heap_size - (sizeof(block_header)));  // set size of first unallocated block
  set_alloc_curr(bp, 0);                                  // set alloc of first unallocated block
  set_size_prev(bp, 0);                                   // set prev block size info as 0 to indicate beginning of heap boundary
  set_alloc_prev(bp, 1);                                  // set prev block as allocated as a gate keeper

  free_list_head->prev = NULL;
  free_list_head->next = (list_node*)bp;
  free_list_head->next = NULL;  
  free_list_tail->prev = (list_node*)bp;

  first_bp = bp;
  free_list = bp;
}

void set_allocated1(void *bp, size_t size) {

  size_t extra_size = get_size(bp) - size;

  if (extra_size > ALIGN(1 + OVERHEAD)) {                 // SPLIT BLOCK
    set_size_curr(bp, size);                              // size first block
    set_size_prev(next_blkp(bp), size);                   // give second block size information about first block for prev_blkp
    set_alloc_prev(next_blkp(bp), 1);                     // give second block alloc information about first block for prev_blkp
    

    set_size_curr(next_blkp(bp), extra_size);             // size second block
    set_alloc_curr(next_blkp(bp), 0);                     // set second block as unallocated

    list_node* temp_node;
    temp_node = free_list_tail->prev;
    free_list_tail->prev = (list_node*)next_blkp(bp);
    temp_node->next = free_list_tail->prev;

  }
  set_alloc_curr(bp, 1);                                  // set first block as allocated
}

static void set_allocated(void *bp)
{}

void *mm_malloc(size_t size)
{
  int need_size = max(size, sizeof(list_node));
  int new_size = ALIGN(need_size + OVERHEAD);

  void *bp = first_bp;
  
  while (get_size(bp) != 0) {
    if(!get_alloc(bp) && ( get_size(bp) >= new_size )) {
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
  GET_ALLOC(HDRP(bp)) = 0;  
}

void *coalesce(void *bp) {
  size_t prev_alloc = get_alloc(prev_blkp(bp));             // Get prev block alloc info
  size_t next_alloc = get_alloc(next_blkp(bp));             // Get next block alloc info
  size_t size = get_size(bp);

    if (prev_alloc && next_alloc) {                         /* Case 1 */ 
    }
    else if (prev_alloc && !next_alloc) {                   /* Case 2 */
      size += get_size(next_blkp(bp));                      // update new size of coalesced block  
      set_size(bp, size);                                   // set size of coalesced block  
      set_size_prev(next_blkp(bp), size);                   // give next block size information about coalesced block size
    }
    else if (!prev_alloc && next_alloc) {                   /* Case 3 */
      size += get_size(prev_blkp(bp));                      // update new size of coalesced block  
      set_size_prev(next_blkp(bp), size);                   // give next block size information about coalesced block size
      set_size_curr(prev_blkp(bp), size);                   // give next block size information about coalesced block size
      bp = prev_blkp(bp);
    }
    else {                                                  /* Case 4 */
      size += get_size(prev_blkp(bp)) + get_size(next_blkp(bp));  
      set_size_curr(prev_blkp(bp), size);                   // set beginning of coalesced block size
      set_size_prev(next_blkp(bp), size);                   // give info to next block about coalesced block size
      bp = prev_blkp(bp);
    }

  return bp;
}

void p_debug(char* str)
{
  if(dbg) printf("init: %i\tset_alloc: %i\tmm_alloc: %i\tfree: %i\tcoal: %i\n", init_count,set_allocated_count, mm_malloc_count, free_count, coal_count);
  if(dbg || 1) printf(str);
  if(dbg || 1) printf("\n");
}

int max(int a, int b)
{
  if(a >= b) return a;
  return b;
}

void p_addr(void* begin, void* end, void* current)
{
  if(dbg) printf("BEGIN:\t\t%p\t\tEND:\t\t%p\t\tCURRENT\t\t%p\n", begin, end, current);
}

void info(void* bp, int loop)
{
  printf("\n\n--------------------------------------------------------------------------------------------------------------------------------\n");
  printf("HEAP ADDR:\t\t%p\t\tHEAP END:\t\t%p\t\t\nCURR ADDR:\t\t%p\t\tCURR ADDR:\t\t%p\t\t\nDIFF:\t\t\t%li\t\t\tDIFF:\t\t\t%li\t\t\n\n ", heap_gbl, end_heap_gbl, bp, bp , bp - heap_gbl, end_heap_gbl - bp);
  printf("PREV SIZE:\t\t%zu\t\t\tCURR SIZE:\t\t%zu\t\t\tNEXT SIZE:\t\t%zu\t\t\n\n", GET_SIZE(HDRP(PREV_BLKP(bp))), GET_SIZE(HDRP(bp)), GET_SIZE(HDRP(NEXT_BLKP(bp))));
  //printf("PREV ADDR:\t\t%zu\t\t\tCURR ADDR:\t\t%zu\t\t\tNEXT ADDR:\t\t%zu\t\t\n\n", PREV_BLKP(bp), bp, NEXT_BLKP(bp));  
  //printf("PREV ADDR DIFF:\t\t%p\t\t\tCURR ADDR DIFF:\t\t%p\t\t\tNEXT ADDR DIFF:\t\t%p\t\t\n\n", PREV_BLKP(bp) - heap_gbl, bp - heap_gbl, NEXT_BLKP(bp) - heap_gbl);  
}

void pack(void* bp, size_t size, size_t alloc)
{ PUT(HDRP(bp), PACK(size, alloc));  }

size_t get_size(void* bp)
{ return GET_SIZE(HDRP(bp));  }

void set_size_curr(void* bp, size_t sz)
{ GET_SIZE(GET_CURR(HDRP(bp))) = sz;    }

void set_size_prev(void* bp, size_t sz)
{ GET_SIZE(GET_PREV(HDRP(bp))) = sz;    }

size_t get_alloc(void* bp)
{ return GET_ALLOC(HDRP(bp)); }

void set_alloc_curr(void* bp, size_t alloc)
{ GET_ALLOC(GET_CURR(HDRP(bp))) = alloc; }

void set_alloc_prev(void* bp, size_t alloc)
{ GET_ALLOC(GET_PREV(HDRP(bp))) = alloc; }

void* prev_blkp(void* bp)
{ return PREV_BLKP(bp);       }

void* next_blkp(void* bp)
{ return NEXT_BLKP(bp);       }
