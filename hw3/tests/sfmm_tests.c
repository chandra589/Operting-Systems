#include <criterion/criterion.h>
#include <errno.h>
#include <signal.h>
#include "debug.h"
#include "sfmm.h"
#define TEST_TIMEOUT 15

/*
 * Assert the total number of free blocks of a specified size.
 * If size == 0, then assert the total number of all free blocks.
 */
void assert_free_block_count(size_t size, int index, int count) {
    int cnt = 0;
    for(int i = 0; i < NUM_FREE_LISTS; i++) {
	sf_block *bp = sf_free_list_heads[i].body.links.next;
	while(bp != &sf_free_list_heads[i]) {
	    if(size == 0 || size == (bp->header & ~0x3f)) {
		cnt++;
		if(size != 0) {
		    cr_assert_eq(index, i, "Block %p (size %ld) is in wrong list for its size "
				 "(expected %d, was %d)",
				 (long *)(bp) + 1, bp->header & ~0x3f, index, i);
		}
	    }
	    bp = bp->body.links.next;
	}
    }
    if(size == 0) {
	cr_assert_eq(cnt, count, "Wrong number of free blocks (exp=%d, found=%d)",
		     count, cnt);
    } else {
	cr_assert_eq(cnt, count, "Wrong number of free blocks of size %ld (exp=%d, found=%d)",
		     size, count, cnt);
    }
}

Test(sfmm_basecode_suite, malloc_an_int, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	size_t sz = sizeof(int);
	int *x = sf_malloc(sz);

	cr_assert_not_null(x, "x is NULL!");

	*x = 4;

	cr_assert(*x == 4, "sf_malloc failed to give proper space for an int!");

	assert_free_block_count(0, 0, 1);
	assert_free_block_count(8000, 8, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
	cr_assert(sf_mem_start() + PAGE_SZ == sf_mem_end(), "Allocated more than necessary!");
}

Test(sfmm_basecode_suite, malloc_four_pages, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;

	void *x = sf_malloc(32624);
	cr_assert_not_null(x, "x is NULL!");
	assert_free_block_count(0, 0, 0);
	cr_assert(sf_errno == 0, "sf_errno is not 0!");
}

Test(sfmm_basecode_suite, malloc_too_large, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	void *x = sf_malloc(524288);

	cr_assert_null(x, "x is not NULL!");
	assert_free_block_count(0, 0, 1);
	assert_free_block_count(130944, 8, 1);
	cr_assert(sf_errno == ENOMEM, "sf_errno is not ENOMEM!");
}

Test(sfmm_basecode_suite, free_no_coalesce, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	size_t sz_x = 8, sz_y = 200, sz_z = 1;
	/* void *x = */ sf_malloc(sz_x);
	void *y = sf_malloc(sz_y);
	/* void *z = */ sf_malloc(sz_z);

	sf_free(y);

	assert_free_block_count(0, 0, 2);
	assert_free_block_count(256, 3, 1);
	assert_free_block_count(7680, 8, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sfmm_basecode_suite, free_coalesce, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	size_t sz_w = 8, sz_x = 200, sz_y = 300, sz_z = 4;
	/* void *w = */ sf_malloc(sz_w);
	void *x = sf_malloc(sz_x);
	void *y = sf_malloc(sz_y);
	/* void *z = */ sf_malloc(sz_z);

	sf_free(y);
	sf_free(x);

	assert_free_block_count(0, 0, 2);
	assert_free_block_count(576, 5, 1);
	assert_free_block_count(7360, 8, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sfmm_basecode_suite, freelist, .timeout = TEST_TIMEOUT) {
        size_t sz_u = 200, sz_v = 300, sz_w = 200, sz_x = 500, sz_y = 200, sz_z = 700;
	void *u = sf_malloc(sz_u);
	/* void *v = */ sf_malloc(sz_v);
	void *w = sf_malloc(sz_w);
	/* void *x = */ sf_malloc(sz_x);
	void *y = sf_malloc(sz_y);
	/* void *z = */ sf_malloc(sz_z);

	sf_free(u);
	sf_free(w);
	sf_free(y);

	assert_free_block_count(0, 0, 4);
	assert_free_block_count(256, 3, 3);
	assert_free_block_count(5696, 8, 1);

	// First block in list should be the most recently freed block.
	int i = 3;
	sf_block *bp = sf_free_list_heads[i].body.links.next;
	cr_assert_eq(bp, (char *)y - 16,
		     "Wrong first block in free list %d: (found=%p, exp=%p)",
                     i, bp, (char *)y - 16);
}

Test(sfmm_basecode_suite, realloc_larger_block, .timeout = TEST_TIMEOUT) {
        size_t sz_x = sizeof(int), sz_y = 10, sz_x1 = sizeof(int) * 20;
	void *x = sf_malloc(sz_x);
	/* void *y = */ sf_malloc(sz_y);
	x = sf_realloc(x, sz_x1);

	cr_assert_not_null(x, "x is NULL!");
	sf_block *bp = (sf_block *)((char *)x - 16);
	cr_assert(*((long *)(bp) + 1) & 0x1, "Allocated bit is not set!");
	cr_assert((*((long *)(bp) + 1) & ~0x3f) == 128,
		  "Realloc'ed block size (%ld) not what was expected (%ld)!",
		  *((long *)(bp) + 1) & ~0x3f, 128);

	assert_free_block_count(0, 0, 2);
	assert_free_block_count(64, 0, 1);
	assert_free_block_count(7808, 8, 1);
}

Test(sfmm_basecode_suite, realloc_smaller_block_splinter, .timeout = TEST_TIMEOUT) {
        size_t sz_x = sizeof(int) * 20, sz_y = sizeof(int) * 16;
	void *x = sf_malloc(sz_x);
	void *y = sf_realloc(x, sz_y);

	cr_assert_not_null(y, "y is NULL!");
	cr_assert(x == y, "Payload addresses are different!");

	sf_block *bp = (sf_block *)((char*)y - 16);
	cr_assert(*((long *)(bp) + 1) & 0x1, "Allocated bit is not set!");
	cr_assert((*((long *)(bp) + 1) & ~0x3f) == 128,
		  "Block size (%ld) not what was expected (%ld)!",
	          *((long *)(bp) + 1) & ~0x3f, 128);

	assert_free_block_count(0, 0, 1);
	assert_free_block_count(7936, 8, 1);
}

Test(sfmm_basecode_suite, realloc_smaller_block_free_block, .timeout = TEST_TIMEOUT) {
        size_t sz_x = sizeof(double) * 8, sz_y = sizeof(int);
	void *x = sf_malloc(sz_x);
	void *y = sf_realloc(x, sz_y);

	cr_assert_not_null(y, "y is NULL!");

	sf_block *bp = (sf_block *)((char *)y - 16);
	cr_assert(*((long *)(bp) + 1) & 0x1, "Allocated bit is not set!");
	cr_assert((*((long *)(bp) + 1) & ~0x3f) == 64,
		  "Realloc'ed block size (%ld) not what was expected (%ld)!",
		  *((long *)(bp) + 1) & ~0x3f, 64);

	assert_free_block_count(0, 0, 1);
	assert_free_block_count(8000, 8, 1);
}

//############################################
//STUDENT UNIT TESTS SHOULD BE WRITTEN BELOW
//DO NOT DELETE THESE COMMENTS
//############################################

Test(sfmm_student_suite, Epilogue_bitChecking, .timeout = TEST_TIMEOUT) {
	double* ptr = sf_malloc(sizeof(double));
    	*ptr = 5.7;
    	sf_malloc(7992);
    	sf_malloc(4);
    	cr_assert (((sf_mem_end() - sf_mem_start()) == 2*PAGE_SZ), "NewPage is not created");
    	 //setting epilogue
	sf_block* epilogue = (sf_block*)(sf_mem_end() - 16);
	int sizeofepilogue = epilogue->header & ~0x3f;
	cr_assert(sizeofepilogue == 0, "Epilogue not set properly");
	bool thisblockallocated = epilogue->header & THIS_BLOCK_ALLOCATED;
	cr_assert(thisblockallocated == true, "Epilogue block allocated bit set wrong");

}

Test(sfmm_student_suite, realloc_invalid_pointer, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	void* x = sf_malloc(sizeof(int));
	void* y = (void*)x + 8;
	sf_realloc(y, 2*sizeof(int));
	cr_assert(sf_errno = EINVAL, "sf_errno not set to EINVAL");
}

Test(sfmm_student_suite, free_invalid_pointer_outside_memory, .timeout = TEST_TIMEOUT, .signal = SIGABRT) {
	void* x = sf_malloc(sizeof(double));
	sf_free(x+PAGE_SZ);
}


Test(sfmm_basecode_suite, one_page_filled_completely, .timeout = TEST_TIMEOUT) {
	sf_malloc(55);
	sf_malloc(7992);
	//all blocks are allocated in this, have to verify the bits are set correctly
	void* memst = sf_mem_start();
	sf_block* startblock = memst + 8*6 + 8*8; //address of the first block
	bool allocbit = startblock->header & THIS_BLOCK_ALLOCATED;
	cr_assert(allocbit == true, "First block alloc bit not set correctly");
	bool preallocbit = startblock->header & PREV_BLOCK_ALLOCATED;
	cr_assert(preallocbit == true, "First block pre-alloc bit not set correctly");
	sf_block* bigblock = (void*)startblock + (startblock->header & ~0x3f);
	allocbit = bigblock->header & THIS_BLOCK_ALLOCATED;
	preallocbit = bigblock->header & PREV_BLOCK_ALLOCATED;
	cr_assert(allocbit == true, "alloc bit of 8000 size block not set correctly");
	cr_assert(preallocbit == true, "prealloc bit of 8000 size block not set correctly");
	sf_block* epilogue = (void*)bigblock + (bigblock->header & ~0x3f);
	size_t epsize = epilogue->header & ~0x3f;
	allocbit = epilogue->header & THIS_BLOCK_ALLOCATED;
	preallocbit = epilogue->header & PREV_BLOCK_ALLOCATED;
	cr_assert(epsize == 0, "Epilogue size incorrect");
	cr_assert(allocbit == true, "Epilogue alloc bit set incorrectly");
	cr_assert(preallocbit == true, "Epilogue prealloc bit set incorrectly");
}

Test(sfmm_student_suite, free_unalligned_pointer, .timeout = TEST_TIMEOUT, .signal = SIGABRT) {
	void* x = sf_malloc(60);
	sf_free(x+10);
}

Test(sfmm_student_suite, free_all, .timeout = TEST_TIMEOUT) {
	void* x = sf_malloc(10);
	void* y = sf_malloc(5000);
	void* z = sf_malloc(3000);
	void* k = sf_malloc(1000);
	sf_free(y);
	sf_free(k);
	sf_free(x);
	sf_free(z);
	//only one free block with a memory of (8192 + 8064 = 16256)
	void* memst = sf_mem_start();
	sf_block* startblock = memst + 8*6 + 8*8; //address of the first block
	bool allocbit = startblock->header & THIS_BLOCK_ALLOCATED;
	cr_assert(allocbit == false, "First block alloc bit not set correctly");
	bool preallocbit = startblock->header & PREV_BLOCK_ALLOCATED;
	cr_assert(preallocbit == true, "First block pre-alloc bit not set correctly");
	size_t blocksize = startblock->header & ~0x3f;
	cr_assert(blocksize == 16256, "First block size not correct");
	sf_block* epilogue = (void*)startblock + blocksize;
	size_t epsize = epilogue->header & ~0x3f;
	allocbit = epilogue->header & THIS_BLOCK_ALLOCATED;
	preallocbit = epilogue->header & PREV_BLOCK_ALLOCATED;
	cr_assert(epsize == 0, "Epilogue size incorrect");
	cr_assert(allocbit == true, "Epilogue alloc bit set incorrectly");
	cr_assert(preallocbit == false, "Epilogue prealloc bit set incorrectly");

}

