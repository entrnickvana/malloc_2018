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
void p_addr(void* begin, void* end, void* current);
void info(void* bp, int loop);
int dbg = 0;
int case_print = 0;
int coal_info = 0;


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
void* heap_gbl;
void* end_heap_gbl;

int init_count = 0;
int mm_malloc_count = 0;
int free_count = 0;
int set_allocated_count = 0;
int coal_count = 0;
int num_blocks = 0;
char scan = 'z';

void mm_init(void *heap, size_t heap_size)
{
  //p_debug("mm_init");
  heap_gbl = heap;
  init_count++;
  void *bp;
  
  bp = (void*)heap + sizeof(block_header);

  GET_SIZE(HDRP(bp)) = 2*sizeof(block_header);   // Install prolog HDR
  GET_ALLOC(HDRP(bp)) = 1;  
  GET_SIZE(bp) = 2*sizeof(block_header);         // Install prolog FTR

  void* terminator = (void*)heap + heap_size; // Install terminator block
  end_heap_gbl = terminator;
  GET_SIZE(HDRP(terminator)) = 0;
  GET_ALLOC(HDRP(terminator)) = 1;

  bp = (void*)bp + (2*sizeof(block_header));   // MOVE TO FIRST BLK HDR, BEYOND PROLOG
  /*                   pro_hdr + pro_ftr  +blk hdr + blk ftr + terminator         */
  GET_SIZE(HDRP(bp)) = heap_size - ((OVERHEAD) + sizeof(block_header));   // Install hdr
  GET_ALLOC(HDRP(bp)) = 0;

  GET_SIZE(FTRP(bp)) = heap_size - ((OVERHEAD) + sizeof(block_header));   // Install ftr

  num_blocks++;
  first_bp = bp;
  free_list = bp;
}

void set_allocated1(void *bp, size_t size) {
  //p_debug("set_allocated");    
  set_allocated_count++;    
  size_t extra_size = GET_SIZE(HDRP(bp)) - size;

  if (extra_size > ALIGN(1 + OVERHEAD)) {
    GET_SIZE(HDRP(bp)) = size;
    GET_SIZE(HDRP(NEXT_BLKP(bp))) = extra_size;
    GET_ALLOC(HDRP(NEXT_BLKP(bp))) = 0;
    num_blocks++;    
  }
  GET_ALLOC(HDRP(bp)) = 1;
}

static void set_allocated(void *bp)
{
  //GET_ALLOC(HDRP(bp)) = 1;
}

void *mm_malloc(size_t size)
{
  //p_debug("mm_malloc\n");
  mm_malloc_count++;
  int need_size = max(size, sizeof(list_node));
  int new_size = ALIGN(need_size + OVERHEAD);

  //printf("**********************************************************************************************************\n");
  //printf("REQUESTED SIZE:\t\t%zu\t\tNEEDED SIZE:\t\t%i\t\tNEW SIZE:\t\t%i\n", size, need_size, new_size);
  void *bp = first_bp;
  int loop = 0;
  
  while (GET_SIZE(HDRP(bp)) != 0) {
    //info(bp, loop++);
    //if((mm_malloc_count % 5) == 0) {scan = 'z'; while(scan == 'z') {scanf("%c", &scan);} }
    if(!GET_ALLOC(HDRP(bp)) && (GET_SIZE(HDRP(bp)) >= new_size)) {
      printf("\t\t\t_______________________FIT FOUND_______________________\n");
      set_allocated1(bp, new_size);
      set_allocated(bp);
      return bp;
    }

    bp = NEXT_BLKP(bp);
  }

  if(dbg) printf("FAIL!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
  return NULL;
}

void mm_free(void *bp)
{
  //p_debug("mm_free");  
  free_count++;  
  //info(bp, 0);
//  printf("free called to remove block of size %d at %p\n", GET_SIZE(HDRP(bp)), bp);
  GET_ALLOC(HDRP(bp)) = 0;  
  //coalesce(bp);

}

void *coalesce(void *bp) {
  //p_debug("coal");
  coal_count++;
  size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp))); // SEG FAULT
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) { /* Case 1 */
      //add_to_free_list((list_node *)bp);
      //if(dbg || case_print) printf("case 1\n");
      //num_blocks--;
    }
    else if (prev_alloc && !next_alloc) { /* Case 2 */
      //if(dbg || case_print) printf("case 2\n");    
      size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
      GET_SIZE(HDRP(bp)) = size;
      GET_SIZE(FTRP(bp)) = size;
      num_blocks--;
    }
    else if (!prev_alloc && next_alloc) { /* Case 3 */
      //if(dbg || case_print) printf("case 3\n");
      size += GET_SIZE(HDRP(PREV_BLKP(bp)));
      GET_SIZE(FTRP(bp)) = size;
      GET_SIZE(HDRP(PREV_BLKP(bp))) = size;
      bp = PREV_BLKP(bp);
      num_blocks--;      
    }
    else { /* Case 4 */
      size += (GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp))));
      if(dbg || case_print) printf("case 4\n");
      GET_SIZE(HDRP(PREV_BLKP(bp))) = size;
      //if(dbg) printf("NXT BLK SIZE: %zu\n", GET_SIZE(HDRP(NEXT_BLKP(bp))));
      //p_addr(heap_gbl, end_heap_gbl, NEXT_BLKP(bp));
      GET_SIZE(FTRP(NEXT_BLKP(bp))) = size;  // seg fault

      bp = PREV_BLKP(bp);
      num_blocks--;      
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

