#ifndef _FFIT_H_
#define _FFIT_H_

// By default this implementation uses a First-Fit strategy, nonetheless if 
// the BESTFIT macro is set then a Best-Fit strategy is used to find a
// free block

#ifdef __cplusplus
extern "C" {
#endif

#define BESTFIT

struct free_ptr {
  struct head_struct *next, *prev;
};

typedef struct head_struct {
  unsigned long size;
  struct head_struct *prev_phys;
  unsigned long pad[2]; // make header 16-bytes
  union mem {
    struct free_ptr free_ptr;
    unsigned char ptr[sizeof (struct free_ptr)];
  } mem;
} list_t;

typedef struct list_header_struct {
  list_t      *head;
  unsigned int HEADER_SIZE; 
  unsigned int MIN_SIZE ;
} list_header_t;


list_header_t *init_memory_pool (unsigned int size, char *ptr);
void *rtl_malloc(list_header_t *list, unsigned int size);
void rtl_free(list_header_t *list, void *ptr);
void destroy_memory_pool(list_header_t *list);

// Next function just can be used if the SANITY_CHECK macro has been defined
#ifdef SANITY_CHECK
void print_list (void);
void dump_memory_region (unsigned char *mem_ptr, unsigned int size);
void print_phys_list (void);
#endif

#define USED_BLOCK 0x80000000
#define FREE_BLOCK ~USED_BLOCK //0x7FFFFFFF

#define LAST_BLOCK 0x40000000
#define NOT_LAST_BLOCK ~LAST_BLOCK //0xBFFFFFFF

#define IS_USED_BLOCK(x) ((x -> size & USED_BLOCK) == USED_BLOCK)
#define IS_LAST_BLOCK(x) ((x -> size & LAST_BLOCK) == LAST_BLOCK)
#define GET_BLOCK_SIZE(x) (x -> size & FREE_BLOCK & NOT_LAST_BLOCK)
#define SET_USED_BLOCK(x) (x -> size |= USED_BLOCK)
#define SET_FREE_BLOCK(x) (x -> size &= FREE_BLOCK)
#define SET_LAST_BLOCK(x) (x -> size |= LAST_BLOCK)
#define SET_NOT_LAST_BLOCK(x) (x -> size &= NOT_LAST_BLOCK)

#define LISTHEADERSIZE offsetof(list_t,mem)

#ifdef __cplusplus
}; // extern "C" 
#endif

#endif
