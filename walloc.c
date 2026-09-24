#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/mman.h>
#include "walloc.h"


#define PAGE_SIZE 4096
#define HEAP_MINIMUM 65536
#define MIN_LEFTOVER 16 //can't be smaller than this

//whole mmap'd
typedef struct s_heap {
	struct s_heap *prev;
	struct s_heap *next;
	//t_heap_group group; //Tiny, Small, or Large
	size_t total_size;
	size_t free_size;
	size_t block_count;
}
t_heap;

//single allocated block
typedef struct s_block {
	struct s_block *prev;
	struct s_block *next;
	size_t data_size;
	bool freed;
}
t_block;

static t_heap *all_heaps_created = NULL;

//mmap function
t_heap *create_heap(size_t len){
	size_t total = len + sizeof(t_heap) + sizeof(t_block);
	if(total < HEAP_MINIMUM){
		total = HEAP_MINIMUM;
	}
	total = (total + PAGE_SIZE - 1) & ~(size_t)(PAGE_SIZE - 1);

	t_heap *heap = (t_heap*)mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (heap == MAP_FAILED){
		return NULL;
	}
	heap->prev = NULL;
	heap->next = NULL;
	heap->total_size = total;
	heap->free_size = heap->total_size - sizeof(t_heap);
	heap->block_count = 1;
	return heap;
}

t_block *create_block(t_heap *heap){
		t_block *block = (t_block *)(heap + 1); //heap + 1 points to the location right after the struct heap points to.
		block->prev = NULL;
		block->next = NULL;
		block->freed = false;
		block->data_size = heap->free_size - sizeof(t_block);
		return block;
}

//helper method
t_block *find_free_block(size_t size)
{
	t_heap *heap = all_heaps_created;

	while (heap != NULL)
	{
		t_block *block = (t_block *)(heap + 1); //basically the first block in heap
		while(block != NULL){
			if (block->freed == true && block->data_size >= size){
				return block;
			}
		block = block->next;
		}
	heap = heap->next;
	}
	return NULL;
}


//my malloc - winzel allocates
void *walloc(size_t size){
	t_block *block;

	if(size == 0 || size > SIZE_MAX/2){ //ask ted.
		return NULL;
	}
	size = (size + 15) & ~(size_t)15; //rounds up to a multiple of 16


	if (all_heaps_created == NULL) { //does a heap exist? no heaps exist yet.
		all_heaps_created = create_heap(size);
		if (all_heaps_created == NULL){ //did it fail?
			return NULL;
		}
		//otherwise, if it didn't fail, create a block from that newly taken heap.
		block = create_block(all_heaps_created);
	}
	else { //a heap already exist
		block = find_free_block(size); //need a helper method
		
		if (block == NULL){ //no free block available
			t_heap *new_heap = create_heap(size);
			if (new_heap == NULL) {
				return NULL;
			}

			t_heap *last = all_heaps_created;
			while (last->next != NULL){
				last = last->next;
			}
			last->next = new_heap;
			block = create_block(new_heap);
		}
	}
	
	if (block == NULL){
		return NULL;
	}
	block->freed = 0;
	return (void *)(block+1);
	
}


//munmap(void *addr, size_t length);
void destroy_heap (t_heap *heap) { //gives heap back to OS
	if (all_heaps_created == heap) {
		all_heaps_created = heap->next; //first heap
	}
	else{
		t_heap *prev = all_heaps_created;
		while (prev != NULL && prev->next != heap){
			prev = prev->next; //find the heap just before it
		}
		if (prev != NULL){
			prev->next = heap->next; //skip
		}
	}
	munmap(heap, heap->total_size);
}

t_heap *find_heap_of(t_block *block){
	t_heap *heap = all_heaps_created;

	while (heap != NULL){
		char *start = (char *)heap;
		char *end = start + heap->total_size;
		if ((char *)block >= start && (char *)block < end){
			return heap;
		}
		heap = heap->next;
	}
	return NULL;
}

//winzel frees
void winfree(void *ptr){
	if (ptr == NULL){
		return;
	}

	t_block *block = (t_block *)ptr - 1; //steps back to its header so it's marked free.

	block->freed = 1; //it just means the block is free.

	if (block->next != NULL && block->next->freed){ //is there a block after, and is it free?
		block->data_size += sizeof(t_block) + block->next->data_size; //puts it into one whole big block
		block->next = block->next->next; 
		if(block->next != NULL){
			block->next->prev = block;
		}
	}
	
	if (block->prev != NULL && block->prev->freed){ //is there a block after, and is it free?
		block->prev->data_size += sizeof(t_block) + block->data_size;
		block->prev->next = block->next;
		if (block->next != NULL) {
			block->next->prev = block->prev;
		}
		block = block->prev;
	}

	t_heap *heap = find_heap_of(block); //if the heap is just one whole free block. return to OS
	if(heap != NULL && (t_block *)(heap + 1) == block && block->next == NULL){
		destroy_heap(heap);
	}
}
