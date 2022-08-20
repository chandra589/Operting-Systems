/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "sfmm.h"
#include <errno.h>
#include "helperFuncs.h"

bool initheapandfreelists();
void InsertinLists(sf_block* block);
bool checkfreelists(size_t size, sf_block** newblock);
bool removefromlists(sf_block* block);
bool IspointerValid(void* pp);

void *sf_malloc(size_t size) {

    if (size == 0) return NULL;

    if (sf_mem_end() - sf_mem_start() == 0)
    {
        bool ret = initheapandfreelists();
        if (!ret) return NULL;
    }

    sf_block* newblock = NULL;
    if (checkfreelists(size, &newblock))
    {
        //block found - newblock
        size_t total_size = size + 8;
        size_t alligned_size = 0;
        int rem = total_size % 64;
        alligned_size = total_size/64;
        if (rem > 0) alligned_size++;
        alligned_size *= 64;
        size_t block_size = newblock->header & ~0x3f;
        size_t rem_block_size = block_size - alligned_size;
        bool preallbit = newblock->header & PREV_BLOCK_ALLOCATED;
        newblock->header = alligned_size;
        newblock->header = newblock->header | THIS_BLOCK_ALLOCATED;
        if (preallbit) newblock->header = newblock->header | PREV_BLOCK_ALLOCATED;
        if (rem_block_size >= 64)
        {
            sf_block* splitblock = (sf_block*)((void*)newblock + alligned_size);
            splitblock->header = rem_block_size;
            splitblock->header = splitblock->header | PREV_BLOCK_ALLOCATED;
            sf_block* nextblock = (sf_block*)((void*)newblock + block_size);
            nextblock->prev_footer = splitblock->header;
            InsertinLists(splitblock);
        }
        else
        {
            //set the next block prev bit to allocated
            sf_block* nextblock = (sf_block*)((void*)newblock + block_size);
            nextblock->header = nextblock->header | PREV_BLOCK_ALLOCATED;
        }

        return ((void*)newblock + 16);
    }
    else
    {
        do
        {
            void* newpagestart = sf_mem_grow();
            if (newpagestart == NULL)
            {
                sf_errno = ENOMEM;
                return NULL;
            }
            //collease this page with any free block before it
            sf_block* prevepilogue = (sf_block*)((void*)newpagestart - 16);
            bool prevblockalloc = prevepilogue->header & PREV_BLOCK_ALLOCATED;
            if (!prevblockalloc)
            {
                size_t prevblock_size = prevepilogue->prev_footer & ~0x3f;
                sf_block* prevblock = (sf_block*)((void*)prevepilogue - prevblock_size);
                bool ret = removefromlists(prevblock);
                if (!ret) fprintf(stderr, "Not able to remove the block from free lists");
                bool preallocbit = prevblock->header & PREV_BLOCK_ALLOCATED;
                prevblock->header = prevblock_size + PAGE_SZ;
                if (preallocbit)
                    prevblock->header = prevblock->header | PREV_BLOCK_ALLOCATED;
                //This block is already unallocated
                //Set new epilogue
                sf_block* newepilogue = (sf_block*)((void*)sf_mem_end() - 16);
                newepilogue->header = 0;
                newepilogue->header = newepilogue->header | THIS_BLOCK_ALLOCATED;
                newepilogue->prev_footer = prevblock->header;
                InsertinLists(prevblock);
            }
            else
            {
                //previous epilogue becomes the header of the new block.
                //nothing to remove from lists
                prevepilogue->header = PAGE_SZ;
                prevepilogue->header = prevepilogue->header | PREV_BLOCK_ALLOCATED; //as we checked it
                prevepilogue->header = prevepilogue->header & ~THIS_BLOCK_ALLOCATED;
                sf_block* newepilogue = (sf_block*)((void*)sf_mem_end() - 16);
                newepilogue->header = 0;
                newepilogue->header = newepilogue->header | THIS_BLOCK_ALLOCATED;
                newepilogue->prev_footer = prevepilogue->header;
                InsertinLists(prevepilogue);
            }
        } while(!checkfreelists(size, &newblock));

        if (newblock)
        {
            //block found - newblock
            size_t total_size = size + 8;
            size_t alligned_size = 0;
            int rem = total_size % 64;
            alligned_size = total_size/64;
            if (rem > 0) alligned_size++;
            alligned_size *= 64;
            size_t block_size = newblock->header & ~0x3f;
            size_t rem_block_size = block_size - alligned_size;
            bool preallbit = newblock->header & PREV_BLOCK_ALLOCATED;
            newblock->header = alligned_size;
            newblock->header = newblock->header | THIS_BLOCK_ALLOCATED;
            if (preallbit) newblock->header = newblock->header | PREV_BLOCK_ALLOCATED;
            if (rem_block_size >= 64)
            {
                sf_block* splitblock = (sf_block*)((void*)newblock + alligned_size);
                splitblock->header = rem_block_size;
                splitblock->header = splitblock->header | PREV_BLOCK_ALLOCATED;
                sf_block* nextblock = (sf_block*)((void*)newblock + block_size);
                nextblock->prev_footer = splitblock->header;
                InsertinLists(splitblock);
            }
            else
            {
                //set the next block prev bit to allocated
                sf_block* nextblock = (sf_block*)((void*)newblock + block_size);
                nextblock->header = nextblock->header | PREV_BLOCK_ALLOCATED;
            }

            return ((void*)newblock + 16);
        }
    }
    return NULL;
}

void sf_free(void *pp) {

    //have to check the validity of the pointer
    bool ret = IspointerValid(pp);
    if (!ret) abort();
    sf_block* blocktobefreed = (sf_block*)((void*)pp - 16);
    size_t block_size = blocktobefreed->header & ~0x3f;
    bool preallocbit = blocktobefreed->header & PREV_BLOCK_ALLOCATED;
    sf_block* nextblock = (sf_block*)((void*)blocktobefreed + block_size);
    bool nextblockallocbit = nextblock->header & THIS_BLOCK_ALLOCATED;

    if (!preallocbit && !nextblockallocbit)
    {
        //remove both the prevblock and the next block from the free lists
        size_t prevblock_size = blocktobefreed->prev_footer & ~0x3f;
        sf_block* prevblock = (sf_block*)((void*)blocktobefreed - prevblock_size);
        bool ret = removefromlists(prevblock);
        if (!ret) fprintf(stderr, "Not able to remove the block from free lists");
        ret = removefromlists(nextblock);
        if (!ret) fprintf(stderr, "Not able to remove the block from free lists");
        bool prevbittoset = prevblock->header & PREV_BLOCK_ALLOCATED;
        size_t nextblock_size = nextblock->header & ~0x3f;
        prevblock->header = prevblock_size + block_size + nextblock_size;
        if (prevbittoset)
            prevblock->header = prevblock->header | PREV_BLOCK_ALLOCATED;
        sf_block* lastblock = (sf_block*)((void*)nextblock + nextblock_size);
        lastblock->header = lastblock->header & ~PREV_BLOCK_ALLOCATED;
        lastblock->prev_footer = prevblock->header;
        InsertinLists(prevblock);
    }
    else if (!preallocbit)
    {
        //remove the prev block from the free list
        size_t prevblock_size = blocktobefreed->prev_footer & ~0x3f;
        sf_block* prevblock = (sf_block*)((void*)blocktobefreed - prevblock_size);
        bool ret = removefromlists(prevblock);
        if (!ret) fprintf(stderr, "Not able to remove the block from free lists");
        bool bittoset = prevblock->header & PREV_BLOCK_ALLOCATED;
        prevblock->header = prevblock_size + block_size;
        if (bittoset)
            prevblock->header = prevblock->header | PREV_BLOCK_ALLOCATED;
        nextblock->header = nextblock->header & ~PREV_BLOCK_ALLOCATED;
        nextblock->prev_footer = prevblock->header;
        InsertinLists(prevblock);

    }
    else if (!nextblockallocbit)
    {
        //remove the next block from free list
        bool ret = removefromlists(nextblock);
        if (!ret) fprintf(stderr, "Not able to remove the block from free lists");
        size_t nextblock_size = nextblock->header & ~0x3f;
        blocktobefreed->header = block_size + nextblock_size;
        blocktobefreed->header = blocktobefreed->header & ~THIS_BLOCK_ALLOCATED;
        blocktobefreed->header = blocktobefreed->header | PREV_BLOCK_ALLOCATED; //as the prealloc bit is true, actual if cond
        sf_block* lastblock = (sf_block*)((void*)nextblock + nextblock_size);
        lastblock->header = lastblock->header & ~PREV_BLOCK_ALLOCATED;
        lastblock->prev_footer = blocktobefreed->header;
        InsertinLists(blocktobefreed);
    }
    else
    {
        //just free this block
        blocktobefreed->header = blocktobefreed->header & ~THIS_BLOCK_ALLOCATED;
        nextblock->prev_footer = blocktobefreed->header;
        nextblock->header = nextblock->header & ~PREV_BLOCK_ALLOCATED; //setting next block prev alloc bit to 0
        InsertinLists(blocktobefreed);
    }

    return;
}

bool IspointerValid(void* pp)
{
    if (pp == NULL) return false;
    int diff1 = (void*)pp - sf_mem_start();
    int diff2 = sf_mem_end() - (void*)pp;
    if (diff1 <= 64) return false; //can't access prologue
    if (diff2 <= 0) return false;
    if (diff1%64 != 0) return false; //has to be alligned
    sf_block* blocktobefreed = (sf_block*)((void*)pp - 16);
    bool allocbit = blocktobefreed->header & THIS_BLOCK_ALLOCATED;
    if (!allocbit) return false;
    bool preallocbit = blocktobefreed->header & PREV_BLOCK_ALLOCATED;
    if (!preallocbit)
    {
        //So the previous block is free
        //confirm with header and footer of previous block
        //first check with footer
        bool checkone = blocktobefreed->prev_footer & THIS_BLOCK_ALLOCATED;
        if (checkone) return false;
        //check with header
        size_t prevblock_size = blocktobefreed->prev_footer & ~0x3f;
        sf_block* prevblock = (sf_block*)((void*)blocktobefreed - prevblock_size);
        bool checktwo = prevblock->header & THIS_BLOCK_ALLOCATED;
        if (checktwo) return false;
    }

    return true;
}

void *sf_realloc(void *pp, size_t rsize) {

    //verify the validity of the pointer
    bool ret = IspointerValid(pp);
    if (!ret)
    {
        sf_errno = EINVAL;
        return NULL;
    }
    if (rsize == 0)
    {
        sf_free(pp);
        return NULL;
    }
    sf_block* realloc_block = (sf_block*)((void*)pp - 16);
    size_t reallocblock_size = realloc_block->header & ~0x3f;
    if (rsize + 8 > reallocblock_size)
    {
        void* newpayload = sf_malloc(rsize);
        if (newpayload == NULL) return NULL;
        memcpy(newpayload, (void*)pp, reallocblock_size - 8);
        sf_free(pp);
        return (void*)newpayload;
    }
    else if (rsize + 8 < reallocblock_size)
    {
        size_t splitblock_size = (reallocblock_size - (rsize+8));
        if (splitblock_size < 64)
        {
            //what does update the header field if necessary?
            return (void*)pp;
        }
        else
        {
            //have to split the block
            size_t newblock_size = ((size_t)splitblock_size/64)*64;
            size_t reallocblock_newsize = reallocblock_size - newblock_size;
            bool preallocbit = realloc_block->header & PREV_BLOCK_ALLOCATED;
            realloc_block->header = reallocblock_newsize;
            if (preallocbit)
                realloc_block->header = realloc_block->header | PREV_BLOCK_ALLOCATED;
            realloc_block->header = realloc_block->header | THIS_BLOCK_ALLOCATED;
            sf_block* newblock = (sf_block*)((void*)realloc_block + reallocblock_newsize);
            newblock->header = newblock_size;
            newblock->header = newblock->header | PREV_BLOCK_ALLOCATED;
            newblock->header = newblock->header & ~THIS_BLOCK_ALLOCATED;
            sf_block* nextblock = (sf_block*)((void*)newblock + newblock_size);
            nextblock->prev_footer = newblock->header;
            nextblock->header = nextblock->header & ~PREV_BLOCK_ALLOCATED;
            bool nextblockallocated = nextblock->header & THIS_BLOCK_ALLOCATED;
            if (nextblockallocated)
            {
                InsertinLists(newblock);
                return (void*)pp;
            }
            else
            {
                //have to colaesce the newblock with the nextblock
                //remove the block from the lists
                bool ret = removefromlists(nextblock);
                if (!ret) fprintf(stderr, "Not able to remove the block from free lists");
                size_t nextblock_size = nextblock->header & ~0x3f;
                sf_block* lastblock = (sf_block*)((void*)nextblock + nextblock_size);
                newblock->header = newblock_size + nextblock_size;
                newblock->header = newblock->header | PREV_BLOCK_ALLOCATED;
                newblock->header = newblock->header & ~THIS_BLOCK_ALLOCATED;
                lastblock->prev_footer = newblock->header;
                lastblock->header = lastblock->header & ~PREV_BLOCK_ALLOCATED;
                InsertinLists(newblock);
                return (void*)pp;
            }

        }

    }
    else
    {
        //If they requests the same size
        return (void*)pp;
    }

    return NULL;
}

bool removefromlists(sf_block* block)
{
    size_t block_size = block->header & ~0x3f;
    block_size = block_size/64;
    if (block_size < 1) return true; //should not call remove for such kind of blks
    size_t arr_index = 0;
    if (block_size == 1)
        arr_index = 0;
    else if (block_size == 2)
        arr_index = 1;
    else if (block_size == 3)
        arr_index = 2;
    else
    {
        bool FoundIndex = false;
        for (int i = 3; i < NUM_FREE_LISTS - 1; i++)
        {
            if (block_size > FibNum(i) && block_size <= FibNum(i+1))
            {
                arr_index = i;
                FoundIndex = true;
                break;
            }
        }
        if (!FoundIndex) arr_index = NUM_FREE_LISTS - 1;
    }


    //insert at the top
    sf_block* sentinel = &sf_free_list_heads[arr_index];
    sf_block* sentinel_next = sentinel->body.links.next;
    if (sentinel_next == block)
    {
        //then only sentinel is present
        sf_free_list_heads[arr_index].body.links.next = &sf_free_list_heads[arr_index];
        sf_free_list_heads[arr_index].body.links.prev = &sf_free_list_heads[arr_index];
        return true;
    }
    else
    {
        while (sentinel_next != block && sentinel_next != sentinel)
            sentinel_next = sentinel_next->body.links.next;

        if (sentinel_next == block)
        {
            sf_block* prev = sentinel_next->body.links.prev;
            sf_block* next = sentinel_next->body.links.next;
            prev->body.links.next = next;
            next->body.links.prev = prev;
        }

        return true;
    }

    return false;
}

bool initheapandfreelists()
{
    //initialize free lists
    for (int i = 0; i < NUM_FREE_LISTS; i++)
    {
        sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
        sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
    }

    void* heapstart = sf_mem_grow();
    if (heapstart == NULL)
    {
        sf_errno = ENOMEM;
        return false;
    }

    //setting prologue

    sf_block* prologue = (sf_block*)(sf_mem_start() + 8*6);
    //prologue->prev_footer; //should not be accessed
    prologue->header = 64;
    prologue->header = prologue->header | THIS_BLOCK_ALLOCATED;

     sf_block* firstblock = (sf_block*)(sf_mem_start() + 8*6 + 8*8);
    size_t block_size = PAGE_SZ - 128;
    firstblock->header = block_size;
    firstblock->header = firstblock->header | PREV_BLOCK_ALLOCATED;
    InsertinLists(firstblock);

    //setting epilogue
    sf_block* epilogue = (sf_block*)(sf_mem_end() - 16);
    epilogue->header = 0;
    epilogue->header = epilogue->header | THIS_BLOCK_ALLOCATED;
    //prev block is free, so no need to set
    epilogue->prev_footer = firstblock->header;

    return true;
}


void InsertinLists(sf_block* block)
{
    //Fib Series: 1, 2, 3, 5, 8, 13, 21, 34;
    size_t block_size = block->header & ~0x3f;
    block_size = block_size/64;

    //have to use FibNum to find Index
    if (block_size < 1) return;
    size_t arr_index = 0;
    if (block_size == 1)
        arr_index = 0;
    else if (block_size == 2)
        arr_index = 1;
    else if (block_size == 3)
        arr_index = 2;
    else
    {
        bool FoundIndex = false;
        for (int i = 3; i < NUM_FREE_LISTS - 1; i++)
        {
            if (block_size > FibNum(i) && block_size <= FibNum(i+1))
            {
                arr_index = i;
                FoundIndex = true;
                break;
            }
        }
        if (!FoundIndex) arr_index = NUM_FREE_LISTS - 1;
    }


    //insert at the top
    sf_block* sentinel = &sf_free_list_heads[arr_index];
    sf_block* sentinel_next = sentinel->body.links.next;
    sf_block* sentinel_prev = sentinel->body.links.prev;
    //if only sentinel is present.
    if (sentinel_next == &sf_free_list_heads[arr_index] && sentinel_prev == &sf_free_list_heads[arr_index])
    {
        sentinel->body.links.next = block;
        block->body.links.prev = sentinel;
        sentinel->body.links.prev = block;
        block->body.links.next = sentinel;
    }
    else
    {
        sf_block* sent_next = sentinel->body.links.next;
        sentinel->body.links.next = block;
        block->body.links.prev = sentinel;
        block->body.links.next = sent_next;
        sent_next->body.links.prev = block;
    }
}


bool checkfreelists(size_t size, sf_block** block)
{
    size = size + 8; //for the header
    size_t alligned_size = 0;
    int rem = size % 64;
    alligned_size = size/64;
    if (rem > 0) alligned_size++;
    size_t arr_index_start = 0;

    if (alligned_size == 1)
        arr_index_start = 0;
    else if (alligned_size == 2)
        arr_index_start = 1;
    else if (alligned_size == 3)
        arr_index_start = 2;
    else
    {
        bool FoundIndex = false;
        for (int i = 3; i < NUM_FREE_LISTS - 1; i++)
        {
            if (alligned_size > FibNum(i) && alligned_size <= FibNum(i+1))
            {
                arr_index_start = i;
                FoundIndex = true;
                break;
            }
        }
        if (!FoundIndex) arr_index_start = NUM_FREE_LISTS - 1;
    }

    alligned_size *= 64;

    for (int i = arr_index_start; i < NUM_FREE_LISTS; i++)
    {
        sf_block* sent_next = sf_free_list_heads[i].body.links.next;
        while (sent_next != &sf_free_list_heads[i])
        {
            size_t block_size = sent_next->header & ~0x3f;
            if (block_size >= alligned_size)
            {
                *block = sent_next;
                //have to remove this block from free list
                if (sent_next->body.links.next == &sf_free_list_heads[i])
                {
                    //then sentinel points back to sentinel
                    sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
                    sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
                }
                else
                {
                    sf_block* prev = sent_next->body.links.prev;
                    sf_block* next = sent_next->body.links.next;
                    prev->body.links.next = next;
                    next->body.links.prev = prev;
                }
                return true;
            }

            sent_next = sent_next->body.links.next;
        }
    }

    return false;
}