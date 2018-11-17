#include "mm.h"
#include <stdio.h>

#define ALIGNMENT 16
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

#define OVERHEAD (sizeof(block_header)+sizeof(block_footer))
#define HDRP(bp) ((char *)(bp) - sizeof(block_header))

#define GET_SIZE(p)  ((block_header *)(p))->size
#define GET_ALLOC(p) ((block_header *)(p))->allocated

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))

#define IS_ALIGNED(p) (((unsigned long)p & 15) == 0)


// ADDED
#define CHUNK_SIZE (1 << 14)
#define CHUNK_ALIGN(size) (((size)+(CHUNK_SIZE-1)) & ~(CHUNK_SIZE-1))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE((char *)(bp)-OVERHEAD))
#define FTRP(bp) ((char *)(bp)+GET_SIZE(HDRP(bp))-OVERHEAD)

void *coalesce(void *bp);
int max(int a, int b);
void p_debug(char* str);
void set_allocated1(void *bp, size_t size);

typedef struct {
  size_t size;
  char   allocated;
} block_header;

typedef struct {
 size_t size;
 int filler;
} block_footer;

typedef struct list_node {
 struct list_node *prev;
 struct list_node *next;
} list_node;


void* first_bp;
void* free_list;

int init_count = 0;
int mm_malloc_count = 0;
int free_count = 0;
int set_allocated_count = 0;
int coal_count = 0;



void mm_init(void *heap, size_t heap_size)
{
  p_debug("mm_init");
  printf("HEAP SIZE: %zu\n", heap_size);
  init_count++;
  void *bp;
  if(!IS_ALIGNED(heap)) printf("heap not aligned: %i\n", IS_ALIGNED(heap));
  else printf("heap aligned: %i\n", IS_ALIGNED(heap));
  
  bp = (void*)heap + sizeof(block_header);

  if(!IS_ALIGNED(bp)) printf("bp not aligned: %i\n", IS_ALIGNED(bp));
  else printf("bp aligned: %i\n", IS_ALIGNED(bp));


  GET_SIZE(HDRP(bp)) = 2;   // Install prolog HDR
  GET_ALLOC(HDRP(bp)) = 1;  
  GET_SIZE(bp) = 2;   // Install prolog FTR

  void* terminator = (void*)heap + heap_size; // Install terminator block

  GET_SIZE(HDRP(terminator)) = 0;
  GET_ALLOC(HDRP(terminator)) = 1;

  bp = (void*)bp + (2*sizeof(block_header));   // MOVE TO FIRST BLK HDR, BEYOND PROLOG

  /*                   pro_hdr + pro_ftr  +blk hdr + blk ftr + terminator         */
  GET_SIZE(HDRP(bp)) = heap_size - ((2*OVERHEAD) + sizeof(block_header));   // Install hdr
  GET_ALLOC(HDRP(bp)) = 0;

  GET_SIZE(FTRP(bp)) = heap_size - ((2*OVERHEAD) + sizeof(block_header));   // Install ftr

  first_bp = bp;
  free_list = bp;
}

void set_allocated1(void *bp, size_t size) {
  p_debug("set_allocated");    
  set_allocated_count++;    
  size_t extra_size = GET_SIZE(HDRP(bp)) - size;
  printf("EXTRA SIZE: %zu\n", extra_size);
  if (extra_size > ALIGN(1 + OVERHEAD)) {
    GET_SIZE(HDRP(bp)) = size;
    GET_SIZE(HDRP(NEXT_BLKP(bp))) = extra_size;
    GET_ALLOC(HDRP(NEXT_BLKP(bp))) = 0;
  }
  GET_ALLOC(HDRP(bp)) = 1;
}

static void set_allocated(void *bp)
{
  //GET_ALLOC(HDRP(bp)) = 1;
}


void *mm_malloc(size_t size)
{
  p_debug("mm_malloc");

  mm_malloc_count++;
  int need_size = max(size, sizeof(list_node));
  int new_size = ALIGN(need_size + OVERHEAD);
  void *bp = first_bp;
  int loop_count = 0;

  while (GET_SIZE(HDRP(bp)) != 0) {
    printf("loop: %i\t SIZE: %zu\n", loop_count++, GET_SIZE(HDRP(bp)));
    if (!GET_ALLOC(HDRP(bp)) && (GET_SIZE(HDRP(bp)) >= new_size)) {
      set_allocated1(bp, new_size);
      set_allocated(bp);

      if(!IS_ALIGNED(bp)) printf("bp not aligned: %i\n", IS_ALIGNED(bp));
      else printf("bp aligned: %i\n", IS_ALIGNED(bp));


      return bp;
    }
    bp = NEXT_BLKP(bp);
  }

  return NULL;
}


void mm_free(void *bp)
{
  p_debug("mm_free");  
  free_count++;  
  GET_ALLOC(HDRP(bp)) = 0;
  coalesce(bp);
}

void *coalesce(void *bp) {
  p_debug("coal");
  coal_count++;
  size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));
  
    if (prev_alloc && next_alloc) { /* Case 1 */
      //add_to_free_list((list_node *)bp);
    }
    else if (prev_alloc && !next_alloc) { /* Case 2 */
     size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
     GET_SIZE(HDRP(bp)) = size;
     GET_SIZE(FTRP(bp)) = size;
    }
    else if (!prev_alloc && next_alloc) { /* Case 3 */
     size += GET_SIZE(HDRP(PREV_BLKP(bp)));
     GET_SIZE(FTRP(bp)) = size;
     GET_SIZE(HDRP(PREV_BLKP(bp))) = size;
     bp = PREV_BLKP(bp);
    }
    else { /* Case 4 */
     size += (GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp))));
     GET_SIZE(HDRP(PREV_BLKP(bp))) = size;
     GET_SIZE(FTRP(NEXT_BLKP(bp))) = size;
     bp = PREV_BLKP(bp);
    }
  
  return bp;

}

void p_debug(char* str)
{
  printf("init: %i\tset_alloc: %i\tmm_alloc: %i\tfree: %i\tcoal: %i\n", init_count,set_allocated_count, mm_malloc_count, free_count, coal_count);
  printf(str);
  printf("\n");
}

int max(int a, int b)
{
  if(a >= b) return a;
  return b;
}




